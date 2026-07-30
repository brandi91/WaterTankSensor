#pragma once

namespace CoreLogic
{
enum class RuntimeMode
{
    Running,
    PreparingSleep,
    Sleeping
};

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

bool shouldEnterDeepSleep(
    bool deepSleepEnabled,
    bool manualOverride,
    bool recoveryMode,
    RuntimeMode runtimeMode
);

bool canStartNormalWork(RuntimeMode runtimeMode);

bool isMeasurementDue(
    unsigned long now,
    unsigned long lastMeasurementAt,
    unsigned long intervalMs
);
}
