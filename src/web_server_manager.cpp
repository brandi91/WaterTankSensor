#include "web_server_manager.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <math.h>

#include "battery.h"
#include "battery_estimator.h"
#include "config.h"
#include "led.h"
#include "logger.h"
#include "measurement_history.h"
#include "sensor.h"
#include "settings.h"
#include "sleep_manager.h"
#include "time_manager.h"
#include "wifi_manager.h" 
#include "version.h"  

#if MQTT_ENABLED
#include "mqtt_manager.h"
#endif

WebServer WebServerManager::server(
    WEB_SERVER_PORT
);

bool WebServerManager::running = false;
bool WebServerManager::configPortalActive = false;
bool WebServerManager::filesystemReady = false;
bool WebServerManager::routesRegistered = false;

bool WebServerManager::restartPending = false;
unsigned long WebServerManager::restartAt = 0;

bool WebServerManager::sleepPending = false;
unsigned long WebServerManager::sleepAt = 0;

/*
 * Zeitpunkt des letzten Batterielernpunkts,
 * der über die Weboberfläche ausgelöst wurde.
 */
unsigned long
    WebServerManager::lastBatteryEstimatorSampleAt = 0;

void WebServerManager::begin()
{
    if (!filesystemReady)
    {
        filesystemReady = LittleFS.begin(true);

        if (!filesystemReady)
        {
            Logger::error(
                "Failed to mount LittleFS"
            );

            return;
        }

        Logger::info(
            "LittleFS mounted successfully"
        );

        MeasurementHistory::begin();
        Logger::enablePersistentLogging();
    }

    if (!routesRegistered)
    {
        registerRoutes();
        routesRegistered = true;
    }

    if (running)
    {
        return;
    }

    server.begin();
    running = true;

    Logger::info("Webserver started");

    if (WiFi.status() == WL_CONNECTED)
    {
        Logger::info(
            "Web interface: http://" +
            WiFi.localIP().toString()
        );
    }
}

void WebServerManager::beginConfigPortal()
{
    begin();

    if (!filesystemReady)
    {
        Logger::error(
            "Cannot start configuration portal: "
            "LittleFS unavailable"
        );

        return;
    }

    if (configPortalActive)
    {
        Logger::warning(
            "Configuration portal already active"
        );

        return;
    }

    Logger::info(
        "Starting configuration portal"
    );

    WiFi.mode(WIFI_AP_STA);

    const bool started = WiFi.softAP(
        CONFIG_AP_SSID,
        CONFIG_AP_PASSWORD
    );

    if (!started)
    {
        Logger::error(
            "Failed to start configuration access point"
        );

        return;
    }

    configPortalActive = true;

    Logger::info(
        "Configuration Wi-Fi: " +
        String(CONFIG_AP_SSID)
    );

    Logger::info(
        "Configuration IP: http://" +
        WiFi.softAPIP().toString()
    );
}

void WebServerManager::loop()
{
    if (running)
    {
        server.handleClient();
    }

    const unsigned long now = millis();

    if (
        restartPending &&
        static_cast<long>(now - restartAt) >= 0
    )
    {
        restartPending = false;

        Logger::warning(
            "Restarting ESP32 now"
        );

        Logger::flushPersistentLogs();

        Serial.flush();
        ESP.restart();
    }

    if (
        sleepPending &&
        static_cast<long>(now - sleepAt) >= 0
    )
    {
        sleepPending = false;

        Logger::info(
            "Entering deep sleep from web interface"
        );

        WebServerManager::stop();
        Led::off();

        Serial.flush();

        SleepManager::sleepNow();
    }
}

void WebServerManager::stop()
{
    if (running)
    {
        server.stop();
        running = false;

        Logger::info(
            "Webserver stopped"
        );
    }

    if (configPortalActive)
    {
        WiFi.softAPdisconnect(true);
        configPortalActive = false;

        Logger::info(
            "Configuration access point stopped"
        );
    }
}

bool WebServerManager::isRunning()
{
    return running;
}

