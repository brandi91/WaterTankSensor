#pragma once

#include <Arduino.h>

class Sensor
{
public:
    static void begin();
    static void loop();

    static bool measure();

    static float getDistanceCm();
    static float getWaterLevelCm();
    static int getPercentage();

    static bool isValid();
    static bool hasMeasurementAttempted();
    static bool isSimulated();

private:
    static float readDistance();
    static float calculateWaterLevel(float distanceCm);
    static int calculatePercentage(float waterLevelCm);

    static float distanceCm;
    static float waterLevelCm;
    static int percentage;
    static bool valid;
    static bool measurementAttempted;
};
