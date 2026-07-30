#include "sleep_manager.h"

#include "config.h"
#include "led.h"
#include "logger.h"
#include "settings.h"

WakeupReason SleepManager::wakeupReason =
    WakeupReason::Unknown;

void SleepManager::begin()
{
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
    Logger::info(
        "Preparing for deep sleep"
    );

    Led::off();

    Logger::flushPersistentLogs();

    delay(100);
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

    prepareForSleep();

    esp_sleep_enable_timer_wakeup(
        static_cast<uint64_t>(seconds) *
        1000000ULL
    );

    esp_sleep_enable_ext0_wakeup(
        static_cast<gpio_num_t>(
            PIN_BUTTON
        ),
        BUTTON_WAKEUP_LEVEL
    );

    Serial.flush();

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
