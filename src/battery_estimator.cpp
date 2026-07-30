#include "battery_estimator.h"

#include <Preferences.h>

#include "logger.h"
#include "settings.h"


namespace
{
    /*
     * Eigener Preferences-Bereich.
     *
     * Dadurch löscht der Reset nur die Batterielernwerte
     * und nicht die allgemeinen Geräteeinstellungen.
     */
    constexpr const char* PREFERENCES_NAMESPACE =
        "batteryEst";


    /*
     * Preferences-Schlüssel
     */
    constexpr const char* KEY_SAMPLE_COUNT =
        "samples";

    constexpr const char* KEY_LEARNING_SECONDS =
        "seconds";

    constexpr const char* KEY_START_VOLTAGE =
        "startV";

    constexpr const char* KEY_LAST_VOLTAGE =
        "lastV";


    /*
     * Unterhalb dieser Spannung betrachten wir
     * den Akku für die Schätzung als leer.
     */
    /*
     * Steigt die Spannung plötzlich um mindestens
     * 0,15 V, gehen wir von Laden oder Akkuwechsel aus.
     */
    constexpr float BATTERY_REPLACEMENT_RISE_VOLTAGE =
        0.15f;


    /*
     * Eine kleinere Spannungsänderung wäre zu stark
     * durch Messrauschen beeinflusst.
     */
    constexpr float MINIMUM_USEFUL_VOLTAGE_DROP =
        0.01f;


    /*
     * Testmodus:
     *
     * - mindestens 5 Messpunkte
     * - mindestens 2 Minuten Lernzeit
     */
    constexpr uint32_t TEST_MINIMUM_SAMPLES =
        5UL;

    constexpr uint32_t TEST_MINIMUM_SECONDS =
        2UL * 60UL;


    /*
     * Normalbetrieb:
     *
     * - mindestens 30 Messpunkte
     * - mindestens 24 Stunden Lernzeit
     */
    constexpr uint32_t NORMAL_MINIMUM_SAMPLES =
        30UL;

    constexpr uint32_t NORMAL_MINIMUM_SECONDS =
        24UL * 60UL * 60UL;


    Preferences preferences;
}


bool BatteryEstimator::initialized = false;
bool BatteryEstimator::ready = false;

uint32_t BatteryEstimator::sampleCount = 0;
uint32_t BatteryEstimator::learningSeconds = 0;

float BatteryEstimator::startVoltage = 0.0f;
float BatteryEstimator::lastVoltage = 0.0f;
float BatteryEstimator::estimatedDays = -1.0f;


void BatteryEstimator::begin()
{
    if (initialized)
    {
        return;
    }

    const bool opened =
        preferences.begin(
            PREFERENCES_NAMESPACE,
            false
        );

    if (!opened)
    {
        Logger::error(
            "Failed to open battery estimator preferences"
        );

        return;
    }

    initialized = true;

    load();

    Logger::info(
        "Battery Estimator initialized"
    );

    Logger::info(
        "Battery estimator samples: " +
        String(sampleCount)
    );

    Logger::info(
        "Battery estimator learning time: " +
        String(learningSeconds) +
        " seconds"
    );
}


void BatteryEstimator::load()
{
    sampleCount =
        preferences.getULong(
            KEY_SAMPLE_COUNT,
            0UL
        );

    learningSeconds =
        preferences.getULong(
            KEY_LEARNING_SECONDS,
            0UL
        );

    startVoltage =
        preferences.getFloat(
            KEY_START_VOLTAGE,
            0.0f
        );

    lastVoltage =
        preferences.getFloat(
            KEY_LAST_VOLTAGE,
            0.0f
        );

    estimatedDays = -1.0f;
    ready = false;

    if (
        sampleCount > 1 &&
        lastVoltage > 0.0f
    )
    {
        updateEstimate(
            lastVoltage
        );
    }
}


void BatteryEstimator::save()
{
    if (!initialized)
    {
        return;
    }

    preferences.putULong(
        KEY_SAMPLE_COUNT,
        sampleCount
    );

    preferences.putULong(
        KEY_LEARNING_SECONDS,
        learningSeconds
    );

    preferences.putFloat(
        KEY_START_VOLTAGE,
        startVoltage
    );

    preferences.putFloat(
        KEY_LAST_VOLTAGE,
        lastVoltage
    );
}


