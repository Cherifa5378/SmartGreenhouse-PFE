#include <SPI.h>                     
#include <RF24.h>                    
#include <DHT.h>                     
#include <OneWire.h>                 
#include <DallasTemperature.h>       

#include <Servo.h>                   
Servo servoMotor;                   

#define DHTPIN 2 //capteur dht11 place dans broche D2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define ONE_WIRE_BUS 3 //capteur temperature de l'eau 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensor(&oneWire);

const int temtPin = A0;// capteur de lumière (TEMT6000)
const int soilPin = A1;// humidité du sol
const int mqPin   = A2;// gaz MQ-135 
const int phPin   = A3;// capteur de pH
const int waterPin = A5;// niveau d’eau

const byte flowPin = 4; //Capteur débit de l'eau 
volatile unsigned int pulseCount = 0;

unsigned long oldTime = 0; 
float flowRate = 0.0; 

#define PIN_FAN 7//ventilateur 
bool commandMode = false;
#define PIN_PUMP 5//pompe d'eau 
#define PIN_SERVO 6// servo moteur sg9O
#define PIN_LAMP 10//lampe

RF24 radio(9, 8); // nrfl0124 utilise les broches CE=9, CSN=8.
const uint64_t pipe = 0xE0E0F1F1E0LL;// Adresse de communication radio
//Fonction de comptage d’impulsions
void pulseCounter() { pulseCount++; }// Incrément à chaque impulsion détectée

void setup() {
  Serial.begin(9600); 
  dht.begin();
  waterTempSensor.begin();
  pinMode(flowPin, INPUT_PULLUP);// Entrée avec résistance interne activée
  attachInterrupt(digitalPinToInterrupt(flowPin), pulseCounter, RISING);// Interruption sur front montant

  pinMode(PIN_FAN, OUTPUT);// Broche ventilateur en sortie
  digitalWrite(PIN_FAN, HIGH);
  pinMode(PIN_PUMP, OUTPUT);// Pompe en sortie
  digitalWrite(PIN_PUMP, HIGH); 
  //digitalWrite(PIN_PUMP, HIGH); // par défaut OFF
  pinMode(PIN_LAMP, OUTPUT);// Lampe en sortie
  digitalWrite(PIN_LAMP, HIGH);
  servoMotor.attach(PIN_SERVO);// Lier le servo à sa broche
  servoMotor.write(0);  

  if (!radio.begin()) { Serial.println("NRF24 non détecté!"); while(1); } 
  radio.setPALevel(RF24_PA_MAX);// Puissance d’émission maximale
  radio.setDataRate(RF24_1MBPS);// Vitesse 1Mbps
  radio.setChannel(0x76);
  radio.openWritingPipe(pipe);// Canal d’émission
  radio.openReadingPipe(1, pipe);// Canal de réception
  radio.enableDynamicPayloads();// Activer la taille dynamique des messages
  radio.startListening();
  oldTime = millis(); 
}


void loop() { 
  if (radio.available()) {
    uint8_t len = radio.getDynamicPayloadSize();
    if (len > 0 && len < 32) {
      char buf[32] = {0};// Buffer de réception
      radio.read(buf, len);
      String cmd(buf); // Le convertir en String
      // Identifier la commande reçue
      if (cmd.startsWith("FAN:")) {
        commandMode = true;
        handleFan(cmd);
        commandMode = false;
      } else if (cmd.startsWith("PUMP:")) {
        handlePump(cmd);
      } else if (cmd.startsWith("LAMP:")) {
        handleLamp(cmd);
      } else if (cmd.startsWith("SERVO:")) {
      String val = cmd.substring(6);
      int angle = val.toInt();
      if (angle >= 0 && angle <= 180) {
        handleServo(cmd);
      } else {
        Serial.println("⚠ Servo angle invalide ignoré");
      }
    }

    } else {
      radio.flush_rx(); // Si message corrompu, vider
    }
    return;
  }
  //Toutes les 3 secondes, si on ne traite pas de commande, on envoie les données des capteurs
  if (!commandMode && millis() - oldTime >= 3000) {
    sendMeasurements();
    oldTime = millis();// Mise à jour du timer
  }
}
//Fonctions de traitement des commandes
void handleFan(String cmd) {
  String val = cmd.substring(cmd.indexOf(':') + 1);
  if (val == "OFF") {
    digitalWrite(PIN_FAN, HIGH); // relais fermé, ventilateur ON
  } else {
    digitalWrite(PIN_FAN, LOW); // relais ouvert, ventilateur OFF
  }
  Serial.print("Ventilateur: ");
  Serial.println(val);
}