bool WebServerManager::isConfigPortalActive()
{
    return configPortalActive;
}
void WebServerManager::registerRoutes()
{
    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    server.on(
        "/status",
        HTTP_GET,
        handleStatus
    );

    server.on(
        "/logs",
        HTTP_GET,
        handleLogs
    );

    server.on(
        "/api/history",
        HTTP_GET,
        handleHistoryApi
    );

    server.on(
        "/api/logs",
        HTTP_GET,
        handleLogsApi
    );

    server.on(
        "/save",
        HTTP_POST,
        handleSave
    );

    server.on(
        "/measure",
        HTTP_POST,
        handleMeasure
    );

    server.on(
    "/reset-battery-estimate",
    HTTP_POST,
    handleResetBatteryEstimate
);

server.on(
    "/mqtt-discovery",
    HTTP_POST,
    handlePublishMqttDiscovery
);

    server.on(
        "/clear-history",
        HTTP_POST,
        handleClearHistory
    );

    server.on(
        "/clear-logs",
        HTTP_POST,
        handleClearLogs
    );

server.on(
    "/sleep",
        HTTP_POST,
        handleSleep
    );

    server.on(
        "/restart",
        HTTP_POST,
        handleRestart
    );

    server.on(
        "/style.css",
        HTTP_GET,
        []()
        {
            WebServerManager::sendFile(
                "/style.css",
                "text/css; charset=utf-8"
            );
        }
    );

    server.on(
        "/favicon.ico",
        HTTP_GET,
        []()
        {
            server.send(
                204,
                "text/plain",
                ""
            );
        }
    );

    server.on(
        "/generate_204",
        HTTP_ANY,
        handleRoot
    );

    server.on(
        "/gen_204",
        HTTP_ANY,
        handleRoot
    );

    server.on(
        "/hotspot-detect.html",
        HTTP_ANY,
        handleRoot
    );

    server.on(
        "/library/test/success.html",
        HTTP_ANY,
        handleRoot
    );

    server.on(
        "/connecttest.txt",
        HTTP_ANY,
        handleRoot
    );

    server.on(
        "/ncsi.txt",
        HTTP_ANY,
        handleRoot
    );

    server.onNotFound(
        handleNotFound
    );
}
void WebServerManager::handleRoot()
{
    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );

    server.sendHeader(
        "Pragma",
        "no-cache"
    );

    server.sendHeader(
        "Expires",
        "0"
    );

    sendTemplate(
        "/index.html"
    );
}

void WebServerManager::handleLogs()
{
    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );

    server.sendHeader(
        "Pragma",
        "no-cache"
    );

    server.sendHeader(
        "Expires",
        "0"
    );

    sendTemplate(
        "/logs.html"
    );
}

void WebServerManager::handleHistoryApi()
{
    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );

    server.sendHeader(
        "Pragma",
        "no-cache"
    );

    server.sendHeader(
        "Expires",
        "0"
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        MeasurementHistory::toJson()
    );
}

void WebServerManager::handleLogsApi()
{
    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );

    server.sendHeader(
        "Pragma",
        "no-cache"
    );

    server.sendHeader(
        "Expires",
        "0"
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        Logger::getPersistentLogsJson()
    );
}

