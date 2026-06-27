#include <Wire.h>
#include <MPU6050_tockn.h>
#include <TinyGPS++.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"

// ---------- OBJECTS ----------
MPU6050 mpu(Wire);
TinyGPSPlus gps;
HardwareSerial SerialGPS(1);
HardwareSerial SerialGSM(2);
BLECharacteristic *alertChar;

// ---------- STATE VARIABLES ----------
String authToken = "";
unsigned long lastGPSSend    = 0;
unsigned long lastSensorSend = 0;
unsigned long lastLoginTime  = 0;
unsigned long lastAlertTime  = 0;   // Cooldown: prevents re-trigger within 60s
bool bleConnected  = false;
bool cancelPressed = false;
float lastKnownLat = 0.0;
float lastKnownLng = 0.0;

// ============================================================
// BLE CALLBACKS
// ============================================================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *s) {
    bleConnected = true;
    Serial.println("[BLE] Connected");
  }
  void onDisconnect(BLEServer *s) {
    bleConnected = false;
    Serial.println("[BLE] Disconnected");
    // Restart advertising so a new client can reconnect
    BLEDevice::getAdvertising()->start();
  }
};

// ============================================================
// BLE SETUP
// ============================================================
void setupBLE() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEServer  *server  = BLEDevice::createServer();
  server->setCallbacks(new MyServerCallbacks());
  BLEService *service = server->createService(BLE_SERVICE_UUID);
  alertChar = service->createCharacteristic(BLE_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  alertChar->addDescriptor(new BLE2902());
  service->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->start();
  Serial.println("[BLE] Advertising started");
}

void sendBLE(const String &msg) {
  if (bleConnected && alertChar) {
    alertChar->setValue(msg.c_str());
    alertChar->notify();
    Serial.println("[BLE] Sent: " + msg);
  }
}

// ============================================================
// GSM HELPERS
// ============================================================

// Non-blocking wait — returns true if cancel button pressed during wait
bool waitForGSMResponseOrCancel(unsigned long timeout) {
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (digitalRead(CANCEL_BTN) == LOW) return true;
    while (SerialGSM.available()) Serial.write(SerialGSM.read());
    delay(10);
  }
  return false;
}

void sendAT(const String &cmd, unsigned long wait = 1000) {
  SerialGSM.println(cmd);
  waitForGSMResponseOrCancel(wait);
}

String readGSMResponse(unsigned long timeout = 3000) {
  String response = "";
  unsigned long t = millis();
  while (millis() - t < timeout) {
    while (SerialGSM.available()) response += (char)SerialGSM.read();
    delay(10);
  }
  return response;
}

// ============================================================
// FIX BUG 3 — Ensure GPRS bearer is open before any HTTP call
// ============================================================
bool ensureGPRS() {
  // Query bearer status
  sendAT("AT+SAPBR=2,1", 1000);
  String status = readGSMResponse(2000);

  // +SAPBR: 1,1,"x.x.x.x"  → connected
  // +SAPBR: 1,3,""          → closed
  if (status.indexOf(",1,") != -1) {
    return true;  // Already open
  }

  Serial.println("[GSM] GPRS bearer closed — reopening...");
  sendAT("AT+SAPBR=0,1", 2000);  // Force close first (clears stale state)
  delay(500);
  sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 500);
  sendAT("AT+SAPBR=3,1,\"APN\",\"" + String(GSM_APN) + "\"", 500);
  sendAT("AT+SAPBR=1,1", 5000);

  // Verify
  sendAT("AT+SAPBR=2,1", 1000);
  String verify = readGSMResponse(2000);
  if (verify.indexOf(",1,") != -1) {
    Serial.println("[GSM] GPRS bearer reopened OK");
    return true;
  }
  Serial.println("[GSM] GPRS bearer failed to open");
  return false;
}

bool isGSMReady() {
  sendAT("AT+CREG?", 1000);
  String resp = readGSMResponse(2000);
  return (resp.indexOf("+CREG: 0,1") != -1 || resp.indexOf("+CREG: 0,5") != -1);
}

