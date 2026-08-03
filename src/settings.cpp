#include "settings.h"

#include <Preferences.h>

#include "config.h"
#include "logger.h"


namespace
{
    /*
     * Namespace used in ESP32 Preferences.
     */
    constexpr const char* PREFERENCES_NAMESPACE =
        "tank";

    /*
     * Short keys reduce Preferences storage overhead.
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
    constexpr const char* KEY_AP_SSID = "apSsid";
    constexpr const char* KEY_AP_PASSWORD = "apPass";
    constexpr const char* KEY_AP_IP = "apIp";
    constexpr const char* KEY_AP_GATEWAY = "apGateway";
    constexpr const char* KEY_AP_SUBNET = "apSubnet";
    constexpr const char* KEY_WEB_LOGIN_ENABLED = "webLogin";
    constexpr const char* KEY_WEB_USERNAME = "webUser";
    constexpr const char* KEY_WEB_PASSWORD = "webPass";
    constexpr const char* KEY_WEB_SESSION_TIMEOUT = "webTimeout";

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
     * Air gap between the sensor and the maximum water level.
     */
constexpr const char* KEY_SENSOR_CLEARANCE =
    "clearance";

constexpr const char* KEY_MEASURE_INTERVAL =
    "interval";
// ESP32 NVS limits key names to 15 characters.
constexpr const char* KEY_DEEP_SLEEP_ENABLED =
    "deepSleep";

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
constexpr const char* KEY_BUTTON_PIN = "pinButton";
constexpr const char* KEY_STATUS_LED_PIN = "pinStatus";
constexpr const char* KEY_LED_RED_PIN = "pinRed";
constexpr const char* KEY_LED_GREEN_PIN = "pinGreen";
constexpr const char* KEY_LED_BLUE_PIN = "pinBlue";
constexpr const char* KEY_BATTERY_ADC_PIN = "pinBattery";
constexpr const char* KEY_SENSOR_TRIGGER_PIN = "pinTrigger";
constexpr const char* KEY_SENSOR_ECHO_PIN = "pinEcho";

/*
 * Persisted switch for the accelerated battery-estimator test mode.
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

    if (!preferences.isKey(KEY_DEEP_SLEEP_ENABLED))
    {
        preferences.putBool(KEY_DEEP_SLEEP_ENABLED, true);
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
bool Settings::pinsFallback = false;
String Settings::pinsWarning;
SettingsData Settings::bootPins;


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
     * Wi-Fi
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
     * Device
     */
    data.deviceName =
        preferences.getString(
            KEY_DEVICE_NAME,
            DEFAULT_HOSTNAME
        );


    /*
     * Tank height
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
     * Distance between the sensor and the maximum water level.
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
     * Measurement and deep-sleep interval
     */
    data.measureInterval =
        preferences.getULong(
            KEY_MEASURE_INTERVAL,
            DEFAULT_MEASURE_INTERVAL
        );
    data.deepSleepEnabled =
        preferences.getBool(
            KEY_DEEP_SLEEP_ENABLED,
            true
        );