void WebServerManager::handleStatus()
{
    const bool sensorValid =
        Sensor::isValid();

    const String distance =
        sensorValid
            ? String(
                Sensor::getDistanceCm(),
                1
              )
            : "-";

    const String waterLevel =
        sensorValid
            ? String(
                Sensor::getWaterLevelCm(),
                1
              )
            : "-";

    const String fillPercent =
        sensorValid
            ? String(
                Sensor::getPercentage()
              )
            : "-";

    const String sensorStatus =
        getSensorStatusText();

    const String batteryStatus =
        getBatteryStatusText();

    const String wifiRssi =
        WifiManager::isConnected()
            ? String(
                WifiManager::getRssi()
              )
            : "-";

    String json;

    json.reserve(768);

    json += "{";

    json += "\"fillPercent\":\"";
    json += fillPercent;
    json += "\",";

    json += "\"waterLevelCm\":\"";
    json += waterLevel;
    json += "\",";

    json += "\"distanceCm\":\"";
    json += distance;
    json += "\",";


    json += "\"sensorClearanceCm\":\"";
    json += String(
        Settings::data.sensorClearance,
        1
    );
    json += "\",";


    json += "\"batteryPercent\":";
    json += String(
        Battery::getPercentage()
    );
    json += ",";

    json += "\"batteryValid\":";
    json += Battery::isValid()
        ? "true"
        : "false";
    json += ",";

    json += "\"batteryVoltage\":\"";
    json += String(
        Battery::getVoltage(),
        2
    );
    json += "\",";

    json += "\"wifiRssi\":\"";
    json += wifiRssi;
    json += "\",";

    json += "\"ipAddress\":\"";
    json += getIpAddress();
    json += "\",";

    json += "\"subnetMask\":\"";
    json += WifiManager::getSubnetMask();
    json += "\",";

    json += "\"gatewayAddress\":\"";
    json += WifiManager::getGatewayAddress();
    json += "\",";

    json += "\"dns1\":\"";
    json += WifiManager::getDnsAddress(0);
    json += "\",";

    json += "\"dns2\":\"";
    json += WifiManager::getDnsAddress(1);
    json += "\",";

    json += "\"hostname\":\"";
    json += WifiManager::getHostname();
    json += "\",";

    json += "\"mdnsName\":\"";
    json += WifiManager::getMdnsName();
    json += "\",";

    json += "\"networkMode\":\"";
    json += getNetworkMode();
    json += "\",";

    json += "\"sensorStatus\":\"";
    json += sensorStatus;
    json += "\",";

    json += "\"sensorStatusClass\":\"";
    json += getSensorStatusClass();
    json += "\",";

    json += "\"batteryStatus\":\"";
    json += batteryStatus;
    json += "\",";

    json += "\"batteryStatusClass\":\"";
    json += getBatteryStatusClass();
    json += "\",";

    json += "\"powerSource\":\"";
    json += Battery::getPowerSourceText();
    json += "\",";

json += "\"batteryEstimate\":\"";
json += BatteryEstimator::getDisplayText();
json += "\",";

json += "\"batterySamples\":";
json += String(
    BatteryEstimator::getSampleCount()
);
json += ",";

json += "\"batteryTestMode\":";
json += Settings::data.batteryEstimateTestMode
    ? "true"
    : "false";
json += ",";

    json += "\"mqttStatus\":\"";
    json += getMqttStatusText();
    json += "\",";

    json += "\"mqttStatusClass\":\"";
    json += getMqttStatusClass();
    json += "\",";

    json += "\"currentTimestamp\":";
    json += String(
        static_cast<uint32_t>(
            TimeManager::now()
        )
    );
    json += ",";

    json += "\"currentTimeValid\":";
    json += TimeManager::hasValidTime()
        ? "true"
        : "false";
    json += ",";

    json += "\"timeSource\":\"";
    json += TimeManager::getTimeSource();
    json += "\",";

    json += "\"lastNtpSync\":";
    json += String(
        static_cast<uint32_t>(
            TimeManager::getLastSuccessfulSync()
        )
    );
    json += ",";

    json += "\"ntpEnabled\":";
    json += Settings::data.ntpEnabled
        ? "true"
        : "false";

    json += "}";

    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void WebServerManager::handleSave()
{
    if (server.hasArg("deviceName"))
    {
        const String deviceName =
            server.arg("deviceName");

        if (!deviceName.isEmpty())
        {
            Settings::data.deviceName =
                deviceName;
        }
    }

    if (server.hasArg("wifiSSID"))
    {
        Settings::data.wifiSSID =
            server.arg("wifiSSID");
    }

    if (server.hasArg("wifiPassword"))
    {
        const String newWifiPassword =
            server.arg("wifiPassword");

        if (!newWifiPassword.isEmpty())
        {
            Settings::data.wifiPassword =
                newWifiPassword;
        }
    }

    Settings::data.wifiDhcp =
        server.hasArg("wifiDhcp");

    if (server.hasArg("wifiStaticIp"))
    {
        Settings::data.wifiStaticIp =
            server.arg("wifiStaticIp");
    }

    if (server.hasArg("wifiGateway"))
    {
        Settings::data.wifiGateway =
            server.arg("wifiGateway");
    }

    if (server.hasArg("wifiSubnet"))
    {
        Settings::data.wifiSubnet =
            server.arg("wifiSubnet");
    }

    if (server.hasArg("wifiDns1"))
    {
        Settings::data.wifiDns1 =
            server.arg("wifiDns1");
    }

    if (server.hasArg("wifiDns2"))
    {
        Settings::data.wifiDns2 =
            server.arg("wifiDns2");
    }

    Settings::data.mqttEnabled =
        server.hasArg("mqttEnabled");

    if (server.hasArg("mqttServer"))
    {
        Settings::data.mqttServer =
            server.arg("mqttServer");
    }

    if (server.hasArg("mqttPort"))
    {
        const long mqttPort =
            server.arg("mqttPort").toInt();

        if (
            mqttPort >= 1 &&
            mqttPort <= 65535
        )
        {
            Settings::data.mqttPort =
                static_cast<uint16_t>(
                    mqttPort
                );
        }
    }

    if (server.hasArg("mqttUser"))
    {
        Settings::data.mqttUser =
            server.arg("mqttUser");
    }

    if (server.hasArg("mqttPassword"))
    {
        const String newMqttPassword =
            server.arg("mqttPassword");

        if (!newMqttPassword.isEmpty())
        {
            Settings::data.mqttPassword =
                newMqttPassword;
        }
    }

    if (server.hasArg("tankHeight"))
    {
        const float tankHeight =
            server.arg(
                "tankHeight"
            ).toFloat();

        if (tankHeight > 0.0f)
        {
            Settings::data.tankHeight =
                tankHeight;
        }
    }


    /*
     * Sensor Clearance:
     * Luftspalt zwischen Sensor und maximalem
     * Wasserstand.
     */
    if (server.hasArg("sensorClearance"))
    {
        const float sensorClearance =
            server.arg(
                "sensorClearance"
            ).toFloat();

        if (sensorClearance >= 0.0f)
        {
            Settings::data.sensorClearance =
                sensorClearance;
        }
    }


    auto parseUnsigned = [](
        const String& text,
        uint32_t& value
    )
    {
        if (text.isEmpty())
        {
            return false;
        }

        char* end = nullptr;
        const unsigned long parsed =
            strtoul(text.c_str(), &end, 10);

        if (
            end == text.c_str() ||
            *end != '\0'
        )
        {
            return false;
        }

        value = static_cast<uint32_t>(parsed);
        return true;
    };

    auto parseFloat = [](
        const String& text,
        float& value
    )
    {
        if (text.isEmpty())
        {
            return false;
        }

        char* end = nullptr;
        value = strtof(text.c_str(), &end);

        return
            end != text.c_str() &&
            *end == '\0' &&
            isfinite(value);
    };

    if (
        server.hasArg("measureInterval") &&
        server.hasArg("measureIntervalUnit")
    )
    {
        uint32_t value = 0;
        const String unit =
            server.arg("measureIntervalUnit");
        uint32_t multiplier = 0;

        if (unit == "seconds")
        {
            multiplier = 1;
        }
        else if (unit == "minutes")
        {
            multiplier = 60;
        }
        else if (unit == "hours")
        {
            multiplier = 3600;
        }

        if (
            parseUnsigned(
                server.arg("measureInterval"),
                value
            ) &&
            value > 0 &&
            multiplier > 0 &&
            value <= 86400UL / multiplier
        )
        {
            Settings::data.measureInterval =
                value * multiplier;
        }
        else
        {
            Logger::warning(
                "Invalid measurement interval; previous value preserved"
            );
        }
    }

    const bool requestedNtpEnabled =
        server.hasArg("ntpEnabled");
    const String ntpServer1 =
        server.arg("ntpServer1");
    const String ntpServer2 =
        server.arg("ntpServer2");
    const String ntpServer3 =
        server.arg("ntpServer3");
    uint32_t ntpTimeout = 0;
    const bool ntpValid =
        !requestedNtpEnabled ||
        (
            (
                !ntpServer1.isEmpty() ||
                !ntpServer2.isEmpty() ||
                !ntpServer3.isEmpty()
            ) &&
            parseUnsigned(
                server.arg("ntpTimeoutSeconds"),
                ntpTimeout
            ) &&
            ntpTimeout >= 1 &&
            ntpTimeout <= 30
        );

    if (ntpValid)
    {
        Settings::data.ntpEnabled =
            requestedNtpEnabled;
        Settings::data.timeZone =
            server.arg("timeZone");
        Settings::data.ntpServer1 = ntpServer1;
        Settings::data.ntpServer2 = ntpServer2;
        Settings::data.ntpServer3 = ntpServer3;

        if (requestedNtpEnabled)
        {
            Settings::data.ntpTimeoutSeconds =
                static_cast<uint8_t>(ntpTimeout);
        }
    }
    else
    {
        Logger::warning(
            "Invalid NTP settings; previous values preserved"
        );
    }

    const float previousEmpty =
        Settings::data.batteryEmptyVoltage;
    const float previousFull =
        Settings::data.batteryFullVoltage;
    const uint32_t previousCapacity =
        Settings::data.batteryCapacityMah;
    const String previousChemistry =
        Settings::data.batteryChemistry;
    const uint8_t previousCells =
        Settings::data.batteryCellCount;

    float emptyVoltage = 0.0f;
    float fullVoltage = 0.0f;
    uint32_t capacity = 0;
    uint32_t cells = 0;
    const String chemistry =
        server.arg("batteryChemistry");
    const bool chemistryValid =
        chemistry == "custom" ||
        chemistry == "li-ion" ||
        chemistry == "lifepo4" ||
        chemistry == "nimh";
    const bool batterySettingsValid =
        parseFloat(
            server.arg("batteryEmptyVoltage"),
            emptyVoltage
        ) &&
        parseFloat(
            server.arg("batteryFullVoltage"),
            fullVoltage
        ) &&
        parseUnsigned(
            server.arg("batteryCapacityMah"),
            capacity
        ) &&
        parseUnsigned(
            server.arg("batteryCellCount"),
            cells
        ) &&
        emptyVoltage > 0.0f &&
        fullVoltage > emptyVoltage &&
        capacity > 0 &&
        capacity <= 100000UL &&
        cells >= 1 &&
        cells <= 255 &&
        chemistryValid;

    if (batterySettingsValid)
    {
        Settings::data.batteryEmptyVoltage =
            emptyVoltage;
        Settings::data.batteryFullVoltage =
            fullVoltage;
        Settings::data.batteryCapacityMah =
            capacity;
        Settings::data.batteryChemistry =
            chemistry;
        Settings::data.batteryCellCount =
            static_cast<uint8_t>(cells);
    }
    else
    {
        Logger::warning(
            "Invalid battery configuration; previous values preserved"
        );
    }

/*
 * Ein nicht gesetztes Checkbox-Feld wird vom
 * Browser überhaupt nicht übertragen.
 */
Settings::data.batteryEstimateTestMode =
    server.hasArg(
        "batteryEstimateTestMode"
    );

Settings::save();

    const bool batteryConfigurationChanged =
        previousEmpty !=
            Settings::data.batteryEmptyVoltage ||
        previousFull !=
            Settings::data.batteryFullVoltage ||
        previousCapacity !=
            Settings::data.batteryCapacityMah ||
        previousChemistry !=
            Settings::data.batteryChemistry ||
        previousCells !=
            Settings::data.batteryCellCount;

    if (batteryConfigurationChanged)
    {
        BatteryEstimator::reset();
        Logger::warning(
            "Battery configuration changed. Battery lifetime learning was reset."
        );
    }

    Logger::info(
        "Configuration saved"
    );

    sendTemplate(
        "/saved.html"
    );
}

void WebServerManager::handleMeasure()
{
    Logger::info(
        "Measurement requested from web interface"
    );

    Led::setColor(
        0,
        0,
        255
    );

    const bool measurementSuccessful =
        Sensor::measure();

    if (measurementSuccessful)
    {
        /*
         * Vergangene Zeit seit dem letzten
         * Web-Messpunkt bestimmen.
         */
        const unsigned long now =
            millis();

        uint32_t elapsedSeconds = 0;

        if (lastBatteryEstimatorSampleAt > 0)
        {
            elapsedSeconds =
                static_cast<uint32_t>(
                    (
                        now -
                        lastBatteryEstimatorSampleAt
                    ) /
                    1000UL
                );
        }

        lastBatteryEstimatorSampleAt = now;

        BatteryEstimator::addSample(
            Battery::getVoltage(),
            elapsedSeconds
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

        Logger::info(
            "Web measurement successful"
        );

        const bool historyStored =
            MeasurementHistory::
                addCurrentMeasurement();

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

#if MQTT_ENABLED
        MqttManager::publishMeasurement();
#endif
    }
    else
    {
        Led::setColor(
            255,
            0,
            0
        );

        Logger::warning(
            "Web measurement failed"
        );
    }

    delay(
        measurementSuccessful
            ? 300
            : 800
    );

    Led::off();

    server.sendHeader(
        "Location",
        "/",
        true
    );

    server.send(
        303,
        "text/plain",
        ""
    );
}
void WebServerManager::handleResetBatteryEstimate()
{
    Logger::warning(
        "Battery estimate reset requested "
        "from web interface"
    );

    BatteryEstimator::reset();

    /*
     * Auch den lokalen Web-Zeitpunkt zurücksetzen,
     * damit die nächste Messung eine neue Lernphase
     * beginnt.
     */
    lastBatteryEstimatorSampleAt = 0;

    server.sendHeader(
        "Location",
        "/",
        true
    );

    server.send(
        303,
        "text/plain",
        ""
    );
}
void WebServerManager::handleClearHistory()
{
    MeasurementHistory::clear();

    server.sendHeader(
        "Location",
        "/logs",
        true
    );

    server.send(
        303,
        "text/plain",
        ""
    );
}

void WebServerManager::handleClearLogs()
{
    Logger::clearPersistentLogs();

    server.sendHeader(
        "Location",
        "/logs",
        true
    );

    server.send(
        303,
        "text/plain",
        ""
    );
}

void WebServerManager::handlePublishMqttDiscovery()
{
    Logger::info(
        "MQTT Discovery requested from web interface"
    );

#if MQTT_ENABLED

    if (!Settings::data.mqttEnabled)
    {
        MqttManager::recordDiscoveryResult(false);
        Logger::warning(
            "MQTT Discovery failed: MQTT disabled in settings"
        );

        server.send(
            409,
            "text/plain; charset=utf-8",
            "MQTT is disabled. Enable MQTT and save the settings first."
        );

        return;
    }

    if (!WifiManager::isConnected())
    {
        WifiManager::connect();

        const unsigned long startedAt = millis();

        while (
            !WifiManager::isConnected() &&
            millis() - startedAt <
                WIFI_CONNECT_TIMEOUT_MS
        )
        {
            WifiManager::loop();
            delay(20);
        }

        if (WifiManager::isConnected())
        {
            TimeManager::syncFromNtp();
        }
    }

    if (!WifiManager::isConnected())
    {
        MqttManager::recordDiscoveryResult(false);
        Logger::warning(
            "MQTT Discovery failed: Wi-Fi connection failed"
        );

        server.send(
            503,
            "text/plain; charset=utf-8",
            "Could not connect to Wi-Fi."
        );

        return;
    }

    const bool wasAlreadyConnected =
        MqttManager::isConnected();

    if (
        !wasAlreadyConnected &&
        !MqttManager::connect()
    )
    {
        MqttManager::recordDiscoveryResult(false);
        Logger::warning(
            "MQTT Discovery failed: broker connection failed"
        );

        server.send(
            502,
            "text/plain; charset=utf-8",
            "Could not connect to the MQTT broker."
        );

        return;
    }

    const bool published =
        MqttManager::publishDiscovery();

    MqttManager::recordDiscoveryResult(
        published
    );

    if (!wasAlreadyConnected)
    {
        MqttManager::disconnect();
    }

    if (!published)
    {
        Logger::warning(
            "MQTT Discovery publish failed"
        );

        server.send(
            500,
            "text/plain; charset=utf-8",
            "MQTT Discovery could not be published."
        );

        return;
    }

    Logger::info(
        "MQTT Discovery successfully triggered "
        "from web interface"
    );

    server.sendHeader(
        "Location",
        "/",
        true
    );

    server.send(
        303,
        "text/plain",
        ""
    );

#else

    Logger::warning(
        "MQTT Discovery unavailable: "
        "MQTT not compiled into firmware"
    );

    server.send(
        501,
        "text/plain; charset=utf-8",
        "MQTT is not compiled into this firmware."
    );

#endif
}
void WebServerManager::handleSleep()
{
    Logger::info(
        "Deep sleep requested from web interface"
    );

    sendTemplate(
        "/sleep.html"
    );

    sleepPending = true;
    sleepAt = millis() + 2500UL;
}

void WebServerManager::handleRestart()
{
    Logger::warning(
        "Restart requested from web interface"
    );

    sendTemplate(
        "/restart.html"
    );

    restartPending = true;
    restartAt = millis() + 3000UL;
}

void WebServerManager::handleNotFound()
{
    Logger::warning(
        "Unknown request: " +
        server.uri()
    );

    server.sendHeader(
        "Location",
        "/",
        true
    );

    server.send(
        302,
        "text/plain",
        ""
    );
}

void WebServerManager::sendTemplate(
    const String& path
)
{
    if (!filesystemReady)
    {
        server.send(
            500,
            "text/plain; charset=utf-8",
            "LittleFS is not available."
        );

        return;
    }

    String page = loadFile(path);

    if (page.isEmpty())
    {
        Logger::error(
            "Template is empty or missing: " +
            path
        );

        server.send(
            404,
            "text/plain; charset=utf-8",
            "Web page not found: " +
            path
        );

        return;
    }

    processTemplate(page);

    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );

    server.send(
        200,
        "text/html; charset=utf-8",
        page
    );
}

void WebServerManager::sendFile(
    const String& path,
    const String& contentType
)
{
    if (!filesystemReady)
    {
        server.send(
            500,
            "text/plain; charset=utf-8",
            "LittleFS is not available."
        );

        return;
    }

    if (!LittleFS.exists(path))
    {
        Logger::warning(
            "Static file not found: " +
            path
        );

        server.send(
            404,
            "text/plain; charset=utf-8",
            "File not found."
        );

        return;
    }

    File file = LittleFS.open(
        path,
        "r"
    );

    if (!file)
    {
        Logger::error(
            "Failed to open file: " +
            path
        );

        server.send(
            500,
            "text/plain; charset=utf-8",
            "Could not open file."
        );

        return;
    }

    server.sendHeader(
        "Cache-Control",
        "no-cache"
    );

    server.streamFile(
        file,
        contentType
    );

    file.close();
}

String WebServerManager::loadFile(
    const String& path
)
{
    if (!LittleFS.exists(path))
    {
        Logger::error(
            "File does not exist: " +
            path
        );

        return "";
    }

    File file = LittleFS.open(
        path,
        "r"
    );

    if (!file)
    {
        Logger::error(
            "Failed to open file: " +
            path
        );

        return "";
    }

    String content;

    content.reserve(
        file.size() + 32
    );

    content = file.readString();

    file.close();

    return content;
}

void WebServerManager::processTemplate(
    String& page
)
{
    page.replace(
        "{{DEVICE_NAME}}",
        htmlEscape(
            Settings::data.deviceName
        )
    );

    page.replace(
        "{{FW_VERSION}}",
        FW_VERSION
    );

    page.replace(
        "{{IP_ADDRESS}}",
        getIpAddress()
    );

    page.replace(
        "{{WIFI_RSSI}}",
        WifiManager::isConnected()
            ? String(
                WifiManager::getRssi()
              )
            : "-"
    );

    page.replace(
        "{{NETWORK_MODE}}",
        getNetworkMode()
    );

    page.replace(
        "{{WIFI_SSID}}",
        htmlEscape(
            Settings::data.wifiSSID
        )
    );

    page.replace(
        "{{WIFI_DHCP_CHECKED}}",
        Settings::data.wifiDhcp
            ? "checked"
            : ""
    );

    page.replace(
        "{{WIFI_STATIC_IP}}",
        htmlEscape(
            Settings::data.wifiStaticIp
        )
    );

    page.replace(
        "{{WIFI_GATEWAY}}",
        htmlEscape(
            Settings::data.wifiGateway
        )
    );

    page.replace(
        "{{WIFI_SUBNET}}",
        htmlEscape(
            Settings::data.wifiSubnet
        )
    );

    page.replace(
        "{{WIFI_DNS_1}}",
        htmlEscape(
            Settings::data.wifiDns1
        )
    );

    page.replace(
        "{{WIFI_DNS_2}}",
        htmlEscape(
            Settings::data.wifiDns2
        )
    );

    page.replace(
        "{{CURRENT_SUBNET}}",
        WifiManager::isConnected()
            ? WifiManager::getSubnetMask()
            : "-"
    );

    page.replace(
        "{{CURRENT_GATEWAY}}",
        WifiManager::isConnected()
            ? WifiManager::getGatewayAddress()
            : "-"
    );

    page.replace(
        "{{CURRENT_DNS_1}}",
        WifiManager::isConnected()
            ? WifiManager::getDnsAddress(0)
            : "-"
    );

    page.replace(
        "{{CURRENT_DNS_2}}",
        WifiManager::isConnected()
            ? WifiManager::getDnsAddress(1)
            : "-"
    );

    page.replace(
        "{{WIFI_HOSTNAME}}",
        htmlEscape(
            WifiManager::getHostname()
        )
    );

    page.replace(
        "{{MDNS_NAME}}",
        htmlEscape(
            WifiManager::getMdnsName()
        )
    );

    page.replace(
        "{{MQTT_CHECKED}}",
        Settings::data.mqttEnabled
            ? "checked"
            : ""
    );

    page.replace(
        "{{MQTT_SERVER}}",
        htmlEscape(
            Settings::data.mqttServer
        )
    );

    page.replace(
        "{{MQTT_PORT}}",
        String(
            Settings::data.mqttPort
        )
    );

    page.replace(
        "{{MQTT_USER}}",
        htmlEscape(
            Settings::data.mqttUser
        )
    );

    page.replace(
        "{{TANK_HEIGHT}}",
        String(
            Settings::data.tankHeight,
            1
        )
    );


    page.replace(
        "{{SENSOR_CLEARANCE}}",
        String(
            Settings::data.sensorClearance,
            1
        )
    );


    uint32_t intervalDisplayValue =
        Settings::data.measureInterval;
    String intervalDisplayUnit = "seconds";

    if (
        Settings::data.measureInterval %
            3600UL == 0
    )
    {
        intervalDisplayValue =
            Settings::data.measureInterval /
            3600UL;
        intervalDisplayUnit = "hours";
    }
    else if (
        Settings::data.measureInterval %
            60UL == 0
    )
    {
        intervalDisplayValue =
            Settings::data.measureInterval /
            60UL;
        intervalDisplayUnit = "minutes";
    }

    page.replace(
        "{{MEASURE_INTERVAL}}",
        String(intervalDisplayValue)
    );

    page.replace(
        "{{INTERVAL_SECONDS_SELECTED}}",
        intervalDisplayUnit == "seconds"
            ? "selected"
            : ""
    );
    page.replace(
        "{{INTERVAL_MINUTES_SELECTED}}",
        intervalDisplayUnit == "minutes"
            ? "selected"
            : ""
    );
    page.replace(
        "{{INTERVAL_HOURS_SELECTED}}",
        intervalDisplayUnit == "hours"
            ? "selected"
            : ""
    );

    page.replace(
        "{{NTP_ENABLED_CHECKED}}",
        Settings::data.ntpEnabled
            ? "checked"
            : ""
    );
    page.replace(
        "{{TIME_ZONE}}",
        htmlEscape(Settings::data.timeZone)
    );
    page.replace(
        "{{NTP_SERVER_1}}",
        htmlEscape(Settings::data.ntpServer1)
    );
    page.replace(
        "{{NTP_SERVER_2}}",
        htmlEscape(Settings::data.ntpServer2)
    );
    page.replace(
        "{{NTP_SERVER_3}}",
        htmlEscape(Settings::data.ntpServer3)
    );
    page.replace(
        "{{NTP_TIMEOUT}}",
        String(Settings::data.ntpTimeoutSeconds)
    );

    page.replace(
        "{{BATTERY_EMPTY_VOLTAGE}}",
        String(Settings::data.batteryEmptyVoltage, 2)
    );
    page.replace(
        "{{BATTERY_FULL_VOLTAGE}}",
        String(Settings::data.batteryFullVoltage, 2)
    );
    page.replace(
        "{{BATTERY_CAPACITY_MAH}}",
        String(Settings::data.batteryCapacityMah)
    );
    page.replace(
        "{{BATTERY_CELL_COUNT}}",
        String(Settings::data.batteryCellCount)
    );
    page.replace(
        "{{CHEM_CUSTOM_SELECTED}}",
        Settings::data.batteryChemistry == "custom"
            ? "selected"
            : ""
    );
    page.replace(
        "{{CHEM_LIION_SELECTED}}",
        Settings::data.batteryChemistry == "li-ion"
            ? "selected"
            : ""
    );
    page.replace(
        "{{CHEM_LIFEPO4_SELECTED}}",
        Settings::data.batteryChemistry == "lifepo4"
            ? "selected"
            : ""
    );
    page.replace(
        "{{CHEM_NIMH_SELECTED}}",
        Settings::data.batteryChemistry == "nimh"
            ? "selected"
            : ""
    );
    page.replace(
        "{{BATTERY_CHEMISTRY}}",
        htmlEscape(Settings::data.batteryChemistry)
    );
    page.replace(
        "{{CURRENT_DEVICE_TIME}}",
        htmlEscape(TimeManager::formatCurrentTime())
    );
    page.replace(
        "{{TIME_SOURCE}}",
        TimeManager::getTimeSource()
    );
    page.replace(
        "{{LAST_MEASUREMENT}}",
        Sensor::isValid()
            ? htmlEscape(
                TimeManager::formatCurrentTime()
              )
            : "Unavailable"
    );

    page.replace(
        "{{DISTANCE_CM}}",
        Sensor::isValid()
            ? String(
                Sensor::getDistanceCm(),
                1
              )
            : "-"
    );

    page.replace(
        "{{WATER_LEVEL_CM}}",
        Sensor::isValid()
            ? String(
                Sensor::getWaterLevelCm(),
                1
              )
            : "-"
    );

    page.replace(
        "{{FILL_PERCENT}}",
        Sensor::isValid()
            ? String(
                Sensor::getPercentage()
              )
            : "-"
    );

    page.replace(
        "{{SENSOR_STATUS}}",
        getSensorStatusText()
    );

    page.replace(
        "{{SENSOR_STATUS_CLASS}}",
        getSensorStatusClass()
    );

    page.replace(
        "{{BATTERY_VOLTAGE}}",
        String(
            Battery::getVoltage(),
            2
        )
    );

page.replace(
    "{{BATTERY_PERCENT}}",
        Battery::isValid()
            ? String(Battery::getPercentage())
            : "-"
    );

page.replace(
    "{{BATTERY_STATUS}}",
    getBatteryStatusText()
);

page.replace(
    "{{BATTERY_PERCENT_DISPLAY}}",
    Battery::isValid()
        ? String(Battery::getPercentage()) + "%"
        : "Unavailable"
);

page.replace(
    "{{BATTERY_STATUS_CLASS}}",
    getBatteryStatusClass()
);

page.replace(
    "{{POWER_SOURCE}}",
    Battery::getPowerSourceText()
);

page.replace(
    "{{CURRENT_WIFI_SSID}}",
    WifiManager::isConnected()
        ? htmlEscape(
            WifiManager::getSsid()
          )
        : "-"
);

page.replace(
    "{{MQTT_STATUS}}",
    getMqttStatusText()
);

page.replace(
    "{{MQTT_STATUS_CLASS}}",
    getMqttStatusClass()
);

#if MQTT_ENABLED
page.replace(
    "{{MQTT_ENABLED_STATUS}}",
    Settings::data.mqttEnabled
        ? "Enabled"
        : "Disabled"
);

page.replace(
    "{{MQTT_DISCOVERY_STATUS}}",
    MqttManager::getDiscoveryStatusText()
);
#else
page.replace(
    "{{MQTT_ENABLED_STATUS}}",
    "Not available"
);

page.replace(
    "{{MQTT_DISCOVERY_STATUS}}",
    "Not available"
);
#endif

page.replace(
    "{{BATTERY_ESTIMATE}}",
    htmlEscape(
        BatteryEstimator::getDisplayText()
    )
);

page.replace(
    "{{BATTERY_SAMPLE_COUNT}}",
    String(
        BatteryEstimator::getSampleCount()
    )
);

page.replace(
    "{{BATTERY_LEARNING_HOURS}}",
    String(
        static_cast<float>(
            BatteryEstimator::
                getLearningSeconds()
        ) /
        3600.0f,
        1
    )
);

page.replace(
    "{{BATTERY_TEST_MODE_CHECKED}}",
    Settings::data.batteryEstimateTestMode
        ? "checked"
        : ""
);

page.replace(
    "{{BATTERY_TEST_MODE_STATUS}}",
    Settings::data.batteryEstimateTestMode
        ? "Test mode active"
        : "Normal mode"
);

}

String WebServerManager::getIpAddress()
{
    if (configPortalActive)
    {
        return WiFi.softAPIP().toString();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        return WiFi.localIP().toString();
    }

    return "Disconnected";
}

String WebServerManager::getNetworkMode()
{
    if (
        configPortalActive &&
        WiFi.status() == WL_CONNECTED
    )
    {
        return "Wi-Fi + configuration access point";
    }

    if (configPortalActive)
    {
        return "Configuration access point";
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        return "Wi-Fi connected";
    }

    return "Offline";
}

String WebServerManager::getSensorStatusText()
{
    if (Sensor::isValid())
    {
        return "OK";
    }

    return Sensor::hasMeasurementAttempted()
        ? "Sensor error"
        : "No measurement yet";
}

String WebServerManager::getSensorStatusClass()
{
    if (Sensor::isValid())
    {
        return "status-ok";
    }

    return Sensor::hasMeasurementAttempted()
        ? "status-error"
        : "status-warning";
}

String WebServerManager::getBatteryStatusText()
{
    const int percentage =
        Battery::getPercentage();

    if (!Battery::isValid())
    {
        return "Unavailable";
    }

    if (percentage < 5)
    {
        return "Critical";
    }

    if (percentage < 20)
    {
        return "Warning";
    }

    return "OK";
}

String WebServerManager::getBatteryStatusClass()
{
    const int percentage =
        Battery::getPercentage();

    if (!Battery::isValid())
    {
        return "status-neutral";
    }

    if (percentage < 5)
    {
        return "status-error";
    }

    if (percentage < 20)
    {
        return "status-warning";
    }

    return "status-ok";
}

String WebServerManager::getMqttStatusText()
{
#if MQTT_ENABLED
    if (!Settings::data.mqttEnabled)
    {
        return "Disabled";
    }

    return MqttManager::isConnected()
        ? "Connected"
        : "Disconnected";
#else
    return "Not available";
#endif
}

String WebServerManager::getMqttStatusClass()
{
#if MQTT_ENABLED
    if (!Settings::data.mqttEnabled)
    {
        return "status-neutral";
    }

    return MqttManager::isConnected()
        ? "status-ok"
        : "status-warning";
#else
    return "status-neutral";
#endif
}

String WebServerManager::htmlEscape(
    const String& value
)
{
    String escaped = value;

    escaped.replace(
        "&",
        "&amp;"
    );

    escaped.replace(
        "<",
        "&lt;"
    );

    escaped.replace(
        ">",
        "&gt;"
    );

    escaped.replace(
        "\"",
        "&quot;"
    );

    escaped.replace(
        "'",
        "&#39;"
    );

    return escaped;
}
