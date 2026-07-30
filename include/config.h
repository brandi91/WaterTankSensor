#pragma once

/*
 * ============================================================
 * WaterTankSensor – zentrale Konfiguration
 * ============================================================
 */


/*
 * ============================================================
 * Ultraschallsensor
 * ============================================================
 */

#define DEFAULT_SENSOR_TRIGGER_PIN 5
#define DEFAULT_SENSOR_ECHO_PIN 18


/*
 * ============================================================
 * Status-LED
 * ============================================================
 */

#define DEFAULT_STATUS_LED_PIN 2


/*
 * ============================================================
 * RGB-LED
 * ============================================================
 *
 * true  = gemeinsame Kathode
 * false = gemeinsame Anode
 */

#define DEFAULT_LED_RED_PIN 25
#define DEFAULT_LED_GREEN_PIN 26
#define DEFAULT_LED_BLUE_PIN 27

#define RGB_COMMON_CATHODE true


/*
 * ============================================================
 * Taster
 * ============================================================
 *
 * Der Taster befindet sich zwischen GPIO 33 und GND.
 *
 * Nicht gedrückt = HIGH
 * Gedrückt       = LOW
 */

#define DEFAULT_BUTTON_PIN 33

/*
 * This fixed pin is deliberately not configurable. It remains the
 * deep-sleep wake/recovery input even when the normal button is moved.
 */
#define RECOVERY_BUTTON_PIN DEFAULT_BUTTON_PIN

// Entprellzeit des Tasters
#define BUTTON_DEBOUNCE_MS 50UL

/*
 * Nach 5 Sekunden:
 * Webserver über das normale WLAN starten.
 */
#define BUTTON_WEB_SERVER_PRESS_MS 5000UL

/*
 * Nach insgesamt 15 Sekunden:
 * Konfigurations-AP und Webserver starten.
 */
#define BUTTON_CONFIG_PORTAL_PRESS_MS 15000UL

#define BUTTON_FACTORY_RESET_PRESS_MS 30000UL
#define FACTORY_RESET_LED_INTERVAL_MS 300UL


/*
 * ============================================================
 * Webserver-Anzeige über RGB-LED
 * ============================================================
 */

// Abstand zwischen LED ein und LED aus
#define WEB_LED_BLINK_INTERVAL_MS 500UL

// Gesamtdauer des Blinkens: 15 Sekunden
#define WEB_LED_INDICATOR_DURATION_MS 15000UL


/*
 * ============================================================
 * Batteriespannungsmessung
 * ============================================================
 */

#define DEFAULT_BATTERY_ADC_PIN 34


/*
 * ============================================================
 * Tank
 * ============================================================
 */

/*
 * Standard-Tankhöhe in Zentimetern.
 */
#define DEFAULT_TANK_HEIGHT_CM 100.0f

/*
 * Abstand zwischen Sensor und maximalem Wasserstand.
 *
 * Der Sensor befindet sich bei vollem Tank weiterhin
 * diesen Abstand oberhalb der Wasseroberfläche.
 */
#define DEFAULT_SENSOR_CLEARANCE_CM 12.0f
/*
 * Standard-Messintervall in Sekunden.
 */
#define DEFAULT_MEASURE_INTERVAL 300UL





/*
 * ============================================================
 * WLAN
 * ============================================================
 */

#define DEFAULT_HOSTNAME "WaterTankSensor"

// Maximale Wartezeit beim Verbinden
#define WIFI_CONNECT_TIMEOUT_MS 15000UL

// Abstand zwischen erneuten Verbindungsversuchen
#define WIFI_RECONNECT_INTERVAL_MS 10000UL


/*
 * ============================================================
 * LED-Selbsttest
 * ============================================================
 *
 * Zum Deaktivieren diese Zeile auskommentieren.
 */

#define DEBUG_LED_TEST


/*
 * ============================================================
 * Deep Sleep
 * ============================================================
 *
 * true:
 * Deep Sleep wird nur simuliert.
 *
 * false:
 * ESP32 geht wirklich in Deep Sleep.
 */

#define DEBUG_DISABLE_DEEP_SLEEP false

/*
 * Optional retained markers for hardware sleep-entry audits.
 * Keep disabled for normal builds.
 */
#define DEEP_SLEEP_AUDIT_DIAGNOSTICS false

/*
 * Der Taster verbindet GPIO 33 beim Drücken mit GND.
 * Deshalb wird bei LOW aufgeweckt.
 */

#define BUTTON_WAKEUP_LEVEL 0


/*
 * ============================================================
 * Konfigurations-Access-Point
 * ============================================================
 */

#define CONFIG_AP_SSID "WaterTankSensor-Setup"
#define CONFIG_AP_PASSWORD "watertank"
#define CONFIG_AP_IP "192.168.4.1"
#define CONFIG_AP_GATEWAY "192.168.4.1"
#define CONFIG_AP_SUBNET "255.255.255.0"


/*
 * ============================================================
 * Webserver
 * ============================================================
 */

#define WEB_SERVER_PORT 80


/*
 * ============================================================
 * MQTT
 * ============================================================
 */

// MQTT-Funktionen global ein- oder ausschalten
#define MQTT_ENABLED true

// Abstand zwischen Verbindungsversuchen
#define MQTT_RECONNECT_INTERVAL_MS 10000UL

// Abstand zwischen regelmäßigen MQTT-Veröffentlichungen
#define MQTT_PUBLISH_INTERVAL_MS 30000UL