// ============================================================
// HTTP POST (generic)
// ============================================================
bool httpPost(const String &endpoint, const String &body) {
  if (!isGSMReady()) {
    Serial.println("[HTTP] GSM not registered — skipping");
    return false;
  }

  // FIX BUG 3 — ensure GPRS is alive before every HTTP attempt
  if (!ensureGPRS()) {
    Serial.println("[HTTP] GPRS unavailable — skipping");
    return false;
  }

  String url    = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + endpoint;
  int    bodyLen = body.length();

  sendAT("AT+HTTPTERM", 500);
  sendAT("AT+HTTPINIT", 1000);
  sendAT("AT+HTTPPARA=\"CID\",1", 500);
  sendAT("AT+HTTPPARA=\"URL\",\"" + url + "\"", 500);
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 500);

  if (authToken.length() > 0) {
    sendAT("AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer " + authToken + "\"", 500);
  }

  sendAT("AT+HTTPDATA=" + String(bodyLen) + ",5000", 1000);
  SerialGSM.print(body);

  if (waitForGSMResponseOrCancel(3000)) {
    Serial.println("[HTTP] Cancelled during data input");
    sendAT("AT+HTTPTERM", 500);
    return false;
  }

  sendAT("AT+HTTPACTION=1", 15000);
  String response = readGSMResponse(10000);
  sendAT("AT+HTTPTERM", 500);

  if (response.indexOf("401") != -1) {
    Serial.println("[HTTP] 401 — token expired, re-logging...");
    loginToServer();
    return false;
  }

  if (response.indexOf(",200,") != -1 || response.indexOf(",201,") != -1) {
    return true;
  }

  Serial.println("[HTTP] POST failed: " + response);
  return false;
}

// ============================================================
// LOGIN  (FIX BUG 3 — ensureGPRS called here too)
// ============================================================
bool loginToServer() {
  Serial.println("[AUTH] Logging in...");

  // FIX BUG 3 — verify GPRS before attempting login HTTP
  if (!ensureGPRS()) {
    Serial.println("[AUTH] GPRS not available — login aborted");
    return false;
  }

  String body    = "{\"username\":\"" + String(API_USERNAME) + "\",\"password\":\"" + String(API_PASSWORD) + "\"}";
  int    bodyLen = body.length();

  sendAT("AT+HTTPTERM", 500);
  sendAT("AT+HTTPINIT", 1000);
  sendAT("AT+HTTPPARA=\"CID\",1", 500);
  sendAT("AT+HTTPPARA=\"URL\",\"http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/auth/login/\"", 500);
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 500);
  sendAT("AT+HTTPDATA=" + String(bodyLen) + ",5000", 1000);
  SerialGSM.print(body);

  if (waitForGSMResponseOrCancel(3000)) {
    Serial.println("[AUTH] Login cancelled");
    sendAT("AT+HTTPTERM", 500);
    return false;
  }

  sendAT("AT+HTTPACTION=1", 15000);
  sendAT("AT+HTTPREAD", 1000);

  String response = readGSMResponse(3000);
  sendAT("AT+HTTPTERM", 500);
  Serial.println("[AUTH] Response: " + response);

  int idx = response.indexOf("\"access\":\"");
  if (idx != -1) {
    idx += 10;
    authToken     = response.substring(idx, response.indexOf("\"", idx));
    lastLoginTime = millis();
    Serial.println("[AUTH] Login OK — token length=" + String(authToken.length()));
    return true;
  }

  Serial.println("[AUTH] Login FAILED — check SERVER_IP, username, password");
  return false;
}

// ============================================================
// GPS
// ============================================================
void readGPS() {
  while (SerialGPS.available()) gps.encode(SerialGPS.read());
  if (gps.location.isValid()) {
    lastKnownLat = gps.location.lat();
    lastKnownLng = gps.location.lng();
  }
}

String getGoogleMapsLink() {
  if (lastKnownLat != 0.0 && lastKnownLng != 0.0) {
    return "https://maps.google.com/?q=" + String(lastKnownLat, 6) + "," + String(lastKnownLng, 6);
  }
  return "GPS not ready";
}

void sendGPSToServer(float lat, float lng) {
  if (authToken.length() == 0) {
    Serial.println("[GPS] No token — skipping server send");
    return;
  }
  Serial.println("[GPS] Sending...");
  String body = "{\"device_id\":\"" + String(DEVICE_ID) + "\","
                "\"latitude\":"  + String(lat, 6) + ","
                "\"longitude\":" + String(lng, 6) + ","
                "\"speed\":"     + String(gps.speed.kmph(), 1) + "}";
  if (httpPost("/api/gps/", body)) {
    Serial.println("[GPS] Sent OK — lat=" + String(lat, 6) + " lng=" + String(lng, 6));
  }
}

