#include "battery.h"

#include <Arduino.h>

#include "config.h"
#include "logger.h"
#include "settings.h"

/*
 * Spannungsteiler:
 *
 * Akku Plus
 *    |
 *   200 kOhm
 *    |
 *    +---- GPIO34
 *    |
 *   200 kOhm
 *    |
 *   GND
 *
 * Da beide Widerstände gleich groß sind:
 *
 * Akkuspannung = ADC-Spannung * 2
 */

namespace
{
    constexpr float BATTERY_DIVIDER_FACTOR = 2.0f;

    constexpr uint8_t BATTERY_SAMPLE_COUNT = 32;

    constexpr unsigned long BATTERY_READ_INTERVAL_MS =
        1000UL;

    float voltage = 0.0f;

    unsigned long lastReadAt = 0;
}

void Battery::begin()
{
    pinMode(
        Settings::activePins().batteryAdcPin,
        INPUT
    );

    /*
     * 11 dB erlaubt einen größeren ADC-Messbereich.
     * Bei maximal 4,2 V Akku liegen durch den
     * Spannungsteiler ungefähr 2,1 V am GPIO an.
     */
    analogSetPinAttenuation(
        Settings::activePins().batteryAdcPin,
        ADC_11db
    );

    voltage = readVoltage();
    lastReadAt = millis();

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
    const unsigned long now = millis();

    if (
        now - lastReadAt <
        BATTERY_READ_INTERVAL_MS
    )
    {
        return;
    }

    lastReadAt = now;
    voltage = readVoltage();
}

float Battery::readVoltage()
{
    uint32_t millivoltSum = 0;

    /*
     * Mehrere Messungen mitteln, damit der Wert
     * weniger springt.
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
    /*
     * Einfache lineare Anzeige für den ersten Test.
     * Später können wir eine realistischere
     * Li-Ion-Kennlinie verwenden.
     */
    if (!isValid())
    {
        return -1;
    }

    const float emptyVoltage =
        Settings::data.batteryEmptyVoltage;
    const float fullVoltage =
        Settings::data.batteryFullVoltage;

    if (
        voltage >= fullVoltage
    )
    {
        return 100;
    }

    if (
        voltage <= emptyVoltage
    )
    {
        return 0;
    }

    const float percentage =
        (
            voltage -
            emptyVoltage
        ) /
        (
            fullVoltage -
            emptyVoltage
        ) *
        100.0f;

    return static_cast<int>(
        percentage + 0.5f
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
    /*
     * Mit der momentanen Schaltung können wir
     * nur die Spannung messen, nicht zuverlässig
     * erkennen, ob geladen wird.
     */
    return false;
}

const char* Battery::getPowerSourceText()
{
    /*
     * The current hardware has no VBUS sense input,
     * charger status signal or other reliable way to
     * distinguish USB power from battery power.
     */
    return "Unknown";
}
