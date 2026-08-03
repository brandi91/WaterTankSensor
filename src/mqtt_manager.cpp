#include "mqtt_manager.h"
#include "sleep_manager.h"

#include <WiFi.h>
#include <PubSubClient.h>

#include "battery.h"
#include "config.h"
#include "logger.h"
#include "sensor.h"
#include "settings.h"
#include "version.h"
#include "wifi_manager.h"


namespace
{
    WiFiClient wifiClient;
    PubSubClient mqttClient(
        wifiClient
    );


    /*
     * The Home Assistant discovery payload exceeds PubSubClient's default
     * packet-size limit.
     */
    constexpr uint16_t MQTT_BUFFER_SIZE =
        4096;
}


MqttState MqttManager::state =
    MqttState::Disabled;

unsigned long
    MqttManager::lastReconnectAttempt = 0;

unsigned long
    MqttManager::lastPublishTime = 0;

String MqttManager::discoveryStatus =
    "Not published yet";


/*
 * ============================================================
 * Initialization
 * ============================================================
 */

void MqttManager::begin()
{
    mqttClient.setCallback(
        callback
    );

    if (
        !mqttClient.setBufferSize(
            MQTT_BUFFER_SIZE
        )
    )
    {
        Logger::warning(
            "Could not enlarge MQTT buffer"
        );
    }

    state =
        MqttState::Disconnected;

    Logger::info(
        "MQTT Manager initialized"
    );

    Logger::info(
        "MQTT buffer size: " +
        String(MQTT_BUFFER_SIZE) +
        " bytes"
    );
}


/*
 * ============================================================
 * Loop
 * ============================================================
 */

void MqttManager::loop()
{
    if (!SleepManager::canStartNormalWork())
    {
        return;
    }

    if (!Settings::data.mqttEnabled)
    {
        state =
            MqttState::Disabled;

        return;
    }

    if (!WifiManager::isConnected())
    {
        state =
            MqttState::Disconnected;

        return;
    }

    if (!isConnected())
    {
        const unsigned long now =
            millis();

        if (
            now - lastReconnectAttempt >=
            MQTT_RECONNECT_INTERVAL_MS
        )
        {
            lastReconnectAttempt =
                now;

            connect();
        }

        return;
    }

    mqttClient.loop();

    const unsigned long now =
        millis();

    if (
        now - lastPublishTime >=
        MQTT_PUBLISH_INTERVAL_MS
    )
    {
        lastPublishTime =
            now;

        publishMeasurement();
        publishStatus();
    }
}


/*
 * ============================================================
 * Connection
 * ============================================================
 */

bool MqttManager::connect()
{
    if (!SleepManager::canStartNormalWork())
    {
        return false;
    }

    if (!Settings::data.mqttEnabled)
    {
        Logger::warning(
            "MQTT is disabled in settings"
        );

        state =
            MqttState::Disabled;

        return false;
    }

    if (!WifiManager::isConnected())
    {
        Logger::warning(
            "MQTT connection skipped: "
            "Wi-Fi disconnected"
        );

        state =
            MqttState::Disconnected;

        return false;
    }

    if (Settings::data.mqttServer.isEmpty())
    {
        Logger::warning(
            "MQTT server is empty"
        );

        state =
            MqttState::Disabled;

        return false;
    }

    mqttClient.setServer(
        Settings::data.mqttServer.c_str(),
        Settings::data.mqttPort
    );

    const String clientId =
        buildClientId();

    const String availabilityTopic =
        buildTopic(
            "availability"
        );

    Logger::info(
        "Connecting to MQTT broker: " +
        Settings::data.mqttServer +
        ":" +
        String(
            Settings::data.mqttPort
        )
    );

    state =
        MqttState::Connecting;

    bool connected = false;

    /*
     * Last Will:
     *
     * The broker publishes "offline" when the connection ends unexpectedly.
     */
    if (Settings::data.mqttUser.isEmpty())
    {
        connected =
            mqttClient.connect(
                clientId.c_str(),
                availabilityTopic.c_str(),
                0,
                true,
                "offline"
            );
    }
    else
    {
        connected =
            mqttClient.connect(
                clientId.c_str(),
                Settings::data.mqttUser.c_str(),
                Settings::data.mqttPassword.c_str(),
                availabilityTopic.c_str(),
                0,
                true,
                "offline"
            );
    }

    if (!connected)
    {
        state =
            MqttState::ConnectionFailed;

        Logger::warning(
            "MQTT connection failed, state: " +
            String(
                mqttClient.state()
            )
        );

        return false;
    }

    state =
        MqttState::Connected;

    Logger::info(
        "MQTT connected"
    );


    /*
     * Publish discovery first.
     *
     * This lets Home Assistant register the sensors before the first state.
     */
    /*
     * Mark the device as available.
     */
    mqttClient.publish(
        availabilityTopic.c_str(),
        "online",
        true
    );

    publishDiscovery();
    publishStatus();
    publishMeasurement();

    return true;
}


