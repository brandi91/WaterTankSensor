#include <Arduino.h>
#include "led.h"
#include "config.h"
#include "logger.h"
#include "settings.h"

void setup()
{
    Logger::begin();
    Settings::begin();
    Logger::info("Settings loaded");
    Logger::info("--------------------------------");
    Logger::info(FW_NAME);
    Logger::info("Firmware: " FW_VERSION);
    Logger::info("Boot successful");
    Logger::info("--------------------------------");

    Logger::info(Settings::data.deviceName);

Logger::info(String(Settings::data.tankHeight));

Logger::info(String(Settings::data.measureInterval));

Led::begin();

Logger::info("Starting LED Test");

Led::test();

Logger::info("LED Test finished");
}

void loop()
{
    Logger::loop();

    delay(10);
}