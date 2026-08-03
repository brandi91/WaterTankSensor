#pragma once

#include <Arduino.h>
#include "config.h"

/*
 * ============================================================
 * Persisted device settings
 * ============================================================
 */

struct SettingsData
{
    /*
     * Wi-Fi
     */
    String wifiSSID;
    String wifiPassword;
    bool wifiDhcp = true;
    String wifiStaticIp;
    String wifiGateway;
    String wifiSubnet;
    String wifiDns1;
    String wifiDns2;

    String apSsid = CONFIG_AP_SSID;
    String apPassword = CONFIG_AP_PASSWORD;
    String apIp = CONFIG_AP_IP;
    String apGateway = CONFIG_AP_GATEWAY;
    String apSubnet = CONFIG_AP_SUBNET;

    /*
     * Optional local web access barrier.
     */
    bool webLoginEnabled = false;
    String webUsername = "admin";
    String webPassword;
    uint16_t webSessionTimeoutMinutes = 30;

    /*
     * MQTT
     */
    String mqttServer;
    uint16_t mqttPort = 1883;
    String mqttUser;
    String mqttPassword;
    bool mqttEnabled = false;

    /*
     * Device
     */
    String deviceName;

    /*
     * Usable tank height.
     */
    float tankHeight = 100.0f;

    /*
     * Distance between the sensor and the maximum water level.
     */
    float sensorClearance = 12.0f;

    /*
     * Measurement and deep-sleep interval in seconds.
     */
    uint32_t measureInterval = 300;
    bool deepSleepEnabled = true;

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
     * 5 samples and at least 2 minutes.
     *
     * false:
     * 30 samples and at least 24 hours.
     */
    bool batteryEstimateTestMode = false;

    uint8_t buttonPin = DEFAULT_BUTTON_PIN;
    uint8_t statusLedPin = DEFAULT_STATUS_LED_PIN;
    uint8_t ledRedPin = DEFAULT_LED_RED_PIN;
    uint8_t ledGreenPin = DEFAULT_LED_GREEN_PIN;
    uint8_t ledBluePin = DEFAULT_LED_BLUE_PIN;
    uint8_t batteryAdcPin = DEFAULT_BATTERY_ADC_PIN;
    uint8_t sensorTriggerPin = DEFAULT_SENSOR_TRIGGER_PIN;
    uint8_t sensorEchoPin = DEFAULT_SENSOR_ECHO_PIN;
};


class Settings
{
public:
    static void begin();

    static void load();
    static void save();
    static void reset();
    static bool validatePins(
        const SettingsData& candidate,
        String& error
    );
    static void restoreDefaultPins();
    static bool pinFallbackActive();
    static String pinWarning();
    static const SettingsData& activePins();

    static SettingsData data;

private:
    static bool pinsFallback;
    static String pinsWarning;
    static SettingsData bootPins;
};