void MqttManager::disconnect()
{
    if (mqttClient.connected())
    {
        /*
         * Deliberately do not publish "offline" here.
         *
         * The device enters deep sleep after each measurement. Publishing
         * offline here would leave the Home Assistant sensors unavailable
         * most of the time.
         *
         * A clean disconnect also prevents the Last Will from firing.
         */
        mqttClient.disconnect();
    }

    state =
        MqttState::Disconnected;

    Logger::info(
        "MQTT disconnected for deep sleep"
    );
}


/*
 * ============================================================
 * Home Assistant MQTT Discovery
 * ============================================================
 */

bool MqttManager::publishDiscovery()
{
    if (!SleepManager::canStartNormalWork())
    {
        return false;
    }

    if (!isConnected())
    {
        Logger::warning(
            "MQTT Discovery skipped: "
            "not connected"
        );

        discoveryStatus = "Publish failed";
        return false;
    }

    const String hardwareId =
        buildHardwareId();


    const String discoveryTopic =
        "homeassistant/device/" +
        hardwareId +
        "/config";

    const String stateTopic =
    buildTopic("state");

const String availabilityTopic =
    buildTopic("availability");


    /*
     * Measurement interval plus 10 percent tolerance, rounded up to a whole
     * second. Example: 300 + 30 = 330 seconds.
     */
    const uint32_t measureInterval =
        static_cast<uint32_t>(
            Settings::data.measureInterval
        );
    const uint32_t expireAfter =
        measureInterval +
        (measureInterval + 9UL) / 10UL;


    String payload;

    payload.reserve(
        3600
    );


    payload += "{";


    /*
     * Device information
     */
    payload += "\"device\":{";

    payload += "\"identifiers\":[\"";
    payload += hardwareId;
    payload += "\"],";

    payload += "\"name\":\"";
    payload += jsonEscape(
        Settings::data.deviceName
    );
    payload += "\",";

    payload += "\"manufacturer\":\"DIY\",";
    payload += "\"model\":\"ESP32 WaterTankSensor\",";

    payload += "\"sw_version\":\"";
    payload += jsonEscape(
        FW_VERSION
    );
    payload += "\",";

    payload += "\"serial_number\":\"";
    payload += hardwareId;
    payload += "\"";

    payload += "},";


    /*
     * Discovery-message origin.
     *
     * Home Assistant device discovery requires this section.
     */
    payload += "\"origin\":{";

    payload += "\"name\":\"WaterTankSensor\",";
    payload += "\"sw\":\"";
    payload += jsonEscape(
        FW_VERSION
    );
    payload += "\"";

    payload += "},";


/*
 * Shared availability topic.
 *
 * Device discovery uses the complete topic instead of a ~ alias.
 */
payload += "\"availability_topic\":\"";
payload += availabilityTopic;
payload += "\",";

    payload += "\"payload_available\":\"online\",";
    payload += "\"payload_not_available\":\"offline\",";


    /*
     * Sensor components
     */
    payload += "\"components\":{";


    /*
     * --------------------------------------------------------
     * Fill level
     * --------------------------------------------------------
     */
    payload += "\"fill_percent\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Fill Level\",";
    payload += "\"unique_id\":\"";
    payload += hardwareId;
    payload += "_fill_percent\",";

    payload += "\"state_topic\":\"";
payload += stateTopic;
payload += "\",";
    payload += "\"value_template\":";
    payload += "\"{{ value_json.fill_percent }}\",";

    payload += "\"unit_of_measurement\":\"%\",";
    payload += "\"state_class\":\"measurement\",";
    payload += "\"icon\":\"mdi:water-percent\",";
    payload += "\"suggested_display_precision\":0,";
    payload += "\"expire_after\":";
    payload += String(
        expireAfter
    );

    payload += "},";


    /*
     * --------------------------------------------------------
     * Water level
     * --------------------------------------------------------
     */
    payload += "\"water_level\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Water Level\",";
    payload += "\"unique_id\":\"";
    payload += hardwareId;
    payload += "_water_level_cm\",";

    payload += "\"state_topic\":\"";
payload += stateTopic;
payload += "\",";
    payload += "\"value_template\":";
    payload += "\"{{ value_json.water_level_cm }}\",";

    payload += "\"unit_of_measurement\":\"cm\",";
    payload += "\"device_class\":\"distance\",";
    payload += "\"state_class\":\"measurement\",";
    payload += "\"icon\":\"mdi:waves-arrow-up\",";
    payload += "\"suggested_display_precision\":1,";
    payload += "\"expire_after\":";
    payload += String(
        expireAfter
    );

    payload += "},";


    /*
     * --------------------------------------------------------
     * Distance to the water surface
     * --------------------------------------------------------
     */
    payload += "\"distance\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Distance to Water Surface\",";
    payload += "\"unique_id\":\"";
    payload += hardwareId;
    payload += "_distance_cm\",";

    payload += "\"state_topic\":\"";
payload += stateTopic;
payload += "\",";
    payload += "\"value_template\":";
    payload += "\"{{ value_json.distance_cm }}\",";

    payload += "\"unit_of_measurement\":\"cm\",";
    payload += "\"device_class\":\"distance\",";
    payload += "\"state_class\":\"measurement\",";
    payload += "\"icon\":\"mdi:arrow-expand-vertical\",";
    payload += "\"suggested_display_precision\":1,";
    payload += "\"expire_after\":";
    payload += String(
        expireAfter
    );

    payload += "},";


    /*
     * --------------------------------------------------------
     * Battery voltage
     * --------------------------------------------------------
     */
    payload += "\"battery_voltage\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Battery Voltage\",";
    payload += "\"unique_id\":\"";
    payload += hardwareId;
    payload += "_battery_voltage\",";

    payload += "\"state_topic\":\"";
payload += stateTopic;
payload += "\",";
    payload += "\"value_template\":";
    payload += "\"{{ value_json.battery_voltage }}\",";

    payload += "\"unit_of_measurement\":\"V\",";
    payload += "\"device_class\":\"voltage\",";
    payload += "\"state_class\":\"measurement\",";
    payload += "\"entity_category\":\"diagnostic\",";
    payload += "\"suggested_display_precision\":2,";
    payload += "\"expire_after\":";
    payload += String(
        expireAfter
    );

    payload += "},";


    /*
     * --------------------------------------------------------
     * Battery level
     * --------------------------------------------------------
     */
    payload += "\"battery_percent\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Battery Level\",";
    payload += "\"unique_id\":\"";
    payload += hardwareId;
    payload += "_battery_percent\",";

    payload += "\"state_topic\":\"";
payload += stateTopic;
payload += "\",";
    payload += "\"value_template\":";
    payload += "\"{{ value_json.battery_percent }}\",";

    payload += "\"unit_of_measurement\":\"%\",";
    payload += "\"device_class\":\"battery\",";
    payload += "\"state_class\":\"measurement\",";
    payload += "\"entity_category\":\"diagnostic\",";
    payload += "\"suggested_display_precision\":0,";
    payload += "\"expire_after\":";
    payload += String(
        expireAfter
    );

    payload += "}";


    /*
     * components
     */
    payload += "}";


    /*
     * Complete JSON document
     */
    payload += "}";


    /*
     * Retain the discovery payload.
     *
     * This keeps the device configuration available in the broker.
     */
    const bool result =
        mqttClient.publish(
            discoveryTopic.c_str(),
            payload.c_str(),
            true
        );

    if (result)
    {
        discoveryStatus = "Published";
        Logger::info(
            "Home Assistant MQTT Discovery published"
        );

        Logger::info(
            "Discovery topic: " +
            discoveryTopic
        );
    }
    else
    {
        discoveryStatus = "Publish failed";
        Logger::warning(
            "Home Assistant MQTT Discovery publish failed"
        );

        Logger::warning(
            "Discovery payload size: " +
            String(
                payload.length()
            ) +
            " bytes"
        );
    }

    return result;
}

