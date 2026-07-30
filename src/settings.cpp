#include "settings.h"

#include <Preferences.h>

#include "config.h"
#include "logger.h"


namespace
{
    /*
     * Namespace innerhalb der ESP32-Preferences.
     */
    constexpr const char* PREFERENCES_NAMESPACE =
        "tank";

    /*
     * Kurze Schlüssel sparen Speicher in der
     * Preferences-Datenbank.
     */
    constexpr const char* KEY_WIFI_SSID =
        "ssid";

    constexpr const char* KEY_WIFI_PASSWORD =
        "pass";

    constexpr const char* KEY_WIFI_DHCP =
        "dhcp";

    constexpr const char* KEY_WIFI_STATIC_IP =
        "staticIp";

    constexpr const char* KEY_WIFI_GATEWAY =
        "gateway";

    constexpr const char* KEY_WIFI_SUBNET =
        "subnet";

    constexpr const char* KEY_WIFI_DNS_1 =
        "dns1";

    constexpr const char* KEY_WIFI_DNS_2 =
        "dns2";

    constexpr const char* KEY_MQTT_SERVER =
        "mqtt";

    constexpr const char* KEY_MQTT_PORT =
        "port";

    constexpr const char* KEY_MQTT_USER =
        "user";

    constexpr const char* KEY_MQTT_PASSWORD =
        "mpass";

    constexpr const char* KEY_MQTT_ENABLED =
        "mqttEnabled";

    constexpr const char* KEY_DEVICE_NAME =
        "device";

    constexpr const char* KEY_TANK_HEIGHT =
        "tank";

    /*
     * Neuer Preferences-Schlüssel für den Abstand
     * zwischen Sensor und maximalem Wasserstand.
     */
constexpr const char* KEY_SENSOR_CLEARANCE =
    "clearance";

constexpr const char* KEY_MEASURE_INTERVAL =
    "interval";

/*
 * Gespeicherter Schalter für den schnellen
 * Battery-Estimator-Testmodus.
 */
constexpr const char* KEY_BATTERY_ESTIMATE_TEST_MODE =
    "batteryTest";

Preferences preferences;
}


SettingsData Settings::data;


void Settings::begin()
{
    const bool opened = preferences.begin(
        PREFERENCES_NAMESPACE,
        false
    );

    if (!opened)
    {
        Logger::error(
            "Failed to open preferences"
        );

        return;
    }

    load();
}


