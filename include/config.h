#pragma once

/*
 * ============================================================
 * WaterTankSensor - central configuration
 * ============================================================
 */


/*
 * ============================================================
 * Ultrasonic sensor
 * ============================================================
 */

#define DEFAULT_SENSOR_TRIGGER_PIN 23
#define DEFAULT_SENSOR_ECHO_PIN 22

// Fixed 3.3 V charger/module-present signal. HIGH means connected.
#define CHARGER_DETECT_PIN 34


/*
 * ============================================================
 * Status LED
 * ============================================================
 */

#define DEFAULT_STATUS_LED_PIN 2


/*
 * ============================================================
 * RGB LED
 * ============================================================
 *
 * true  = common cathode
 * false = common anode
 */

#define DEFAULT_LED_RED_PIN 27
#define DEFAULT_LED_GREEN_PIN 26
#define DEFAULT_LED_BLUE_PIN 25

#define RGB_COMMON_CATHODE true


/*
 * ============================================================
 * Button
 * ============================================================
 *
 * The button connects GPIO 33 to GND.
 *
 * Released = HIGH
 * Pressed  = LOW
 */

#define DEFAULT_BUTTON_PIN 33

/*
 * This fixed pin is deliberately not configurable. It remains the
 * deep-sleep wake/recovery input even when the normal button is moved.
 */
#define RECOVERY_BUTTON_PIN 33

// Button debounce time.
#define BUTTON_DEBOUNCE_MS 50UL

/*
 * After 5 seconds, start the web server on the configured Wi-Fi network.
 */
#define BUTTON_WEB_SERVER_PRESS_MS 5000UL

/*
 * After 15 seconds, start the configuration access point and web server.
 */
#define BUTTON_CONFIG_PORTAL_PRESS_MS 15000UL

#define BUTTON_FACTORY_RESET_PRESS_MS 30000UL
#define FACTORY_RESET_LED_INTERVAL_MS 300UL


/*
 * ============================================================
 * Web-server indicator on the RGB LED
 * ============================================================
 */

// Interval between LED state changes.
#define WEB_LED_BLINK_INTERVAL_MS 500UL

// Total indicator duration: 15 seconds.
#define WEB_LED_INDICATOR_DURATION_MS 15000UL


/*
 * ============================================================
 * Battery-voltage measurement
 * ============================================================
 */

#define DEFAULT_BATTERY_ADC_PIN 35


/*
 * ============================================================
 * Tank
 * ============================================================
 */

/*
 * Default usable tank height in centimeters.
 */
#define DEFAULT_TANK_HEIGHT_CM 100.0f

/*
 * Distance between the sensor and the maximum water level.
 *
 * This air gap remains when the tank is full.
 */
#define DEFAULT_SENSOR_CLEARANCE_CM 12.0f
/*
 * Default measurement interval in seconds.
 */
#define DEFAULT_MEASURE_INTERVAL 300UL





/*
 * ============================================================
 * Wi-Fi
 * ============================================================
 */

#define DEFAULT_HOSTNAME "WaterTankSensor"

// Maximum connection wait time.
#define WIFI_CONNECT_TIMEOUT_MS 15000UL

// Interval between connection attempts.
#define WIFI_RECONNECT_INTERVAL_MS 10000UL


/*
 * ============================================================
 * LED self-test
 * ============================================================
 *
 * Enable only for hardware diagnostics. Normal release builds keep it off.
 */

#define DEBUG_LED_TEST false


/*
 * ============================================================
 * Deep Sleep
 * ============================================================
 *
 * true:
 * Deep sleep is simulated.
 *
 * false:
 * The ESP32 enters real deep sleep.
 */

#define DEBUG_DISABLE_DEEP_SLEEP false

/*
 * Optional retained markers for hardware sleep-entry audits.
 * Keep disabled for normal builds.
 */
#define DEEP_SLEEP_AUDIT_DIAGNOSTICS false

/*
 * The button connects GPIO 33 to GND, so LOW is the wake level.
 */

#define BUTTON_WAKEUP_LEVEL 0


/*
 * ============================================================
 * Configuration access point
 * ============================================================
 */

#define CONFIG_AP_SSID "WaterTankSensor-Setup"
#define CONFIG_AP_PASSWORD "watertank"
#define CONFIG_AP_IP "192.168.4.1"
#define CONFIG_AP_GATEWAY "192.168.4.1"
#define CONFIG_AP_SUBNET "255.255.255.0"


/*
 * ============================================================
 * Web server
 * ============================================================
 */

#define WEB_SERVER_PORT 80


/*
 * ============================================================
 * MQTT
 * ============================================================
 */

// Enable or disable MQTT support globally.
#define MQTT_ENABLED true

// Interval between connection attempts.
#define MQTT_RECONNECT_INTERVAL_MS 10000UL

// Interval between periodic MQTT publications while awake.
#define MQTT_PUBLISH_INTERVAL_MS 30000UL
