#pragma once

#include <Arduino.h>

enum class ButtonEvent
{
    None,
    ShortPress,
    LongPress
};

class Button
{
public:
    static void begin();
    static void loop();
    static ButtonEvent getEvent();

private:
    static bool lastStableState;
    static bool lastReading;
    static bool longPressSent;

    static unsigned long lastChangeTime;
    static unsigned long pressedSince;

    static ButtonEvent event;
};