    data.apSsid = preferences.isKey(KEY_AP_SSID)
        ? preferences.getString(KEY_AP_SSID)
        : CONFIG_AP_SSID;
    data.apPassword = preferences.isKey(KEY_AP_PASSWORD)
        ? preferences.getString(KEY_AP_PASSWORD)
        : CONFIG_AP_PASSWORD;
    data.apIp = preferences.isKey(KEY_AP_IP)
        ? preferences.getString(KEY_AP_IP)
        : CONFIG_AP_IP;
    data.apGateway = preferences.isKey(KEY_AP_GATEWAY)
        ? preferences.getString(KEY_AP_GATEWAY)
        : CONFIG_AP_GATEWAY;
    data.apSubnet = preferences.isKey(KEY_AP_SUBNET)
        ? preferences.getString(KEY_AP_SUBNET)
        : CONFIG_AP_SUBNET;
    data.webLoginEnabled = preferences.isKey(KEY_WEB_LOGIN_ENABLED)
        ? preferences.getBool(KEY_WEB_LOGIN_ENABLED)
        : false;
    data.webUsername = preferences.isKey(KEY_WEB_USERNAME)
        ? preferences.getString(KEY_WEB_USERNAME)
        : "admin";
    data.webPassword = preferences.isKey(KEY_WEB_PASSWORD)
        ? preferences.getString(KEY_WEB_PASSWORD)
        : "";
    data.webSessionTimeoutMinutes =
        preferences.isKey(KEY_WEB_SESSION_TIMEOUT)
            ? preferences.getUShort(KEY_WEB_SESSION_TIMEOUT)
            : 30;
    data.webUsername.trim();
    if (
        data.webUsername.isEmpty() ||
        data.webUsername.length() > 32
    )
    {
        data.webUsername = "admin";
    }
    if (data.webPassword.length() > 64)
    {
        data.webPassword = "";
        data.webLoginEnabled = false;
    }
    if (
        data.webSessionTimeoutMinutes < 1 ||
        data.webSessionTimeoutMinutes > 1440
    )
    {
        data.webSessionTimeoutMinutes = 30;
    }
    if (
        data.webLoginEnabled &&
        data.webPassword.length() < 4
    )
    {
        data.webLoginEnabled = false;
    }

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
 * Load battery-estimator test mode.
 */
data.batteryEstimateTestMode =
    preferences.getBool(
        KEY_BATTERY_ESTIMATE_TEST_MODE,
        false
    );

data.buttonPin = preferences.isKey(KEY_BUTTON_PIN)
    ? preferences.getUChar(KEY_BUTTON_PIN)
    : DEFAULT_BUTTON_PIN;
data.statusLedPin = preferences.isKey(KEY_STATUS_LED_PIN)
    ? preferences.getUChar(KEY_STATUS_LED_PIN)
    : DEFAULT_STATUS_LED_PIN;
data.ledRedPin = preferences.isKey(KEY_LED_RED_PIN)
    ? preferences.getUChar(KEY_LED_RED_PIN)
    : DEFAULT_LED_RED_PIN;
data.ledGreenPin = preferences.isKey(KEY_LED_GREEN_PIN)
    ? preferences.getUChar(KEY_LED_GREEN_PIN)
    : DEFAULT_LED_GREEN_PIN;
data.ledBluePin = preferences.isKey(KEY_LED_BLUE_PIN)
    ? preferences.getUChar(KEY_LED_BLUE_PIN)
    : DEFAULT_LED_BLUE_PIN;
data.batteryAdcPin = preferences.isKey(KEY_BATTERY_ADC_PIN)
    ? preferences.getUChar(KEY_BATTERY_ADC_PIN)
    : DEFAULT_BATTERY_ADC_PIN;
data.sensorTriggerPin = preferences.isKey(KEY_SENSOR_TRIGGER_PIN)
    ? preferences.getUChar(KEY_SENSOR_TRIGGER_PIN)
    : DEFAULT_SENSOR_TRIGGER_PIN;
data.sensorEchoPin = preferences.isKey(KEY_SENSOR_ECHO_PIN)
    ? preferences.getUChar(KEY_SENSOR_ECHO_PIN)
    : DEFAULT_SENSOR_ECHO_PIN;

// Migrate the former default pairs to the verified JSN-SR04T wiring.
if ((data.sensorTriggerPin == 5 && data.sensorEchoPin == 18) ||
    (data.sensorTriggerPin == 22 && data.sensorEchoPin == 23))
{
    data.sensorTriggerPin = DEFAULT_SENSOR_TRIGGER_PIN;
    data.sensorEchoPin = DEFAULT_SENSOR_ECHO_PIN;
    Logger::info("Migrated ultrasonic pins to trigger GPIO 23 / echo GPIO 22");
}

String pinError;
pinsFallback = !validatePins(data, pinError);
if (pinsFallback)
{
    pinsWarning =
        "Stored pin configuration is invalid (" + pinError +
        "). Compile-time defaults are active for this boot.";
    Logger::error(pinsWarning);
    restoreDefaultPins();
}
else
{
    pinsWarning = "";
}
bootPins = data;

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
     * Wi-Fi
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

