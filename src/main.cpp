#include <Arduino.h>

#include "battery.h"
#include "battery_estimator.h"
#include "button.h"
#include "config.h"
#include "core_logic.h"
#include "factory_reset.h"
#include "led.h"
#include "logger.h"
#include "measurement_history.h"
#include "sensor.h"
#include "settings.h"
#include "sleep_manager.h"
#include "time_manager.h"
#include "version.h"
#include "web_server_manager.h"
#include "wifi_manager.h"

#if MQTT_ENABLED
#include "mqtt_manager.h"
#endif


/*
 * ============================================================
 * Webserver LED indicator
 * ============================================================
 */

enum class WebIndicatorMode
{
    Off,
    WebServer,
    ConfigPortal,
    FactoryResetArmed
};


WebIndicatorMode webIndicatorMode =
    WebIndicatorMode::Off;

bool webIndicatorLedState = false;

unsigned long lastWebIndicatorToggle = 0;
unsigned long webIndicatorStartedAt = 0;
unsigned long lastBatteryLog = 0;
bool timeSyncAttempted = false;
unsigned long lastAutomaticMeasurementAt = 0;
bool awakeServicesInitialized = false;
bool awakeSchedulingActive = false;
bool lastDeepSleepEnabled = true;


/*
 * ============================================================
 * Helper functions
 * ============================================================
 */

void updateWebIndicator()
{
    if (
        webIndicatorMode ==
        WebIndicatorMode::Off
    )
    {
        return;
    }

    const unsigned long now =
        millis();

    /*
     * LED blinkt nur 15 Sekunden.
     *
     * Webserver oder Config-AP laufen danach
     * trotzdem weiter.
     */
    if (
        webIndicatorMode !=
            WebIndicatorMode::FactoryResetArmed &&
        now - webIndicatorStartedAt >=
        WEB_LED_INDICATOR_DURATION_MS
    )
    {
        webIndicatorMode =
            WebIndicatorMode::Off;

        webIndicatorLedState = false;

        Led::off();

        Logger::info(
            "Web indicator LED finished"
        );

        return;
    }

    const unsigned long indicatorInterval =
        webIndicatorMode == WebIndicatorMode::FactoryResetArmed
            ? FACTORY_RESET_LED_INTERVAL_MS
            : WEB_LED_BLINK_INTERVAL_MS;

    if (now - lastWebIndicatorToggle < indicatorInterval)
    {
        return;
    }

    lastWebIndicatorToggle = now;

    webIndicatorLedState =
        !webIndicatorLedState;

    if (webIndicatorMode == WebIndicatorMode::FactoryResetArmed)
    {
        if (webIndicatorLedState)
        {
            Led::setColor(0, 0, 255);
        }
        else
        {
            Led::setColor(255, 0, 0);
        }
        return;
    }

    if (!webIndicatorLedState)
    {
        Led::off();
        return;
    }

    switch (webIndicatorMode)
    {
        case WebIndicatorMode::WebServer:
            Led::setColor(
                0,
                0,
                255
            );
            break;

        case WebIndicatorMode::ConfigPortal:
            Led::setColor(
                255,
                0,
                0
            );
            break;

        case WebIndicatorMode::Off:
        case WebIndicatorMode::FactoryResetArmed:
        default:
            Led::off();
            break;
    }
}


void startWebServerIndicator()
{
    const unsigned long now =
        millis();

    webIndicatorMode =
        WebIndicatorMode::WebServer;

    webIndicatorLedState = true;

    webIndicatorStartedAt = now;
    lastWebIndicatorToggle = now;

    Led::setColor(
        0,
        0,
        255
    );
}


void startConfigPortalIndicator()
{
    const unsigned long now =
        millis();

    webIndicatorMode =
        WebIndicatorMode::ConfigPortal;

    webIndicatorLedState = true;

    webIndicatorStartedAt = now;
    lastWebIndicatorToggle = now;

    Led::setColor(
        255,
        0,
        0
    );
}


/*
 * Wartet begrenzt auf eine WLAN-Verbindung.
 *
 * Der ESP32 bleibt bei einem WLAN-Fehler nicht
 * dauerhaft wach.
 */
bool waitForWifi(
    unsigned long timeoutMs
)
{
    const unsigned long startedAt =
        millis();

    while (
        !WifiManager::isConnected() &&
        millis() - startedAt < timeoutMs
    )
    {
        WifiManager::loop();

        delay(20);
    }

    if (WifiManager::isConnected())
    {
        Logger::info(
            "Wi-Fi ready for data transmission"
        );

        return true;
    }

    Logger::warning(
        "Wi-Fi unavailable, continuing without transmission"
    );

    return false;
}


