import RPi.GPIO as GPIO         
import spidev                   
import time                     
import requests                
from lib_nrf24 import NRF24     
from http.server import BaseHTTPRequestHandler, HTTPServer  
import json                     
import threading               
from queue import Queue        


ip="192.168.11.154"

#  Configuration NRF24L01 
GPIO.setmode(GPIO.BCM)        
pipes = [[0xE0, 0xE0, 0xF1, 0xF1, 0xE0]]  # Adresse de communication 
radio = NRF24(GPIO, spidev.SpiDev())     # Initialisation du module radio avec SPI
radio.begin(0, 25)                        
radio.setRetries(15, 15)                 # Délais et nombre de tentatives d’envoi
radio.setPayloadSize(32)                 
radio.setChannel(0x76)                   
radio.setDataRate(NRF24.BR_1MBPS)        # Vitesse de transmission 
radio.setPALevel(NRF24.PA_MAX)           # Puissance d’émission maximale
radio.setAutoAck(True)                   # Activation de l’accusé de réception automatique
radio.enableDynamicPayloads()            # Active la taille dynamique des messages
radio.openReadingPipe(1, pipes[0])       # Ouvre un canal pour écouter les messages
radio.openWritingPipe(pipes[0])          # Prépare le canal d’envoi (même adresse que la lecture)
radio.startListening()  

#  Configuration Orion 
ORION_URL = (
    "http://"+ip+":1026/ngsi-ld/v1/entities/"
    "urn:ngsi-ld:SmartGreenhouse:Sensor001/attrs"
)
CONTEXT_URL = "https://schema.lab.fiware.org/ld/context" # Contexte JSON-LD de FIWARE
HEADERS = {
    "Content-Type": "application/json",
    "Fiware-Service": "openiot",
    "Link": f'<{CONTEXT_URL}>; rel="http://www.w3.org/ns/json-ld#context";'
            ' type="application/ld+json"'
}# En-têtes HTTP requis pour les requêtes NGSI-LD

# Queue et flag pour commandes
command_queue = Queue()           # File d’attente pour stocker les commandes à envoyer à l’Arduino
processing_command = threading.Event()  # Indique si une commande est en cours de traitement


def check_orion_connection():
    try:
        r = requests.get("http://"+ip+":1026/version", timeout=5)# Vérifie si Orion répond
        r.raise_for_status()
        ent = requests.get(
            "http://"+ip+":1026/ngsi-ld/v1/entities/"
            "urn:ngsi-ld:SmartGreenhouse:Sensor001",
            headers=HEADERS,
            timeout=5
        ) # Vérifie si l'entité existe
        if ent.status_code == 404: 
            create_entity()  
        return True
    except Exception as e:
        print(f"Orion connection failed: {e}")
        return False


def create_entity():
    data = {
        "id": "urn:ngsi-ld:SmartGreenhouse:Sensor001",
        "type": "SmartGreenhouse",
        **{k: {"type": "Property", "value": 0}
           for k in ["temperature","humidity","light","gas",
                     "soil","water","ph","flow","watertemp"]},
        "fan": {"type": "Property", "value": "OFF"},
        "pump": {"type": "Property", "value": "OFF"},
        "lamp": {"type": "Property", "value": "OFF"},
        "servo": {"type": "Property", "value": "0"}
        
    }
    try:
        r = requests.post(
            "http://"+ip+":1026/ngsi-ld/v1/entities",
            headers=HEADERS,
            json=data,
            timeout=5
        )  # Envoie la requête de création
        print(f"Created entity, status {r.status_code}")
    except Exception as e:
        print(f"Entity creation failed: {e}")

LOG_PREFIX = "[RF24-Radio]"
# Envoie des données capteurs à Orion
def send_sensor_data(data):
    payload = {k: {"type": "Property", "value": v} for k, v in data.items()}
    # Affiche le payload
    #print(f"{LOG_PREFIX} Envoi vers Orion – payload:", json.dumps(payload))
    try:
        r = requests.patch(ORION_URL, headers=HEADERS, json=payload, timeout=5)
        print(f"{LOG_PREFIX} Orion update status: {r.status_code}")
    except Exception as e:
        print(f"{LOG_PREFIX} Orion update failed: {e}")

