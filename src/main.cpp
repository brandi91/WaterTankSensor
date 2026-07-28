#include <Arduino.h>

#include "config.h"
#include "version.h"

#include "logger.h"
#include "settings.h"
#include "led.h"
#include "battery.h"
#include "button.h"

unsigned long lastBatteryLog = 0;

void setup()
{
    Logger::begin();

    Logger::info("=================================");
    Logger::info(FW_NAME);
    Logger::info("Firmware: " FW_VERSION);
    Logger::info("Booting...");
    Logger::info("=================================");

    Settings::begin();
    Battery::begin();
    Led::begin();
    Button::begin();

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

    switch (Button::getEvent())
    {
        case ButtonEvent::ShortPress:
            Logger::info("Button: Short press");

            // Grün als Bestätigung
            Led::setColor(0, 255, 0);
            delay(300);
            Led::off();

            break;

        case ButtonEvent::LongPress:
            Logger::info("Button: Long press");

            // Blau als Bestätigung
            Led::setColor(0, 0, 255);
            delay(1000);
            Led::off();

            break;

        case ButtonEvent::None:
        default:
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