#include "web_server_manager.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <Update.h>
#include <math.h>
#include <esp_system.h>

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

namespace
{
bool parseIpv4(const String& text, IPAddress& address)
{
    return !text.isEmpty() && address.fromString(text);
}

bool isValidSubnet(const IPAddress& address)
{
    uint32_t mask = 0;
    for (uint8_t index = 0; index < 4; ++index)
    {
        mask = (mask << 8) | address[index];
    }
    const uint32_t inverse = ~mask;
    return mask != 0 && (inverse & (inverse + 1U)) == 0;
}

bool validateNetworkSettings(
    SettingsData& candidate,
    String& error
)
{
    if (candidate.wifiDhcp)
    {
        return true;
    }

    IPAddress staticIp;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns;

    if (!parseIpv4(candidate.wifiStaticIp, staticIp))
    {
        error = "Manual Static IP must be a valid IPv4 address.";
        return false;
    }
    if (!parseIpv4(candidate.wifiGateway, gateway))
    {
        error = "Manual Gateway must be a valid IPv4 address.";
        return false;
    }
    if (
        !parseIpv4(candidate.wifiSubnet, subnet) ||
        !isValidSubnet(subnet)
    )
    {
        error = "Manual Subnet Mask must be a valid contiguous subnet mask.";
        return false;
    }
    if (!candidate.wifiDns1.isEmpty() && !parseIpv4(candidate.wifiDns1, dns))
    {
        error = "DNS 1 must be empty or a valid IPv4 address.";
        return false;
    }
    if (!candidate.wifiDns2.isEmpty() && !parseIpv4(candidate.wifiDns2, dns))
    {
        error = "DNS 2 must be empty or a valid IPv4 address.";
        return false;
    }

    if (candidate.wifiDns1.isEmpty() && !candidate.wifiDns2.isEmpty())
    {
        candidate.wifiDns1 = candidate.wifiDns2;
        candidate.wifiDns2 = "";
    }
    return true;
}

bool validateApSettings(
    SettingsData& candidate,
    String& error
)
{
    if (candidate.apSsid.isEmpty() || candidate.apSsid.length() > 32)
    {
        error = "Access point SSID must contain 1 to 32 characters.";
        return false;
    }
    if (
        !candidate.apPassword.isEmpty() &&
        (
            candidate.apPassword.length() < 8 ||
            candidate.apPassword.length() > 63
        )
    )
    {
        error = "Access point password must contain 8 to 63 characters.";
        return false;
    }

    if (candidate.apGateway.isEmpty())
    {
        candidate.apGateway = candidate.apIp;
    }

    IPAddress apIp;
    IPAddress gateway;
    IPAddress subnet;
    if (
        !parseIpv4(candidate.apIp, apIp) ||
        apIp == IPAddress(0, 0, 0, 0)
    )
    {
        error = "Access point IP must be a valid non-zero IPv4 address.";
        return false;
    }
    if (!parseIpv4(candidate.apGateway, gateway))
    {
        error = "Access point Gateway must be a valid IPv4 address.";
        return false;
    }
    if (
        !parseIpv4(candidate.apSubnet, subnet) ||
        !isValidSubnet(subnet)
    )
    {
        error = "Access point Subnet Mask must be a valid contiguous subnet mask.";
        return false;
    }
    return true;
}

SettingsData defaultApSettings()
{
    SettingsData defaults;
    defaults.apSsid = CONFIG_AP_SSID;
    defaults.apPassword = CONFIG_AP_PASSWORD;
    defaults.apIp = CONFIG_AP_IP;
    defaults.apGateway = CONFIG_AP_GATEWAY;
    defaults.apSubnet = CONFIG_AP_SUBNET;
    return defaults;
}
}

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
 * Timestamp of the latest battery-learning sample initiated from the web UI.
 */
unsigned long
    WebServerManager::lastBatteryEstimatorSampleAt = 0;
