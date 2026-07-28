#include <Arduino.h>

#include "config.h"
#include "version.h"
#include "sleep_manager.h"
#include "logger.h"
#include "settings.h"
#include "led.h"
#include "battery.h"
#include "button.h"
#include "sensor.h"
#include "wifi_manager.h"
#include "web_server_manager.h"

#if MQTT_ENABLED
#include "mqtt_manager.h"
#endif

enum class WebIndicatorMode
{
    Off,
    WebServer,
    ConfigPortal
};

unsigned long lastBatteryLog = 0;

bool automaticWebServerAllowed = false;
bool automaticWebServerStarted = false;

WebIndicatorMode webIndicatorMode =
    WebIndicatorMode::Off;

bool webIndicatorLedState = false;

unsigned long lastWebIndicatorToggle = 0;
unsigned long webIndicatorStartedAt = 0;

constexpr unsigned long WEB_INDICATOR_DURATION_MS =
    15000UL;

void updateWebIndicator()
{
    if (
        webIndicatorMode ==
        WebIndicatorMode::Off
    )
    {
        return;
    }

    const unsigned long now =
        millis();

    /*
     * Nach 15 Sekunden Blinken abschalten.
     * Webserver oder Config-AP laufen weiter.
     */
    if (
        now - webIndicatorStartedAt >=
        WEB_INDICATOR_DURATION_MS
    )
    {
        webIndicatorMode =
            WebIndicatorMode::Off;

        webIndicatorLedState = false;

        Led::off();

        Logger::info(
            "Web indicator LED finished"
        );

        return;
    }

    if (
        now - lastWebIndicatorToggle <
        WEB_LED_BLINK_INTERVAL_MS
    )
    {
        return;
    }

    lastWebIndicatorToggle = now;

    webIndicatorLedState =
        !webIndicatorLedState;

    if (!webIndicatorLedState)
    {
        Led::off();
        return;
    }

    switch (webIndicatorMode)
    {
        case WebIndicatorMode::WebServer:
            Led::setColor(
                0,
                0,
                255
            );
            break;

        case WebIndicatorMode::ConfigPortal:
            Led::setColor(
                255,
                0,
                0
            );
            break;

        case WebIndicatorMode::Off:
        default:
            Led::off();
            break;
    }
}

void startWebServerIndicator()
{
    const unsigned long now =
        millis();

    webIndicatorMode =
        WebIndicatorMode::WebServer;

    webIndicatorLedState = true;

    webIndicatorStartedAt = now;
    lastWebIndicatorToggle = now;

    Led::setColor(
        0,
        0,
        255
    );
}

void startConfigPortalIndicator()
{
    const unsigned long now =
        millis();

    webIndicatorMode =
        WebIndicatorMode::ConfigPortal;

    webIndicatorLedState = true;

    webIndicatorStartedAt = now;
    lastWebIndicatorToggle = now;

    Led::setColor(
        255,
        0,
        0
    );
}

void performMeasurement()
{
    Logger::info(
        "Starting tank measurement"
    );

    /*
     * Während der Messung dauerhaft blau.
     */
    Led::setColor(
        0,
        0,
        255
    );

    if (Sensor::measure())
    {
        Logger::info(
            "Tank measurement successful"
        );

        Led::setColor(
            0,
            255,
            0
        );

#if MQTT_ENABLED
        MqttManager::publishMeasurement();
#endif

        delay(500);
    }
    else
    {
        Logger::warning(
            "Tank measurement failed"
        );

        Led::setColor(
            255,
            0,
            0
        );

        delay(1000);
    }

    Led::off();

    /*
     * Falls ein manueller Webmodus läuft,
     * beginnt das Blinken im nächsten Loop
     * automatisch wieder.
     */
    webIndicatorLedState = false;
    lastWebIndicatorToggle = millis();
}

