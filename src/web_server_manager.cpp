#include "web_server_manager.h"

#include <WiFi.h>
#include <LittleFS.h>

#include "config.h"
#include "logger.h"
#include "settings.h"
#include "wifi_manager.h"

// =====================================================
// Statische Variablen
// =====================================================

WebServer WebServerManager::server(
    WEB_SERVER_PORT
);

bool WebServerManager::running = false;
bool WebServerManager::configPortalActive = false;
bool WebServerManager::filesystemReady = false;
bool WebServerManager::routesRegistered = false;

bool WebServerManager::restartPending = false;
unsigned long WebServerManager::restartAt = 0;

// =====================================================
// Webserver initialisieren
// =====================================================

void WebServerManager::begin()
{
    if (!filesystemReady)
    {
        /*
         * true bedeutet:
         * Falls LittleFS nicht gemountet werden kann,
         * wird es automatisch formatiert.
         */
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

// =====================================================
// Konfigurations-Access-Point starten
// =====================================================

void WebServerManager::beginConfigPortal()
{
    /*
     * Sicherstellen, dass LittleFS und der
     * Webserver bereits initialisiert sind.
     */
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

    /*
     * Station und Access Point gleichzeitig.
     * Dadurch bleibt die Verbindung zum normalen
     * WLAN bestehen, während der Setup-AP läuft.
     */
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

// =====================================================
// Webserver regelmäßig verarbeiten
// =====================================================

void WebServerManager::loop()
{
    if (running)
    {
        server.handleClient();
    }

    /*
     * Verzögerter Neustart:
     * So kann der Browser die Neustartseite und
     * das CSS noch vollständig herunterladen.
     */
    if (
        restartPending &&
        static_cast<long>(
            millis() - restartAt
        ) >= 0
    )
    {
        restartPending = false;

        Logger::warning(
            "Restarting ESP32 now"
        );

        Serial.flush();

        ESP.restart();
    }
}

// =====================================================
// Webserver stoppen
// =====================================================

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

// =====================================================
// Status
// =====================================================

bool WebServerManager::isRunning()
{
    return running;
}

bool WebServerManager::isConfigPortalActive()
{
    return configPortalActive;
}

// =====================================================
// Routen registrieren
// =====================================================

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

    /*
     * Typische Captive-Portal-Anfragen von
     * Android, Apple und Windows.
     */
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

// =====================================================
// Hauptseite
// =====================================================

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

// =====================================================
// Einstellungen speichern
// =====================================================

void WebServerManager::handleSave()
{
    // -------------------------------------------------
    // Gerätename
    // -------------------------------------------------

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

    // -------------------------------------------------
    // WLAN
    // -------------------------------------------------

    if (server.hasArg("wifiSSID"))
    {
        Settings::data.wifiSSID =
            server.arg("wifiSSID");
    }

    /*
     * Leeres Passwort bedeutet:
     * bestehendes Passwort nicht überschreiben.
     */
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

    // -------------------------------------------------
    // MQTT
    // -------------------------------------------------

    /*
     * Eine nicht ausgewählte Checkbox wird vom
     * Browser gar nicht übertragen.
     */
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

    // -------------------------------------------------
    // Tankhöhe
    // -------------------------------------------------

    if (server.hasArg("tankHeight"))
    {
        const float tankHeight =
            server.arg("tankHeight").toFloat();

        if (tankHeight > 0.0f)
        {
            Settings::data.tankHeight =
                tankHeight;
        }
        else
        {
            Logger::warning(
                "Invalid tank height received"
            );
        }
    }

    // -------------------------------------------------
    // Messintervall
    // -------------------------------------------------

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
        else
        {
            Logger::warning(
                "Invalid measurement interval received"
            );
        }
    }

    // -------------------------------------------------
    // Dauerhaft speichern
    // -------------------------------------------------

    Settings::save();

    Logger::info(
        "Configuration saved"
    );

    Logger::info(
        "Tank height: " +
        String(
            Settings::data.tankHeight,
            1
        ) +
        " cm"
    );

    Logger::info(
        "Measurement interval: " +
        String(
            Settings::data.measureInterval
        ) +
        " seconds"
    );

    sendTemplate(
        "/saved.html"
    );
}

// =====================================================
// Neustart
// =====================================================

void WebServerManager::handleRestart()
{
    Logger::warning(
        "Restart requested from web interface"
    );

    sendTemplate(
        "/restart.html"
    );

    /*
     * Neustart erst nach drei Sekunden.
     * Der Browser kann dadurch vorher noch HTML
     * und style.css herunterladen.
     */
    restartPending = true;
    restartAt = millis() + 3000UL;
}

// =====================================================
// Nicht gefundene Route
// =====================================================

void WebServerManager::handleNotFound()
{
    Logger::warning(
        "Unknown request: " +
        server.uri()
    );

    /*
     * Im Konfigurationsmodus alle unbekannten
     * URLs zur Hauptseite umleiten.
     */
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

// =====================================================
// Template-Datei senden
// =====================================================

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

// =====================================================
// Statische Datei senden
// =====================================================

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

// =====================================================
// Datei aus LittleFS laden
// =====================================================

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

// =====================================================
// Platzhalter ersetzen
// =====================================================

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
    String(Settings::data.tankHeight, 1)
);

    page.replace(
        "{{MEASURE_INTERVAL}}",
        String(
            Settings::data.measureInterval
        )
    );
Logger::info(
    "Template tank height: " +
    String(Settings::data.tankHeight, 1)
);

Logger::info(
    String("Tank placeholder remains: ") +
    (page.indexOf("{{TANK_HEIGHT}}") >= 0 ? "yes" : "no")
);
    return page;
}

// =====================================================
// Aktuelle IP-Adresse
// =====================================================

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

// =====================================================
// Aktueller Netzwerkmodus
// =====================================================

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

// =====================================================
// HTML-Sonderzeichen absichern
// =====================================================

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