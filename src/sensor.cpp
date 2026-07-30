#include "sensor.h"

#include "config.h"
#include "logger.h"
#include "settings.h"


float Sensor::distanceCm = 0.0f;
float Sensor::waterLevelCm = 0.0f;

int Sensor::percentage = 0;

bool Sensor::valid = false;
bool Sensor::measurementAttempted = false;


void Sensor::begin()
{
    Logger::info(
        "Sensor Manager initialized"
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

    waterLevelCm =
        calculateWaterLevel(
            distanceCm
        );

    percentage =
        calculatePercentage(
            waterLevelCm
        );

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


float Sensor::calculateWaterLevel(
    float measuredDistanceCm
)
{
    const float tankHeight =
        Settings::data.tankHeight;

    const float sensorClearance =
        Settings::data.sensorClearance;


    if (tankHeight <= 0.0f)
    {
        Logger::error(
            "Invalid tank height"
        );

        return 0.0f;
    }


    /*
     * Gemessen wird vom Sensor bis zur
     * Wasseroberfläche.
     *
     * Gesamtabstand Sensor → Tankboden:
     *
     * Tankhöhe + Sensor Clearance
     *
     * Daraus folgt:
     *
     * Wasserhöhe =
     * Tankhöhe
     * + Sensor Clearance
     * - gemessener Abstand
     */
    float level =
        tankHeight +
        sensorClearance -
        measuredDistanceCm;


    /*
     * Ergebnis auf den physikalisch sinnvollen
     * Tankbereich begrenzen.
     */
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


int Sensor::calculatePercentage(
    float measuredWaterLevelCm
)
{
    const float tankHeight =
        Settings::data.tankHeight;

    if (tankHeight <= 0.0f)
    {
        return 0;
    }


    int result =
        static_cast<int>(
            (
                measuredWaterLevelCm /
                tankHeight
            ) *
            100.0f
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

bool Sensor::hasMeasurementAttempted()
{
    return measurementAttempted;
}
