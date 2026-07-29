#pragma once

#include <Arduino.h>
#include <esp_sleep.h>

enum class WakeupReason
{
    Unknown,
    PowerOn,
    Timer,
    Button
};

class SleepManager
{
public:
    static void begin();

    static void sleepForSeconds(
        uint32_t seconds
    );

    static void sleepNow();

    static WakeupReason getWakeupReason();

    static const char* getWakeupReasonText();

private:
    static WakeupReason wakeupReason;

    static void detectWakeupReason();

    static void prepareForSleep();
};