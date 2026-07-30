#pragma once

#include <Arduino.h>
#include <esp_sleep.h>
#include "core_logic.h"

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
    static bool isPreparingForSleep();
    static bool canStartNormalWork();
    static CoreLogic::RuntimeMode getRuntimeMode();

    static WakeupReason getWakeupReason();

    static const char* getWakeupReasonText();

private:
    static WakeupReason wakeupReason;
    static CoreLogic::RuntimeMode runtimeMode;

    static void detectWakeupReason();

    static void prepareForSleep();
};
