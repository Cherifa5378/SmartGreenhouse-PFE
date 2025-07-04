# Smart Greenhouse Monitoring and Control System

This is my Final Year Project for a Master's in parallel and embedded industrial computing. It is an IoT-based system designed to **monitor environmental conditions in real time** and **remotely control greenhouse actuators**, using FIWARE technologies and low-cost embedded devices.

##  Overview

The system enables:
- Real-time monitoring of both air temperature, air humidity, soil moisture, light, CO₂, water level, pH, water temperature, water flow.
- Automated alerts based on Thresholds
- Remote control of actuators (fan, pump, lamp, servo motor)
- Wireless communication between Arduino and Raspberry Pi using NRF24L01
- Integration with FIWARE's Orion Context Broker using NGSI-LD
-  Web interface for live data visualization 

## Technologies & Components

### Hardware
- **Arduino Uno** (collects sensor data and controls actuators)
- **Raspberry Pi 4** (central controller + sends sensor data to orion context broker and receives commands from web app and send them back to arduino )
- **NRF24L01** wireless modules (Arduino ↔ Raspberry Pi)
- **Sensors**: 
  - DHT22 (temperature/humidity)
  - TEMT6000 (light)
  - MQ-135 (das CO₂)
  - Soil moisture
  - Water level sensor
  - pH sensor
  - DS18B20 (water temperature)
- **Actuators**: 
  - Relay-controlled water pump
  - Fan
  - Lamp
  - SG90 servo motor

### Software
- Python (Flask, Requests)
- FIWARE stack: Orion Context Broker, MongoDB
- Docker (for containerized services)
- JavaScript + HTML + CSS + Bootstrap  (for web interface)
  

##  How It Works

1. Sensors send data from Arduino to Raspberry Pi via NRF24L01
2. Raspberry Pi receives, formats, and forwards data to Orion (NGSI-LD)
3. An http server gets notified every time an atribute (for sensor) changes its value and store the history of all sensors values in mongodb in a collection
4. A web dashboard displays live data using Chart.js
5. Commands (e.g., `FAN:ON`) sent from Orion via PATCH
6. A second server gets notified every time an atribute (for actuator) changes its value
7. Commands are relayed back to Arduino to control actuators

## How to Run
### Option 1: Manual Deployment
1. Upload Arduino code on Aduino IDE
2. Run `combined.py` on your Raspberry Pi on Thonny 
3. Start docker-compose.yml file (Docker Desktop recommended)

