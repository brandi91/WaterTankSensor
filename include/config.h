#pragma once

// --------------------------------------------------
// Firmware
// --------------------------------------------------

#define FW_NAME        "WaterTankSensor"
#define FW_VERSION     "0.1.0"

// --------------------------------------------------
// GPIO
// --------------------------------------------------

#define PIN_TRIGGER    5
#define PIN_ECHO       18

#define PIN_STATUS_LED 2

#define PIN_RGB_RED    25
#define PIN_RGB_GREEN  26
#define PIN_RGB_BLUE   27
// true = gemeinsame Kathode (Common Cathode)
// false = gemeinsame Anode (Common Anode)
#define RGB_COMMON_CATHODE true


#define PIN_BUTTON     0

#define PIN_BATTERY    34

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