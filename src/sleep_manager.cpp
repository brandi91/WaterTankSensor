#include "sleep_manager.h"

#include "config.h"
#include "led.h"
#include "logger.h"
#include "settings.h"
#include "time_manager.h"
#include "web_server_manager.h"
#include "wifi_manager.h"

#if MQTT_ENABLED
#include "mqtt_manager.h"
#endif

WakeupReason SleepManager::wakeupReason =
    WakeupReason::Unknown;
CoreLogic::RuntimeMode SleepManager::runtimeMode =
    CoreLogic::RuntimeMode::Running;

#if DEEP_SLEEP_AUDIT_DIAGNOSTICS
RTC_DATA_ATTR uint32_t deepSleepAuditBootCount = 0;
RTC_DATA_ATTR bool deepSleepPreparationCompleted = false;
#endif

void SleepManager::begin()
{
    runtimeMode = CoreLogic::RuntimeMode::Running;
#if DEEP_SLEEP_AUDIT_DIAGNOSTICS
    deepSleepAuditBootCount++;
    Logger::info(
        "Deep-sleep audit: boot=" +
        String(deepSleepAuditBootCount) +
        ", previousPreparation=" +
        String(deepSleepPreparationCompleted ? "complete" : "not marked")
    );
    deepSleepPreparationCompleted = false;
#endif
    detectWakeupReason();

    Logger::info(
        "Wakeup reason: " +
        String(getWakeupReasonText())
    );
}

void SleepManager::detectWakeupReason()
{
    const esp_sleep_wakeup_cause_t cause =
        esp_sleep_get_wakeup_cause();

    switch (cause)
    {
        case ESP_SLEEP_WAKEUP_TIMER:
            wakeupReason =
                WakeupReason::Timer;
            break;

        case ESP_SLEEP_WAKEUP_EXT0:
            wakeupReason =
                WakeupReason::Button;
            break;

        case ESP_SLEEP_WAKEUP_UNDEFINED:
            wakeupReason =
                WakeupReason::PowerOn;
            break;

        default:
            wakeupReason =
                WakeupReason::Unknown;
            break;
    }
}

void SleepManager::prepareForSleep()
{
    /*
     * This is the final normal diagnostic. Once the guard is active,
     * callbacks and managers must not queue application work.
     */
    Logger::info(
        "Preparing for deep sleep"
    );

    runtimeMode = CoreLogic::RuntimeMode::PreparingSleep;
    WebServerManager::stop();

#if MQTT_ENABLED
    MqttManager::disconnect();
#endif
    WifiManager::disconnect();

    digitalWrite(
        Settings::activePins().sensorTriggerPin,
        LOW
    );
    Led::off();

    Logger::flushPersistentLogs();

    delay(100);

#if DEEP_SLEEP_AUDIT_DIAGNOSTICS
    deepSleepPreparationCompleted = true;
#endif
}

void SleepManager::sleepForSeconds(
    uint32_t seconds
)
{
    if (seconds == 0)
    {
        Logger::warning(
            "Sleep time must be greater than 0"
        );

        return;
    }

    Logger::info(
        "Deep sleep for " +
        String(seconds) +
        " seconds"
    );

#if DEBUG_DISABLE_DEEP_SLEEP

    Logger::warning(
        "Deep sleep disabled in debug mode"
    );

    return;

#else

    TimeManager::prepareForSleep(seconds);
    prepareForSleep();

    esp_sleep_disable_wakeup_source(
        ESP_SLEEP_WAKEUP_ALL
    );
    esp_sleep_enable_timer_wakeup(
        static_cast<uint64_t>(seconds) *
        1000000ULL
    );

    esp_sleep_enable_ext0_wakeup(
        static_cast<gpio_num_t>(
            RECOVERY_BUTTON_PIN
        ),
        BUTTON_WAKEUP_LEVEL
    );

    Serial.flush();

    runtimeMode = CoreLogic::RuntimeMode::Sleeping;
    esp_deep_sleep_start();

#endif
}

void SleepManager::sleepNow()
{
    uint32_t sleepSeconds =
        static_cast<uint32_t>(
            Settings::data.measureInterval
        );

    if (sleepSeconds == 0)
    {
        sleepSeconds =
            DEFAULT_MEASURE_INTERVAL;
    }

    Logger::info(
        "Using configured measure interval "
        "as deep-sleep interval"
    );

    sleepForSeconds(
        sleepSeconds
    );
}

WakeupReason SleepManager::getWakeupReason()
{
    return wakeupReason;
}

const char* SleepManager::getWakeupReasonText()
{
    switch (wakeupReason)
    {
        case WakeupReason::PowerOn:
            return "Power on / Reset";

        case WakeupReason::Timer:
            return "Timer";

        case WakeupReason::Button:
            return "Button";

        case WakeupReason::Unknown:
        default:
            return "Unknown";
    }
}

bool SleepManager::isPreparingForSleep()
{
    return runtimeMode != CoreLogic::RuntimeMode::Running;
}

bool SleepManager::canStartNormalWork()
{
    return CoreLogic::canStartNormalWork(runtimeMode);
}

CoreLogic::RuntimeMode SleepManager::getRuntimeMode()
{
    return runtimeMode;
}
