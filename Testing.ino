// ============================================================
//  NEXIS Bike Lock — Combined Firmware
//
//  Components:
//    KY-037 Sound Sensor  — AO→GPIO4, DO→GPIO23
//    MPU6050 Accelerometer — SDA→GPIO21, SCL→GPIO22
//    GT-U7 GPS            — TXD→GPIO16, RXD→GPIO17
//    Passive Buzzer       — (+)→GPIO15, (-)→GND
//
//  Commands (serial monitor):
//    arm      — set geofence + start monitoring
//    disarm   — stop everything
//    status   — show current state + sensor data
//    gps      — show GPS position
// ============================================================

#include <Wire.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ---- Pin definitions ----
#define SOUND_DIGITAL_PIN 23
#define SOUND_ANALOG_PIN  4
#define BUZZER_PIN        15
#define GPS_RX_PIN        16
#define GPS_TX_PIN        17
#define MPU6050_ADDR      0x68

// ---- Buzzer tones ----
#define TONE_ARM_CONFIRM    1000
#define TONE_DISARM_CONFIRM 500
#define TONE_WARNING        2000
#define TONE_ALERT          4000

// ---- Sound detection config ----
#define SMOOTHING_FACTOR      0.01f
#define SPIKE_MULTIPLIER      1.5f
#define SPIKE_ABSOLUTE_MIN    300.0f
#define SOUND_EVENTS_TO_ALERT 3
#define SOUND_WINDOW_MS       5000
#define SOUND_COOLDOWN_MS     500

// ---- Motion detection config ----
#define MOTION_THRESHOLD      8000    // Raw accel delta to count as movement (raised to reduce false triggers)
#define MOTION_EVENTS_TO_ALERT 3
#define MOTION_WINDOW_MS      5000
#define MOTION_CHECK_INTERVAL 100     // Check every 100ms

// ---- GPS geofence config ----
#define GEOFENCE_RADIUS_M     50.0
#define GPS_CHECK_INTERVAL    10000   // Check every 10 seconds
#define MIN_SATELLITES        4

// ---- Warmup after arming ----
#define WARMUP_MS             3000

// ============================================================
//  STATE MACHINE
// ============================================================
enum DeviceState {
  STATE_DISARMED,
  STATE_ARMING,     // Waiting for GPS fix to set anchor
  STATE_ARMED,
  STATE_WARNING,
  STATE_ALERT
};

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
//  GPS
// ============================================================
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

double anchorLat = 0.0;
double anchorLng = 0.0;
bool anchorSet = false;
unsigned long lastGPSCheck = 0;

double haversineM(double lat1, double lng1, double lat2, double lng2) {
  const double R = 6371000.0;
  double dLat = radians(lat2 - lat1);
  double dLng = radians(lng2 - lng1);
  double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
             cos(radians(lat1)) * cos(radians(lat2)) *
             sin(dLng / 2.0) * sin(dLng / 2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return R * c;
}

void feedGPS() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

// ============================================================
//  MPU6050
// ============================================================
int16_t baseAx = 0, baseAy = 0, baseAz = 0;
bool mpuBaselineSet = false;
unsigned long lastMotionCheck = 0;

// Motion event buffer
#define MAX_MOTION_EVENTS 10
unsigned long motionEvents[MAX_MOTION_EVENTS];
int motionHead = 0;
int motionCount = 0;
unsigned long lastMotionTrigger = 0;

void readMPU(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 6, true);
  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
}

void setMPUBaseline() {
  readMPU(baseAx, baseAy, baseAz);
  mpuBaselineSet = true;
}

void recordMotionEvent() {
  unsigned long now = millis();
  if (now - lastMotionTrigger < SOUND_COOLDOWN_MS) return;
  lastMotionTrigger = now;

  motionEvents[motionHead] = now;
  motionHead = (motionHead + 1) % MAX_MOTION_EVENTS;
  if (motionCount < MAX_MOTION_EVENTS) motionCount++;
}

int countRecentMotionEvents() {
  unsigned long now = millis();
  int count = 0;
  for (int i = 0; i < motionCount; i++) {
    if (now - motionEvents[i] <= MOTION_WINDOW_MS) count++;
  }
  return count;
}

// ============================================================
//  SOUND (KY-037 adaptive baseline)
// ============================================================
float soundBaseline = 0.0;
bool soundBaselineInit = false;

#define MAX_SOUND_EVENTS 10
unsigned long soundEvents[MAX_SOUND_EVENTS];
int soundHead = 0;
int soundCount = 0;
unsigned long lastSoundTrigger = 0;

volatile bool soundISR = false;
void IRAM_ATTR onSoundISR() { soundISR = true; }

void updateSoundBaseline(int val) {
  if (!soundBaselineInit) {
    soundBaseline = (float)val;
    soundBaselineInit = true;
    return;
  }
  float thresh = max(SPIKE_ABSOLUTE_MIN, soundBaseline * SPIKE_MULTIPLIER);
  if ((float)val < thresh) {
    soundBaseline = soundBaseline * (1.0f - SMOOTHING_FACTOR) + (float)val * SMOOTHING_FACTOR;
  }
}

