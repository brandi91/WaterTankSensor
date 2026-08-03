#include "battery.h"

#include <Arduino.h>

#include "config.h"
#include "logger.h"
#include "settings.h"
#include "core_logic.h"

/*
 * Voltage divider:
 *
 * Battery positive
 *    |
 *   200 kOhm
 *    |
 *    +---- GPIO35
 *    |
 *   200 kOhm
 *    |
 *   GND
 *
 * Both resistors have the same value:
 *
 * Battery voltage = ADC voltage * 2
 */

namespace
{
    constexpr float BATTERY_DIVIDER_FACTOR = 2.0f;

    constexpr uint8_t BATTERY_SAMPLE_COUNT = 32;

    float voltage = 0.0f;
}

void Battery::begin()
{
    // GPIO34 has no internal pull-up/down. The charger module must drive
    // this input LOW when absent and HIGH (3.3 V maximum) when connected.
    pinMode(CHARGER_DETECT_PIN, INPUT);

    pinMode(
        Settings::activePins().batteryAdcPin,
        INPUT
    );

    /*
     * 11 dB attenuation enables a wider ADC input range. A 4.2 V battery
     * produces approximately 2.1 V at the GPIO through the divider.
     */
    analogSetPinAttenuation(
        Settings::activePins().batteryAdcPin,
        ADC_11db
    );

    voltage = readVoltage();

    Logger::info(
        "Battery Manager initialized on GPIO " +
        String(Settings::activePins().batteryAdcPin)
    );

    Logger::info(
        "Initial battery voltage: " +
        String(voltage, 3) +
        " V"
    );
}

void Battery::loop()
{
    voltage = readVoltage();
}

float Battery::readVoltage()
{
    uint32_t millivoltSum = 0;

    /*
     * Average multiple samples to reduce measurement noise.
     */
    for (
        uint8_t sample = 0;
        sample < BATTERY_SAMPLE_COUNT;
        sample++
    )
    {
        millivoltSum +=
            analogReadMilliVolts(
                Settings::activePins().batteryAdcPin
            );

        delay(2);
    }

    const float adcMillivolts =
        static_cast<float>(millivoltSum) /
        static_cast<float>(
            BATTERY_SAMPLE_COUNT
        );

    const float adcVoltage =
        adcMillivolts / 1000.0f;

    const float batteryVoltage =
        adcVoltage *
        BATTERY_DIVIDER_FACTOR;

    return batteryVoltage;
}

float Battery::getVoltage()
{
    return voltage;
}

int Battery::getPercentage()
{
    return CoreLogic::calculateBatteryPercentage(
        voltage,
        Settings::data.batteryEmptyVoltage,
        Settings::data.batteryFullVoltage
    );
}

bool Battery::isValid()
{
    return
        isfinite(voltage) &&
        voltage > 0.1f &&
        Settings::data.batteryEmptyVoltage > 0.0f &&
        Settings::data.batteryFullVoltage >
            Settings::data.batteryEmptyVoltage;
}

bool Battery::isLow()
{
    const int percentage = getPercentage();
    return percentage >= 0 && percentage < 20;
}

bool Battery::isCritical()
{
    const int percentage = getPercentage();
    return percentage >= 0 && percentage < 5;
}

bool Battery::isCharging()
{
    return digitalRead(CHARGER_DETECT_PIN) == HIGH;
}

const char* Battery::getPowerSourceText()
{
    return isCharging()
        ? "Connected"
        : "Disconnected";
}
