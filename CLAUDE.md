# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
IoT bike lock system: ESP32 hardware detects theft via accelerometer movement + GPS geofence breach, triggers a physical buzzer alert, and sends real-time alerts to a web dashboard.

**Two components:**
1. **Firmware** (Arduino/C++) — runs on ESP32, handles sensors, detection, buzzer, WiFi provisioning via BLE
2. **Web app** (React + Firebase) — real-time dashboard showing lock status, GPS location on a map, alert history

## Architecture

```
ESP32 (on bike)
  ├── Detects: motion (MPU6050), geofence breach (GT-U7 GPS), sound (KY-037)
  ├── Physical alert: passive buzzer on GPIO15
  ├── WiFi: connects to network via BLE provisioning (NRFconnect)
  └── On theft detection:
        └── HTTPS POST → Firebase Firestore REST API
              └── Write to /alerts collection
              └── Update /locks/{lockId}/status

Web Dashboard (React)
  ├── Firebase Hosting (static)
  ├── Firestore realtime listener → instant alert updates
  ├── Leaflet.js map → shows bike GPS location + geofence radius
  ├── Alert history log
  └── Arm/disarm control (writes to Firestore, ESP32 polls or listens)
```

## Tech Stack

### Firmware
- **Board:** ESP-WROOM-32
- **Language:** Arduino C++
- **Libraries:** TinyGPSPlus, ArduinoJson, BLE, WiFi, Preferences
- **Sensors:** MPU6050 (accel), GT-U7 (GPS), KY-037 (sound), passive buzzer

### Web App
- **Frontend:** React (Vite)
- **Styling:** Tailwind CSS
- **Map:** Leaflet.js + react-leaflet
- **Backend/DB:** Firebase (Firestore, Hosting, Auth)
- **No server.** ESP32 writes directly to Firestore via REST API. Dashboard reads via Firestore SDK with realtime listeners.

### Notifications
- **Primary:** Browser push notifications from dashboard (Notification API)
- **Secondary:** Discord webhook — ESP32 sends a second HTTP POST to a Discord webhook URL on theft detection for mobile push via Discord app

## Project Structure

```
NEXIS_Bike_Lock/
├── CLAUDE.md
├── firmware/
│   ├── detection.ino        # Sensor detection, state machine, buzzer, alert logic
│   ├── provisioning.ino     # BLE WiFi provisioning, credential storage
│   └── combined/            # Merged firmware (TODO: combine both .ino files)
│       └── nexis_lock.ino
├── web/
│   ├── package.json
│   ├── vite.config.js
│   ├── tailwind.config.js
│   ├── firebase.json         # Hosting config
│   ├── .firebaserc
│   ├── src/
│   │   ├── main.jsx
│   │   ├── App.jsx
│   │   ├── firebase.js       # Firebase config + init
│   │   ├── components/
│   │   │   ├── Map.jsx        # Leaflet map with bike marker + geofence circle
│   │   │   ├── AlertList.jsx  # Real-time alert history
│   │   │   ├── StatusPanel.jsx # Lock state: armed/disarmed/alert
│   │   │   └── Controls.jsx   # Arm/disarm buttons
│   │   └── hooks/
│   │       ├── useAlerts.js   # Firestore listener for /alerts
│   │       └── useLockStatus.js # Firestore listener for /locks/{id}/status
│   └── public/
│       └── index.html
└── docs/
    └── wiring.md             # Pin assignments, hardware notes
```

## Hardware Pin Map

| Component       | Pin    | GPIO |
|-----------------|--------|------|
| KY-037 AO       | Analog | 4    |
| KY-037 DO       | Digital| 23   |
| MPU6050 SDA     | I2C    | 21   |
| MPU6050 SCL     | I2C    | 22   |
| GT-U7 GPS TXD   | UART   | 16   |
| GT-U7 GPS RXD   | UART   | 17   |
| Passive Buzzer + | PWM    | 15   |