# Buffer parser 
import re
# Interprétation du message radio
# extraire chaque valeur des capteurs(int or float)
def parse_buffer(buf: str) -> dict:
    d = {}
    # Temperature
    m = re.search(r'T:([\d\.]+)C', buf)
    if m: d['temperature'] = float(m.group(1))
    # Humidity
    m = re.search(r'H:([\d\.]+)%', buf)
    if m: d['humidity'] = float(m.group(1))
    # Light (lux)
    m = re.search(r'L:\s*([\d]+)lux', buf)
    if m: d['light'] = int(m.group(1))
    # Water temperature
    m = re.search(r'WT:([\d\.]+)C', buf)
    if m: d['watertemp'] = float(m.group(1))
    # Soil humidity
    m = re.search(r'S:\s*([\d]+)%', buf)
    if m: d['soil'] = int(m.group(1))
    # Water level
    m = re.search(r'W:\s*([\d]+)%', buf)
    if m: d['water'] = int(m.group(1))
    # Flow
    m = re.search(r'P:([\d\.]+)L', buf)
    if m: d['flow'] = float(m.group(1))
    #gas
    m = re.search(r'G:([\d\.]+)V', buf)
    if m: d['gas'] = float(m.group(1))
    #ph
    m = re.search(r'PH:([\d\.]+)(?:V)?', buf)
    if m: d['ph'] = float(m.group(1))
    return d

# thread gère l'envoi des commandes vers l'Arduino
def handle_commands():
    while True:
        cmd = command_queue.get()  # Prend une commande dans la file
        processing_command.set()   # Indique qu’une commande est en cours
        radio.stopListening()      # Arrête l’écoute pour envoyer
        success = radio.write(cmd.encode('utf-8'))   # Envoie la commande à l’Arduino 
        print(f"Command sent: {cmd}, ok={success}")
        time.sleep(0.5)
        radio.startListening()     # Reprend l’écoute
        processing_command.clear() # Fin de traitement

# Dictionnaire pour stocker les dernières valeurs connues des actionneurs
last_values = {
    "fan": None,
    "pump": None,
    "lamp": None,
    "servo": None
}

# HTTP server for notifications
class NotifyHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0)) # Longueur du corps JSON
        raw = self.rfile.read(length) # Lecture du corps de la requête
        try:
            data = json.loads(raw)  # Chargement JSON en dictionnaire
        except json.JSONDecodeError:
            print(f"{LOG_PREFIX}  JSON invalide reçu : {raw!r}")
            self.send_response(400)
            self.end_headers()
            return
        #print(f"{LOG_PREFIX} Notification reçue :", json.dumps(data))

        # Parcours des entités dans la notification
        for ent in data.get('data', []):  # Pour chaque entité reçue
            if ent.get('id') != 'urn:ngsi-ld:SmartGreenhouse:Sensor001':
                continue

            # Pour chaque attribut surveillé, si présent, on envoie la commande correspondante
            for attr in ('fan', 'pump', 'lamp', 'servo'):
                if attr in ent:
                    val = ent[attr].get('value')
                    # Envoie uniquement si la valeur a changé
                    if val != last_values[attr]:
                        last_values[attr] = val  # Mise à jour de l'état
                        print(f"{LOG_PREFIX}  → commande {attr.upper()} détectée: {val}")
                        cmd = f"{attr.upper()}:{val}"
                        command_queue.put(cmd) # Ajoute à la file de commande
        self.send_response(200)  # Répond OK au serveur Orion
        self.end_headers()

def run_server():
    server = HTTPServer(('0.0.0.0', 5001), NotifyHandler)
    print("Notification server listening on 5001")
    server.serve_forever()

# Main loop
if __name__ == '__main__':
    if not check_orion_connection():
        exit(1) # Stoppe le programme si Orion ne répond pas

    threading.Thread(target=handle_commands, daemon=True).start() # Lance thread commandes
    threading.Thread(target=run_server, daemon=True).start()      # Lance thread serveur HTTP

    buffer = ''
    while True:
        if processing_command.is_set():
            time.sleep(0.1)
            continue
        if radio.available(): # Si message radio disponible
            payload = []
            radio.read(payload, radio.getDynamicPayloadSize()) # Lit le message
            part = ''.join(chr(n) for n in payload if n > 0)   # Convertit en texte
            if part == 'END': # Fin du message
                data = parse_buffer(buffer)  # Parse le message complet
                if data:
                    send_sensor_data(data)   # Envoie à Orion
                buffer = ''                  # Réinitialise le buffer
            else:
                buffer += part               # Ajoute la partie au message total
        time.sleep(0.1)
