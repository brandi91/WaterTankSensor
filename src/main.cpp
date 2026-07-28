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

unsigned long lastBatteryLog = 0;

void setup()
{

    Logger::info("=================================");
    Logger::info(FW_NAME);
    Logger::info("Firmware: " FW_VERSION);
    Logger::info("Booting...");
    Logger::info("=================================");

Logger::begin();
SleepManager::begin();

Settings::begin();
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
    Logger::info("Running LED self test...");
    Led::test();
    Logger::info("LED self test finished");
#endif

    Logger::info("System ready.");
    Logger::info("=================================");
}

void loop()
{
    Button::loop();
    Battery::loop();
    Sensor::loop();

    switch (Button::getEvent())
    {
case ButtonEvent::LongPress:
    Logger::info("Button: Long press");
    Logger::info("Starting deep sleep test");

    Led::setColor(0, 0, 255);
    delay(500);
    Led::off();

    SleepManager::sleepNow();
    break;
    }

    

    // Batterie alle 5 Sekunden ausgeben
    if (millis() - lastBatteryLog >= 5000)
    {
        lastBatteryLog = millis();

        Logger::info(
            "Battery: " +
            String(Battery::getVoltage(), 2) +
            " V (" +
            String(Battery::getPercentage()) +
            "%)"
        );
    }

    delay(5);
}