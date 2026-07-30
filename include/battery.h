#pragma once

class Battery
{
public:
    static void begin();
    static void loop();

    static float getVoltage();
    static int getPercentage();

    static bool isLow();
    static bool isCritical();
    static bool isCharging();
    static const char* getPowerSourceText();

private:
    static float readVoltage();
};
