#ifndef CONFIG_H
#define CONFIG_H

// ---------- PIN CONFIGURATION (ESP32) ----------
#define GPS_RX      16
#define GPS_TX      17

#define GSM_RX      26
#define GSM_TX      27

#define MPU_SDA     21
#define MPU_SCL     22

#define BUZZER_PIN  5
#define CANCEL_BTN  4

// ---------- FALL DETECTION THRESHOLDS ----------
#define FALL_THRESHOLD    1.8f     // g-force threshold
#define NO_MOVE_TIME_MS   8000     // ms of no movement after impact = fall confirmed

// ---------- BLE CONFIG ----------
#define BLE_DEVICE_NAME   "SmartHelmetX"

// ---------- EMERGENCY CONTACT ----------
#define EMERGENCY_NUMBER  "+97798XXXXXXXX"   // ← edit: real phone number

// ---------- SERVER CONFIG ----------
#define SERVER_IP         "192.168.1.X"      // ← edit: run ipconfig, put your IPv4
#define SERVER_PORT       "8000"
#define API_USERNAME      "your_username"    // ← edit: Django login username
#define API_PASSWORD      "your_password"    // ← edit: Django login password
#define DEVICE_ID         "helmet_001"       // ← edit: unique ID for this helmet

// ---------- GSM / SIM CONFIG ----------
#define GSM_APN           "internet"         // ← edit: your SIM card APN
                                             //   NTC Nepal  → "ntc.net.np"
                                             //   Ncell Nepal → "ncell"
                                             //   Airtel India → "airtelgprs.com"

// ---------- TIMING ----------
#define GPS_SEND_INTERVAL_MS      30000UL    // send GPS every 30 seconds
#define SENSOR_SEND_INTERVAL_MS   10000UL    // send sensor data every 10 seconds
#define LOGIN_INTERVAL_MS         39600000UL // re-login every 11 hours

#endif