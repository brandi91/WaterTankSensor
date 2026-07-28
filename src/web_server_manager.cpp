#include "web_server_manager.h"

#include <WiFi.h>
#include <LittleFS.h>

#include "battery.h"
#include "config.h"
#include "led.h"
#include "logger.h"
#include "sensor.h"
#include "settings.h"
#include "sleep_manager.h"
#include "wifi_manager.h"

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
            server.arg("tankHeight").toFloat();

        if (tankHeight > 0.0f)
        {
            Settings::data.tankHeight =
                tankHeight;
        }
    }

    if (server.hasArg("measureInterval"))
    {
        const long interval =
            server.arg(
                "measureInterval"
            ).toInt();

        if (interval > 0)
        {
            Settings::data.measureInterval =
                static_cast<unsigned long>(
                    interval
                );
        }
    }

    Settings::save();

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
        Led::setColor(
            0,
            255,
            0
        );

        Logger::info(
            "Web measurement successful"
        );
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
            "LittleFS ist nicht verfügbar."
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
            "Webseite nicht gefunden: " +
            path
        );

        return;
    }

    page = processTemplate(page);

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
            "LittleFS ist nicht verfügbar."
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
            "Datei nicht gefunden."
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
            "Datei konnte nicht geöffnet werden."
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

String WebServerManager::processTemplate(
    String page
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
            static_cast<float>(
                Settings::data.tankHeight
            ),
            1
        )
    );

    page.replace(
        "{{MEASURE_INTERVAL}}",
        String(
            Settings::data.measureInterval
        )
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
        Sensor::isValid()
            ? "OK"
            : "Noch keine gültige Messung"
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
        String(
            Battery::getPercentage()
        )
    );

    page.replace(
        "{{BATTERY_STATUS}}",
        Battery::isCritical()
            ? "Kritisch"
            : Battery::isLow()
                ? "Niedrig"
                : "OK"
    );

    return page;
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

    return "Nicht verbunden";
}

String WebServerManager::getNetworkMode()
{
    if (
        configPortalActive &&
        WiFi.status() == WL_CONNECTED
    )
    {
        return "WLAN + Konfigurations-AP";
    }

    if (configPortalActive)
    {
        return "Konfigurations-AP";
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        return "WLAN";
    }

    return "Offline";
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