String MqttManager::getDiscoveryStatusText()
{
    if (!Settings::data.mqttEnabled)
    {
        return "Disabled";
    }

    return discoveryStatus;
}

void MqttManager::recordDiscoveryResult(
    bool published
)
{
    discoveryStatus =
        published
            ? "Published"
            : "Publish failed";
}


/*
 * ============================================================
     * Measurements
 * ============================================================
 */

bool MqttManager::publishMeasurement()
{
    if (!SleepManager::canStartNormalWork())
    {
        return false;
    }

    if (!isConnected())
    {
        return false;
    }

    if (!Sensor::isValid())
    {
        Logger::warning(
            "MQTT measurement skipped: "
            "sensor value is invalid"
        );

        return false;
    }

    const String topic =
        buildTopic(
            "state"
        );

    String payload;

    payload.reserve(
        200
    );

    payload += "{";

    payload += "\"distance_cm\":";
    payload += String(
        Sensor::getDistanceCm(),
        1
    );
    payload += ",";

    payload += "\"water_level_cm\":";
    payload += String(
        Sensor::getWaterLevelCm(),
        1
    );
    payload += ",";

    payload += "\"fill_percent\":";
    payload += String(
        Sensor::getPercentage()
    );
    payload += ",";

    payload += "\"battery_voltage\":";
    payload += String(
        Battery::getVoltage(),
        2
    );
    payload += ",";

    payload += "\"battery_percent\":";
    payload += String(
        Battery::getPercentage()
    );

    payload += "}";


    /*
     * Do not retain state messages:
     *
     * Home Assistant stores the latest state, while expire_after detects
     * missed measurement cycles.
     */
    const bool result =
        mqttClient.publish(
            topic.c_str(),
            payload.c_str(),
            false
        );

    if (result)
    {
        Logger::info(
            "MQTT measurement published: " +
            payload
        );
    }
    else
    {
        Logger::warning(
            "MQTT measurement publish failed"
        );
    }

    return result;
}


