# NEXIS IoT Bike Lock

A smart bike security system that combines a physical locking mechanism with motion sensing, tamper detection, GPS-based geofencing, and real-time alerts.

## Overview

The NEXIS IoT Bike Lock is an embedded security project designed to improve bike protection beyond a standard passive lock. The system uses an on-bike ESP32-based device to detect suspicious activity, monitor wire-loop integrity, track location, and notify the user when a theft-related event occurs.

This project is currently being developed as an MVP focused on campus use, with connectivity through WiFi and alert delivery through a phone app or web-based interface.

## Core Features

- Motion detection using an MPU6050 accelerometer/gyroscope
- Conductive wire loop for tamper detection
- GPS-based geofencing and location tracking
- Audible buzzer for local warning/alarm behavior
- WiFi connectivity for sending alerts
- User-side control for arming, disarming, and viewing alerts

## How It Works

The system uses multiple detection layers:

1. **Wire loop integrity**
   - If the conductive loop is cut or disconnected, the device detects a break in continuity and sends an alert.

2. **Motion detection**
   - If movement passes a defined threshold for long enough, the device treats it as suspicious activity and triggers a warning or alarm.

3. **Geofence breach**
   - If the bike moves outside a user-defined boundary, the device sends an alert and provides GPS location data.

## Planned Alert Behavior

- **Light movement:** log event, no alarm
- **Sustained movement:** warning buzzer + user alert
- **Wire cut or geofence breach:** full alert/alarm + location update

## System Architecture

### On-Bike Unit

- **ESP32** — main microcontroller and WiFi communication
- **MPU6050** — motion and tilt sensing
- **NEO-6M GPS module** — location tracking and geofencing
- **Conductive wire loop** — tamper detection
- **Piezo buzzer** — local audio alert
- **18650 battery + TP4056** — portable power and charging
- **Weather-resistant enclosure** — mounting and protection

### User Side

The user interface is planned to support:

- Arm/disarm controls
- Geofence setup
- Push notifications
- Bike location display
- Status monitoring

## Tech Stack

### Hardware
- ESP32
- MPU6050
- NEO-6M GPS
- Piezo buzzer
- 18650 battery
- TP4056 charging module
- Conductive wire/cable loop

### Software
- Embedded C/C++ for ESP32 firmware
- WiFi-based communication
- Firebase or similar notification backend
- Mobile app or web dashboard for user control

## Current Goals

- Build a breadboard prototype
- Test ESP32 connectivity over WiFi
- Implement motion threshold detection
- Test wire continuity monitoring
- Prototype GPS/geofence logic
- Define alert pipeline from device to user interface

## Open Design Questions

This project is still actively being defined and prototyped. Current engineering questions include:

- What motion threshold best avoids false positives?
- How should the wire physically reconnect to the enclosure?
- How should battery life be optimized?
- How should the device authenticate to campus WiFi?
- Should GPS remain active at all times or only after motion events?
- What backend/event format should be used for alerts?

## Repository Structure

Planned structure:

```text
nexis-iot-bike-lock/
├── firmware/         # ESP32 code
├── hardware/         # wiring notes, schematics, parts info
├── app/              # mobile app or dashboard
├── docs/             # design notes, architecture, diagrams
└── README.md
