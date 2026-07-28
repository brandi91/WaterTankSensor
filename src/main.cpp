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

unsigned long lastBatteryLog = 0;

void setup()
{
Logger::begin();
SleepManager::begin();


    Logger::info("=================================");
    Logger::info(FW_NAME);
    Logger::info("Firmware: " FW_VERSION);
    Logger::info("Booting...");
    Logger::info("=================================");



Settings::begin();
    if (Settings::data.wifiSSID.isEmpty())
    {
        Settings::data.wifiSSID = "Bartlingsend";
        Settings::data.wifiPassword = "gefunden";
    }




WifiManager::begin();
WifiManager::connect();

Battery::begin();
Led::begin();
Button::begin();
Sensor::begin();

    Logger::info(
        "Device Name      : " +
        String(Settings::data.deviceName)
    );

    Logger::info(
        "Tank Height      : " +
        String(Settings::data.tankHeight) +
        " cm"
    );

    Logger::info(
        "Measure Interval : " +
        String(Settings::data.measureInterval) +
        " s"
    );

#ifdef DEBUG_LED_TEST
if (SleepManager::getWakeupReason() == WakeupReason::PowerOn)
{
    Logger::info("Running LED self test...");
    Led::test();
    Logger::info("LED self test finished");
}
#endif

    Logger::info("System ready.");
    Logger::info("=================================");
}

void loop()
{
    Button::loop();
    Battery::loop();
    Sensor::loop();
    WifiManager::loop();
    WebServerManager::loop();

    static bool normalWebserverStarted = false;

    if (
        WifiManager::isConnected() &&
        !normalWebserverStarted &&
        !WebServerManager::isConfigPortalActive()
    )
    {
        WebServerManager::begin();
        normalWebserverStarted = true;
    }

    switch (Button::getEvent())
    {
        case ButtonEvent::ShortPress:
            Logger::info("Button: Short press");
            Logger::info("Starting tank measurement");

            Led::setColor(0, 0, 255);

            if (Sensor::measure())
            {
                Led::setColor(0, 255, 0);
                delay(500);
            }
            else
            {
                Led::setColor(255, 0, 0);
                delay(1000);
            }

            Led::off();
            break;

        case ButtonEvent::LongPress:
            Logger::info("Button: Long press");
            Logger::info("Starting configuration portal");

            Led::setColor(0, 0, 255);

            WebServerManager::beginConfigPortal();

            break;

        case ButtonEvent::None:
        default:
            break;
    }

    delay(5);
}