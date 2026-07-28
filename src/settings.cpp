#include "settings.h"

#include <Preferences.h>

Preferences preferences;

SettingsData Settings::data;

void Settings::begin()
{
    preferences.begin("tank", false);

    load();
}

void Settings::load()
{
    data.wifiSSID = preferences.getString("ssid", "");
    data.wifiPassword = preferences.getString("pass", "");

    data.mqttServer = preferences.getString("mqtt", "");
    data.mqttPort = preferences.getUShort("port", 1883);

    data.mqttUser = preferences.getString("user", "");
    data.mqttPassword = preferences.getString("mpass", "");

    data.deviceName = preferences.getString("device", "WaterTankSensor");

    data.tankHeight = preferences.getUShort("tank", 100);

    data.measureInterval = preferences.getUShort("interval", 300);
}

void Settings::save()
{
    preferences.putString("ssid", data.wifiSSID);
    preferences.putString("pass", data.wifiPassword);

    preferences.putString("mqtt", data.mqttServer);
    preferences.putUShort("port", data.mqttPort);

    preferences.putString("user", data.mqttUser);
    preferences.putString("mpass", data.mqttPassword);

    preferences.putString("device", data.deviceName);

    preferences.putUShort("tank", data.tankHeight);

    preferences.putUShort("interval", data.measureInterval);
}

void Settings::reset()
{
    preferences.clear();

    load();
}