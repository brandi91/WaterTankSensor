#include "mqtt_manager.h"

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
     * Die Home-Assistant-Discovery-Nachricht ist
     * größer als das Standardlimit von PubSubClient.
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


/*
 * ============================================================
 * Initialisierung
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
 * Verbindung
 * ============================================================
 */

bool MqttManager::connect()
{
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
     * Wenn die Verbindung unerwartet abbricht,
     * veröffentlicht der Broker "offline".
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
     * Discovery zuerst senden.
     *
     * Danach kennt Home Assistant die Sensoren,
     * bevor der erste Messwert veröffentlicht wird.
     */
    publishDiscovery();


    /*
     * Gerät als erreichbar kennzeichnen.
     */
    mqttClient.publish(
        availabilityTopic.c_str(),
        "online",
        true
    );


    publishStatus();
    publishMeasurement();

    return true;
}


void MqttManager::disconnect()
{
    if (mqttClient.connected())
    {
        /*
         * Absichtlich KEIN "offline" veröffentlichen.
         *
         * Das Gerät geht nach jeder Messung in Deep Sleep.
         * Würden wir hier offline senden, wären die Sensoren
         * in Home Assistant fast ständig nicht verfügbar.
         *
         * mqttClient.disconnect() beendet die Verbindung
         * kontrolliert, sodass auch das Last Will nicht
         * ausgelöst wird.
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
    if (!isConnected())
    {
        Logger::warning(
            "MQTT Discovery skipped: "
            "not connected"
        );

        return false;
    }

    const String hardwareId =
        buildHardwareId();

    const String baseTopic =
        buildTopic("");

    const String discoveryTopic =
        "homeassistant/device/" +
        hardwareId +
        "/config";

    const String stateTopic =
    buildTopic("state");

const String availabilityTopic =
    buildTopic("availability");


    /*
     * Ein Sensor gilt als veraltet, wenn mehrere
     * geplante Messzyklen ausbleiben.
     *
     * Beispiel bei 60 Sekunden:
     *
     * 3 × 60 + 60 = 240 Sekunden.
     */
    uint32_t expireAfter =
        static_cast<uint32_t>(
            Settings::data.measureInterval
        ) *
        3UL +
        60UL;

    if (expireAfter < 180UL)
    {
        expireAfter =
            180UL;
    }


    String payload;

    payload.reserve(
        3600
    );


    payload += "{";


    /*
     * Geräteinformationen
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
     * Ursprung der Discovery-Nachricht.
     *
     * Bei Home-Assistant-Device-Discovery ist
     * dieser Bereich erforderlich.
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
 * Gemeinsames Availability-Topic.
 *
 * Bei Device Discovery verwenden wir hier
 * das vollständige Topic und keinen ~-Alias.
 */
payload += "\"availability_topic\":\"";
payload += availabilityTopic;
payload += "\",";

    payload += "\"payload_available\":\"online\",";
    payload += "\"payload_not_available\":\"offline\",";


    /*
     * Sensor-Komponenten
     */
    payload += "\"components\":{";


    /*
     * --------------------------------------------------------
     * Füllstand
     * --------------------------------------------------------
     */
    payload += "\"fill_percent\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Füllstand\",";
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
     * Wasserhöhe
     * --------------------------------------------------------
     */
    payload += "\"water_level\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Wasserhöhe\",";
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
     * Abstand zur Wasseroberfläche
     * --------------------------------------------------------
     */
    payload += "\"distance\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Abstand zur Wasseroberfläche\",";
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
     * Batteriespannung
     * --------------------------------------------------------
     */
    payload += "\"battery_voltage\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Batteriespannung\",";
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
     * Batteriestand
     * --------------------------------------------------------
     */
    payload += "\"battery_percent\":{";

    payload += "\"platform\":\"sensor\",";
    payload += "\"name\":\"Batteriestand\",";
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
     * Gesamtes JSON
     */
    payload += "}";


    /*
     * Discovery retained veröffentlichen.
     *
     * Dadurch bleibt die Gerätekonfiguration
     * im Broker gespeichert.
     */
    const bool result =
        mqttClient.publish(
            discoveryTopic.c_str(),
            payload.c_str(),
            true
        );

    if (result)
    {
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


/*
 * ============================================================
 * Messwerte
 * ============================================================
 */

bool MqttManager::publishMeasurement()
{
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
     * Nicht retained:
     *
     * Home Assistant speichert den letzten Zustand selbst.
     * expire_after erkennt ausgefallene Messzyklen.
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
            return "Deaktiviert";

        case MqttState::Disconnected:
            return "Nicht verbunden";

        case MqttState::Connecting:
            return "Verbindung wird aufgebaut";

        case MqttState::Connected:
            return "Verbunden";

        case MqttState::ConnectionFailed:
            return "Verbindung fehlgeschlagen";

        default:
            return "Unbekannt";
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
 * IDs und Topics
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