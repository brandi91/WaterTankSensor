#include "web_server_manager.h"

#include <WiFi.h>

#include "config.h"
#include "logger.h"
#include "settings.h"

// =====================================================
// Statische Variablen
// =====================================================

WebServer WebServerManager::server(WEB_SERVER_PORT);

bool WebServerManager::running = false;
bool WebServerManager::configPortalActive = false;

// =====================================================
// Webserver starten
// =====================================================

void WebServerManager::begin()
{
    if (running)
    {
        return;
    }

    registerRoutes();
    server.begin();

    running = true;
    configPortalActive = false;

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
    if (configPortalActive)
    {
        Logger::warning("Configuration portal already active");
        return;
    }

    Logger::info("Starting configuration portal");

    // Station und Access Point gleichzeitig aktivieren
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

    if (!running)
    {
        registerRoutes();
        server.begin();

        running = true;
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
// Webserver aktualisieren
// =====================================================

void WebServerManager::loop()
{
    if (!running)
    {
        return;
    }

    server.handleClient();
}

// =====================================================
// Webserver stoppen
// =====================================================

void WebServerManager::stop()
{
    if (!running)
    {
        return;
    }

    server.stop();

    if (configPortalActive)
    {
        WiFi.softAPdisconnect(true);
    }

    running = false;
    configPortalActive = false;

    Logger::info("Webserver stopped");
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
// Webserver-Routen
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

    // Browser fragt häufig automatisch nach einem Favicon
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

    // Typische Captive-Portal-Anfragen
    server.on(
        "/generate_204",
        HTTP_ANY,
        handleRoot
    );

    server.on(
        "/hotspot-detect.html",
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

    server.onNotFound(handleNotFound);
}

// =====================================================
// Hauptseite anzeigen
// =====================================================

void WebServerManager::handleRoot()
{
    // Verhindert, dass der Browser alte Werte cached
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
        "text/html; charset=utf-8",
        buildPage()
    );
}

// =====================================================
// Einstellungen speichern
// =====================================================

void WebServerManager::handleSave()
{
    if (!server.hasArg("wifiSSID"))
    {
        server.send(
            400,
            "text/plain; charset=utf-8",
            "WLAN-Name fehlt"
        );

        return;
    }

    // WLAN-Name
    Settings::data.wifiSSID =
        server.arg("wifiSSID");

    // Leeres Passwort bedeutet:
    // bestehendes Passwort nicht überschreiben
    const String newPassword =
        server.arg("wifiPassword");

    if (!newPassword.isEmpty())
    {
        Settings::data.wifiPassword =
            newPassword;
    }

    // Gerätename
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

    // Tankhöhe
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

    // Messintervall
    if (server.hasArg("measureInterval"))
    {
        const unsigned long interval =
            server.arg("measureInterval").toInt();

        if (interval > 0)
        {
            Settings::data.measureInterval =
                interval;
        }
    }

    Settings::save();

    Logger::info("Configuration saved");

    Logger::info(
        "Saved tank height: " +
        String(
            static_cast<float>(
                Settings::data.tankHeight
            ),
            1
        )
    );

    const String response = R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">

    <meta
        name="viewport"
        content="width=device-width, initial-scale=1">

    <meta
        http-equiv="refresh"
        content="15; url=/">

    <title>Konfiguration gespeichert</title>

    <style>
        * {
            box-sizing: border-box;
        }

        body {
            font-family: Arial, sans-serif;
            background: #f1f5f9;
            color: #1e293b;
            margin: 0;
            padding: 24px;
        }

        .card {
            max-width: 520px;
            margin: 40px auto;
            padding: 28px;
            background: white;
            border-radius: 14px;
            box-shadow: 0 4px 18px rgba(0, 0, 0, 0.10);
        }

        h1 {
            margin-top: 0;
        }

        .success {
            padding: 12px;
            margin: 18px 0;
            border: 1px solid #86efac;
            border-radius: 8px;
            background: #f0fdf4;
            color: #166534;
        }

        .warning {
            padding: 12px;
            margin-top: 18px;
            border: 1px solid #fdba74;
            border-radius: 8px;
            background: #fff7ed;
            color: #9a3412;
        }

        .countdown {
            margin-top: 22px;
            font-size: 24px;
            font-weight: bold;
        }

        a {
            color: #2563eb;
        }
    </style>
</head>

<body>
    <div class="card">
        <h1>Konfiguration gespeichert</h1>

        <div class="success">
            Die Einstellungen wurden dauerhaft gespeichert.
        </div>

        <div class="warning">
            Änderungen am WLAN-Namen oder WLAN-Passwort
            werden erst nach einem Neustart des ESP32 aktiv.
        </div>

        <p>
            Du wirst in 15 Sekunden automatisch zur
            Hauptseite zurückgeleitet.
        </p>

        <div class="countdown">
            <span id="seconds">15</span> Sekunden
        </div>

        <p>
            <a href="/">Jetzt zur Hauptseite</a>
        </p>
    </div>

    <script>
        let seconds = 15;
        const output = document.getElementById("seconds");

        const timer = setInterval(() => {
            seconds--;

            if (seconds >= 0) {
                output.textContent = seconds;
            }

            if (seconds <= 0) {
                clearInterval(timer);
            }
        }, 1000);
    </script>
</body>
</html>
)HTML";

    server.send(
        200,
        "text/html; charset=utf-8",
        response
    );

    // Kein automatischer Neustart
}

// =====================================================
// Neustart über Weboberfläche
// =====================================================

void WebServerManager::handleRestart()
{
    Logger::warning(
        "Restart requested from web interface"
    );

    const String response = R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">

    <meta
        name="viewport"
        content="width=device-width, initial-scale=1">

    <title>ESP32 Neustart</title>

    <style>
        * {
            box-sizing: border-box;
        }

        body {
            font-family: Arial, sans-serif;
            background: #f1f5f9;
            color: #1e293b;
            margin: 0;
            padding: 24px;
        }

        .card {
            max-width: 520px;
            margin: 40px auto;
            padding: 28px;
            background: white;
            border-radius: 14px;
            box-shadow: 0 4px 18px rgba(0, 0, 0, 0.10);
        }

        h1 {
            margin-top: 0;
        }

        .info {
            padding: 12px;
            margin-top: 18px;
            border: 1px solid #93c5fd;
            border-radius: 8px;
            background: #eff6ff;
            color: #1e40af;
        }
    </style>
</head>

<body>
    <div class="card">
        <h1>ESP32 wird neu gestartet</h1>

        <div class="info">
            Bitte warte einige Sekunden und verbinde dich
            anschließend erneut mit dem Gerät.
        </div>
    </div>
</body>
</html>
)HTML";

    server.send(
        200,
        "text/html; charset=utf-8",
        response
    );

    delay(1000);

    ESP.restart();
}