void Settings::load()
{
    /*
     * WLAN
     */
    data.wifiSSID =
        preferences.getString(
            KEY_WIFI_SSID,
            ""
        );

    data.wifiPassword =
        preferences.getString(
            KEY_WIFI_PASSWORD,
            ""
        );

    data.wifiDhcp =
        preferences.getBool(
            KEY_WIFI_DHCP,
            true
        );

    data.wifiStaticIp =
        preferences.getString(
            KEY_WIFI_STATIC_IP,
            ""
        );

    data.wifiGateway =
        preferences.getString(
            KEY_WIFI_GATEWAY,
            ""
        );

    data.wifiSubnet =
        preferences.getString(
            KEY_WIFI_SUBNET,
            ""
        );

    data.wifiDns1 =
        preferences.getString(
            KEY_WIFI_DNS_1,
            ""
        );

    data.wifiDns2 =
        preferences.getString(
            KEY_WIFI_DNS_2,
            ""
        );


    /*
     * MQTT
     */
    data.mqttServer =
        preferences.getString(
            KEY_MQTT_SERVER,
            ""
        );

    data.mqttPort =
        preferences.getUShort(
            KEY_MQTT_PORT,
            1883
        );

    data.mqttUser =
        preferences.getString(
            KEY_MQTT_USER,
            ""
        );

    data.mqttPassword =
        preferences.getString(
            KEY_MQTT_PASSWORD,
            ""
        );

    data.mqttEnabled =
        preferences.getBool(
            KEY_MQTT_ENABLED,
            false
        );


    /*
     * Gerät
     */
    data.deviceName =
        preferences.getString(
            KEY_DEVICE_NAME,
            DEFAULT_HOSTNAME
        );


    /*
     * Tankhöhe
     */
    data.tankHeight =
        preferences.getFloat(
            KEY_TANK_HEIGHT,
            DEFAULT_TANK_HEIGHT_CM
        );

    if (data.tankHeight <= 0.0f)
    {
        data.tankHeight =
            DEFAULT_TANK_HEIGHT_CM;
    }


    /*
     * Abstand zwischen Sensor und maximalem
     * Wasserstand.
     */
    data.sensorClearance =
    preferences.getFloat(
        KEY_SENSOR_CLEARANCE,
        DEFAULT_SENSOR_CLEARANCE_CM
    );

    if (data.sensorClearance < 0.0f)
    {
        data.sensorClearance =
            DEFAULT_SENSOR_CLEARANCE_CM;
    }


    /*
     * Mess- und Deep-Sleep-Intervall
     */
    data.measureInterval =
        preferences.getUShort(
            KEY_MEASURE_INTERVAL,
            DEFAULT_MEASURE_INTERVAL
        );

if (data.measureInterval == 0)
{
    data.measureInterval =
        DEFAULT_MEASURE_INTERVAL;
}

/*
 * Battery Estimate Test Mode laden.
 */
data.batteryEstimateTestMode =
    preferences.getBool(
        KEY_BATTERY_ESTIMATE_TEST_MODE,
        false
    );

Logger::info(
    "Settings loaded"
);

    Logger::info(
        "Tank height: " +
        String(data.tankHeight, 1) +
        " cm"
    );

Logger::info(
    "Sensor clearance: " +
    String(data.sensorClearance, 1) +
    " cm"
);

Logger::info(
    "Battery estimate test mode: " +
    String(
        data.batteryEstimateTestMode
            ? "enabled"
            : "disabled"
    )
);
}


void Settings::save()
{
    /*
     * WLAN
     */
    preferences.putString(
        KEY_WIFI_SSID,
        data.wifiSSID
    );

    preferences.putString(
        KEY_WIFI_PASSWORD,
        data.wifiPassword
    );

    preferences.putBool(
        KEY_WIFI_DHCP,
        data.wifiDhcp
    );

    preferences.putString(
        KEY_WIFI_STATIC_IP,
        data.wifiStaticIp
    );

    preferences.putString(
        KEY_WIFI_GATEWAY,
        data.wifiGateway
    );

    preferences.putString(
        KEY_WIFI_SUBNET,
        data.wifiSubnet
    );

    preferences.putString(
        KEY_WIFI_DNS_1,
        data.wifiDns1
    );

    preferences.putString(
        KEY_WIFI_DNS_2,
        data.wifiDns2
    );


    /*
     * MQTT
     */
    preferences.putString(
        KEY_MQTT_SERVER,
        data.mqttServer
    );

    preferences.putUShort(
        KEY_MQTT_PORT,
        data.mqttPort
    );

    preferences.putString(
        KEY_MQTT_USER,
        data.mqttUser
    );

    preferences.putString(
        KEY_MQTT_PASSWORD,
        data.mqttPassword
    );

    preferences.putBool(
        KEY_MQTT_ENABLED,
        data.mqttEnabled
    );


    /*
     * Gerät und Tank
     */
    preferences.putString(
        KEY_DEVICE_NAME,
        data.deviceName
    );

    preferences.putFloat(
        KEY_TANK_HEIGHT,
        data.tankHeight
    );

    preferences.putFloat(
        KEY_SENSOR_CLEARANCE,
        data.sensorClearance
    );

    preferences.putUShort(
    KEY_MEASURE_INTERVAL,
    data.measureInterval
);

preferences.putBool(
    KEY_BATTERY_ESTIMATE_TEST_MODE,
    data.batteryEstimateTestMode
);

Logger::info(
    "Settings saved"
);
}


void Settings::reset()
{
    preferences.clear();

    Logger::warning(
        "Settings reset"
    );

    load();
}
