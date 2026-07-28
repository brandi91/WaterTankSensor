#include "mqtt_manager.h"

#include <WiFi.h>
#include <PubSubClient.h>

#include "battery.h"
#include "logger.h"
#include "sensor.h"
#include "settings.h"
#include "wifi_manager.h"
#include "config.h"

namespace
{
    WiFiClient wifiClient;
    PubSubClient mqttClient(wifiClient);
}

MqttState MqttManager::state =
    MqttState::Disabled;

unsigned long MqttManager::lastReconnectAttempt = 0;
unsigned long MqttManager::lastPublishTime = 0;

void MqttManager::begin()
{
    mqttClient.setCallback(callback);

    state = MqttState::Disconnected;

    Logger::info("MQTT Manager initialized");
}

void MqttManager::loop()
{
    if (!WifiManager::isConnected())
    {
        state = MqttState::Disconnected;
        return;
    }

    if (!isConnected())
    {
        const unsigned long now = millis();

        if (
            now - lastReconnectAttempt >=
            MQTT_RECONNECT_INTERVAL_MS
        )
        {
            lastReconnectAttempt = now;
            connect();
        }

        return;
    }

    mqttClient.loop();

    const unsigned long now = millis();

    if (
        now - lastPublishTime >=
        MQTT_PUBLISH_INTERVAL_MS
    )
    {
        lastPublishTime = now;

        publishMeasurement();
        publishStatus();
    }
}

bool MqttManager::connect()
{
    if (!WifiManager::isConnected())
    {
        Logger::warning(
            "MQTT connection skipped: Wi-Fi disconnected"
        );

        state = MqttState::Disconnected;
        return false;
    }

    if (Settings::data.mqttServer.isEmpty())
    {
        Logger::warning("MQTT server is empty");

        state = MqttState::Disabled;
        return false;
    }

    mqttClient.setServer(
        Settings::data.mqttServer.c_str(),
        Settings::data.mqttPort
    );

    const String clientId = buildClientId();

    const String availabilityTopic =
        buildTopic("availability");

    Logger::info(
        "Connecting to MQTT broker: " +
        Settings::data.mqttServer +
        ":" +
        String(Settings::data.mqttPort)
    );

    state = MqttState::Connecting;

    bool connected = false;

    if (Settings::data.mqttUser.isEmpty())
    {
        connected = mqttClient.connect(
            clientId.c_str(),
            availabilityTopic.c_str(),
            0,
            true,
            "offline"
        );
    }
    else
    {
        connected = mqttClient.connect(
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
        state = MqttState::ConnectionFailed;

        Logger::warning(
            "MQTT connection failed, state: " +
            String(mqttClient.state())
        );

        return false;
    }

    state = MqttState::Connected;

    Logger::info("MQTT connected");

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
        mqttClient.publish(
            buildTopic("availability").c_str(),
            "offline",
            true
        );

        mqttClient.disconnect();
    }

    state = MqttState::Disconnected;

    Logger::info("MQTT disconnected");
}

bool MqttManager::publishMeasurement()
{
    if (!isConnected())
    {
        return false;
    }

    const String topic =
        buildTopic("state");

    String payload;

    payload.reserve(180);

    payload += "{";
    payload += "\"distance_cm\":";
    payload += String(Sensor::getDistanceCm(), 1);
    payload += ",";

    payload += "\"water_level_cm\":";
    payload += String(Sensor::getWaterLevelCm(), 1);
    payload += ",";

    payload += "\"fill_percent\":";
    payload += String(Sensor::getPercentage());
    payload += ",";

    payload += "\"battery_voltage\":";
    payload += String(Battery::getVoltage(), 2);
    payload += ",";

    payload += "\"battery_percent\":";
    payload += String(Battery::getPercentage());

    payload += "}";

    const bool result = mqttClient.publish(
        topic.c_str(),
        payload.c_str(),
        true
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

bool MqttManager::publishStatus()
{
    if (!isConnected())
    {
        return false;
    }

    const String topic =
        buildTopic("status");

    String payload;

    payload.reserve(120);

    payload += "{";
    payload += "\"wifi_rssi\":";
    payload += String(WifiManager::getRssi());
    payload += ",";

    payload += "\"ip\":\"";
    payload += WifiManager::getIpAddress();
    payload += "\",";

    payload += "\"firmware\":\"";
    payload += FW_VERSION;
    payload += "\"";

    payload += "}";

    return mqttClient.publish(
        topic.c_str(),
        payload.c_str(),
        true
    );
}

bool MqttManager::isConnected()
{
    return mqttClient.connected();
}

MqttState MqttManager::getState()
{
    return state;
}

void MqttManager::callback(
    char* topic,
    byte* payload,
    unsigned int length
)
{
    String message;

    message.reserve(length);

    for (unsigned int index = 0; index < length; index++)
    {
        message += static_cast<char>(payload[index]);
    }

    Logger::info(
        "MQTT message received [" +
        String(topic) +
        "]: " +
        message
    );
}

String MqttManager::buildClientId()
{
    String clientId =
        Settings::data.deviceName;

    clientId.replace(" ", "-");

    clientId += "-";
    clientId += String(
        static_cast<uint32_t>(
            ESP.getEfuseMac()
        ),
        HEX
    );

    return clientId;
}

String MqttManager::buildTopic(
    const String& suffix
)
{
    String deviceName =
        Settings::data.deviceName;

    deviceName.toLowerCase();
    deviceName.replace(" ", "_");

    return
        "watertank/" +
        deviceName +
        "/" +
        suffix;
}