// ============================================================
// SOS  (FIX WARNING 1 — SMS-only fallback when token is missing)
// ============================================================
void sendSOSToServer(float lat, float lng) {
  String link = getGoogleMapsLink();
  String smsMsg = "ACCIDENT ALERT! SmartHelmetX detected a crash.\nLocation: " + link;

  // FIX WARNING 1 — if token is missing, send SMS immediately and return
  if (authToken.length() == 0) {
    Serial.println("[SOS] No auth token — SMS-only fallback!");
    sendSMS(smsMsg);
    sendBLE(smsMsg);
    return;
  }

  Serial.println("[SOS] Sending to server...");

  // Push latest GPS first
  sendGPSToServer(lat, lng);
  delay(500);

  // Trigger SOS endpoint
  String body = "{\"device_id\":\"" + String(DEVICE_ID) + "\","
                "\"message\":\"Crash detected!\","
                "\"latitude\":"  + String(lat, 6) + ","
                "\"longitude\":" + String(lng, 6) + "}";

  bool serverOK = httpPost("/api/sos/", body);

  // Always send SMS regardless of server result (safety-critical fallback)
  sendSMS(smsMsg);
  sendBLE(smsMsg);

  if (serverOK) {
    Serial.println("[SOS] Server alert sent OK");
  } else {
    Serial.println("[SOS] Server alert failed — SMS still sent");
  }
}

// ============================================================
// SENSOR DATA
// ============================================================
void sendSensorToServer(float ax, float ay, float az, float gx, float gy, float gz) {
  if (authToken.length() == 0) {
    Serial.println("[SENSOR] No token — skipping");
    return;
  }
  Serial.println("[SENSOR] Sending...");
  String body = "{\"device_id\":\"" + String(DEVICE_ID) + "\","
                "\"accel_x\":" + String(ax, 3) + ","
                "\"accel_y\":" + String(ay, 3) + ","
                "\"accel_z\":" + String(az, 3) + ","
                "\"gyro_x\":"  + String(gx, 3) + ","
                "\"gyro_y\":"  + String(gy, 3) + ","
                "\"gyro_z\":"  + String(gz, 3) + "}";
  if (httpPost("/api/sensor-data/", body)) {
    Serial.println("[SENSOR] Sent OK");
  }
}

// ============================================================
// SMS
// ============================================================
void sendSMS(const String &text) {
  Serial.println("[SMS] Sending to " + String(EMERGENCY_NUMBER) + "...");
  sendAT("AT+CMGF=1", 500);
  SerialGSM.print("AT+CMGS=\"");
  SerialGSM.print(EMERGENCY_NUMBER);
  SerialGSM.println("\"");
  delay(500);
  SerialGSM.print(text);
  SerialGSM.write(26);  // CTRL+Z to send
  delay(3000);
  Serial.println("[SMS] Sent!");
}

