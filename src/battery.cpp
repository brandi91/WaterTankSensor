#include "battery.h"
#include "logger.h"

static float voltage = 4.15f;

void Battery::begin()
{
    Logger::info("Battery Manager initialized");
}

void Battery::loop()
{
    voltage = readVoltage();
}

float Battery::readVoltage()
{
    // Simulation bis die Hardware da ist
    return voltage;
}

float Battery::getVoltage()
{
    return voltage;
}

int Battery::getPercentage()
{
    float v = voltage;

    if (v >= 4.20) return 100;
    if (v <= 3.20) return 0;

    return (int)(((v - 3.20f) / (4.20f - 3.20f)) * 100.0f);
}

bool Battery::isLow()
{
    return voltage < 3.50;
}

bool Battery::isCritical()
{
    return voltage < 3.30;
}

bool Battery::isCharging()
{
    return false;
}