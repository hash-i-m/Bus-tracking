# Bus-tracking
# IoT-Based Real-Time Bus Tracking System
 
An end-to-end embedded IoT system for real-time bus tracking, combining GPS acquisition, cellular data transmission, secure backend communication, and live visualization. Built as a group project.
 
## Overview
 
This system enables real-time tracking of a bus's location, speed, heading, and timestamp using an ESP32 microcontroller as the central controller. Location data is acquired via a NEO-6M GPS module and transmitted over cellular (GSM/GPRS) using a SIM900A module to a backend server, where it is authenticated, processed, and made available through a live tracker application.
 
## Features
 
- Real-time acquisition of GPS coordinates, speed, time, and heading
- GSM/GPRS-based data transmission (no WiFi/internet dependency required on the bus)
- Custom power subsystem regulating the bus's 12V/24V line to a stable 5V rail
- JSON-based tracking payloads sent over TCP/IP and HTTP
- HMAC-SHA256 authentication on all transmitted data
- Replay-attack protection via incremental counters
- Relay-server backend architecture for processing and routing data
- Live visualization through a tracker application
## System Architecture
 
```
[NEO-6M GPS] --> [ESP32] --> [SIM900A GSM/GPRS] --> [Relay Server] --> [Backend / Database] --> [Tracker App]
                     ^
                     |
            [LM2596 Buck Converter]
            (12/24V bus supply -> 5V)
```
 
## Hardware
 
| Component | Role |
|---|---|
| ESP32 | Main controller — GPS parsing, payload construction, GSM communication |
| NEO-6M | GPS module — location, speed, heading, time |
| SIM900A | GSM/GPRS module — cellular data transmission |
| LM2596 Buck Converter | Steps down bus 12V/24V supply to a regulated 5V rail |
 
## Firmware
 
The ESP32 firmware:
- Parses raw NMEA sentences from the NEO-6M GPS module
- Constructs structured JSON tracking payloads (coordinates, speed, heading, timestamp)
- Communicates with the backend over GSM/GPRS using TCP/IP and HTTP
- Signs outgoing payloads with HMAC-SHA256 for authentication
- Attaches an incrementing counter to each payload to prevent replay attacks
## Backend
 
- Relay-server architecture receives and validates incoming device data
- Verifies HMAC-SHA256 signatures and replay counters before accepting data
- Applies route-state logic to processed location data
- Persists authenticated tracking data to a database
- Serves live location data to a tracker application for visualization
## Security
 
- **Authentication:** HMAC-SHA256 signing ensures tracking data originates from a legitimate, provisioned device
- **Replay protection:** incremental counters reject duplicate or replayed payloads
- **Power isolation:** regulated 5V rail via LM2596 protects downstream electronics from the bus's unstable 12/24V supply
## Team
 
This was built as a group project. Contributors:
- *Arundas S G,Hisham Anjumukkil,Adil pp*
## Setup
 
1. Flash the ESP32 firmware (`/firmware`) using Arduino IDE / PlatformIO
2. Wire NEO-6M and SIM900A to the ESP32 per the wiring diagram (`/docs`)
3. Configure GSM APN and backend server credentials in `config.h`
4. Deploy the backend relay server (`/server`)
5. Connect the tracker application to the backend's live data endpoint
## Status
 
Actively developed / completed as part of coursework at NIT Calicut.