void handlePump(String cmd) {
  String val = cmd.substring(cmd.indexOf(':') + 1);
  // ACTIVE-LOW : ON = LOW, OFF = HIGH
  //digitalWrite(PIN_PUMP, val == "ON" ? HIGH:LOW);
  digitalWrite(PIN_PUMP, val == "OFF" ? LOW:HIGH);
  Serial.print("Pompe: "); Serial.println(val);
}
void handleLamp(String cmd) {
  String val = cmd.substring(cmd.indexOf(':') + 1);
  // ACTIVE-LOW : ON = LOW, OFF = HIGH
  //digitalWrite(PIN_LAMP, val == "ON" ? HIGH:LOW);
  digitalWrite(PIN_LAMP, val == "OFF" ? LOW:HIGH);
  Serial.print("Lamp: "); Serial.println(val);
}
void handleServo(String cmd) {
  String val = cmd.substring(cmd.indexOf(':') + 1);
  int angle = val.toInt();
  angle = constrain(angle, 0, 180);
  servoMotor.write(angle);
  Serial.print("Servo angle: "); Serial.println(angle);
}

//Fonction envoie de mesures 
void sendMeasurements() {
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();
  waterTempSensor.requestTemperatures();// Demander mesure au DS18B20
  float watertemp   = waterTempSensor.getTempCByIndex(0);// Lire température de l'eau 

  int lightRaw    = analogRead(temtPin);
  float lightLux  = lightRaw * (500.0 / 1023.0);
  int soilRaw     = analogRead(soilPin);
  int soilPercent = map(soilRaw, 1023, 300, 0, 100);
  soilPercent     = constrain(soilPercent, 0, 100);
  int waterRaw    = analogRead(waterPin);
  int waterPercent= map(waterRaw, 1023, 300, 0, 100);
  waterPercent    = constrain(waterPercent, 0, 100);
  int gasRaw      = analogRead(mqPin);
  float gasVoltage= gasRaw * (5.0 / 1023.0);
  // Lecture et conversion du pH
  int phRaw       = analogRead(phPin);// Lire tension du capteur pH
  float phVoltage = phRaw * (5.0 / 1023.0);
  // Conversion en valeur de pH (à adapter selon capteur)
  float phValue   = 7.0 + ((2.5 - phVoltage) * 3.5);

  detachInterrupt(digitalPinToInterrupt(flowPin));//Lire debit de leau 
  flowRate = pulseCount / 7.5;
  pulseCount = 0;
  attachInterrupt(digitalPinToInterrupt(flowPin), pulseCounter, RISING);
  
  // Création de buffers pour contenir les chaînes formatées
  char tStr[8], hStr[8], lStr[8], sStr[8], wStr[8],
       pStr[8], fStr[8], wtStr[8], gStr[8], phStr[8];
  
  dtostrf(isnan(temperature) ? -1 : temperature, 4, 1, tStr);
  dtostrf(isnan(humidity)    ? -1 : humidity,    4, 1, hStr);
  dtostrf(lightLux,           4, 0, lStr);
  dtostrf(isnan(watertemp)   ? -1 : watertemp,   4, 1, wtStr);
  dtostrf(soilPercent,        4, 0, sStr);
  dtostrf(waterPercent,       4, 0, wStr);
  dtostrf(isnan(flowRate)    ? -1 : flowRate,    4, 2, fStr); 
  dtostrf(isnan(gasVoltage)  ? -1 : gasVoltage,  4, 2, gStr);
  dtostrf(isnan(phValue)     ? -1 : phValue,     4, 2, phStr); 

   char msg[160];
  snprintf(msg, sizeof(msg),
    "T:%sC H:%s%% L:%slux WT:%sC S:%s%% W:%s%% G:%sV F:%sL PH:%s END",
    tStr, hStr, lStr, wtStr, sStr, wStr, gStr, fStr, phStr
  );  

  //  Envoyer le message par paquets de 32 octets
  const int chunkSize = 32;
  int len = strlen(msg);
  radio.stopListening();
  for (int i = 0; i < len; i += chunkSize) {
    char chunk[33] = {0};//déclare un tableau de 33 caractères et initialise tous ces éléments à 0 ('\0')
    strncpy(chunk, msg + i, chunkSize);
    radio.write(chunk, strlen(chunk) + 1); //envoie le contenu avec le '\0' final (grâce au +1)
    delay(10);
  }
  radio.write("END", 4);
  radio.startListening();// Passer en mode émission
  Serial.print("Mesures envoyées: ");
  Serial.println(msg);

}