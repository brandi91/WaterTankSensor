#include "sensor.h"

#include "config.h"
#include "logger.h"
#include "settings.h"
#include "core_logic.h"
#include "sleep_manager.h"


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

    // Allow the JSN-SR04T controller to stabilize after power-up.
    delay(100);

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
     * Intentionally empty; main.cpp coordinates measurement scheduling.
     */
}


bool Sensor::measure()
{
    if (!SleepManager::canStartNormalWork())
    {
        return false;
    }

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
    // JSN-SR04T trigger pulse followed by one bounded echo measurement.
    constexpr unsigned long ECHO_TIMEOUT_US = 40000UL;
    constexpr float SOUND_SPEED_CM_PER_US = 0.0343f;
    constexpr float MAX_DISTANCE_CM = 600.0f;

    const uint8_t triggerPin = Settings::activePins().sensorTriggerPin;
    const uint8_t echoPin = Settings::activePins().sensorEchoPin;

    digitalWrite(triggerPin, LOW);
    delayMicroseconds(3);
    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(20);
    digitalWrite(triggerPin, LOW);

    const unsigned long echoDuration =
        pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);
    if (echoDuration == 0)
    {
        Logger::warning("Ultrasonic echo timeout");
        return -1.0f;
    }

    const float measuredDistance =
        static_cast<float>(echoDuration) *
        SOUND_SPEED_CM_PER_US /
        2.0f;
    if (
        !isfinite(measuredDistance) ||
        measuredDistance <= 0.0f ||
        measuredDistance > MAX_DISTANCE_CM
    )
    {
        Logger::warning(
            "Ultrasonic distance out of range: " +
            String(measuredDistance, 1) +
            " cm"
        );
        return -1.0f;
    }

    return measuredDistance;
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
    return false;
}
