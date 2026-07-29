#pragma once

#include <Arduino.h>

/*
 * ============================================================
 * Gespeicherte Geräteeinstellungen
 * ============================================================
 */

struct SettingsData
{
    /*
     * WLAN
     */
    String wifiSSID;
    String wifiPassword;

    /*
     * MQTT
     */
    String mqttServer;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPassword;
    bool mqttEnabled = false;

    /*
     * Gerät
     */
    String deviceName;

    /*
     * Tankhöhe:
     * Nutzbare Höhe zwischen Tankboden und maximalem
     * Wasserstand.
     */
    float tankHeight = 100.0f;

    /*
     * Sensor Clearance:
     * Abstand zwischen Sensor und maximalem Wasserstand.
     *
     * Der Sensor bleibt dadurch auch bei vollem Tank
     * oberhalb des Wassers.
     */
    float sensorClearance = 12.0f;

    /*
     * Mess- und Deep-Sleep-Intervall in Sekunden.
     */
    uint16_t measureInterval = 300;
};


class Settings
{
public:
    static void begin();

    static void load();
    static void save();
    static void reset();

    static SettingsData data;
};