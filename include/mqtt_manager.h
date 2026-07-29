#pragma once

#include <Arduino.h>

enum class MqttState
{
    Disabled,
    Disconnected,
    Connecting,
    Connected,
    ConnectionFailed
};

class MqttManager
{
public:
    static void begin();
    static void loop();
    static bool connect();
    static void disconnect();
    static bool publishStatus();
    static bool publishMeasurement();
    static bool isConnected();
    static MqttState getState();
    static String getStateText();

private:
    static MqttState state;
    static unsigned long lastReconnectAttempt;
    static unsigned long lastPublishTime;

    static void callback(
        char* topic,
        byte* payload,
        unsigned int length
    );

    static String buildClientId();
    static String buildTopic(const String& suffix);
};