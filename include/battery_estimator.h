#pragma once

#include <Arduino.h>

/*
 * ============================================================
 * Battery Estimator
 * ============================================================
 *
 * Learns the voltage decline rate from multiple battery samples.
 *
 * Uses that rate to estimate the remaining runtime.
 */
class BatteryEstimator
{
public:
    /*
     * Load persisted learning data.
     */
    static void begin();

    /*
     * Add a battery-voltage sample.
     *
     * elapsedSeconds:
     * Time since the previous persisted sample.
     */
    static void addSample(
        float voltage,
        uint32_t elapsedSeconds
    );

    /*
     * Clear all learned battery data.
     *
     * Wi-Fi, MQTT, and tank settings remain unchanged.
     */
    static void reset();

    /*
     * True when enough samples and learning time are available.
     */
    static bool isReady();

    /*
     * True while the estimator is still learning.
     */
    static bool isLearning();

    /*
     * Current number of stored samples.
     */
    static uint32_t getSampleCount();

    /*
     * Total learning time in seconds.
     */
    static uint32_t getLearningSeconds();

    /*
     * Estimated remaining runtime in days.
     *
     * Returns -1 when no meaningful estimate is available yet.
     */
    static float getEstimatedDays();

    /*
     * Ready-to-display text for the web interface:
     *
     * Learning...
     * Learning... ~8.2 days
     * ~8.2 days
     */
    static String getDisplayText();

private:
    static void load();
    static void save();
    static void updateEstimate(
        float currentVoltage
    );

    static bool initialized;
    static bool ready;

    static uint32_t sampleCount;
    static uint32_t learningSeconds;

    static float startVoltage;
    static float lastVoltage;
    static float estimatedDays;
};
