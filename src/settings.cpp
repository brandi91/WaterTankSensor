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

constexpr const char* KEY_NTP_ENABLED = "ntpEnabled";
constexpr const char* KEY_TIME_ZONE = "timeZone";
constexpr const char* KEY_NTP_SERVER_1 = "ntp1";
constexpr const char* KEY_NTP_SERVER_2 = "ntp2";
constexpr const char* KEY_NTP_SERVER_3 = "ntp3";
constexpr const char* KEY_NTP_TIMEOUT = "ntpTimeout";

constexpr const char* KEY_BATTERY_EMPTY = "batEmpty";
constexpr const char* KEY_BATTERY_FULL = "batFull";
constexpr const char* KEY_BATTERY_CAPACITY = "batCapacity";
constexpr const char* KEY_BATTERY_CHEMISTRY = "batChem";
constexpr const char* KEY_BATTERY_CELLS = "batCells";

/*
 * Gespeicherter Schalter für den schnellen
 * Battery-Estimator-Testmodus.
 */
constexpr const char* KEY_BATTERY_ESTIMATE_TEST_MODE =
    "batteryTest";

Preferences preferences;

void initializeMissingDefaults()
{
    if (!preferences.isKey(KEY_WIFI_SSID))
    {
        preferences.putString(KEY_WIFI_SSID, "");
    }

    if (!preferences.isKey(KEY_WIFI_PASSWORD))
    {
        preferences.putString(KEY_WIFI_PASSWORD, "");
    }

    if (!preferences.isKey(KEY_WIFI_DHCP))
    {
        preferences.putBool(KEY_WIFI_DHCP, true);
    }

    if (!preferences.isKey(KEY_WIFI_STATIC_IP))
    {
        preferences.putString(KEY_WIFI_STATIC_IP, "");
    }

    if (!preferences.isKey(KEY_WIFI_GATEWAY))
    {
        preferences.putString(KEY_WIFI_GATEWAY, "");
    }

    if (!preferences.isKey(KEY_WIFI_SUBNET))
    {
        preferences.putString(KEY_WIFI_SUBNET, "");
    }

    if (!preferences.isKey(KEY_WIFI_DNS_1))
    {
        preferences.putString(KEY_WIFI_DNS_1, "");
    }

    if (!preferences.isKey(KEY_WIFI_DNS_2))
    {
        preferences.putString(KEY_WIFI_DNS_2, "");
    }

    if (!preferences.isKey(KEY_MQTT_SERVER))
    {
        preferences.putString(KEY_MQTT_SERVER, "");
    }

    if (!preferences.isKey(KEY_MQTT_PORT))
    {
        preferences.putUShort(KEY_MQTT_PORT, 1883);
    }

    if (!preferences.isKey(KEY_MQTT_USER))
    {
        preferences.putString(KEY_MQTT_USER, "");
    }

    if (!preferences.isKey(KEY_MQTT_PASSWORD))
    {
        preferences.putString(KEY_MQTT_PASSWORD, "");
    }

    if (!preferences.isKey(KEY_MQTT_ENABLED))
    {
        preferences.putBool(KEY_MQTT_ENABLED, false);
    }

    if (!preferences.isKey(KEY_DEVICE_NAME))
    {
        preferences.putString(
            KEY_DEVICE_NAME,
            DEFAULT_HOSTNAME
        );
    }

    if (!preferences.isKey(KEY_TANK_HEIGHT))
    {
        preferences.putFloat(
            KEY_TANK_HEIGHT,
            DEFAULT_TANK_HEIGHT_CM
        );
    }

    if (!preferences.isKey(KEY_SENSOR_CLEARANCE))
    {
        preferences.putFloat(
            KEY_SENSOR_CLEARANCE,
            DEFAULT_SENSOR_CLEARANCE_CM
        );
    }

    if (!preferences.isKey(KEY_MEASURE_INTERVAL))
    {
        preferences.putULong(
            KEY_MEASURE_INTERVAL,
            DEFAULT_MEASURE_INTERVAL
        );
    }

    if (!preferences.isKey(KEY_NTP_ENABLED))
    {
        preferences.putBool(KEY_NTP_ENABLED, true);
    }

    if (!preferences.isKey(KEY_TIME_ZONE))
    {
        preferences.putString(KEY_TIME_ZONE, "UTC0");
    }

    if (!preferences.isKey(KEY_NTP_SERVER_1))
    {
        preferences.putString(
            KEY_NTP_SERVER_1,
            "pool.ntp.org"
        );
    }

    if (!preferences.isKey(KEY_NTP_SERVER_2))
    {
        preferences.putString(
            KEY_NTP_SERVER_2,
            "time.nist.gov"
        );
    }

    if (!preferences.isKey(KEY_NTP_SERVER_3))
    {
        preferences.putString(
            KEY_NTP_SERVER_3,
            "time.google.com"
        );
    }

    if (!preferences.isKey(KEY_NTP_TIMEOUT))
    {
        preferences.putUChar(KEY_NTP_TIMEOUT, 8);
    }

    if (!preferences.isKey(KEY_BATTERY_EMPTY))
    {
        preferences.putFloat(KEY_BATTERY_EMPTY, 3.20f);
    }

    if (!preferences.isKey(KEY_BATTERY_FULL))
    {
        preferences.putFloat(KEY_BATTERY_FULL, 4.20f);
    }

    if (!preferences.isKey(KEY_BATTERY_CAPACITY))
    {
        preferences.putULong(KEY_BATTERY_CAPACITY, 2000UL);
    }

    if (!preferences.isKey(KEY_BATTERY_CHEMISTRY))
    {
        preferences.putString(
            KEY_BATTERY_CHEMISTRY,
            "custom"
        );
    }

    if (!preferences.isKey(KEY_BATTERY_CELLS))
    {
        preferences.putUChar(KEY_BATTERY_CELLS, 1);
    }

    if (
        !preferences.isKey(
            KEY_BATTERY_ESTIMATE_TEST_MODE
        )
    )
    {
        preferences.putBool(
            KEY_BATTERY_ESTIMATE_TEST_MODE,
            false
        );
    }
}
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
    initializeMissingDefaults();

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
        preferences.getULong(
            KEY_MEASURE_INTERVAL,
            DEFAULT_MEASURE_INTERVAL
        );