bool isSoundSpike(int val) {
  float thresh = max(SPIKE_ABSOLUTE_MIN, soundBaseline * SPIKE_MULTIPLIER);
  return (float)val > thresh;
}

void recordSoundEvent() {
  unsigned long now = millis();
  if (now - lastSoundTrigger < SOUND_COOLDOWN_MS) return;
  lastSoundTrigger = now;

  soundEvents[soundHead] = now;
  soundHead = (soundHead + 1) % MAX_SOUND_EVENTS;
  if (soundCount < MAX_SOUND_EVENTS) soundCount++;
}

int countRecentSoundEvents() {
  unsigned long now = millis();
  int count = 0;
  for (int i = 0; i < soundCount; i++) {
    if (now - soundEvents[i] <= SOUND_WINDOW_MS) count++;
  }
  return count;
}

// ============================================================
//  BUZZER (non-blocking)
// ============================================================
unsigned long buzzerLastToggle = 0;
bool buzzerOn = false;

void buzzerConfirm(int freq, int ms) {
  tone(BUZZER_PIN, freq, ms);
  delay(ms);
  noTone(BUZZER_PIN);
}

void buzzerWarningTick() {
  unsigned long now = millis();
  if (now - buzzerLastToggle >= 800) {
    buzzerLastToggle = now;
    tone(BUZZER_PIN, TONE_WARNING, 150);
  }
}

void buzzerAlertTick() {
  unsigned long now = millis();
  if (now - buzzerLastToggle >= 200) {
    buzzerLastToggle = now;
    buzzerOn = !buzzerOn;
    tone(BUZZER_PIN, buzzerOn ? TONE_ALERT : TONE_ALERT / 2);
  }
}

void buzzerStop() {
  noTone(BUZZER_PIN);
  buzzerOn = false;
}

// ============================================================
//  ALERT HANDLER
// ============================================================
void triggerAlert(const char* reason) {
  if (state != STATE_ALERT) {
    state = STATE_ALERT;
    Serial.printf("!!! ALERT: %s !!!\n", reason);

    // Print GPS if available
    if (gps.location.isValid()) {
      Serial.printf("[ALERT] Location: %.6f, %.6f\n",
                    gps.location.lat(), gps.location.lng());
    }

    // TODO: sendAlert(reason, lat, lng, dist) via WiFi
  }
}

// ============================================================
//  ARM / DISARM
// ============================================================
unsigned long armedAtMs = 0;

void armDevice() {
  if (state != STATE_DISARMED) return;

  // Reset all event buffers
  soundCount = 0; soundHead = 0; soundBaselineInit = false;
  motionCount = 0; motionHead = 0; mpuBaselineSet = false;
  anchorSet = false;

  armedAtMs = millis();
  buzzerConfirm(TONE_ARM_CONFIRM, 200);

  // Try to set GPS anchor immediately
  if (gps.location.isValid() && gps.satellites.value() >= MIN_SATELLITES) {
    anchorLat = gps.location.lat();
    anchorLng = gps.location.lng();
    anchorSet = true;
    state = STATE_ARMED;
    Serial.printf("[ARM] Armed with GPS anchor: %.6f, %.6f\n", anchorLat, anchorLng);
  } else {
    state = STATE_ARMING;
    Serial.println("[ARM] Armed — waiting for GPS fix to set geofence...");
    Serial.println("[ARM] Motion + sound detection active immediately");
  }
}

void disarmDevice() {
  state = STATE_DISARMED;
  buzzerStop();
  buzzerConfirm(TONE_DISARM_CONFIRM, 300);
  Serial.println("[DISARM] Device disarmed");
}