bool WebServerManager::sessionActive = false;
String WebServerManager::sessionToken;
unsigned long WebServerManager::sessionLastActiveAt = 0;
bool WebServerManager::firmwareUploadAuthorized = false;
bool WebServerManager::firmwareUploadSuccessful = false;
String WebServerManager::firmwareUploadError;

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

    Logger::info("Web server started");

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

    SettingsData apSettings = Settings::data;
    String apError;
    if (!validateApSettings(apSettings, apError))
    {
        Logger::warning(
            "Invalid saved access point settings; using compile-time defaults"
        );
        apSettings = defaultApSettings();
    }

    IPAddress apIp;
    IPAddress apGateway;
    IPAddress apSubnet;
    apIp.fromString(apSettings.apIp);
    apGateway.fromString(apSettings.apGateway);
    apSubnet.fromString(apSettings.apSubnet);

    if (!WiFi.softAPConfig(apIp, apGateway, apSubnet))
    {
        Logger::warning(
            "Failed to configure access point network; using framework defaults"
        );
    }

    const bool started = apSettings.apPassword.isEmpty()
        ? WiFi.softAP(apSettings.apSsid.c_str())
        : WiFi.softAP(
            apSettings.apSsid.c_str(),
            apSettings.apPassword.c_str()
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
        apSettings.apSsid
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
            "Web server stopped"
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

void WebServerManager::invalidateAuthenticationSession()
{
    invalidateSession();
}

void WebServerManager::registerRoutes()
{
    const char* collectedHeaders[] = {"Cookie"};
    server.collectHeaders(collectedHeaders, 1);

    server.on(
        "/login",
        HTTP_GET,
        handleLoginGet
    );

    server.on(
        "/login",
        HTTP_POST,
        handleLoginPost
    );

    server.on(
        "/logout",
        HTTP_POST,
        handleLogout
    );

    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    server.on(
        "/info",
        HTTP_GET,
        handleInfo
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
        "/save-pins",
        HTTP_POST,
        handleSavePins
    );

    server.on(
        "/restore-default-pins",
        HTTP_POST,
        handleRestoreDefaultPins
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
        "/firmware",
        HTTP_GET,
        handleFirmwarePage
    );

    server.on(
        "/firmware",
        HTTP_POST,
        handleFirmwareUploadComplete,
        handleFirmwareUpload
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

void WebServerManager::handleLoginGet()
{
    const String redirectTarget = safeRedirect(
        server.hasArg("redirect") ? server.arg("redirect") : "/"
    );

    if (
        !Settings::data.webLoginEnabled ||
        configPortalActive ||
        hasValidSession()
    )
    {
        server.sendHeader("Location", redirectTarget, true);
        server.send(302, "text/plain", "");
        return;
    }

    sendLoginPage("", redirectTarget);
}

void WebServerManager::handleLoginPost()
{
    const String redirectTarget = safeRedirect(server.arg("redirect"));

    if (!Settings::data.webLoginEnabled || configPortalActive)
    {
        server.sendHeader("Location", redirectTarget, true);
        server.send(302, "text/plain", "");
        return;
    }

    const bool usernameValid = constantTimeEquals(
        server.arg("username"),
        Settings::data.webUsername
    );
    const bool passwordValid = constantTimeEquals(
        server.arg("password"),
        Settings::data.webPassword
    );
    const bool valid = usernameValid & passwordValid;

    if (!valid)
    {
        // Deliberately generic: never identify which credential was wrong.
        sendLoginPage(
            "Invalid username or password.",
            redirectTarget
        );
        return;
    }

    invalidateSession();
    sessionToken = createSessionToken();
    sessionActive = !sessionToken.isEmpty();
    sessionLastActiveAt = millis();

    if (!sessionActive)
    {
        server.send(
            500,
            "text/plain; charset=utf-8",
            "Unable to create a web session."
        );
        return;
    }

    server.sendHeader(
        "Set-Cookie",
        "WTSSESSION=" + sessionToken +
            "; Path=/; HttpOnly; SameSite=Strict"
    );
    server.sendHeader("Location", redirectTarget, true);
    server.send(302, "text/plain", "");
}

void WebServerManager::handleLogout()
{
    invalidateSession();
    server.sendHeader(
        "Set-Cookie",
        "WTSSESSION=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0"
    );
    server.sendHeader("Location", "/login", true);
    server.send(302, "text/plain", "");
}

bool WebServerManager::requireAuthentication(const bool apiRequest)
{
    if (
        !Settings::data.webLoginEnabled ||
        configPortalActive
    )
    {
        /*
         * Recovery/configuration AP intentionally bypasses web login so a
         * forgotten credential can never disable physical GPIO 33 recovery.
         */
        return true;
    }

    if (hasValidSession())
    {
        return true;
    }

    if (apiRequest || server.method() != HTTP_GET)
    {
        if (apiRequest)
        {
            server.send(
                401,
                "application/json; charset=utf-8",
                "{\"error\":\"authentication required\"}"
            );
        }
        else
        {
            server.send(
                401,
                "text/plain; charset=utf-8",
                "Authentication required."
            );
        }
        return false;
    }

    const String target = safeRedirect(server.uri());
    server.sendHeader(
        "Location",
        "/login?redirect=" + urlEncode(target),
        true
    );
    server.send(302, "text/plain", "");
    return false;
}

bool WebServerManager::hasValidSession(const bool refreshActivity)
{
    if (!sessionActive || sessionToken.isEmpty())
    {
        return false;
    }

    const uint32_t timeoutMs =
        static_cast<uint32_t>(
            Settings::data.webSessionTimeoutMinutes
        ) * 60000UL;
    if (millis() - sessionLastActiveAt >= timeoutMs)
    {
        invalidateSession();
        return false;
    }

    const String cookieHeader = server.header("Cookie");
    const String cookiePrefix = "WTSSESSION=";
    int start = cookieHeader.indexOf(cookiePrefix);
    if (start < 0)
    {
        return false;
    }
    start += cookiePrefix.length();
    int end = cookieHeader.indexOf(';', start);
    if (end < 0)
    {
        end = cookieHeader.length();
    }

    const String submitted = cookieHeader.substring(start, end);
    if (!constantTimeEquals(submitted, sessionToken))
    {
        return false;
    }

    if (refreshActivity)
    {
        sessionLastActiveAt = millis();
    }
    return true;
}

void WebServerManager::invalidateSession()
{
    sessionActive = false;
    sessionToken = "";
    sessionLastActiveAt = 0;
}

String WebServerManager::createSessionToken()
{
    static const char hex[] = "0123456789abcdef";
    String token;
    token.reserve(64);
    for (uint8_t index = 0; index < 8; ++index)
    {
        const uint32_t randomValue = esp_random();
        for (int8_t shift = 28; shift >= 0; shift -= 4)
        {
            token += hex[(randomValue >> shift) & 0x0f];
        }
    }
    return token;
}

bool WebServerManager::constantTimeEquals(
    const String& left,
    const String& right
)
{
    const size_t maximumLength = max(left.length(), right.length());
    size_t difference = left.length() ^ right.length();
    for (size_t index = 0; index < maximumLength; ++index)
    {
        const uint8_t leftByte =
            index < left.length() ? left[index] : 0;
        const uint8_t rightByte =
            index < right.length() ? right[index] : 0;
        difference |= leftByte ^ rightByte;
    }
    return difference == 0;
}

String WebServerManager::safeRedirect(const String& requested)
{
    if (
        requested.isEmpty() ||
        requested.length() > 128 ||
        requested[0] != '/' ||
        requested.startsWith("//") ||
        requested.indexOf('\\') >= 0 ||
        requested.indexOf("://") >= 0 ||
        requested.indexOf('\r') >= 0 ||
        requested.indexOf('\n') >= 0
    )
    {
        return "/";
    }
    return requested;
}

String WebServerManager::urlEncode(const String& value)
{
    static const char hex[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 3);
    for (size_t index = 0; index < value.length(); ++index)
    {
        const uint8_t character = value[index];
        if (
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '-' ||
            character == '_' ||
            character == '.' ||
            character == '~'
        )
        {
            encoded += static_cast<char>(character);
        }
        else
        {
            encoded += '%';
            encoded += hex[character >> 4];
            encoded += hex[character & 0x0f];
        }
    }
    return encoded;
}

void WebServerManager::sendLoginPage(
    const String& error,
    const String& redirectTarget
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

    String page = loadFile("/login.html");
    if (page.isEmpty())
    {
        server.send(
            404,
            "text/plain; charset=utf-8",
            "Login page not found."
        );
        return;
    }

    page.replace(
        "{{DEVICE_NAME}}",
        htmlEscape(Settings::data.deviceName)
    );
    page.replace("{{LOGIN_ERROR}}", htmlEscape(error));
    page.replace(
        "{{LOGIN_REDIRECT}}",
        htmlEscape(safeRedirect(redirectTarget))
    );
    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );
    server.send(200, "text/html; charset=utf-8", page);
}

void WebServerManager::handleRoot()
{
    if (!requireAuthentication())
    {
        return;
    }
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
    if (!requireAuthentication())
    {
        return;
    }
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

void WebServerManager::handleInfo()
{
    if (!requireAuthentication())
    {
        return;
    }
    sendTemplate(
        "/info.html"
    );
}

void WebServerManager::handleHistoryApi()
{
    if (!requireAuthentication(true))
    {
        return;
    }
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
    if (!requireAuthentication(true))
    {
        return;
    }
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
    if (!requireAuthentication(true))
    {
        return;
    }
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
    if (!requireAuthentication())
    {
        return;
    }
    if (server.hasArg("buttonPin"))
    {
        server.send(
            400,
            "text/plain; charset=utf-8",
            "Pin settings must be saved from the Device Info page."
        );
        return;
    }

    SettingsData loginCandidate = Settings::data;
    loginCandidate.webLoginEnabled =
        server.hasArg("webLoginEnabled");
    const String submittedWebPassword =
        server.arg("webPassword");

    if (loginCandidate.webLoginEnabled)
    {
        loginCandidate.webUsername =
            server.arg("webUsername");
        loginCandidate.webUsername.trim();

        if (
            loginCandidate.webUsername.isEmpty() ||
            loginCandidate.webUsername.length() > 32
        )
        {
            server.send(
                400,
                "text/plain; charset=utf-8",
                "Web username must contain 1 to 32 characters."
            );
            return;
        }

        if (!submittedWebPassword.isEmpty())
        {
            if (
                submittedWebPassword.length() < 4 ||
                submittedWebPassword.length() > 64
            )
            {
                server.send(
                    400,
                    "text/plain; charset=utf-8",
                    "Web password must contain 4 to 64 characters."
                );
                return;
            }
            loginCandidate.webPassword = submittedWebPassword;
        }

        if (
            loginCandidate.webPassword.length() < 4 ||
            loginCandidate.webPassword.length() > 64
        )
        {
            server.send(
                400,
                "text/plain; charset=utf-8",
                "Set a valid web password before enabling login."
            );
            return;
        }

        const String timeoutText =
            server.arg("webSessionTimeoutMinutes");
        bool timeoutValid = !timeoutText.isEmpty();
        for (
            size_t index = 0;
            index < timeoutText.length() && timeoutValid;
            ++index
        )
        {
            timeoutValid =
                timeoutText[index] >= '0' &&
                timeoutText[index] <= '9';
        }
        const unsigned long timeout =
            timeoutValid ? timeoutText.toInt() : 0;
        if (timeout < 1 || timeout > 1440)
        {
            server.send(
                400,
                "text/plain; charset=utf-8",
                "Web session timeout must be between 1 and 1440 minutes."
            );
            return;
        }
        loginCandidate.webSessionTimeoutMinutes =
            static_cast<uint16_t>(timeout);
    }

    SettingsData networkCandidate = Settings::data;
    if (server.hasArg("networkMode"))
    {
        const String networkMode = server.arg("networkMode");
        if (networkMode != "dhcp" && networkMode != "manual")
        {
            server.send(400, "text/plain; charset=utf-8", "Invalid network mode.");
            return;
        }
        networkCandidate.wifiDhcp = networkMode == "dhcp";
    }
    networkCandidate.wifiStaticIp = server.arg("wifiStaticIp");
    networkCandidate.wifiGateway = server.arg("wifiGateway");
    networkCandidate.wifiSubnet = server.arg("wifiSubnet");
    networkCandidate.wifiDns1 = server.arg("wifiDns1");
    networkCandidate.wifiDns2 = server.arg("wifiDns2");

    networkCandidate.apSsid = server.arg("apSsid");
    networkCandidate.apIp = server.arg("apIp");
    networkCandidate.apGateway = server.arg("apGateway");
    networkCandidate.apSubnet = server.arg("apSubnet");

    const bool openAccessPoint = server.hasArg("apOpen");
    const String submittedApPassword = server.arg("apPassword");
    if (openAccessPoint)
    {
        networkCandidate.apPassword = "";
    }
    else if (!submittedApPassword.isEmpty())
    {
        networkCandidate.apPassword = submittedApPassword;
    }
    else if (networkCandidate.apPassword.isEmpty())
    {
        server.send(
            400,
            "text/plain; charset=utf-8",
            "Enter an access point password or explicitly select open access point."
        );
        return;
    }

    String networkError;
    if (!validateNetworkSettings(networkCandidate, networkError))
    {
        Logger::warning("Network configuration rejected");
        server.send(400, "text/plain; charset=utf-8", networkError);
        return;
    }
    if (!validateApSettings(networkCandidate, networkError))
    {
        Logger::warning("Access point configuration rejected");
        server.send(400, "text/plain; charset=utf-8", networkError);
        return;
    }

    Settings::data.wifiDhcp = networkCandidate.wifiDhcp;
    Settings::data.wifiStaticIp = networkCandidate.wifiStaticIp;
    Settings::data.wifiGateway = networkCandidate.wifiGateway;
    Settings::data.wifiSubnet = networkCandidate.wifiSubnet;
    Settings::data.wifiDns1 = networkCandidate.wifiDns1;
    Settings::data.wifiDns2 = networkCandidate.wifiDns2;
    Settings::data.apSsid = networkCandidate.apSsid;
    Settings::data.apPassword = networkCandidate.apPassword;
    Settings::data.apIp = networkCandidate.apIp;
    Settings::data.apGateway = networkCandidate.apGateway;
    Settings::data.apSubnet = networkCandidate.apSubnet;

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
     * Sensor clearance is the air gap between the sensor and the maximum
     * water level.
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

    Settings::data.deepSleepEnabled =
        server.hasArg("deepSleepEnabled");

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
 * Browsers omit unchecked checkbox fields from form submissions.
 */
Settings::data.batteryEstimateTestMode =
    server.hasArg(
        "batteryEstimateTestMode"
    );

const bool webCredentialsChanged =
    Settings::data.webUsername != loginCandidate.webUsername ||
    Settings::data.webPassword != loginCandidate.webPassword;
const bool webLoginDisabled =
    Settings::data.webLoginEnabled &&
    !loginCandidate.webLoginEnabled;
Settings::data.webLoginEnabled =
    loginCandidate.webLoginEnabled;
Settings::data.webUsername =
    loginCandidate.webUsername;
Settings::data.webPassword =
    loginCandidate.webPassword;
Settings::data.webSessionTimeoutMinutes =
    loginCandidate.webSessionTimeoutMinutes;

Settings::save();

if (webCredentialsChanged || webLoginDisabled)
{
    invalidateSession();
}

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

void WebServerManager::handleSavePins()
{
    if (!requireAuthentication())
    {
        return;
    }
    SettingsData candidate = Settings::data;
    const char* pinArguments[] =
    {
        "buttonPin", "statusLedPin", "ledRedPin", "ledGreenPin",
        "ledBluePin", "batteryAdcPin", "sensorTriggerPin", "sensorEchoPin"
    };
    uint8_t* candidatePins[] =
    {
        &candidate.buttonPin, &candidate.statusLedPin, &candidate.ledRedPin,
        &candidate.ledGreenPin, &candidate.ledBluePin,
        &candidate.batteryAdcPin, &candidate.sensorTriggerPin,
        &candidate.sensorEchoPin
    };

    for (size_t i = 0; i < sizeof(pinArguments) / sizeof(pinArguments[0]); ++i)
    {
        if (!server.hasArg(pinArguments[i]))
        {
            server.send(
                400,
                "text/plain; charset=utf-8",
                String("Missing pin field: ") + pinArguments[i]
            );
            return;
        }

        const String value = server.arg(pinArguments[i]);
        char* end = nullptr;
        const long pin = strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0' || pin < 0 || pin > 39)
        {
            server.send(
                400,
                "text/plain; charset=utf-8",
                String("Invalid GPIO value for ") + pinArguments[i] + "."
            );
            return;
        }
        *candidatePins[i] = static_cast<uint8_t>(pin);
    }

    String pinError;
    if (!Settings::validatePins(candidate, pinError))
    {
        Logger::warning("Pin configuration rejected: " + pinError);
        server.send(400, "text/plain; charset=utf-8", pinError);
        return;
    }

    Settings::data.buttonPin = candidate.buttonPin;
    Settings::data.statusLedPin = candidate.statusLedPin;
    Settings::data.ledRedPin = candidate.ledRedPin;
    Settings::data.ledGreenPin = candidate.ledGreenPin;
    Settings::data.ledBluePin = candidate.ledBluePin;
    Settings::data.batteryAdcPin = candidate.batteryAdcPin;
    Settings::data.sensorTriggerPin = candidate.sensorTriggerPin;
    Settings::data.sensorEchoPin = candidate.sensorEchoPin;
    Settings::save();
    Logger::info("Pin configuration saved; restart required");

    server.send(
        200,
        "text/html; charset=utf-8",
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<link rel=\"stylesheet\" href=\"/style.css\"><title>Pins saved</title>"
        "</head><body><main class=\"message-page\"><section class=\"message-card\">"
        "<h1>Pin configuration saved</h1>"
        "<p>Pin configuration saved. Restart required.</p>"
        "<a href=\"/info\">Return to Device Info</a></section></main></body></html>"
    );
}

void WebServerManager::handleRestoreDefaultPins()
{
    if (!requireAuthentication())
    {
        return;
    }
    Settings::restoreDefaultPins();
    Settings::save();
    Logger::warning("Default pin configuration restored; restart required");
    server.send(
        200,
        "text/html; charset=utf-8",
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<link rel=\"stylesheet\" href=\"/style.css\"><title>Pins restored</title>"
        "</head><body><main class=\"message-page\"><section class=\"message-card\">"
        "<h1>Default pin configuration restored</h1>"
        "<p>Default pin configuration restored. Restart required.</p>"
        "<a href=\"/info\">Return to Device Info</a></section></main></body></html>"
    );
}

void WebServerManager::handleMeasure()
{
    if (!requireAuthentication())
    {
        return;
    }
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
         * Determine elapsed time since the previous web-initiated sample.
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
    if (!requireAuthentication())
    {
        return;
    }
    Logger::warning(
        "Battery estimate reset requested "
        "from web interface"
    );

    BatteryEstimator::reset();

    /*
     * Reset the local timestamp so the next sample starts a new learning
     * phase.
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
    if (!requireAuthentication())
    {
        return;
    }
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
    if (!requireAuthentication())
    {
        return;
    }
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
    if (!requireAuthentication())
    {
        return;
    }
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
    if (!requireAuthentication())
    {
        return;
    }
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
    if (!requireAuthentication())
    {
        return;
    }
    Logger::warning(
        "Restart requested from web interface"
    );

    sendTemplate(
        "/restart.html"
    );

    restartPending = true;
    restartAt = millis() + 3000UL;
}

void WebServerManager::handleFirmwarePage()
{
    if (!requireAuthentication())
    {
        return;
    }

    firmwareUploadError = "";
    sendTemplate("/firmware.html");
}

void WebServerManager::handleFirmwareUpload()
{
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        firmwareUploadAuthorized =
            !Settings::data.webLoginEnabled ||
            configPortalActive ||
            hasValidSession();
        firmwareUploadSuccessful = false;
        firmwareUploadError = "";

        if (!firmwareUploadAuthorized)
        {
            return;
        }

        String filename = upload.filename;
        filename.toLowerCase();
        if (filename.isEmpty() || !filename.endsWith(".bin"))
        {
            firmwareUploadError =
                "Please select a valid firmware .bin file.";
            return;
        }

        Logger::warning(
            "Firmware upload started: " + upload.filename
        );

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            firmwareUploadError =
                "Firmware update could not start: " +
                String(Update.errorString());
        }
        return;
    }

    if (!firmwareUploadAuthorized || !firmwareUploadError.isEmpty())
    {
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            firmwareUploadError =
                "Firmware write failed: " +
                String(Update.errorString());
            Update.abort();
        }
        return;
    }

    if (upload.status == UPLOAD_FILE_END)
    {
        if (!Update.end(true))
        {
            firmwareUploadError =
                "Firmware validation failed: " +
                String(Update.errorString());
            return;
        }

        firmwareUploadSuccessful = true;
        Logger::warning(
            "Firmware upload completed successfully (" +
            String(upload.totalSize) +
            " bytes)"
        );
        return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED)
    {
        Update.abort();
        firmwareUploadError = "Firmware upload was cancelled.";
        Logger::warning("Firmware upload aborted");
    }
}

void WebServerManager::handleFirmwareUploadComplete()
{
    if (!firmwareUploadAuthorized)
    {
        requireAuthentication();
        return;
    }

    if (!firmwareUploadSuccessful)
    {
        if (firmwareUploadError.isEmpty())
        {
            firmwareUploadError = "No firmware data was received.";
        }

        Logger::error(
            "Firmware upload failed: " + firmwareUploadError
        );
        sendTemplate("/firmware.html");
        return;
    }

    sendTemplate("/firmware-success.html");
    restartPending = true;
    restartAt = millis() + 4000UL;
}

void WebServerManager::handleNotFound()
{
    if (!requireAuthentication())
    {
        return;
    }
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
        "{{FIRMWARE_UPLOAD_ERROR_BLOCK}}",
        firmwareUploadError.isEmpty()
            ? ""
            : "<div class=\"firmware-error\" role=\"alert\">" +
                htmlEscape(firmwareUploadError) +
                "</div>"
    );

    page.replace(
        "{{BOARD_TYPE}}",
        "DOIT ESP32 DevKit V1"
    );

    page.replace(
        "{{MEASURE_INTERVAL_SECONDS}}",
        String(Settings::data.measureInterval)
    );

    page.replace(
        "{{RECOVERY_PIN_CURRENT}}",
        String(RECOVERY_BUTTON_PIN)
    );

    page.replace(
        "{{RECOVERY_PIN_DEFAULT}}",
        String(RECOVERY_BUTTON_PIN)
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
        "{{NETWORK_MODE_DHCP_SELECTED}}",
        Settings::data.wifiDhcp ? "selected" : ""
    );
    page.replace(
        "{{NETWORK_MODE_MANUAL_SELECTED}}",
        Settings::data.wifiDhcp ? "" : "selected"
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

    const String suggestedDhcpIp =
        WifiManager::getLastDhcpIp().isEmpty()
            ? WifiManager::getIpAddress()
            : WifiManager::getLastDhcpIp();
    const String suggestedDhcpGateway =
        WifiManager::getLastDhcpGateway().isEmpty()
            ? WifiManager::getGatewayAddress()
            : WifiManager::getLastDhcpGateway();
    const String suggestedDhcpSubnet =
        WifiManager::getLastDhcpSubnet().isEmpty()
            ? WifiManager::getSubnetMask()
            : WifiManager::getLastDhcpSubnet();
    const String suggestedDhcpDns1 =
        WifiManager::getLastDhcpDns1().isEmpty()
            ? WifiManager::getDnsAddress(0)
            : WifiManager::getLastDhcpDns1();
    const String suggestedDhcpDns2 =
        WifiManager::getLastDhcpDns2().isEmpty()
            ? WifiManager::getDnsAddress(1)
            : WifiManager::getLastDhcpDns2();

    page.replace("{{LAST_DHCP_IP}}", suggestedDhcpIp);
    page.replace("{{LAST_DHCP_GATEWAY}}", suggestedDhcpGateway);
    page.replace("{{LAST_DHCP_SUBNET}}", suggestedDhcpSubnet);
    page.replace("{{LAST_DHCP_DNS_1}}", suggestedDhcpDns1);
    page.replace("{{LAST_DHCP_DNS_2}}", suggestedDhcpDns2);

    page.replace("{{AP_SSID}}", htmlEscape(Settings::data.apSsid));
    page.replace("{{AP_IP}}", htmlEscape(Settings::data.apIp));
    page.replace("{{AP_GATEWAY}}", htmlEscape(Settings::data.apGateway));
    page.replace("{{AP_SUBNET}}", htmlEscape(Settings::data.apSubnet));
    page.replace(
        "{{AP_SECURITY}}",
        Settings::data.apPassword.isEmpty() ? "Open" : "Protected"
    );
    page.replace(
        "{{WEB_LOGIN_ENABLED_CHECKED}}",
        Settings::data.webLoginEnabled ? "checked" : ""
    );
    page.replace(
        "{{WEB_USERNAME}}",
        htmlEscape(Settings::data.webUsername)
    );
    page.replace(
        "{{WEB_SESSION_TIMEOUT_MINUTES}}",
        String(Settings::data.webSessionTimeoutMinutes)
    );
    page.replace(
        "{{LOGOUT_CONTROL}}",
        (
            Settings::data.webLoginEnabled &&
            !configPortalActive &&
            hasValidSession(false)
        )
            ? "<form method=\"POST\" action=\"/logout\">"
              "<button class=\"nav-link\" type=\"submit\">Logout</button>"
              "</form>"
            : ""
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
        "{{DEEP_SLEEP_ENABLED_CHECKED}}",
        Settings::data.deepSleepEnabled
            ? "checked"
            : ""
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

static const uint8_t outputPins[] =
    {4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32};
static const uint8_t buttonPins[] =
    {4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
static const uint8_t inputPins[] =
    {4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39};
static const uint8_t adcPins[] = {32, 34, 35, 36, 39};
static const uint8_t statusPins[] =
    {2, 4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32};
static const uint8_t triggerPins[] =
    {5, 4, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32};

page.replace("{{BUTTON_PIN_OPTIONS}}",
    pinOptions(Settings::data.buttonPin, buttonPins, sizeof(buttonPins)));
page.replace("{{STATUS_LED_PIN_OPTIONS}}",
    pinOptions(Settings::data.statusLedPin, statusPins, sizeof(statusPins)));
page.replace("{{LED_RED_PIN_OPTIONS}}",
    pinOptions(Settings::data.ledRedPin, outputPins, sizeof(outputPins)));
page.replace("{{LED_GREEN_PIN_OPTIONS}}",
    pinOptions(Settings::data.ledGreenPin, outputPins, sizeof(outputPins)));
page.replace("{{LED_BLUE_PIN_OPTIONS}}",
    pinOptions(Settings::data.ledBluePin, outputPins, sizeof(outputPins)));
page.replace("{{BATTERY_ADC_PIN_OPTIONS}}",
    pinOptions(Settings::data.batteryAdcPin, adcPins, sizeof(adcPins)));
page.replace("{{SENSOR_TRIGGER_PIN_OPTIONS}}",
    pinOptions(Settings::data.sensorTriggerPin, triggerPins, sizeof(triggerPins)));
page.replace("{{SENSOR_ECHO_PIN_OPTIONS}}",
    pinOptions(Settings::data.sensorEchoPin, inputPins, sizeof(inputPins)));

#define PIN_TEMPLATE(name, field, defaultValue) \
    page.replace("{{" name "_CURRENT}}", String(Settings::data.field)); \
    page.replace("{{" name "_DEFAULT}}", String(defaultValue)); \
    page.replace("{{" name "_BADGE}}", \
        Settings::data.field == defaultValue ? "" : "<span class=\"custom-badge\">Custom</span>")

PIN_TEMPLATE("BUTTON_PIN", buttonPin, DEFAULT_BUTTON_PIN);
PIN_TEMPLATE("STATUS_LED_PIN", statusLedPin, DEFAULT_STATUS_LED_PIN);
PIN_TEMPLATE("LED_RED_PIN", ledRedPin, DEFAULT_LED_RED_PIN);
PIN_TEMPLATE("LED_GREEN_PIN", ledGreenPin, DEFAULT_LED_GREEN_PIN);
PIN_TEMPLATE("LED_BLUE_PIN", ledBluePin, DEFAULT_LED_BLUE_PIN);
PIN_TEMPLATE("BATTERY_ADC_PIN", batteryAdcPin, DEFAULT_BATTERY_ADC_PIN);
PIN_TEMPLATE("SENSOR_TRIGGER_PIN", sensorTriggerPin, DEFAULT_SENSOR_TRIGGER_PIN);
PIN_TEMPLATE("SENSOR_ECHO_PIN", sensorEchoPin, DEFAULT_SENSOR_ECHO_PIN);
#undef PIN_TEMPLATE

page.replace(
    "{{PIN_WARNING}}",
    Settings::pinFallbackActive()
        ? "<div class=\"calculation-note\">" +
            htmlEscape(Settings::pinWarning()) + "</div>"
        : ""
);

}

String WebServerManager::pinOptions(
    uint8_t selected,
    const uint8_t* pins,
    size_t count
)
{
    String options;
    for (size_t i = 0; i < count; ++i)
    {
        options += "<option value=\"" + String(pins[i]) + "\"";
        if (pins[i] == selected)
        {
            options += " selected";
        }
        options += ">GPIO " + String(pins[i]) + "</option>";
    }
    return options;
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
