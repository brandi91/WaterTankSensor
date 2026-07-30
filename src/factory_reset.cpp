#include "factory_reset.h"

#include <Arduino.h>
#include <Preferences.h>

#include "battery_estimator.h"
#include "led.h"
#include "logger.h"
#include "measurement_history.h"
#include "settings.h"
#include "time_manager.h"
#include "web_server_manager.h"

namespace
{
constexpr const char* RECOVERY_NAMESPACE = "recovery";
constexpr const char* CONFIG_AP_KEY = "startConfigAp";
}

void FactoryReset::execute()
{
    Logger::warning(
        "Factory reset requested; clearing all user data"
    );
    Logger::flushPersistentLogs();

    WebServerManager::invalidateAuthenticationSession();
    Settings::reset();
    BatteryEstimator::reset();
    TimeManager::resetPersistedState();
    MeasurementHistory::clear();

    Preferences recovery;
    if (recovery.begin(RECOVERY_NAMESPACE, false))
    {
        recovery.clear();
        recovery.putBool(CONFIG_AP_KEY, true);
        recovery.end();
    }

    Logger::clearPersistentLogs();

    Serial.println(
        "[RECOVERY] Factory reset complete; restarting in configuration mode"
    );
    Led::off();
    Serial.flush();
    delay(100);
    ESP.restart();
}

bool FactoryReset::shouldStartConfigPortal()
{
    Preferences recovery;
    if (!recovery.begin(RECOVERY_NAMESPACE, true))
    {
        return false;
    }

    const bool requested =
        recovery.isKey(CONFIG_AP_KEY) &&
        recovery.getBool(CONFIG_AP_KEY, false);
    recovery.end();
    return requested;
}

void FactoryReset::clearConfigPortalRequest()
{
    Preferences recovery;
    if (recovery.begin(RECOVERY_NAMESPACE, false))
    {
        recovery.clear();
        recovery.end();
    }
}