## Firmware State Machine

```
DISARMED → (arm command) → ARMING → (GPS fix) → ARMED
ARMED → (1-2 events) → WARNING → (3+ events) → ALERT
ARMED → (geofence breach) → ALERT
WARNING → (no events) → ARMED
ALERT → (disarm only) → DISARMED
```

## Detection Thresholds (current firmware values)
- **Motion:** raw accel delta > 8000, 3 events in 5s → ALERT
- **Sound:** adaptive baseline, spike > 1.5x baseline, 3 events in 5s → ALERT
- **Geofence:** 50m radius from anchor point, checked every 10s
- **Warmup:** 3s after arming (motion/sound ignored, GPS still active)

## Firestore Schema

### /locks/{lockId}
```json
{
  "state": "DISARMED|ARMING|ARMED|WARNING|ALERT",
  "lat": 43.0381,
  "lng": -76.1347,
  "anchorLat": 43.0381,
  "anchorLng": -76.1347,
  "geofenceRadius": 50,
  "batteryPct": 87,
  "lastSeen": "2026-04-10T12:00:00Z",
  "wifiSSID": "AirOrangeX",
  "ip": "10.1.225.34"
}
```

### /alerts/{alertId}
```json
{
  "lockId": "nexis_001",
  "type": "TAMPERING|GEOFENCE_BREACH",
  "timestamp": "2026-04-10T12:05:00Z",
  "lat": 43.0382,
  "lng": -76.1345,
  "distFromAnchor": 52.3,
  "motionEvents": 3,
  "soundEvents": 1,
  "acknowledged": false
}
```

## Key Tasks

### Firmware
1. **Merge provisioning.ino + detection.ino** into a single combined firmware
2. **Add WiFi HTTP client** — on ALERT state, POST alert data to Firestore REST API
3. **Add Discord webhook** — second HTTP POST on alert
4. **Add Firestore polling** — periodically check /locks/{id} for remote arm/disarm commands
5. **Add status heartbeat** — periodic PUT to /locks/{id} with GPS, state, battery

### Web App
1. **Firebase project setup** — Firestore, Hosting, Auth (anonymous or email)
2. **Dashboard layout** — map + status panel + alert list + controls
3. **Real-time Firestore listeners** — onSnapshot for alerts and lock status
4. **Leaflet map** — bike marker, geofence circle overlay, auto-center on location update
5. **Arm/disarm controls** — write to Firestore, ESP32 picks up on next poll
6. **Browser notifications** — request permission, fire on new alert
7. **Alert history** — scrollable list, newest first, with ack button

## Dev Commands

```bash
# Web app
cd web
npm install
npm run dev          # Vite dev server
npm run build        # Production build
firebase deploy      # Deploy to Firebase Hosting

# Firebase emulator (local dev)
firebase emulators:start

# Firmware
# Open firmware/combined/nexis_lock.ino in Arduino IDE
# Board: ESP32 Dev Module
# Upload speed: 115200
```

## Conventions
- Use functional React components with hooks
- Tailwind for all styling, no CSS files
- Firebase v9+ modular SDK (`import { getFirestore } from 'firebase/firestore'`)
- All Firestore reads use `onSnapshot` for real-time, not `getDocs`
- Keep firmware `#define` thresholds at top of file for easy tuning
- Timestamps in ISO 8601 / Firestore Timestamp
- Console errors and edge cases handled — don't leave unhandled promise rejections

## WiFi Context
- Campus network: AirOrangeX (open, no password)
- ESP32 current IP: 10.1.225.34 (DHCP, may change)
- BLE provisioning via NRFconnect sends JSON: `{"ssid":"...","passphrase":"..."}`

## Notes
- Hardware is complete and soldered — do not suggest hardware changes
- No iOS/Android app — web only
- No paid services required — Firebase free tier + Discord webhook
- Syracuse University campus deployment
