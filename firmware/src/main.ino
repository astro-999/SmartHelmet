#include <Wire.h>
#include <MPU6050_tockn.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"

// -------- OBJECTS ----------
MPU6050 mpu(Wire);
TinyGPSPlus gps;
HardwareSerial SerialGPS(1);
HardwareSerial SerialGSM(2);
BLECharacteristic *alertChar;

// -------- STATE VARIABLES ----------
String authToken             = "";
unsigned long lastMoveTime   = 0;
unsigned long lastGPSSend    = 0;
unsigned long lastSensorSend = 0;
unsigned long lastLoginTime  = 0;
bool firstMove               = false;
bool bleConnected            = false;
bool cancelPressed           = false;

// ==================================================
//  BLE
// ==================================================
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *s)    { bleConnected = true;  Serial.println("BLE connected"); }
    void onDisconnect(BLEServer *s) { bleConnected = false; Serial.println("BLE disconnected"); }
};

void setupBLE() {
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());
    BLEService *service = server->createService("1234");
    alertChar = service->createCharacteristic("5678", BLECharacteristic::PROPERTY_NOTIFY);
    alertChar->addDescriptor(new BLE2902());
    service->start();
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID("1234");
    adv->start();
    Serial.println("BLE advertising started");
}

void sendBLE(const String &msg) {
    if (bleConnected) {
        alertChar->setValue(msg.c_str());
        alertChar->notify();
        Serial.println("BLE: " + msg);
    }
}

// ==================================================
//  GSM HELPERS
// ==================================================
void waitForGSMResponse(unsigned long timeout = 2000) {
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (SerialGSM.available()) Serial.write(SerialGSM.read());
    }
}

void sendAT(const String &cmd, unsigned long wait = 1000) {
    SerialGSM.println(cmd);
    waitForGSMResponse(wait);
}

String readGSMResponse(unsigned long timeout = 3000) {
    String response = "";
    unsigned long t = millis();
    while (millis() - t < timeout) {
        while (SerialGSM.available()) response += (char)SerialGSM.read();
    }
    return response;
}

// Generic HTTP POST — always sends device_id in body
void httpPost(const String &endpoint, const String &body) {
    String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + endpoint;
    int bodyLen = body.length();
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
    delay(3000);
    sendAT("AT+HTTPACTION=1", 6000);
    sendAT("AT+HTTPTERM", 500);
}

// ==================================================
//  LOGIN
// ==================================================
bool loginToServer() {
    Serial.println("Logging in...");
    String body = "{\"username\":\"" + String(API_USERNAME) +
                  "\",\"password\":\"" + String(API_PASSWORD) + "\"}";
    int bodyLen = body.length();

    sendAT("AT+HTTPTERM", 500);
    sendAT("AT+HTTPINIT", 1000);
    sendAT("AT+HTTPPARA=\"CID\",1", 500);
    sendAT("AT+HTTPPARA=\"URL\",\"http://" + String(SERVER_IP) + ":" +
           String(SERVER_PORT) + "/api/auth/login/\"", 500);
    sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 500);
    sendAT("AT+HTTPDATA=" + String(bodyLen) + ",5000", 1000);
    SerialGSM.print(body);
    delay(3000);
    sendAT("AT+HTTPACTION=1", 6000);
    sendAT("AT+HTTPREAD", 1000);

    String response = readGSMResponse(3000);
    sendAT("AT+HTTPTERM", 500);
    Serial.println("Login response: " + response);

    int idx = response.indexOf("\"access\":\"");
    if (idx != -1) {
        idx += 10;
        authToken     = response.substring(idx, response.indexOf("\"", idx));
        lastLoginTime = millis();
        Serial.println("Login OK! Token length=" + String(authToken.length()));
        return true;
    }
    Serial.println("Login FAILED — check SERVER_IP, username, password");
    return false;
}

// ==================================================
//  FIX 1 — GPS with device_id
// ==================================================
void sendGPSToServer(float lat, float lng) {
    if (authToken.length() == 0) { Serial.println("No token — skip GPS"); return; }
    Serial.println("Sending GPS...");
    String body = "{\"device_id\":\"" + String(DEVICE_ID) + "\","
                  "\"latitude\":"     + String(lat, 6)    + ","
                  "\"longitude\":"    + String(lng, 6)    + ","
                  "\"speed\":"        + String(gps.speed.kmph(), 1) + "}";
    httpPost("/api/gps/", body);
    Serial.println("GPS sent lat=" + String(lat,6) + " lng=" + String(lng,6));
}

// ==================================================
//  FIX 2 — SOS: push GPS first, then trigger SOS
// ==================================================
void sendSOSToServer(float lat, float lng) {
    if (authToken.length() == 0) { Serial.println("No token — skip SOS"); return; }
    Serial.println("Sending SOS...");

    // Push latest GPS to DB first so server can read it
    sendGPSToServer(lat, lng);
    delay(1000);

    // Now trigger SOS (server reads GPS from DB)
    String body = "{\"device_id\":\"" + String(DEVICE_ID) + "\","
                  "\"message\":\"Crash detected!\","
                  "\"latitude\":"  + String(lat, 6) + ","
                  "\"longitude\":" + String(lng, 6) + "}";
    httpPost("/api/sos/", body);
    Serial.println("SOS sent!");
}

