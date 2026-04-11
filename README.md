# NEXIS Bike Lock

IoT bike security system — ESP32 on the bike detects theft via accelerometer + GPS geofence, triggers a buzzer, and sends real-time alerts to a web dashboard.

## Architecture

```
ESP32 (on bike)
  ├── Detects: motion (MPU6050), geofence breach (GT-U7 GPS)
  ├── Physical alert: passive buzzer on GPIO15
  ├── WiFi: provisioned via BLE (NRFconnect app)
  └── On theft: HTTPS POST → Firestore REST API

Web Dashboard (React + Firebase)
  ├── Firestore realtime listeners → instant alert updates
  ├── Leaflet.js map → GPS location + geofence overlay
  ├── Alert history with acknowledge
  └── Arm/disarm control → writes to Firestore → ESP32 polls
```

## Tech Stack

**Firmware** — Arduino C++ on ESP32-WROOM-32  
Libraries: TinyGPSPlus, ArduinoJson v7, BLE, WiFi, Preferences, esp_task_wdt

**Web app** — React + Vite + Tailwind CSS  
Map: Leaflet.js + react-leaflet  
Backend: Firebase Firestore + Hosting (no server)

## Hardware

| Component      | Interface | GPIO |
|----------------|-----------|------|
| MPU6050 SDA    | I2C       | 21   |
| MPU6050 SCL    | I2C       | 22   |
| GT-U7 GPS TXD  | UART      | 16   |
| GT-U7 GPS RXD  | UART      | 17   |
| Passive Buzzer | PWM       | 15   |

## Repository Structure

```
firmware/
├── detection.ino             # Motion detection, state machine, buzzer
├── provisioning.ino          # BLE WiFi provisioning (reference)
└── combined/
    ├── nexis_lock.ino        # Production firmware
    └── nexis_lock_debug.ino  # Debug firmware (serial inject commands)
web/
├── src/
│   ├── App.jsx
│   ├── firebase.js
│   ├── components/           # Map, StatusPanel, Controls, AlertList
│   └── hooks/                # useLockStatus, useAlerts, useNotifications
└── firebase-test.html        # Standalone Firebase connection test
```

## Setup

### Web app
```bash
cd web
npm install
npm run dev        # Vite dev server → localhost:5173
npm run build
firebase deploy    # Deploy to Firebase Hosting
```

### Firmware
Open `firmware/combined/nexis_lock.ino` in Arduino IDE.  
Board: ESP32 Dev Module · Upload speed: 115200  
First boot: use NRFconnect to provision WiFi via BLE.

### Firebase connection test
Run `npm run dev` then open `http://localhost:5173/firebase-test.html`.

## Current Status

| Component | Status |
|---|---|
| Hardware | Complete and soldered |
| Web dashboard | Complete — Firestore realtime, map, alerts, arm/disarm |
| Firebase ↔ web | Connected and verified |
| Firmware cloud integration | TODOs — `sendHeartbeat()`, `sendAlert()`, Firestore polling |
