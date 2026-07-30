#pragma once

#include <Arduino.h>
#include <time.h>

class TimeManager
{
public:
    static void begin();
    static bool syncFromNtp();
    static bool hasValidTime();
    static time_t now();
    static String getTimeSource();
    static String getStorageTimeSource();
    static time_t getLastSuccessfulSync();
    static String formatCurrentTime();
    static void prepareForSleep(uint32_t sleepSeconds);

private:
    static bool initialized;
    static bool valid;
    static time_t baseTimestamp;
    static unsigned long baseMillis;
    static time_t lastSuccessfulSync;
    static String timeSource;
};