/*
 * Lokale Tankmessung durchführen.
 *
 * estimatorElapsedSeconds:
 * Zeit seit dem vorherigen automatischen Messpunkt.
 */
bool performMeasurement(
    uint32_t estimatorElapsedSeconds
)
{
    Logger::info(
        "Starting tank measurement"
    );

    Led::setColor(
        0,
        0,
        255
    );

    /*
     * Batteriewert unmittelbar vor der Messung
     * aktualisieren.
     */
    Battery::loop();

    const bool measurementSuccessful =
        Sensor::measure();

    if (!measurementSuccessful)
    {
        Logger::warning(
            "Tank measurement failed"
        );

        Led::setColor(
            255,
            0,
            0
        );

        delay(1000);

        Led::off();

        return false;
    }

    Logger::info(
        "Tank measurement successful"
    );

    const bool historyStored =
        MeasurementHistory::addCurrentMeasurement();

    if (historyStored)
    {
        Logger::info(
            "Measurement history entry stored"
        );
    }
    else
    {
        Logger::warning(
            "Measurement history entry was not stored"
        );
    }

    BatteryEstimator::addSample(
        Battery::getVoltage(),
        estimatorElapsedSeconds
    );

    Logger::info(
        "Battery estimator: " +
        BatteryEstimator::getDisplayText()
    );

    Led::setColor(
        0,
        255,
        0
    );

    /*
     * Im manuellen Webbetrieb kann MQTT bereits
     * verbunden sein.
     */
#if MQTT_ENABLED
    if (MqttManager::isConnected())
    {
        MqttManager::publishMeasurement();
        MqttManager::publishStatus();
    }
#endif

    delay(500);

    Led::off();

    /*
     * Falls der manuelle Webmodus läuft, beginnt
     * dessen Blinkanzeige anschließend erneut.
     */
    webIndicatorLedState = false;
    lastWebIndicatorToggle = millis();

    return true;
}


/*
 * WLAN und MQTT starten und die aktuelle Messung senden.
 */
void transmitMeasurement()
{
#if MQTT_ENABLED
    Logger::info(
        "Starting data transmission"
    );

    WifiManager::begin();
    WifiManager::connect();

    if (
        !waitForWifi(
            WIFI_CONNECT_TIMEOUT_MS
        )
    )
    {
        WifiManager::disconnect();
        return;
    }

    MqttManager::begin();

    /*
     * connect() sendet in deinem MQTT-Manager nach
     * erfolgreicher Verbindung bereits Status und
     * Messwert.
     */
    if (!MqttManager::connect())
    {
        Logger::warning(
            "MQTT transmission failed"
        );
    }
    else
    {
        /*
         * Kurz Zeit für den Netzwerkstack lassen.
         */
        const unsigned long mqttStartedAt =
            millis();

        while (
            millis() - mqttStartedAt <
            500UL
        )
        {
            MqttManager::loop();
            delay(10);
        }

        Logger::info(
            "MQTT transmission completed"
        );
    }

    MqttManager::disconnect();
    WifiManager::disconnect();

#else

    Logger::warning(
        "MQTT is disabled; measurement was not transmitted"
    );

#endif
}


/*
 * Gerät mit dem gespeicherten Messintervall schlafen legen.
 */
void enterNormalDeepSleep()
{
    if (
        !CoreLogic::shouldEnterDeepSleep(
            Settings::data.deepSleepEnabled,
            false,
            WebServerManager::isConfigPortalActive(),
            SleepManager::getRuntimeMode()
        )
    )
    {
        lastAutomaticMeasurementAt = millis();
        awakeSchedulingActive = true;
        Logger::info(
            "Automatic deep sleep disabled; remaining awake"
        );

        if (!awakeServicesInitialized)
        {
            WebServerManager::begin();
#if MQTT_ENABLED
            MqttManager::begin();
#endif
            awakeServicesInitialized = true;
        }
        return;
    }

    const uint32_t sleepSeconds =
        static_cast<uint32_t>(
            Settings::data.measureInterval
        );

    Logger::info(
        "Normal cycle finished"
    );

    Logger::info(
        "Sleeping for " +
        String(sleepSeconds) +
        " seconds"
    );

    Serial.flush();

    SleepManager::sleepForSeconds(
        sleepSeconds
    );
}


/*
 * Ein vollständiger automatischer Messzyklus.
 */