// ============================================================
// FALL DETECTION
// FIX BUG 1  — use NO_MOVE_TIME_MS from config.h (was hardcoded 10000)
// FIX BUG 2  — removed wrong mpu.begin() return check
// FIX WARNING 2 — added no-movement stillness confirmation phase
// ============================================================
bool detectFall() {
  mpu.update();

  float ax = mpu.getAccX();
  float ay = mpu.getAccY();
  float az = mpu.getAccZ();
  float magnitude = sqrt(ax * ax + ay * ay + az * az);

  // 60-second cooldown between alerts
  if (magnitude <= FALL_THRESHOLD || (millis() - lastAlertTime <= 60000)) {
    return false;
  }

  Serial.printf("[FALL] Impact detected! mag=%.2f ax=%.2f ay=%.2f az=%.2f\n",
                magnitude, ax, ay, az);

  // ── FIX WARNING 2: No-movement stillness confirmation ──────────────────
  // After the spike, check for 2 seconds that the rider is no longer moving.
  // A pothole/bump will show continued high movement; a real fall goes still.
  Serial.println("[FALL] Checking stillness (2s)...");
  unsigned long stillCheck = millis();
  while (millis() - stillCheck < 2000) {
    mpu.update();
    float mx = mpu.getAccX();
    float my = mpu.getAccY();
    float mz = mpu.getAccZ();
    float m2  = sqrt(mx * mx + my * my + mz * mz);
    if (m2 > 1.2f) {
      // Still significant movement — likely a bump, not a fall
      Serial.println("[FALL] Still moving — not a fall (bump/pothole)");
      return false;
    }
    delay(50);
  }

  Serial.println("[FALL] Stillness confirmed — genuine fall!");
  return true;
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== SmartHelmetX Booting ===");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(CANCEL_BTN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  // ── MPU6050 ─────────────────────────────────────────────────────────────
  Wire.begin(MPU_SDA, MPU_SCL);

  // FIX BUG 2 — mpu.begin() returns void; use Wire probe to detect presence
  Wire.beginTransmission(0x68);  // MPU6050 default I2C address
  uint8_t i2cError = Wire.endTransmission();
  if (i2cError != 0) {
    Serial.println("[MPU] ERROR: MPU6050 not found! Check SDA/SCL wiring. I2C error=" + String(i2cError));
    // Halt with blink so user knows hardware is not ready
    while (true) {
      digitalWrite(BUZZER_PIN, HIGH); delay(200);
      digitalWrite(BUZZER_PIN, LOW);  delay(200);
    }
  }
  mpu.begin();
  mpu.calcGyroOffsets(true);
  Serial.println("[MPU] MPU6050 ready — gyro offsets calculated");

  // ── GPS ──────────────────────────────────────────────────────────────────
  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("[GPS] UART1 started on RX=" + String(GPS_RX) + " TX=" + String(GPS_TX));

  // ── GSM ──────────────────────────────────────────────────────────────────
  SerialGSM.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  Serial.println("[GSM] UART2 started on RX=" + String(GSM_RX) + " TX=" + String(GSM_TX));

  // ── BLE ──────────────────────────────────────────────────────────────────
  setupBLE();

  // ── GSM Network & GPRS ───────────────────────────────────────────────────
  Serial.println("[GSM] Waiting for network registration...");
  delay(5000);
  sendAT("AT",       1000);
  sendAT("AT+CREG?", 1000);

  // FIX BUG 3 — initial GPRS setup via ensureGPRS() instead of inline AT calls
  // (so the same logic is reused on every subsequent reconnect)
  ensureGPRS();

  // ── Initial login ────────────────────────────────────────────────────────
  loginToServer();

  Serial.println("=== SmartHelmetX Ready ===\n");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  // Always read GPS
  readGPS();

  // ── Token refresh every 11 hours ────────────────────────────────────────
  if (millis() - lastLoginTime > LOGIN_INTERVAL_MS) {
    loginToServer();
  }

  // ── GPS update every 30 seconds ─────────────────────────────────────────
  if (millis() - lastGPSSend > GPS_SEND_INTERVAL_MS) {
    if (lastKnownLat != 0.0 && lastKnownLng != 0.0) {
      sendGPSToServer(lastKnownLat, lastKnownLng);
    } else {
      Serial.println("[GPS] Fix not valid yet — skipping send");
    }
    lastGPSSend = millis();
  }

  // ── Sensor data every 10 seconds ────────────────────────────────────────
  if (millis() - lastSensorSend > SENSOR_SEND_INTERVAL_MS) {
    mpu.update();
    sendSensorToServer(
      mpu.getAccX(), mpu.getAccY(), mpu.getAccZ(),
      mpu.getGyroX(), mpu.getGyroY(), mpu.getGyroZ()
    );
    lastSensorSend = millis();
  }

  // ── Fall detection ───────────────────────────────────────────────────────
  if (detectFall()) {
    // Start intermittent buzzer to alert rider
    digitalWrite(BUZZER_PIN, HIGH);
    sendBLE("ALERT: Possible accident! Press cancel button within " +
            String(NO_MOVE_TIME_MS / 1000) + " seconds.");

    // FIX BUG 1 — use NO_MOVE_TIME_MS from config.h (was hardcoded 10000ms)
    Serial.println("[FALL] Waiting " + String(NO_MOVE_TIME_MS / 1000) + "s for cancel...");

    cancelPressed        = false;
    unsigned long waitStart   = millis();
    unsigned long buzzerTimer = millis();

    while (millis() - waitStart < NO_MOVE_TIME_MS) {
      readGPS();

      if (digitalRead(CANCEL_BTN) == LOW) {
        cancelPressed = true;
        Serial.println("[FALL] Cancel pressed by rider");
        break;
      }

      // Intermittent buzzer during cancel window (toggle every 500ms)
      if (millis() - buzzerTimer > 500) {
        digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        buzzerTimer = millis();
      }

      delay(50);
    }

    if (!cancelPressed) {
      // Confirmed fall — continuous buzzer
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("[FALL] No cancel received — dispatching SOS!");

      float lat = lastKnownLat;
      float lng = lastKnownLng;

      // FIX WARNING 1 — sendSOSToServer handles SMS fallback internally
      sendSOSToServer(lat, lng);

      // Update cooldown AFTER alert sent
      lastAlertTime = millis();

      Serial.println("[FALL] All alerts dispatched!");
    } else {
      Serial.println("[FALL] Alert cancelled by rider — no SOS sent");
      sendBLE("Alert cancelled by rider.");
    }

    // Always silence buzzer at end of fall handling
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(100);
}