    preferences.putString(KEY_AP_SSID, data.apSsid);
    preferences.putString(KEY_AP_PASSWORD, data.apPassword);
    preferences.putString(KEY_AP_IP, data.apIp);
    preferences.putString(KEY_AP_GATEWAY, data.apGateway);
    preferences.putString(KEY_AP_SUBNET, data.apSubnet);
    preferences.putBool(KEY_WEB_LOGIN_ENABLED, data.webLoginEnabled);
    preferences.putString(KEY_WEB_USERNAME, data.webUsername);
    preferences.putString(KEY_WEB_PASSWORD, data.webPassword);
    preferences.putUShort(
        KEY_WEB_SESSION_TIMEOUT,
        data.webSessionTimeoutMinutes
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
     * Device and tank
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
    preferences.putBool(
        KEY_DEEP_SLEEP_ENABLED,
        data.deepSleepEnabled
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

preferences.putUChar(KEY_BUTTON_PIN, data.buttonPin);
preferences.putUChar(KEY_STATUS_LED_PIN, data.statusLedPin);
preferences.putUChar(KEY_LED_RED_PIN, data.ledRedPin);
preferences.putUChar(KEY_LED_GREEN_PIN, data.ledGreenPin);
preferences.putUChar(KEY_LED_BLUE_PIN, data.ledBluePin);
preferences.putUChar(KEY_BATTERY_ADC_PIN, data.batteryAdcPin);
preferences.putUChar(KEY_SENSOR_TRIGGER_PIN, data.sensorTriggerPin);
preferences.putUChar(KEY_SENSOR_ECHO_PIN, data.sensorEchoPin);

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

namespace
{
bool containsPin(const uint8_t pin, const uint8_t* pins, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (pins[i] == pin)
        {
            return true;
        }
    }
    return false;
}
}

bool Settings::validatePins(
    const SettingsData& candidate,
    String& error
)
{
    // Conservative ESP32 DevKit V1 allowlists. GPIO 6-11 (flash),
    // UART0 1/3 and unsafe strapping pins are excluded from custom use.
    static const uint8_t outputPins[] =
        {4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32};
    static const uint8_t inputPins[] =
        {4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32,
         35, 36, 39};
    static const uint8_t pullupPins[] =
        {4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
    static const uint8_t adc1Pins[] = {32, 35, 36, 39};

    struct PinUse
    {
        const char* name;
        uint8_t pin;
        const uint8_t* allowed;
        size_t count;
        uint8_t legacyDefault;
    };

    const PinUse uses[] =
    {
        {"Button", candidate.buttonPin, pullupPins,
            sizeof(pullupPins), DEFAULT_BUTTON_PIN},
        {"Status LED", candidate.statusLedPin, outputPins,
            sizeof(outputPins), DEFAULT_STATUS_LED_PIN},
        {"RGB LED Red", candidate.ledRedPin, outputPins,
            sizeof(outputPins), DEFAULT_LED_RED_PIN},
        {"RGB LED Green", candidate.ledGreenPin, outputPins,
            sizeof(outputPins), DEFAULT_LED_GREEN_PIN},
        {"RGB LED Blue", candidate.ledBluePin, outputPins,
            sizeof(outputPins), DEFAULT_LED_BLUE_PIN},
        {"Battery ADC", candidate.batteryAdcPin, adc1Pins,
            sizeof(adc1Pins), DEFAULT_BATTERY_ADC_PIN},
        {"Sensor Trigger", candidate.sensorTriggerPin, outputPins,
            sizeof(outputPins), DEFAULT_SENSOR_TRIGGER_PIN},
        {"Sensor Echo", candidate.sensorEchoPin, inputPins,
            sizeof(inputPins), DEFAULT_SENSOR_ECHO_PIN}
    };

    for (const PinUse& use : uses)
    {
        if (
            use.pin != use.legacyDefault &&
            !containsPin(
                use.pin,
                use.allowed,
                use.count / sizeof(uint8_t)
            )
        )
        {
            error =
                String(use.name) + " cannot use GPIO " +
                String(use.pin) + " on ESP32 DevKit V1.";
            return false;
        }
    }

    for (size_t first = 0; first < sizeof(uses) / sizeof(uses[0]); ++first)
    {
        for (
            size_t second = first + 1;
            second < sizeof(uses) / sizeof(uses[0]);
            ++second
        )
        {
            if (uses[first].pin == uses[second].pin)
            {
                error =
                    "GPIO " + String(uses[first].pin) +
                    " is assigned to both " + uses[first].name +
                    " and " + uses[second].name + ".";
                return false;
            }
        }
    }

    return true;
}

void Settings::restoreDefaultPins()
{
    data.buttonPin = DEFAULT_BUTTON_PIN;
    data.statusLedPin = DEFAULT_STATUS_LED_PIN;
    data.ledRedPin = DEFAULT_LED_RED_PIN;
    data.ledGreenPin = DEFAULT_LED_GREEN_PIN;
    data.ledBluePin = DEFAULT_LED_BLUE_PIN;
    data.batteryAdcPin = DEFAULT_BATTERY_ADC_PIN;
    data.sensorTriggerPin = DEFAULT_SENSOR_TRIGGER_PIN;
    data.sensorEchoPin = DEFAULT_SENSOR_ECHO_PIN;
}

bool Settings::pinFallbackActive()
{
    return pinsFallback;
}

String Settings::pinWarning()
{
    return pinsWarning;
}

const SettingsData& Settings::activePins()
{
    return bootPins;
}
