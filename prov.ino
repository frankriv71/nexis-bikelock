#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>

Preferences prefs;

BLEServer* pServer = nullptr;
BLECharacteristic* pProvChar = nullptr;

static const char* SERVICE_UUID = "021a9004-0382-4aea-bff4-6b3f1c5adfb4";
static const char* CHAR_UUID = "021a9005-0382-4aea-bff4-6b3f1c5adfb4";

String blePayload = "";
bool credentialsReceived = false;

class ProvisioningCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    blePayload = (const char*)characteristic->getValue().c_str();  // fixed conversion
    credentialsReceived = true;
    Serial.println("[BLE] Received credentials payload:");
    Serial.println(blePayload);
  }
};

void startBLE() {
  BLEDevice::init("PROV_123");
  pServer = BLEDevice::createServer();

  BLEService* provService = pServer->createService(SERVICE_UUID);
  pProvChar = provService->createCharacteristic(
    CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pProvChar->setCallbacks(new ProvisioningCallback());
  provService->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();
  Serial.println("[BLE] Advertising started");
}

void tryConnectToWiFi(String ssid, String password) {
  Serial.printf("[WiFi] Connecting to SSID: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connected!");
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
  } else {
    Serial.println("[WiFi] Failed to connect.");
  }
}

void setup() {
  Serial.begin(115200);
  prefs.begin("wifi", false);

  String savedSsid = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");

  if (savedSsid.length()) {
    Serial.println("[WiFi] Found stored credentials");
    tryConnectToWiFi(savedSsid, savedPass);
  }

  if (WiFi.status() != WL_CONNECTED) {
    startBLE();
  }
}

void loop() {
  if (credentialsReceived) {
    credentialsReceived = false;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, blePayload);
    if (err) {
      Serial.print("[ERROR] JSON parse failed: ");
      Serial.println(err.c_str());
      return;
    }

    String ssid = doc["ssid"] | "";
    String pass = doc["passphrase"] | "";

    if (ssid.length()) {
      tryConnectToWiFi(ssid, pass);
    }
  }

  delay(100);
}
