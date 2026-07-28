#pragma once

// --------------------------------------------------
// Firmware
// --------------------------------------------------

#define FW_NAME        "WaterTankSensor"
#define FW_VERSION "0.6.0"

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
#define BUTTON_LONG_PRESS_MS 3000UL

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

// --------------------------------------------------

// LED-Test beim Start aktivieren/deaktivieren
#define DEBUG_LED_TEST