void setup()
{
    Logger::begin();
    SleepManager::begin();

    Logger::info(
        "================================="
    );

    Logger::info(FW_NAME);
    Logger::info(
        "Firmware: " FW_VERSION
    );

    Logger::info("Booting...");

    Logger::info(
        "================================="
    );

    Settings::begin();

    if (
        Settings::data.wifiSSID.isEmpty()
    )
    {
        Settings::data.wifiSSID =
            "Bartlingsend";

        Settings::data.wifiPassword =
            "gefunden";
    }

    if (
        Settings::data.mqttServer.isEmpty()
    )
    {
        Settings::data.mqttServer =
            "192.168.178.10";

        Settings::data.mqttPort =
            1883;

        Settings::data.mqttUser =
            "mqtt-user";

        Settings::data.mqttPassword =
            "mqtt-password";
    }

    Battery::begin();
    Led::begin();
    Button::begin();
    Sensor::begin();

    WifiManager::begin();
    WifiManager::connect();

#if MQTT_ENABLED
    MqttManager::begin();
#endif

    /*
     * Webserver darf nur nach einem normalen
     * Power-on/Reset automatisch starten.
     *
     * Nach einem Deep-Sleep-Wakeup bleibt er aus.
     */
    automaticWebServerAllowed =
        SleepManager::getWakeupReason() ==
        WakeupReason::PowerOn;

    if (automaticWebServerAllowed)
    {
        Logger::info(
            "Normal boot: automatic webserver enabled"
        );
    }
    else
    {
        Logger::info(
            "Deep-sleep wakeup: automatic webserver disabled"
        );
    }

    Logger::info(
        "Device Name      : " +
        String(
            Settings::data.deviceName
        )
    );

    Logger::info(
        "Tank Height      : " +
        String(
            Settings::data.tankHeight
        ) +
        " cm"
    );

    Logger::info(
        "Measure Interval : " +
        String(
            Settings::data.measureInterval
        ) +
        " s"
    );

#ifdef DEBUG_LED_TEST
    if (
        SleepManager::getWakeupReason() ==
        WakeupReason::PowerOn
    )
    {
        Logger::info(
            "Running LED self test..."
        );

        Led::test();

        Logger::info(
            "LED self test finished"
        );
    }
#endif

    Logger::info("System ready.");

    Logger::info(
        "================================="
    );
}

void loop()
{
    Button::loop();
    Battery::loop();
    Sensor::loop();
    WifiManager::loop();

    if (WebServerManager::isRunning())
    {
        WebServerManager::loop();
    }

#if MQTT_ENABLED
    MqttManager::loop();
#endif

    /*
     * Automatischer Webserverstart nur nach
     * normalem Power-on oder Reset.
     */
    if (
        automaticWebServerAllowed &&
        !automaticWebServerStarted &&
        WifiManager::isConnected() &&
        !WebServerManager::
            isConfigPortalActive()
    )
    {
        Logger::info(
            "Starting automatic webserver"
        );

        WebServerManager::begin();

        automaticWebServerStarted = true;

        /*
         * Beim automatischen Start blinkt
         * die LED nicht.
         */
        webIndicatorMode =
            WebIndicatorMode::Off;

        Led::off();
    }

    const ButtonEvent buttonEvent =
        Button::getEvent();

    switch (buttonEvent)
    {
        case ButtonEvent::ShortPress:
        {
            Logger::info(
                "Button: Short press"
            );

            performMeasurement();
            break;
        }

        case ButtonEvent::WebServerPress:
        {
            Logger::info(
                "Button: 5 second press"
            );

            Logger::info(
                "Starting normal webserver"
            );

            /*
             * WLAN wurde bereits im Setup gestartet.
             * Der Webserver läuft über das normale WLAN.
             */
            WebServerManager::begin();

            startWebServerIndicator();

            Logger::info(
                "Webserver active - LED blinking blue"
            );

            break;
        }

        case ButtonEvent::ConfigPortalPress:
        {
            Logger::info(
                "Button: 15 second press"
            );

            Logger::info(
                "Starting configuration portal"
            );

            /*
             * beginConfigPortal() stellt sicher,
             * dass auch der Webserver läuft.
             */
            WebServerManager::
                beginConfigPortal();

            startConfigPortalIndicator();

            Logger::info(
                "Configuration portal active - "
                "LED blinking red"
            );

            break;
        }

        case ButtonEvent::None:
        default:
            break;
    }

    updateWebIndicator();

    const unsigned long now =
        millis();

    if (
        now - lastBatteryLog >=
        5000UL
    )
    {
        lastBatteryLog = now;

        Logger::info(
            "Battery: " +
            String(
                Battery::getVoltage(),
                2
            ) +
            " V (" +
            String(
                Battery::getPercentage()
            ) +
            "%)"
        );
    }

    delay(5);
}