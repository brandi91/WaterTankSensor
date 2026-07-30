#pragma once

#include <Arduino.h>

enum class ButtonEvent
{
    None,
    ShortPress,
    WebServerPress,
    ConfigPortalPress,
    FactoryResetArmed,
    FactoryResetConfirmed
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

    static bool webServerPressSent;
    static bool configPortalPressSent;
    static bool factoryResetArmed;

    static unsigned long lastChangeTime;
    static unsigned long pressedSince;

    static ButtonEvent event;
};
