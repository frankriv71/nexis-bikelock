// ============================================================
//  NEXIS Bike Lock — Dummy / Debug Firmware
//
//  For software development when hardware sensors are unavailable.
//  Replaces MPU6050 and GT-U7 GPS with simulated data.
//  WiFi, BLE provisioning, state machine, and buzzer are real.
//
//  Extra serial commands (debug only):
//    inject motion  — simulate a motion detection event
//    inject breach  — move dummy GPS outside geofence (~100m shift)
//    inject fix     — restore dummy GPS to anchor/default position
//
//  Standard commands still work:
//    arm, disarm, status, gps
// ============================================================

#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>

// ---- Identity ----
#define LOCK_ID               "nexis_001"

// ---- Pin definitions (buzzer only — no sensors wired) ----
#define BUZZER_PIN            15

// ---- BLE provisioning ----
#define BLE_DEVICE_NAME          "NEXIS_LOCK"
#define BLE_SERVICE_UUID         "021a9004-0382-4aea-bff4-6b3f1c5adfb4"
#define BLE_CHAR_UUID            "021a9005-0382-4aea-bff4-6b3f1c5adfb4"
#define BLE_PROVISION_TIMEOUT_MS  60000

// ---- Buzzer tones ----
#define TONE_ARM_CONFIRM      1000
#define TONE_DISARM_CONFIRM    500
#define TONE_WARNING          2000
#define TONE_ALERT            4000

// ---- Motion detection ----
#define MOTION_EVENTS_TO_ALERT 3
#define MOTION_WINDOW_MS       5000
#define MOTION_COOLDOWN_MS     500

// ---- GPS geofence ----
#define GEOFENCE_RADIUS_M      50.0
#define GPS_CHECK_INTERVAL     10000
#define MIN_SATELLITES         4

// ---- Warmup after arming ----
#define WARMUP_MS              3000

// ---- Dummy GPS (Syracuse University campus) ----
#define DUMMY_DEFAULT_LAT      43.038100
#define DUMMY_DEFAULT_LNG     -76.134700
#define DUMMY_BREACH_LAT       43.039000   // ~100m north of default
#define DUMMY_BREACH_LNG      -76.134700

// ============================================================
//  BLE PROVISIONING
// ============================================================
Preferences prefs;
BLEServer* pServer = nullptr;
BLECharacteristic* pProvChar = nullptr;
bool bleRunning = false;
String blePayload = "";
volatile bool credentialsReceived = false;

class ProvisioningCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    blePayload = (const char*)characteristic->getValue().c_str();
    credentialsReceived = true;
    Serial.println("[BLE] Credentials received");
  }
};