void BatteryEstimator::addSample(
    float voltage,
    uint32_t elapsedSeconds
)
{
    if (!initialized)
    {
        begin();
    }

    /*
     * Ungültige Werte nicht übernehmen.
     */
    if (
        !isfinite(voltage) ||
        voltage <= 0.0f
    )
    {
        Logger::warning(
            "Battery estimator rejected invalid voltage: " +
            String(voltage, 3) +
            " V"
        );

        return;
    }

    /*
     * Erster Messpunkt beginnt eine neue Lernphase.
     */
    if (
        sampleCount == 0 ||
        startVoltage <= 0.0f
    )
    {
        sampleCount = 1;
        learningSeconds = 0;

        startVoltage = voltage;
        lastVoltage = voltage;

        estimatedDays = -1.0f;
        ready = false;

        save();

        Logger::info(
            "Battery estimator learning started at " +
            String(voltage, 3) +
            " V"
        );

        return;
    }

    /*
     * Automatischer Reset nach Laden oder Akkuwechsel.
     */
    if (
        voltage >=
        lastVoltage +
        BATTERY_REPLACEMENT_RISE_VOLTAGE
    )
    {
        Logger::warning(
            "Battery voltage increased significantly"
        );

        Logger::warning(
            "Battery replacement or charging detected"
        );

        reset();

        /*
         * Aktuelle Spannung direkt als ersten
         * Messpunkt der neuen Lernphase übernehmen.
         */
        sampleCount = 1;
        startVoltage = voltage;
        lastVoltage = voltage;

        save();

        return;
    }

    sampleCount++;

    learningSeconds +=
        elapsedSeconds;

    lastVoltage =
        voltage;

    updateEstimate(
        voltage
    );

    save();

    Logger::info(
        "Battery estimator sample " +
        String(sampleCount) +
        ": " +
        String(voltage, 3) +
        " V"
    );

    Logger::info(
        "Battery estimate: " +
        getDisplayText()
    );
}


void BatteryEstimator::updateEstimate(
    float currentVoltage
)
{
    estimatedDays = -1.0f;
    ready = false;

    if (
        sampleCount < 2 ||
        learningSeconds == 0
    )
    {
        return;
    }

    const float voltageDrop =
        startVoltage -
        currentVoltage;

    /*
     * Noch kein ausreichend großer Spannungsabfall.
     */
    if (
        voltageDrop <
        MINIMUM_USEFUL_VOLTAGE_DROP
    )
    {
        return;
    }

    const float learningDays =
        static_cast<float>(
            learningSeconds
        ) /
        86400.0f;

    if (learningDays <= 0.0f)
    {
        return;
    }

    const float voltageDropPerDay =
        voltageDrop /
        learningDays;

    if (voltageDropPerDay <= 0.0f)
    {
        return;
    }

    const float remainingVoltage =
        currentVoltage -
        Settings::data.batteryEmptyVoltage;

    if (remainingVoltage <= 0.0f)
    {
        estimatedDays = 0.0f;
    }
    else
    {
        estimatedDays =
            remainingVoltage /
            voltageDropPerDay;
    }

    const uint32_t requiredSamples =
        Settings::data.batteryEstimateTestMode
            ? TEST_MINIMUM_SAMPLES
            : NORMAL_MINIMUM_SAMPLES;

    const uint32_t requiredSeconds =
        Settings::data.batteryEstimateTestMode
            ? TEST_MINIMUM_SECONDS
            : NORMAL_MINIMUM_SECONDS;

    ready =
        sampleCount >= requiredSamples &&
        learningSeconds >= requiredSeconds;
}


void BatteryEstimator::reset()
{
    if (!initialized)
    {
        begin();
    }

    preferences.clear();

    sampleCount = 0;
    learningSeconds = 0;

    startVoltage = 0.0f;
    lastVoltage = 0.0f;
    estimatedDays = -1.0f;

    ready = false;

    Logger::warning(
        "Battery estimate reset"
    );
}


bool BatteryEstimator::isReady()
{
    return ready;
}


bool BatteryEstimator::isLearning()
{
    return !ready;
}


uint32_t BatteryEstimator::getSampleCount()
{
    return sampleCount;
}


uint32_t BatteryEstimator::getLearningSeconds()
{
    return learningSeconds;
}


float BatteryEstimator::getEstimatedDays()
{
    return estimatedDays;
}


String BatteryEstimator::getDisplayText()
{
    /*
     * Noch kein berechenbarer Spannungsabfall.
     */
    if (estimatedDays < 0.0f)
    {
        return "Learning...";
    }

    String estimate =
        "~" +
        String(estimatedDays, 1) +
        " days";

    /*
     * Es gibt bereits eine grobe Berechnung,
     * aber noch nicht genug Messpunkte oder Lernzeit.
     */
    if (!ready)
    {
        return "Learning... " +
               estimate;
    }

    return estimate;
}