// =====================================================
// Unbekannte URL
// =====================================================

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

// =====================================================
// HTML-Hauptseite bauen
// =====================================================

String WebServerManager::buildPage()
{
    Logger::info(
        "Building webpage, tank height: " +
        String(
            static_cast<float>(
                Settings::data.tankHeight
            ),
            1
        )
    );

    const String deviceName =
        htmlEscape(Settings::data.deviceName);

    const String wifiSSID =
        htmlEscape(Settings::data.wifiSSID);

    String page;

    page.reserve(8000);

    page += R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">

    <meta
        name="viewport"
        content="width=device-width, initial-scale=1">

    <title>Water Tank Sensor</title>

    <style>
        * {
            box-sizing: border-box;
        }

        body {
            font-family: Arial, sans-serif;
            background: #f1f5f9;
            color: #1e293b;
            margin: 0;
            padding: 20px;
        }

        .container {
            max-width: 620px;
            margin: 0 auto;
        }

        .header {
            margin-bottom: 20px;
        }

        .header h1 {
            margin-bottom: 6px;
        }

        .header p {
            margin-top: 0;
            color: #64748b;
        }

        .card {
            background: white;
            padding: 22px;
            margin-bottom: 18px;
            border-radius: 14px;
            box-shadow: 0 3px 14px rgba(0, 0, 0, 0.08);
        }

        h2 {
            margin-top: 0;
            font-size: 20px;
        }

        label {
            display: block;
            margin-top: 16px;
            margin-bottom: 6px;
            font-weight: bold;
        }

        input {
            width: 100%;
            padding: 12px;
            border: 1px solid #cbd5e1;
            border-radius: 8px;
            font-size: 16px;
        }

        small {
            display: block;
            margin-top: 5px;
            color: #64748b;
        }

        button {
            width: 100%;
            padding: 14px;
            border: none;
            border-radius: 9px;
            background: #2563eb;
            color: white;
            font-size: 17px;
            font-weight: bold;
            cursor: pointer;
        }

        button:hover {
            background: #1d4ed8;
        }

        .status {
            padding: 12px;
            border-radius: 8px;
            background: #e0f2fe;
            line-height: 1.7;
        }

        .warning {
            margin-top: 16px;
            padding: 12px;
            border: 1px solid #fdba74;
            border-radius: 8px;
            background: #fff7ed;
            color: #9a3412;
            line-height: 1.4;
        }

        .restart-button {
            background: #dc2626;
        }

        .restart-button:hover {
            background: #b91c1c;
        }

        .footer {
            margin-top: 24px;
            color: #64748b;
            font-size: 13px;
            text-align: center;
        }
    </style>
</head>

<body>
<div class="container">

    <div class="header">
        <h1>Water Tank Sensor</h1>
        <p>Gerätekonfiguration</p>
    </div>

    <div class="card">
        <h2>Status</h2>

        <div class="status">
            Firmware: )HTML";

    page += FW_VERSION;

    page += R"HTML(<br>
            IP-Adresse: )HTML";

    if (configPortalActive)
    {
        page += WiFi.softAPIP().toString();
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        page += WiFi.localIP().toString();
    }
    else
    {
        page += "Nicht verbunden";
    }

    page += R"HTML(
        </div>
    </div>

    <form method="POST" action="/save">

        <div class="card">
            <h2>Gerät</h2>

            <label for="deviceName">
                Gerätename
            </label>

            <input
                id="deviceName"
                name="deviceName"
                type="text"
                value=")HTML";

    page += deviceName;

    page += R"HTML("
                required>
        </div>

        <div class="card">
            <h2>WLAN</h2>

            <label for="wifiSSID">
                WLAN-Name
            </label>

            <input
                id="wifiSSID"
                name="wifiSSID"
                type="text"
                value=")HTML";

    page += wifiSSID;

    page += R"HTML("
                required>

            <label for="wifiPassword">
                WLAN-Passwort
            </label>

            <input
                id="wifiPassword"
                name="wifiPassword"
                type="password"
                placeholder="Unverändert lassen">

            <small>
                Leer lassen, um das gespeicherte Passwort
                beizubehalten.
            </small>

            <div class="warning">
                Änderungen am WLAN-Namen oder WLAN-Passwort
                werden erst nach einem Neustart des ESP32 aktiv.
            </div>
        </div>

        <div class="card">
            <h2>Tank</h2>

            <label for="tankHeight">
                Tankhöhe in cm
            </label>

            <input
                id="tankHeight"
                name="tankHeight"
                type="number"
                min="1"
                step="0.1"
                value=")HTML";

    page += String(
        static_cast<float>(
            Settings::data.tankHeight
        ),
        1
    );

    page += R"HTML("
                required>

            <label for="measureInterval">
                Messintervall in Sekunden
            </label>

            <input
                id="measureInterval"
                name="measureInterval"
                type="number"
                min="1"
                value=")HTML";

    page += String(
        Settings::data.measureInterval
    );

    page += R"HTML("
                required>
        </div>

        <button type="submit">
            Einstellungen speichern
        </button>

    </form>

    <div class="card" style="margin-top: 18px;">
        <h2>System</h2>

        <p>
            Starte den ESP32 neu, um geänderte
            WLAN-Einstellungen zu übernehmen.
        </p>

        <form
            method="POST"
            action="/restart"
            onsubmit="return confirm('ESP32 wirklich neu starten?');">

            <button
                type="submit"
                class="restart-button">

                ESP32 neu starten
            </button>
        </form>
    </div>

    <div class="footer">
        Water Tank Sensor · Firmware )HTML";

    page += FW_VERSION;

    page += R"HTML(
    </div>

</div>
</body>
</html>
)HTML";

    return page;
}

// =====================================================
// HTML-Sonderzeichen absichern
// =====================================================

String WebServerManager::htmlEscape(
    const String& value
)
{
    String escaped = value;

    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");

    return escaped;
}