// ==================================================
//  FIX 4 — SENSOR DATA (fills dashboard charts)
// ==================================================
void sendSensorToServer(float ax, float ay, float az,
                        float gx, float gy, float gz) {
    if (authToken.length() == 0) { Serial.println("No token — skip sensor"); return; }
    Serial.println("Sending sensor data...");
    String body = "{\"device_id\":\"" + String(DEVICE_ID) + "\","
                  "\"accel_x\":"  + String(ax, 3) + ","
                  "\"accel_y\":"  + String(ay, 3) + ","
                  "\"accel_z\":"  + String(az, 3) + ","
                  "\"gyro_x\":"   + String(gx, 3) + ","
                  "\"gyro_y\":"   + String(gy, 3) + ","
                  "\"gyro_z\":"   + String(gz, 3) + "}";
    httpPost("/api/sensor-data/", body);
    Serial.println("Sensor sent");
}

// ==================================================
//  SMS
// ==================================================
void sendSMS(const String &text) {
    Serial.println("Sending SMS...");
    sendAT("AT+CMGF=1", 500);
    SerialGSM.print("AT+CMGS=\"");
    SerialGSM.print(EMERGENCY_NUMBER);
    SerialGSM.println("\"");
    delay(500);
    SerialGSM.print(text);
    SerialGSM.write(26);   // CTRL+Z
    delay(3000);
    Serial.println("SMS sent to " + String(EMERGENCY_NUMBER));
}

// ==================================================
//  GPS
// ==================================================
void readGPS() {
    while (SerialGPS.available()) gps.encode(SerialGPS.read());
}

String getGoogleMapsLink() {
    if (gps.location.isValid())
        return "https://maps.google.com/?q=" +
               String(gps.location.lat(), 6) + "," +
               String(gps.location.lng(), 6);
    return "GPS not ready";
}

// ==================================================
//  FALL DETECTION (fixed)
// ==================================================
bool detectFall() {
    mpu.update();
    float ax = abs(mpu.getAccX());
    float ay = abs(mpu.getAccY());
    float az = abs(mpu.getAccZ());

    if (ax > FALL_THRESHOLD || ay > FALL_THRESHOLD || az > FALL_THRESHOLD) {
        lastMoveTime = millis();
        firstMove    = true;
        Serial.println("Impact! ax=" + String(ax) + " ay=" + String(ay) + " az=" + String(az));
        return true;
    }
    if (firstMove && (millis() - lastMoveTime > NO_MOVE_TIME_MS)) {
        firstMove = false;
        Serial.println("No movement after impact — possible fall!");
        return true;
    }
    return false;
}

// ==================================================
//  SETUP
// ==================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== SmartHelmetX Booting ===");

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CANCEL_BTN, INPUT_PULLUP);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(MPU_SDA, MPU_SCL);
    mpu.begin();
    mpu.calcGyroOffsets(true);
    Serial.println("MPU6050 ready");

    SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    Serial.println("GPS UART started");

    SerialGSM.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
    Serial.println("GSM UART started");

    setupBLE();

    Serial.println("Waiting for GSM network...");
    delay(5000);
    sendAT("AT",       1000);
    sendAT("AT+CREG?", 1000);
    sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"",        500);
    sendAT("AT+SAPBR=3,1,\"APN\",\"" + String(GSM_APN) + "\"", 500);
    sendAT("AT+SAPBR=1,1", 3000);

    loginToServer();
    Serial.println("=== SmartHelmetX Ready ===");
}

// ==================================================
//  LOOP
// ==================================================
void loop() {
    readGPS();

    // Re-login every 11 hours — FIX 5: unsigned subtraction handles millis() overflow safely
    if (millis() - lastLoginTime > LOGIN_INTERVAL_MS) {
        loginToServer();
    }

    // Send GPS every 30 seconds
    if (millis() - lastGPSSend > GPS_SEND_INTERVAL_MS) {
        if (gps.location.isValid()) {
            sendGPSToServer(gps.location.lat(), gps.location.lng());
        } else {
            Serial.println("GPS not valid yet");
        }
        lastGPSSend = millis();
    }

    // FIX 4 — Send sensor data every 10 seconds
    if (millis() - lastSensorSend > SENSOR_SEND_INTERVAL_MS) {
        mpu.update();
        sendSensorToServer(
            mpu.getAccX(),  mpu.getAccY(),  mpu.getAccZ(),
            mpu.getGyroX(), mpu.getGyroY(), mpu.getGyroZ()
        );
        lastSensorSend = millis();
    }

    // Fall detection
    if (detectFall()) {
        digitalWrite(BUZZER_PIN, HIGH);
        sendBLE("ALERT: Possible Accident! Press cancel button within 10 seconds.");
        Serial.println("Fall detected — 10s cancel window...");

        // FIX 3 — Poll button every 50ms during 10-second window
        cancelPressed = false;
        unsigned long waitStart = millis();
        while (millis() - waitStart < 10000) {
            readGPS();
            if (digitalRead(CANCEL_BTN) == LOW) {   // LOW = pressed (INPUT_PULLUP)
                cancelPressed = true;
                Serial.println("Cancel pressed!");
                break;
            }
            delay(50);
        }

        if (!cancelPressed) {
            Serial.println("No cancel — sending SOS!");

            float lat = gps.location.isValid() ? gps.location.lat() : 0.0;
            float lng = gps.location.isValid() ? gps.location.lng() : 0.0;

            sendSOSToServer(lat, lng);
            delay(2000);

            String link = getGoogleMapsLink();
            String msg  = "ACCIDENT ALERT! SmartHelmetX detected a crash.\nLocation: " + link;
            sendSMS(msg);
            sendBLE(msg);
            Serial.println("All alerts sent!");
        } else {
            Serial.println("Alert cancelled");
            sendBLE("Alert cancelled by rider.");
        }

        digitalWrite(BUZZER_PIN, LOW);
    }

    delay(100);
}