#pragma once

#include <Arduino.h>

struct SettingsData
{
    String wifiSSID;
    String wifiPassword;

    String mqttServer;
    uint16_t mqttPort;

    String mqttUser;
    String mqttPassword;

    String deviceName;

    uint16_t tankHeight;
    uint16_t measureInterval;
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