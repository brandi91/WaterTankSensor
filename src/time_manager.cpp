#include "time_manager.h"
#include "sleep_manager.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "logger.h"
#include "settings.h"

namespace
{
    constexpr time_t MIN_VALID_TIMESTAMP = 1700000000;
    constexpr const char* PREF_NAMESPACE = "deviceTime";
    constexpr const char* KEY_LAST_SYNC = "lastSync";

    RTC_DATA_ATTR time_t rtcTimestampAtSleep = 0;
    RTC_DATA_ATTR uint32_t rtcPlannedSleepSeconds = 0;
    RTC_DATA_ATTR bool rtcEstimateAvailable = false;

    Preferences preferences;
}

bool TimeManager::initialized = false;
bool TimeManager::valid = false;
time_t TimeManager::baseTimestamp = 0;
unsigned long TimeManager::baseMillis = 0;
time_t TimeManager::lastSuccessfulSync = 0;
String TimeManager::timeSource = "Unavailable";

void TimeManager::begin()
{
    if (initialized)
    {
        return;
    }

    initialized = true;
    baseMillis = millis();

    if (preferences.begin(PREF_NAMESPACE, false))
    {
        if (preferences.isKey(KEY_LAST_SYNC))
        {
            lastSuccessfulSync =
                static_cast<time_t>(
                    preferences.getULong64(
                        KEY_LAST_SYNC,
                        0
                    )
                );
        }
    }

    if (
        esp_sleep_get_wakeup_cause() ==
            ESP_SLEEP_WAKEUP_TIMER &&
        rtcEstimateAvailable &&
        rtcTimestampAtSleep >= MIN_VALID_TIMESTAMP
    )
    {
        baseTimestamp =
            rtcTimestampAtSleep +
            rtcPlannedSleepSeconds;
        valid = true;
        timeSource = "Estimated";
    }
}

bool TimeManager::syncFromNtp()
{
    if (!SleepManager::canStartNormalWork())
    {
        return false;
    }

    begin();

    if (
        !Settings::data.ntpEnabled ||
        WiFi.status() != WL_CONNECTED
    )
    {
        return false;
    }

    configTzTime(
        Settings::data.timeZone.c_str(),
        Settings::data.ntpServer1.c_str(),
        Settings::data.ntpServer2.c_str(),
        Settings::data.ntpServer3.c_str()
    );

    const unsigned long startedAt = millis();
    const unsigned long timeoutMs =
        static_cast<unsigned long>(
            Settings::data.ntpTimeoutSeconds
        ) * 1000UL;

    time_t synchronizedTime = 0;

    while (millis() - startedAt < timeoutMs)
    {
        synchronizedTime = time(nullptr);

        if (synchronizedTime >= MIN_VALID_TIMESTAMP)
        {
            baseTimestamp = synchronizedTime;
            baseMillis = millis();
            lastSuccessfulSync = synchronizedTime;
            valid = true;
            timeSource = "NTP";
            rtcEstimateAvailable = true;

            if (preferences.isKey(KEY_LAST_SYNC))
            {
                preferences.remove(KEY_LAST_SYNC);
            }

            preferences.putULong64(
                KEY_LAST_SYNC,
                static_cast<uint64_t>(synchronizedTime)
            );

            Logger::info("NTP synchronization successful");
            return true;
        }

        delay(100);
    }

    Logger::warning(
        valid
            ? "NTP synchronization failed; using estimated time"
            : "NTP synchronization failed; time unavailable"
    );

    return false;
}

bool TimeManager::hasValidTime()
{
    return initialized && valid;
}

time_t TimeManager::now()
{
    if (!initialized || !valid)
    {
        return 0;
    }

    return baseTimestamp +
        static_cast<time_t>(
            (millis() - baseMillis) / 1000UL
        );
}

String TimeManager::getTimeSource()
{
    return initialized
        ? timeSource
        : "Unavailable";
}

String TimeManager::getStorageTimeSource()
{
    String source = getTimeSource();
    source.toLowerCase();
    return source;
}

time_t TimeManager::getLastSuccessfulSync()
{
    return initialized
        ? lastSuccessfulSync
        : 0;
}

String TimeManager::formatCurrentTime()
{
    const time_t timestamp = now();

    if (timestamp == 0)
    {
        return "Unavailable";
    }

    struct tm timeInfo;

    if (!localtime_r(&timestamp, &timeInfo))
    {
        return "Unavailable";
    }

    char buffer[32];
    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    );

    return String(buffer);
}

void TimeManager::prepareForSleep(
    uint32_t sleepSeconds
)
{
    if (!hasValidTime())
    {
        rtcEstimateAvailable = false;
        rtcTimestampAtSleep = 0;
        rtcPlannedSleepSeconds = 0;
        return;
    }

    rtcTimestampAtSleep = now();
    rtcPlannedSleepSeconds = sleepSeconds;
    rtcEstimateAvailable = true;
}

void TimeManager::resetPersistedState()
{
    if (!initialized)
    {
        preferences.begin(PREF_NAMESPACE, false);
    }
    preferences.clear();

    rtcTimestampAtSleep = 0;
    rtcPlannedSleepSeconds = 0;
    rtcEstimateAvailable = false;
    baseTimestamp = 0;
    baseMillis = millis();
    lastSuccessfulSync = 0;
    valid = false;
    timeSource = "Unavailable";
}