if (
    data.measureInterval == 0 ||
    data.measureInterval > 86400UL
)
{
    data.measureInterval =
        DEFAULT_MEASURE_INTERVAL;
}

data.ntpEnabled =
    preferences.getBool(KEY_NTP_ENABLED, true);
data.timeZone =
    preferences.getString(KEY_TIME_ZONE, "UTC0");
data.ntpServer1 =
    preferences.getString(KEY_NTP_SERVER_1, "pool.ntp.org");
data.ntpServer2 =
    preferences.getString(KEY_NTP_SERVER_2, "time.nist.gov");
data.ntpServer3 =
    preferences.getString(KEY_NTP_SERVER_3, "time.google.com");
data.ntpTimeoutSeconds =
    preferences.getUChar(KEY_NTP_TIMEOUT, 8);

if (
    data.ntpTimeoutSeconds < 1 ||
    data.ntpTimeoutSeconds > 30
)
{
    data.ntpTimeoutSeconds = 8;
}

data.batteryEmptyVoltage =
    preferences.getFloat(KEY_BATTERY_EMPTY, 3.20f);
data.batteryFullVoltage =
    preferences.getFloat(KEY_BATTERY_FULL, 4.20f);
data.batteryCapacityMah =
    preferences.getULong(KEY_BATTERY_CAPACITY, 2000UL);
data.batteryChemistry =
    preferences.getString(KEY_BATTERY_CHEMISTRY, "custom");
data.batteryCellCount =
    preferences.getUChar(KEY_BATTERY_CELLS, 1);

if (
    !isfinite(data.batteryEmptyVoltage) ||
    !isfinite(data.batteryFullVoltage) ||
    data.batteryEmptyVoltage <= 0.0f ||
    data.batteryFullVoltage <= data.batteryEmptyVoltage
)
{
    data.batteryEmptyVoltage = 3.20f;
    data.batteryFullVoltage = 4.20f;
}

if (
    data.batteryCapacityMah == 0 ||
    data.batteryCapacityMah > 100000UL
)
{
    data.batteryCapacityMah = 2000UL;
}

if (data.batteryCellCount == 0)
{
    data.batteryCellCount = 1;
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

    preferences.putULong(
    KEY_MEASURE_INTERVAL,
    data.measureInterval
);

preferences.putBool(KEY_NTP_ENABLED, data.ntpEnabled);
preferences.putString(KEY_TIME_ZONE, data.timeZone);
preferences.putString(KEY_NTP_SERVER_1, data.ntpServer1);
preferences.putString(KEY_NTP_SERVER_2, data.ntpServer2);
preferences.putString(KEY_NTP_SERVER_3, data.ntpServer3);
preferences.putUChar(KEY_NTP_TIMEOUT, data.ntpTimeoutSeconds);

preferences.putFloat(KEY_BATTERY_EMPTY, data.batteryEmptyVoltage);
preferences.putFloat(KEY_BATTERY_FULL, data.batteryFullVoltage);
preferences.putULong(KEY_BATTERY_CAPACITY, data.batteryCapacityMah);
preferences.putString(KEY_BATTERY_CHEMISTRY, data.batteryChemistry);
preferences.putUChar(KEY_BATTERY_CELLS, data.batteryCellCount);

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