/*
 * ============================================================
 * Status
 * ============================================================
 */

bool MqttManager::publishStatus()
{
    if (!SleepManager::canStartNormalWork())
    {
        return false;
    }

    if (!isConnected())
    {
        return false;
    }

    const String topic =
        buildTopic(
            "status"
        );

    String payload;

    payload.reserve(
        160
    );

    payload += "{";

    payload += "\"wifi_rssi\":";
    payload += String(
        WifiManager::getRssi()
    );
    payload += ",";

    payload += "\"ip\":\"";
    payload += jsonEscape(
        WifiManager::getIpAddress()
    );
    payload += "\",";

    payload += "\"firmware\":\"";
    payload += jsonEscape(
        FW_VERSION
    );
    payload += "\"";

    payload += "}";

    return mqttClient.publish(
        topic.c_str(),
        payload.c_str(),
        false
    );
}


/*
 * ============================================================
 * Statusabfragen
 * ============================================================
 */

bool MqttManager::isConnected()
{
    return mqttClient.connected();
}


MqttState MqttManager::getState()
{
    return state;
}


String MqttManager::getStateText()
{
    switch (state)
    {
        case MqttState::Disabled:
            return "Disabled";

        case MqttState::Disconnected:
            return "Disconnected";

        case MqttState::Connecting:
            return "Connecting";

        case MqttState::Connected:
            return "Connected";

        case MqttState::ConnectionFailed:
            return "Connection failed";

        default:
            return "Unknown";
    }
}


/*
 * ============================================================
 * Empfangene MQTT-Nachrichten
 * ============================================================
 */

void MqttManager::callback(
    char* topic,
    byte* payload,
    unsigned int length
)
{
    String message;

    message.reserve(
        length
    );

    for (
        unsigned int index = 0;
        index < length;
        index++
    )
    {
        message +=
            static_cast<char>(
                payload[index]
            );
    }

    Logger::info(
        "MQTT message received [" +
        String(topic) +
        "]: " +
        message
    );
}


/*
 * ============================================================
 * IDs and topics
 * ============================================================
 */

String MqttManager::buildHardwareId()
{
    const uint64_t chipId =
        ESP.getEfuseMac();

    const uint16_t upper =
        static_cast<uint16_t>(
            chipId >> 32
        );

    const uint32_t lower =
        static_cast<uint32_t>(
            chipId
        );

    char idBuffer[32];

    snprintf(
        idBuffer,
        sizeof(idBuffer),
        "watertank_%04x%08x",
        upper,
        lower
    );

    String hardwareId =
        idBuffer;

    hardwareId.toLowerCase();

    return hardwareId;
}


String MqttManager::buildClientId()
{
    String clientId =
        Settings::data.deviceName;

    clientId.replace(
        " ",
        "-"
    );

    clientId += "-";
    clientId += buildHardwareId();

    return clientId;
}


String MqttManager::buildTopic(
    const String& suffix
)
{
    String deviceName =
        Settings::data.deviceName;

    deviceName.toLowerCase();

    deviceName.replace(
        " ",
        "_"
    );

    String topic =
        "watertank/" +
        deviceName;

    if (!suffix.isEmpty())
    {
        topic += "/";
        topic += suffix;
    }

    return topic;
}


/*
 * ============================================================
 * JSON absichern
 * ============================================================
 */

String MqttManager::jsonEscape(
    const String& value
)
{
    String escaped =
        value;

    escaped.replace(
        "\\",
        "\\\\"
    );

    escaped.replace(
        "\"",
        "\\\""
    );

    escaped.replace(
        "\n",
        "\\n"
    );

    escaped.replace(
        "\r",
        "\\r"
    );

    escaped.replace(
        "\t",
        "\\t"
    );

    return escaped;
}
