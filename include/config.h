#pragma once

// --------------------------------------------------
// Firmware
// --------------------------------------------------

#define FW_NAME        "WaterTankSensor"
#define FW_VERSION "0.9.1"

// --------------------------------------------------
// GPIO
// --------------------------------------------------

#define PIN_TRIGGER    5
#define PIN_ECHO       18

#define PIN_STATUS_LED 2

#define PIN_RGB_RED    25
#define PIN_RGB_GREEN  26
#define PIN_RGB_BLUE   27

#define PIN_BUTTON 33

#define BUTTON_DEBOUNCE_MS 50UL

#define BUTTON_WEB_SERVER_PRESS_MS 5000UL
#define BUTTON_CONFIG_PORTAL_PRESS_MS 15000UL

#define WEB_LED_BLINK_INTERVAL_MS 500UL

#define PIN_BATTERY    34
#define RGB_COMMON_CATHODE true





// --------------------------------------------------
// Tank
// --------------------------------------------------

#define DEFAULT_TANK_HEIGHT_CM   100

// --------------------------------------------------
// Timing
// --------------------------------------------------

#define DEFAULT_MEASURE_INTERVAL 300

// Sekunden

#define CONFIG_TIMEOUT           600

// --------------------------------------------------
// WiFi
// --------------------------------------------------

#define DEFAULT_HOSTNAME "WaterTankSensor"
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define WIFI_RECONNECT_INTERVAL_MS 10000UL

// --------------------------------------------------

// LED-Test beim Start aktivieren/deaktivieren
#define DEBUG_LED_TEST

// =========================
// Deep Sleep
// =========================

// Standard-Schlafzeit in Sekunden
#define DEFAULT_SLEEP_TIME_SECONDS 60UL
#define DEBUG_DISABLE_DEEP_SLEEP true

// Taster zieht GPIO33 beim Drücken auf GND
#define BUTTON_WAKEUP_LEVEL 0

// =========================
// Configuration portal
// =========================

#define CONFIG_AP_SSID "WaterTankSensor-Setup"
#define CONFIG_AP_PASSWORD "watertank"

#define WEB_SERVER_PORT 80

// =========================
// MQTT
// =========================

#define MQTT_ENABLED false
#define MQTT_RECONNECT_INTERVAL_MS 10000UL
#define MQTT_PUBLISH_INTERVAL_MS 30000UL