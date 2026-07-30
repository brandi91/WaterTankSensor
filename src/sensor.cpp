#include "sensor.h"

#include "config.h"
#include "logger.h"
#include "settings.h"
#include "core_logic.h"


float Sensor::distanceCm = 0.0f;
float Sensor::waterLevelCm = 0.0f;

int Sensor::percentage = 0;

bool Sensor::valid = false;
bool Sensor::measurementAttempted = false;


void Sensor::begin()
{
    pinMode(Settings::activePins().sensorTriggerPin, OUTPUT);
    digitalWrite(Settings::activePins().sensorTriggerPin, LOW);
    pinMode(Settings::activePins().sensorEchoPin, INPUT);

    Logger::info(
        "Sensor Manager initialized on trigger GPIO " +
        String(Settings::activePins().sensorTriggerPin) +
        " and echo GPIO " +
        String(Settings::activePins().sensorEchoPin)
    );
}


void Sensor::loop()
{
    /*
     * Später können hier automatische
     * Messintervalle verarbeitet werden.
     */
}


bool Sensor::measure()
{
    measurementAttempted = true;

    const float measuredDistance =
        readDistance();

    if (measuredDistance < 0.0f)
    {
        valid = false;

        Logger::error(
            "Sensor measurement failed"
        );

        return false;
    }

    distanceCm = measuredDistance;

    const CoreLogic::TankReading reading =
        CoreLogic::calculateTankReading(
            distanceCm,
            Settings::data.tankHeight,
            Settings::data.sensorClearance
        );
    if (!reading.valid)
    {
        valid = false;
        Logger::error("Invalid tank measurement configuration");
        return false;
    }

    waterLevelCm = reading.waterLevelCm;
    percentage = reading.fillPercent;
    valid = true;


    Logger::info(
        "Sensor: distance=" +
        String(distanceCm, 1) +
        " cm, clearance=" +
        String(
            Settings::data.sensorClearance,
            1
        ) +
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
    /*
     * Simulation, bis der echte Ultraschallsensor
     * angeschlossen ist.
     *
     * Beispiel:
     *
     * Tankhöhe:         100 cm
     * Sensor Clearance:  12 cm
     * Abstand:            54 cm
     *
     * Wasserhöhe:
     * 100 + 12 - 54 = 58 cm
     */
    return 54.0f;
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

bool Sensor::hasMeasurementAttempted()
{
    return measurementAttempted;
}

bool Sensor::isSimulated()
{
    return true;
}
