#pragma once

#include <Arduino.h>

/*
 * ============================================================
 * Battery Estimator
 * ============================================================
 *
 * Lernt aus mehreren Batteriespannungs-Messungen,
 * wie schnell die Spannung abfällt.
 *
 * Daraus wird eine grobe verbleibende Laufzeit geschätzt.
 */
class BatteryEstimator
{
public:
    /*
     * Gespeicherte Lernwerte laden.
     */
    static void begin();

    /*
     * Neue Batteriespannung hinzufügen.
     *
     * elapsedSeconds:
     * Zeit seit dem letzten gespeicherten Messpunkt.
     */
    static void addSample(
        float voltage,
        uint32_t elapsedSeconds
    );

    /*
     * Alle erlernten Batteriewerte löschen.
     *
     * WLAN-, MQTT- und Tankeinstellungen
     * bleiben dabei erhalten.
     */
    static void reset();

    /*
     * true, sobald genug Messpunkte und genügend
     * Lernzeit vorhanden sind.
     */
    static bool isReady();

    /*
     * true, solange noch gelernt wird.
     */
    static bool isLearning();

    /*
     * Aktuelle Anzahl gespeicherter Messpunkte.
     */
    static uint32_t getSampleCount();

    /*
     * Gesamte Lernzeit in Sekunden.
     */
    static uint32_t getLearningSeconds();

    /*
     * Geschätzte Restlaufzeit in Tagen.
     *
     * Gibt -1 zurück, wenn noch keine sinnvolle
     * Schätzung möglich ist.
     */
    static float getEstimatedDays();

    /*
     * Fertiger Text für das Webinterface:
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