void startBLE() {
  BLEDevice::init(BLE_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  BLEService* svc = pServer->createService(BLE_SERVICE_UUID);
  pProvChar = svc->createCharacteristic(BLE_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  pProvChar->setCallbacks(new ProvisioningCallback());
  svc->start();
  BLEDevice::getAdvertising()->addServiceUUID(BLE_SERVICE_UUID);
  BLEDevice::getAdvertising()->start();
  bleRunning = true;
  Serial.println("[BLE] Advertising as " BLE_DEVICE_NAME);
}

void stopBLE() {
  if (!bleRunning) return;
  BLEDevice::deinit(true);
  bleRunning = false;
  Serial.println("[BLE] Stopped");
}

// ============================================================
//  WIFI
// ============================================================
bool wifiConnected = false;

void tryConnectToWiFi(const String& ssid, const String& pass) {
  Serial.printf("[WiFi] Connecting to %s...", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] Connection failed.");
  }
}

// ============================================================
//  STATE MACHINE
// ============================================================
enum DeviceState { STATE_DISARMED, STATE_ARMING, STATE_ARMED, STATE_WARNING, STATE_ALERT };
DeviceState state = STATE_DISARMED;

const char* stateStr() {
  switch (state) {
    case STATE_DISARMED: return "DISARMED";
    case STATE_ARMING:   return "ARMING";
    case STATE_ARMED:    return "ARMED";
    case STATE_WARNING:  return "WARNING";
    case STATE_ALERT:    return "ALERT";
    default:             return "UNKNOWN";
  }
}

// ============================================================
//  DUMMY GPS — wrappers match production firmware signatures
// ============================================================
double dummyLat = DUMMY_DEFAULT_LAT;
double dummyLng = DUMMY_DEFAULT_LNG;
bool   dummyGpsValid = true;
int    dummySats = 6;

double anchorLat = 0.0, anchorLng = 0.0;
bool anchorSet = false;
unsigned long lastGPSCheck = 0;

double haversineM(double lat1, double lng1, double lat2, double lng2) {
  const double R = 6371000.0;
  double dLat = radians(lat2 - lat1), dLng = radians(lng2 - lng1);
  double a = sin(dLat/2)*sin(dLat/2) + cos(radians(lat1))*cos(radians(lat2))*sin(dLng/2)*sin(dLng/2);
  return R * 2.0 * atan2(sqrt(a), sqrt(1.0-a));
}

void feedGPS() { /* no-op */ }

double getGpsLat()   { return dummyLat; }
double getGpsLng()   { return dummyLng; }
double getGpsAlt()   { return 0.0; }
bool   gpsIsValid()  { return dummyGpsValid; }
int    gpsSatCount() { return dummySats; }

// ============================================================
//  DUMMY MOTION — events injected via serial only
// ============================================================
#define MAX_MOTION_EVENTS 10
unsigned long motionEvents[MAX_MOTION_EVENTS];
int motionHead = 0, motionCount = 0;
unsigned long lastMotionTrigger = 0;

void recordMotionEvent() {
  unsigned long now = millis();
  if (now - lastMotionTrigger < MOTION_COOLDOWN_MS) return;
  lastMotionTrigger = now;
  motionEvents[motionHead] = now;
  motionHead = (motionHead + 1) % MAX_MOTION_EVENTS;
  if (motionCount < MAX_MOTION_EVENTS) motionCount++;
}

int countRecentMotionEvents() {
  unsigned long now = millis();
  int count = 0;
  for (int i = 0; i < motionCount; i++)
    if (now - motionEvents[i] <= MOTION_WINDOW_MS) count++;
  return count;
}

// ============================================================
//  BUZZER (non-blocking) — real hardware, unchanged
// ============================================================
unsigned long buzzerLastToggle = 0;
bool buzzerOn = false;

void buzzerConfirm(int freq, int ms) { tone(BUZZER_PIN, freq, ms); delay(ms); noTone(BUZZER_PIN); }

void buzzerWarningTick() {
  unsigned long now = millis();
  if (now - buzzerLastToggle >= 800) { buzzerLastToggle = now; tone(BUZZER_PIN, TONE_WARNING, 150); }
}

void buzzerAlertTick() {
  unsigned long now = millis();
  if (now - buzzerLastToggle >= 200) {
    buzzerLastToggle = now;
    buzzerOn = !buzzerOn;
    tone(BUZZER_PIN, buzzerOn ? TONE_ALERT : TONE_ALERT / 2);
  }
}

void buzzerStop() { noTone(BUZZER_PIN); buzzerOn = false; }

// ============================================================
//  CLOUD (TODO stubs — identical to production)
// ============================================================
void sendAlert(const char* type, double lat, double lng, float distFromAnchor) {
  if (!wifiConnected) { Serial.println("[CLOUD] Alert skipped — no WiFi"); return; }
  // TODO: POST to Firestore REST API /alerts/{auto-id}
  // TODO: POST to Discord webhook
  Serial.printf("[CLOUD] TODO sendAlert: %s @ %.6f, %.6f (%.1fm)\n", type, lat, lng, distFromAnchor);
}

void sendHeartbeat() {
  if (!wifiConnected) return;
  // TODO: PUT /locks/nexis_001 — fields: state, lat, lng, anchorLat, anchorLng, geofenceRadius, lastSeen, wifiSSID, ip
}

// ============================================================
//  ALERT HANDLER
// ============================================================
void triggerAlert(const char* reason) {
  if (state == STATE_ALERT) return;
  state = STATE_ALERT;
  Serial.printf("!!! ALERT: %s !!!\n", reason);
  double lat = getGpsLat(), lng = getGpsLng();
  float dist = anchorSet ? (float)haversineM(anchorLat, anchorLng, lat, lng) : 0.0f;
  Serial.printf("[ALERT] Location: %.6f, %.6f\n", lat, lng);
  sendAlert(reason, lat, lng, dist);
}

// ============================================================
//  ARM / DISARM
// ============================================================
unsigned long armedAtMs = 0;

void armDevice() {
  if (state != STATE_DISARMED) return;
  motionCount = 0; motionHead = 0;
  anchorSet = false;
  armedAtMs = millis();
  buzzerConfirm(TONE_ARM_CONFIRM, 200);
  anchorLat = getGpsLat(); anchorLng = getGpsLng();
  anchorSet = true;
  state = STATE_ARMED;
  Serial.printf("[ARM] Armed. Anchor: %.6f, %.6f (DUMMY)\n", anchorLat, anchorLng);
}

void disarmDevice() {
  state = STATE_DISARMED;
  buzzerStop();
  buzzerConfirm(TONE_DISARM_CONFIRM, 300);
  dummyLat = DUMMY_DEFAULT_LAT;
  dummyLng = DUMMY_DEFAULT_LNG;
  Serial.println("[DISARM] Disarmed");
}

// ============================================================
//  SENSOR PROCESSING
// ============================================================
void processSensors() {
  unsigned long now = millis();
  bool inWarmup = (now - armedAtMs < WARMUP_MS);

  if (anchorSet && now - lastGPSCheck >= GPS_CHECK_INTERVAL) {
    lastGPSCheck = now;
    double dist = haversineM(anchorLat, anchorLng, getGpsLat(), getGpsLng());
    if (dist > GEOFENCE_RADIUS_M) {
      Serial.printf("[GPS] BREACH! %.1fm from anchor\n", dist);
      triggerAlert("GEOFENCE_BREACH");
    }
  }

  if (inWarmup) return;

  if (state == STATE_ALERT) { buzzerAlertTick(); return; }

  int recentMotion = countRecentMotionEvents();
  if (state == STATE_ARMED || state == STATE_ARMING) {
    if      (recentMotion >= MOTION_EVENTS_TO_ALERT) triggerAlert("TAMPERING");
    else if (recentMotion >= 1) {
      state = STATE_WARNING;
      Serial.println("[WARNING] Motion detected");
    }
  } else if (state == STATE_WARNING) {
    buzzerWarningTick();
    if      (recentMotion >= MOTION_EVENTS_TO_ALERT) triggerAlert("TAMPERING");
    else if (recentMotion == 0) {
      state = STATE_ARMED;
      buzzerStop();
      Serial.println("[STATE] Quiet — back to monitoring");
    }
  }
}

// ============================================================
//  SERIAL COMMANDS
// ============================================================
void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); cmd.toLowerCase();

  if (cmd == "arm") {
    armDevice();
  } else if (cmd == "disarm") {
    disarmDevice();
  } else if (cmd == "status") {
    Serial.println("---- STATUS (DUMMY) ----");
    Serial.printf("  State:    %s\n", stateStr());
    Serial.printf("  WiFi:     %s\n", wifiConnected ? WiFi.localIP().toString().c_str() : "not connected");
    Serial.printf("  Motion:   events=%d (injected)\n", countRecentMotionEvents());
    if (anchorSet)
      Serial.printf("  Geofence: %.6f, %.6f (%.0fm radius)\n", anchorLat, anchorLng, GEOFENCE_RADIUS_M);
    else
      Serial.println("  Geofence: not armed yet");
    Serial.printf("  GPS:      %.6f, %.6f (%d sats) [DUMMY]\n", getGpsLat(), getGpsLng(), gpsSatCount());
    Serial.println("------------------------");
  } else if (cmd == "gps") {
    Serial.printf("[GPS] %.6f, %.6f | %d sats | %.1fm alt [DUMMY]\n",
                  getGpsLat(), getGpsLng(), gpsSatCount(), getGpsAlt());
  } else if (cmd == "inject motion") {
    if (state == STATE_DISARMED) { Serial.println("[INJECT] Arm first"); return; }
    recordMotionEvent();
    Serial.printf("[INJECT] Motion event injected (total recent: %d)\n", countRecentMotionEvents());
  } else if (cmd == "inject breach") {
    dummyLat = DUMMY_BREACH_LAT;
    dummyLng = DUMMY_BREACH_LNG;
    double dist = anchorSet ? haversineM(anchorLat, anchorLng, dummyLat, dummyLng) : 0.0;
    Serial.printf("[INJECT] GPS moved to breach position (%.1fm from anchor)\n", dist);
  } else if (cmd == "inject fix") {
    dummyLat = anchorSet ? anchorLat : DUMMY_DEFAULT_LAT;
    dummyLng = anchorSet ? anchorLng : DUMMY_DEFAULT_LNG;
    Serial.println("[INJECT] GPS restored to anchor/default position");
  } else if (cmd.length() > 0) {
    Serial.println("Commands: arm, disarm, status, gps");
    Serial.println("Debug:    inject motion, inject breach, inject fix");
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("================================");
  Serial.println("  NEXIS Bike Lock — DUMMY MODE");
  Serial.println("================================");
  Serial.println("[DUMMY] No sensors. Use inject commands to simulate events.");

  prefs.begin("wifi", false);
  String savedSsid = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  if (savedSsid.length()) {
    Serial.println("[WiFi] Trying saved credentials...");
    tryConnectToWiFi(savedSsid, savedPass);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Starting BLE provisioning (60s window)...");
    startBLE();
    unsigned long bleStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - bleStart < BLE_PROVISION_TIMEOUT_MS) {
      if (credentialsReceived) {
        credentialsReceived = false;
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, blePayload)) {
          String ssid = doc["ssid"] | "";
          String pass = doc["passphrase"] | "";
          if (ssid.length()) tryConnectToWiFi(ssid, pass);
        }
      }
      delay(100);
    }
    stopBLE();
  }

  if (WiFi.status() != WL_CONNECTED)
    Serial.println("[WiFi] No connection — local-only mode");

  Serial.println("Ready. Commands: arm, disarm, status, gps");
  Serial.println("Debug:  inject motion, inject breach, inject fix");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  if (state != STATE_DISARMED) processSensors();
  handleSerial();
  delay(50);
}
