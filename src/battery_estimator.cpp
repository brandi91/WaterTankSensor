#include "battery_estimator.h"
#include "sleep_manager.h"

#include <Preferences.h>

#include "logger.h"
#include "settings.h"


namespace
{
    /*
     * Dedicated Preferences namespace.
     *
     * Resetting this namespace leaves general device settings untouched.
     */
    constexpr const char* PREFERENCES_NAMESPACE =
        "batteryEst";


    /*
     * Preferences keys
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
     * A sudden rise of at least 0.15 V indicates charging or battery
     * replacement.
     */
    constexpr float BATTERY_REPLACEMENT_RISE_VOLTAGE =
        0.15f;


    /*
     * A smaller threshold would be too sensitive to measurement noise.
     */
    constexpr float MINIMUM_USEFUL_VOLTAGE_DROP =
        0.01f;


    /*
     * Test mode:
     *
     * - at least 5 samples
     * - at least 2 minutes of learning time
     */
    constexpr uint32_t TEST_MINIMUM_SAMPLES =
        5UL;

    constexpr uint32_t TEST_MINIMUM_SECONDS =
        2UL * 60UL;


    /*
     * Normalbetrieb:
     *
     * - at least 30 samples
     * - at least 24 hours of learning time
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
        preferences.isKey(KEY_SAMPLE_COUNT)
            ? preferences.getULong(
                KEY_SAMPLE_COUNT,
                0UL
              )
            : 0UL;

    learningSeconds =
        preferences.isKey(KEY_LEARNING_SECONDS)
            ? preferences.getULong(
                KEY_LEARNING_SECONDS,
                0UL
              )
            : 0UL;

    startVoltage =
        preferences.isKey(KEY_START_VOLTAGE)
            ? preferences.getFloat(
                KEY_START_VOLTAGE,
                0.0f
              )
            : 0.0f;

    lastVoltage =
        preferences.isKey(KEY_LAST_VOLTAGE)
            ? preferences.getFloat(
                KEY_LAST_VOLTAGE,
                0.0f
              )
            : 0.0f;

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
    if (!SleepManager::canStartNormalWork())
    {
        return;
    }

    if (!initialized)
    {
        begin();
    }

    /*
     * Reject invalid values.
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
     * The first sample starts a new learning phase.
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
     * Reset automatically after charging or battery replacement.
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
         * Keep the current voltage as the first sample of the new phase.
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
     * The voltage drop is not large enough yet.
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
     * No measurable voltage drop is available yet.
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
     * A rough estimate exists, but more samples or learning time are required.
     */
    if (!ready)
    {
        return "Learning... " +
               estimate;
    }

    return estimate;
}
