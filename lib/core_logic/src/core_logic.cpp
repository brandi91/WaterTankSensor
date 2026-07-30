#include "core_logic.h"

#include <cmath>

namespace CoreLogic
{
TankReading calculateTankReading(
    const float distanceCm,
    const float tankHeightCm,
    const float sensorClearanceCm
)
{
    TankReading reading;
    if (
        !std::isfinite(distanceCm) ||
        !std::isfinite(tankHeightCm) ||
        !std::isfinite(sensorClearanceCm) ||
        distanceCm < 0.0f ||
        tankHeightCm <= 0.0f ||
        sensorClearanceCm < 0.0f
    )
    {
        return reading;
    }

    float waterLevelCm =
        tankHeightCm + sensorClearanceCm - distanceCm;
    if (waterLevelCm < 0.0f)
    {
        waterLevelCm = 0.0f;
    }
    else if (waterLevelCm > tankHeightCm)
    {
        waterLevelCm = tankHeightCm;
    }

    int fillPercent = static_cast<int>(
        waterLevelCm / tankHeightCm * 100.0f
    );
    if (fillPercent < 0)
    {
        fillPercent = 0;
    }
    else if (fillPercent > 100)
    {
        fillPercent = 100;
    }

    reading.valid = true;
    reading.waterLevelCm = waterLevelCm;
    reading.fillPercent = fillPercent;
    return reading;
}

int calculateBatteryPercentage(
    const float voltage,
    const float emptyVoltage,
    const float fullVoltage
)
{
    if (
        !std::isfinite(voltage) ||
        !std::isfinite(emptyVoltage) ||
        !std::isfinite(fullVoltage) ||
        voltage <= 0.1f ||
        emptyVoltage <= 0.0f ||
        fullVoltage <= emptyVoltage
    )
    {
        return -1;
    }
    if (voltage <= emptyVoltage)
    {
        return 0;
    }
    if (voltage >= fullVoltage)
    {
        return 100;
    }

    return static_cast<int>(
        ((voltage - emptyVoltage) /
         (fullVoltage - emptyVoltage) *
         100.0f) +
        0.5f
    );
}

bool shouldEnterDeepSleep(
    const bool deepSleepEnabled,
    const bool manualOverride,
    const bool recoveryMode,
    const RuntimeMode runtimeMode
)
{
    if (runtimeMode != RuntimeMode::Running)
    {
        return false;
    }
    return manualOverride || (deepSleepEnabled && !recoveryMode);
}

bool canStartNormalWork(const RuntimeMode runtimeMode)
{
    return runtimeMode == RuntimeMode::Running;
}

bool isMeasurementDue(
    const unsigned long now,
    const unsigned long lastMeasurementAt,
    const unsigned long intervalMs
)
{
    return
        intervalMs > 0 &&
        now - lastMeasurementAt >= intervalMs;
}
}