void runAutomaticCycle(
    uint32_t estimatorElapsedSeconds
)
{
    /*
     * WLAN arbeitet parallel zur Sensormessung.
     */
    bool wifiReady = false;

    if (
        Settings::data.ntpEnabled ||
        MQTT_ENABLED
    )
    {
        WifiManager::begin();
        WifiManager::connect();

        wifiReady =
            waitForWifi(
                WIFI_CONNECT_TIMEOUT_MS
            );

        if (
            wifiReady &&
            Settings::data.ntpEnabled
        )
        {
            TimeManager::syncFromNtp();
        }
    }

    const bool measurementSuccessful =
        performMeasurement(
            estimatorElapsedSeconds
        );

#if MQTT_ENABLED
    if (measurementSuccessful)
    {
        if (wifiReady)
        {
            MqttManager::begin();

            /*
             * MqttManager::connect() veröffentlicht
             * bereits Status und Messwert.
             */
            if (!MqttManager::connect())
            {
                Logger::warning(
                    "Automatic MQTT transmission failed"
                );
            }
            else
            {
                const unsigned long mqttStartedAt =
                    millis();

                while (
                    millis() - mqttStartedAt <
                    500UL
                )
                {
                    MqttManager::loop();
                    delay(10);
                }
            }
        }
    }
#endif

    enterNormalDeepSleep();
}


/*
 * ============================================================
 * Setup
 * ============================================================
 */

void setup()
{
    Logger::begin();
    MeasurementHistory::begin();
    SleepManager::begin();

    Logger::info(
        "================================="
    );

    Logger::info(
        FW_NAME
    );

    Logger::info(
        "Firmware: " FW_VERSION
    );

    Logger::info(
        "Booting..."
    );

    Logger::info(
        "================================="
    );

    Settings::begin();
    lastDeepSleepEnabled =
        Settings::data.deepSleepEnabled;
    TimeManager::begin();

    Battery::begin();
    BatteryEstimator::begin();

    Led::begin();
    Button::begin();
    Sensor::begin();

    if (FactoryReset::shouldStartConfigPortal())
    {
        Logger::warning(
            "Factory reset recovery boot: starting configuration access point"
        );

        WifiManager::begin();
        WebServerManager::beginConfigPortal();

        if (WebServerManager::isConfigPortalActive())
        {
            FactoryReset::clearConfigPortalRequest();
            startConfigPortalIndicator();
            Logger::info(
                "Factory reset complete; configuration portal active"
            );
        }
        else
        {
            Logger::error(
                "Configuration portal failed; recovery request retained"
            );
        }

        return;
    }

    const WakeupReason wakeupReason =
        SleepManager::getWakeupReason();

    Logger::info(
        "Device Name      : " +
        Settings::data.deviceName
    );

    Logger::info(
        "Tank Height      : " +
        String(
            Settings::data.tankHeight,
            1
        ) +
        " cm"
    );

    Logger::info(
        "Sensor Clearance : " +
        String(
            Settings::data.sensorClearance,
            1
        ) +
        " cm"
    );

    Logger::info(
        "Measure Interval : " +
        String(
            Settings::data.measureInterval
        ) +
        " s"
    );

    Logger::info(
        "Deep Sleep       : " +
        String(
            Settings::data.deepSleepEnabled
                ? "enabled"
                : "disabled"
        )
    );


    /*
     * ========================================================
     * Timer-Wakeup
     * ========================================================
     *
     * Messen, senden und sofort wieder schlafen.
     *
     * Kein Webserver.
     */
    if (
        wakeupReason ==
        WakeupReason::Timer
    )
    {
        Logger::info(
            "Timer wakeup: starting automatic cycle"
        );

        runAutomaticCycle(
            static_cast<uint32_t>(
                Settings::data.measureInterval
            )
        );

        return;
    }


    /*
     * ========================================================
     * Normaler Power-on / Reset
     * ========================================================
     *
     * Ebenfalls ein automatischer Messzyklus.
     *
     * Dadurch bleibt das Gerät im Normalbetrieb
     * nicht unnötig wach und startet keinen Webserver.
     */
    if (
        wakeupReason ==
            WakeupReason::PowerOn ||
        wakeupReason ==
            WakeupReason::Unknown
    )
    {
#ifdef DEBUG_LED_TEST
        Logger::info(
            "Running LED self test..."
        );

        Led::test();

        Logger::info(
            "LED self test finished"
        );
#endif

        Logger::info(
            "Power-on: starting automatic cycle"
        );

        runAutomaticCycle(
            0
        );

        return;
    }


    /*
     * ========================================================
     * Button-Wakeup
     * ========================================================
     *
     * Das Gerät bleibt zunächst wach, damit die Länge
     * des Tastendrucks ausgewertet werden kann.
     *
     * Der Webserver wird noch nicht automatisch gestartet.
     */
    Logger::info(
        "Button wakeup: waiting for button action"
    );

    WifiManager::begin();
    WifiManager::connect();

#if MQTT_ENABLED
    MqttManager::begin();
#endif

    Logger::info(
        "Web interface remains disabled until "
        "a 5 or 15 second button press"
    );

    Logger::info(
        "System ready"
    );

    lastAutomaticMeasurementAt = millis();

    Logger::info(
        "================================="
    );
}


