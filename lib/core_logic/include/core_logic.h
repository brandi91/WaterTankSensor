#pragma once

namespace CoreLogic
{
struct TankReading
{
    bool valid = false;
    float waterLevelCm = 0.0f;
    int fillPercent = 0;
};

TankReading calculateTankReading(
    float distanceCm,
    float tankHeightCm,
    float sensorClearanceCm
);

int calculateBatteryPercentage(
    float voltage,
    float emptyVoltage,
    float fullVoltage
);
}
