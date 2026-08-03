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

    static bool publishDiscovery();
    static String getDiscoveryStatusText();
    static void recordDiscoveryResult(bool published);
    static bool publishStatus();
    static bool publishMeasurement();

    static bool isConnected();

    static MqttState getState();
    static String getStateText();


private:
    static MqttState state;

    static unsigned long lastReconnectAttempt;
    static unsigned long lastPublishTime;
    static String discoveryStatus;


    static void callback(
        char* topic,
        byte* payload,
        unsigned int length
    );


    /*
     * Example:
     *
     * WaterTankSensor-a1b2c3d4
     */
    static String buildClientId();


    /*
     * Stable identifier derived from the ESP32 chip ID.
     *
     * This ID remains unchanged when the device name changes.
     */
    static String buildHardwareId();


    /*
     * Example:
     *
     * watertank/watertanksensor/state
     */
    static String buildTopic(
        const String& suffix
    );


    /*
     * Escape special characters for JSON.
     */
    static String jsonEscape(
        const String& value
    );
};
