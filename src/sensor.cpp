#include "sensor.h"

#include "config.h"
#include "settings.h"
#include "logger.h"

float Sensor::distanceCm = 0.0f;
float Sensor::waterLevelCm = 0.0f;
int Sensor::percentage = 0;
bool Sensor::valid = false;

void Sensor::begin()
{
    Logger::info("Sensor Manager initialized");
}

void Sensor::loop()
{
    // Später können hier automatische Messintervalle verarbeitet werden.
}

bool Sensor::measure()
{
    const float measuredDistance = readDistance();

    if (measuredDistance < 0.0f)
    {
        valid = false;
        Logger::error("Sensor measurement failed");
        return false;
    }

    distanceCm = measuredDistance;
    waterLevelCm = calculateWaterLevel(distanceCm);
    percentage = calculatePercentage(waterLevelCm);
    valid = true;

    Logger::info(
        "Sensor: distance=" +
        String(distanceCm, 1) +
        " cm, level=" +
        String(waterLevelCm, 1) +
        " cm, fill=" +
        String(percentage) +
        "%"
    );

    return true;
}

float Sensor::readDistance()
{
    // Simulation, bis der Ultraschallsensor angeschlossen ist.
    return 42.0f;
}

float Sensor::calculateWaterLevel(float measuredDistanceCm)
{
    const float tankHeight = Settings::data.tankHeight;

    float level = tankHeight - measuredDistanceCm;

    if (level < 0.0f)
    {
        level = 0.0f;
    }

    if (level > tankHeight)
    {
        level = tankHeight;
    }

    return level;
}

int Sensor::calculatePercentage(float measuredWaterLevelCm)
{
    const float tankHeight = Settings::data.tankHeight;

    if (tankHeight <= 0.0f)
    {
        return 0;
    }

    int result = static_cast<int>(
        (measuredWaterLevelCm / tankHeight) * 100.0f
    );

    if (result < 0)
    {
        result = 0;
    }

    if (result > 100)
    {
        result = 100;
    }

    return result;
}

float Sensor::getDistanceCm()
{
    return distanceCm;
}

float Sensor::getWaterLevelCm()
{
    return waterLevelCm;
}

int Sensor::getPercentage()
{
    return percentage;
}

bool Sensor::isValid()
{
    return valid;
}