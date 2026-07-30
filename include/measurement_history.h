#pragma once

#include <Arduino.h>

class MeasurementHistory
{
public:
    static void begin();
    static bool addCurrentMeasurement();
    static String toJson();
    static void clear();
    static size_t count();

private:
    static bool initialized;
    static bool filesystemReady;
};
