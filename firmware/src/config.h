#ifndef CONFIG_H
#define CONFIG_H

// ---------- PIN CONFIGURATION (ESP32) ----------
#define GPS_RX      16
#define GPS_TX      17
#define GSM_RX      26  // ← Matches your previous config (SIM800L TX -> ESP32 GPIO26)
#define GSM_TX      27  // ← Matches your previous config (SIM800L RX -> ESP32 GPIO27)
#define MPU_SDA     21
#define MPU_SCL     22
#define BUZZER_PIN  5
#define CANCEL_BTN  4

// ---------- FALL DETECTION THRESHOLDS ----------
#define FALL_THRESHOLD    2.5f     // ← Updated: 2.5G reduces false alarms from bumps
#define NO_MOVE_TIME_MS   8000     // 8 seconds of no movement after impact = Fall Confirmed

// ---------- BLE CONFIG ----------
#define BLE_DEVICE_NAME   "SmartHelmetX"
// ↓ Added: Required for the corrected main.ino to work with iOS/Android
#define BLE_SERVICE_UUID  "0000FFE0-0000-1000-8000-00805F9B34FB"
#define BLE_CHAR_UUID     "0000FFE1-0000-1000-8000-00805F9B34FB"

// ---------- EMERGENCY CONTACT ----------
// ↓ Must Edit: Enter the phone number to receive SOS SMS
#define EMERGENCY_NUMBER  "+97798XXXXXXXX"   

// ---------- SERVER CONFIG ----------
// ↓ Must Edit: Enter your computer's IPv4 address (find it via `ipconfig`)
#define SERVER_IP         "192.168.1.X"      
#define SERVER_PORT       8000               // Port number
#define API_USERNAME      "your_username"    // Must Edit: Django username
#define API_PASSWORD      "your_password"    // Must Edit: Django password
#define DEVICE_ID         "helmet_001"       // Unique ID for this helmet

// ---------- GSM / SIM CONFIG ----------
// ↓ Must Edit: Change to match your SIM provider
#define GSM_APN           "ntc"              
//   NTC Nepal: "ntc" or "web"
//   Ncell:     "ncell"
//   WorldLink: "worldlink"

// ---------- TIMING ----------
#define GPS_SEND_INTERVAL_MS      30000UL    // GPS updates every 30s
#define SENSOR_SEND_INTERVAL_MS   10000UL    // Sensor updates every 10s
#define LOGIN_INTERVAL_MS         39600000UL // Server login refresh every 11 hours

#endif