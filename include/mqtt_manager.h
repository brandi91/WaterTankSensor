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


    /*
     * Beispielsweise:
     *
     * WaterTankSensor-a1b2c3d4
     */
    static String buildClientId();


    /*
     * Eindeutige ID aus der ESP32-Chip-ID.
     *
     * Diese ID bleibt auch gleich, wenn der
     * Gerätename später geändert wird.
     */
    static String buildHardwareId();


    /*
     * Beispielsweise:
     *
     * watertank/watertanksensor/state
     */
    static String buildTopic(
        const String& suffix
    );


    /*
     * Sonderzeichen für JSON absichern.
     */
    static String jsonEscape(
        const String& value
    );
};