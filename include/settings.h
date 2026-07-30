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
    bool wifiDhcp = true;
    String wifiStaticIp;
    String wifiGateway;
    String wifiSubnet;
    String wifiDns1;
    String wifiDns2;

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
     * Nutzbare Tankhöhe.
     */
    float tankHeight = 100.0f;

    /*
     * Abstand zwischen Sensor und maximalem Wasserstand.
     */
    float sensorClearance = 12.0f;

    /*
     * Mess- und Deep-Sleep-Intervall in Sekunden.
     */
    uint32_t measureInterval = 300;

    bool ntpEnabled = true;
    String timeZone = "UTC0";
    String ntpServer1 = "pool.ntp.org";
    String ntpServer2 = "time.nist.gov";
    String ntpServer3 = "time.google.com";
    uint8_t ntpTimeoutSeconds = 8;

    float batteryEmptyVoltage = 3.20f;
    float batteryFullVoltage = 4.20f;
    uint32_t batteryCapacityMah = 2000;
    String batteryChemistry = "custom";
    uint8_t batteryCellCount = 1;

    /*
     * Battery Estimate Test Mode:
     *
     * true:
     * 5 Messpunkte und mindestens 2 Minuten.
     *
     * false:
     * 30 Messpunkte und mindestens 24 Stunden.
     */
    bool batteryEstimateTestMode = false;
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