// ============================================================
//  MAIN SENSOR PROCESSING
// ============================================================
void processSensors() {
  unsigned long now = millis();
  bool inWarmup = (now - armedAtMs < WARMUP_MS);

  // ---- Always feed GPS ----
  feedGPS();

  // ---- Try to set GPS anchor if still arming ----
  if (state == STATE_ARMING && !anchorSet) {
    if (gps.location.isValid() && gps.satellites.value() >= MIN_SATELLITES) {
      anchorLat = gps.location.lat();
      anchorLng = gps.location.lng();
      anchorSet = true;
      state = STATE_ARMED;
      Serial.printf("[GPS] Anchor set: %.6f, %.6f (%d sats)\n",
                    anchorLat, anchorLng, gps.satellites.value());
    }
  }

  // ---- GPS geofence check ----
  if (anchorSet && now - lastGPSCheck >= GPS_CHECK_INTERVAL) {
    lastGPSCheck = now;
    if (gps.location.isValid()) {
      double dist = haversineM(anchorLat, anchorLng,
                               gps.location.lat(), gps.location.lng());
      if (dist > GEOFENCE_RADIUS_M) {
        Serial.printf("[GPS] BREACH! %.1fm from anchor\n", dist);
        triggerAlert("GEOFENCE BREACH");
      }
    }
  }

  if (inWarmup) return; // Skip sound + motion during warmup


  // ---- Sound detection ----
  int soundVal = analogRead(SOUND_ANALOG_PIN);
  updateSoundBaseline(soundVal);

  if (isSoundSpike(soundVal)) {
    recordSoundEvent();
    Serial.printf("[SOUND] Spike! raw=%d baseline=%.0f\n", soundVal, soundBaseline);
  }

  if (soundISR) {
    soundISR = false;
    recordSoundEvent();
  }


  // ---- Motion detection ----
  if (now - lastMotionCheck >= MOTION_CHECK_INTERVAL) {
    lastMotionCheck = now;

    int16_t ax, ay, az;
    readMPU(ax, ay, az);

    if (!mpuBaselineSet) {
      setMPUBaseline();
    } else {
      int16_t dx = abs(ax - baseAx);
      int16_t dy = abs(ay - baseAy);
      int16_t dz = abs(az - baseAz);
      int16_t maxDelta = max(dx, max(dy, dz));

      if (maxDelta > MOTION_THRESHOLD) {
        recordMotionEvent();
        Serial.printf("[MOTION] Movement! delta=%d (X=%d Y=%d Z=%d)\n",
                      maxDelta, dx, dy, dz);
        // Update baseline so small shifts don't keep triggering
        setMPUBaseline();
      }
    }
  }

  // ---- Evaluate alert level ----
  if (state == STATE_ALERT) {
    buzzerAlertTick();
    return; // Stay in alert until disarmed
  }

  int recentSound = countRecentSoundEvents();
  int recentMotion = countRecentMotionEvents();
  int totalEvents = recentSound + recentMotion;

  if (state == STATE_ARMED || state == STATE_ARMING) {
    if (totalEvents >= SOUND_EVENTS_TO_ALERT) {
      triggerAlert("TAMPERING DETECTED");
    } else if (totalEvents >= 1) {
      state = STATE_WARNING;
      Serial.printf("[WARNING] Activity detected (sound=%d motion=%d)\n",
                    recentSound, recentMotion);
    }
  } else if (state == STATE_WARNING) {
    buzzerWarningTick();
    if (totalEvents >= SOUND_EVENTS_TO_ALERT) {
      triggerAlert("TAMPERING DETECTED");
    } else if (totalEvents == 0) {
      state = (anchorSet) ? STATE_ARMED : STATE_ARMING;
      buzzerStop();
      Serial.println("[STATE] Activity stopped — back to monitoring");
    }
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // KY-037
  pinMode(SOUND_DIGITAL_PIN, INPUT_PULLDOWN);  // Pull down so floating pin reads LOW
  pinMode(SOUND_ANALOG_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SOUND_DIGITAL_PIN), onSoundISR, RISING);

  // MPU6050
  Wire.begin(21, 22);
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  // GPS
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  Serial.println("================================");
  Serial.println("  NEXIS Bike Lock v1.0");
  Serial.println("================================");
  Serial.println("Commands: arm, disarm, status, gps");
  Serial.println();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Always feed GPS even when disarmed
  feedGPS();

  // Process sensors when armed
  if (state != STATE_DISARMED) {
    processSensors();
  }

  // Serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "arm") {
      armDevice();

    } else if (cmd == "disarm") {
      disarmDevice();

    } else if (cmd == "status") {
      Serial.println("---- STATUS ----");
      Serial.printf("  State:    %s\n", stateStr());
      Serial.printf("  Sound:    baseline=%.0f events=%d\n",
                    soundBaseline, countRecentSoundEvents());
      Serial.printf("  Motion:   events=%d\n", countRecentMotionEvents());
      if (anchorSet) {
        Serial.printf("  Geofence: %.6f, %.6f (%.0fm radius)\n",
                      anchorLat, anchorLng, GEOFENCE_RADIUS_M);
      } else {
        Serial.println("  Geofence: waiting for GPS fix");
      }
      if (gps.location.isValid()) {
        Serial.printf("  GPS:      %.6f, %.6f (%d sats)\n",
                      gps.location.lat(), gps.location.lng(),
                      gps.satellites.value());
      } else {
        Serial.printf("  GPS:      no fix (%d sats)\n", gps.satellites.value());
      }
      Serial.println("----------------");

    } else if (cmd == "gps") {
      if (gps.location.isValid()) {
        Serial.printf("[GPS] Position: %.6f, %.6f\n",
                      gps.location.lat(), gps.location.lng());
        Serial.printf("[GPS] Satellites: %d\n", gps.satellites.value());
        Serial.printf("[GPS] Altitude: %.1fm\n", gps.altitude.meters());
      } else {
        Serial.printf("[GPS] No fix — satellites seen: %d\n",
                      gps.satellites.value());
        Serial.printf("[GPS] Chars processed: %lu\n", gps.charsProcessed());
      }
    }
  }

  delay(50);
}