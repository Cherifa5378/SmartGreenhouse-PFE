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
- **Arduino Uno ** (collects sensor data and controls actuators)
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

## 📂 Repository Structure