/*
 * ============================================================
 * Main loop
 * ============================================================
 */

void loop()
{
    if (!SleepManager::canStartNormalWork())
    {
        delay(5);
        return;
    }

    Button::loop();
    Battery::loop();
    Sensor::loop();
    WifiManager::loop();

    if (
        !timeSyncAttempted &&
        WifiManager::isConnected()
    )
    {
        timeSyncAttempted = true;
        TimeManager::syncFromNtp();
    }

    if (WebServerManager::isRunning())
    {
        WebServerManager::loop();
    }

#if MQTT_ENABLED
    MqttManager::loop();
#endif

    const ButtonEvent buttonEvent =
        Button::getEvent();

    switch (buttonEvent)
    {
        /*
         * Kurzer Tastendruck:
         *
         * messen, senden und wieder schlafen.
         */
        case ButtonEvent::ShortPress:
        {
            Logger::info(
                "Button: short press"
            );

            const bool measurementSuccessful =
                performMeasurement(
                    0
                );

#if MQTT_ENABLED
            if (measurementSuccessful)
            {
                if (
                    waitForWifi(
                        WIFI_CONNECT_TIMEOUT_MS
                    )
                )
                {
                    if (!MqttManager::isConnected())
                    {
                        MqttManager::connect();
                    }

                    if (MqttManager::isConnected())
                    {
                        MqttManager::
                            publishMeasurement();

                        MqttManager::
                            publishStatus();

                        delay(300);
                    }
                }
            }
#endif

            enterNormalDeepSleep();

            break;
        }


        /*
         * 5 Sekunden:
         *
         * normaler Webserver über das gespeicherte WLAN.
         */
        case ButtonEvent::WebServerPress:
        {
            Logger::info(
                "Button: 5 second press"
            );

            if (!WifiManager::isConnected())
            {
                WifiManager::connect();
            }

            WebServerManager::begin();

            startWebServerIndicator();

            Logger::info(
                "Webserver active - LED blinking blue"
            );

            break;
        }


        /*
         * 15 Sekunden:
         *
         * Konfigurations-AP und Webserver.
         */
        case ButtonEvent::ConfigPortalPress:
        {
            Logger::info(
                "Button: 15 second press"
            );

            WebServerManager::
                beginConfigPortal();

            startConfigPortalIndicator();

            Logger::info(
                "Configuration portal active - "
                "LED blinking red"
            );

            break;
        }

        case ButtonEvent::FactoryResetArmed:
        {
            webIndicatorMode =
                WebIndicatorMode::FactoryResetArmed;
            webIndicatorLedState = false;
            webIndicatorStartedAt = millis();
            lastWebIndicatorToggle =
                millis() - FACTORY_RESET_LED_INTERVAL_MS;

            Logger::warning(
                "Factory reset armed - release GPIO 33 to erase data"
            );
            break;
        }

        case ButtonEvent::FactoryResetConfirmed:
        {
            FactoryReset::execute();
            break;
        }


        case ButtonEvent::None:
        default:
            break;
    }

    updateWebIndicator();


    /*
     * Batterieprotokoll nur im manuellen Wachbetrieb.
     */
    const unsigned long now =
        millis();

    /*
     * Awake-mode scheduling deliberately uses the last automatic
     * measurement as its baseline. Manual web/button measurements do not
     * postpone the configured periodic cycle.
     */
    const unsigned long intervalMs =
        static_cast<unsigned long>(
            Settings::data.measureInterval
        ) * 1000UL;
    if (
        Settings::data.deepSleepEnabled !=
        lastDeepSleepEnabled
    )
    {
        lastDeepSleepEnabled =
            Settings::data.deepSleepEnabled;
        lastAutomaticMeasurementAt = now;
    }
    if (!Settings::data.deepSleepEnabled)
    {
        awakeSchedulingActive = true;
    }
    if (
        awakeSchedulingActive &&
        !WebServerManager::isConfigPortalActive() &&
        CoreLogic::isMeasurementDue(
            now,
            lastAutomaticMeasurementAt,
            intervalMs
        )
    )
    {
        lastAutomaticMeasurementAt = now;
        performMeasurement(
            static_cast<uint32_t>(
                Settings::data.measureInterval
            )
        );
        enterNormalDeepSleep();
    }

    if (
        now - lastBatteryLog >=
        5000UL
    )
    {
        lastBatteryLog = now;

        Logger::info(
            "Battery: " +
            String(
                Battery::getVoltage(),
                2
            ) +
            " V (" +
            String(
                Battery::getPercentage()
            ) +
            "%)"
        );
    }

    delay(5);
}
