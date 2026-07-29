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

#define PIN_TRIGGER 5
#define PIN_ECHO 18


/*
 * ============================================================
 * Status-LED
 * ============================================================
 */

#define PIN_STATUS_LED 2


/*
 * ============================================================
 * RGB-LED
 * ============================================================
 *
 * true  = gemeinsame Kathode
 * false = gemeinsame Anode
 */

#define PIN_RGB_RED 25
#define PIN_RGB_GREEN 26
#define PIN_RGB_BLUE 27

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

#define PIN_BUTTON 33

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

#define PIN_BATTERY 34


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
 * Messintervall
 * ============================================================
 *
 * Wert in Sekunden.
 *
 * 300 Sekunden = 5 Minuten
 *
 * Dieser Wert wird ebenfalls als Deep-Sleep-Dauer verwendet,
 * solange kein anderer Wert gespeichert wurde.
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
#define MQTT_ENABLED false

// Abstand zwischen Verbindungsversuchen
#define MQTT_RECONNECT_INTERVAL_MS 10000UL

// Abstand zwischen regelmäßigen MQTT-Veröffentlichungen
#define MQTT_PUBLISH_INTERVAL_MS 30000UL