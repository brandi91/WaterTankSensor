#pragma once

#include <Arduino.h>
#include <WebServer.h>

class WebServerManager
{
public:
    static void begin();
    static void beginConfigPortal();
    static void loop();
    static void stop();

    static bool isRunning();
    static bool isConfigPortalActive();
    static void invalidateAuthenticationSession();

private:
    static WebServer server;

    static bool running;
    static bool configPortalActive;
    static bool filesystemReady;
    static bool routesRegistered;

    static bool restartPending;
    static unsigned long restartAt;

    static bool sleepPending;
    static unsigned long sleepAt;

    /*
     * Wird für Batterielernpunkte verwendet,
     * die über den Web-Messbutton entstehen.
     */
    static unsigned long lastBatteryEstimatorSampleAt;
    static bool sessionActive;
    static String sessionToken;
    static unsigned long sessionLastActiveAt;

    static void registerRoutes();

    static void handleLoginGet();
    static void handleLoginPost();
    static void handleLogout();
    static void handleRoot();
    static void handleInfo();
    static void handleLogs();
    static void handleStatus();
    static void handleHistoryApi();
    static void handleLogsApi();
    static void handleSave();
    static void handleSavePins();
    static void handleRestoreDefaultPins();
    static void handleMeasure();
    static void handlePublishMqttDiscovery();
    static void handleResetBatteryEstimate();
    static void handleClearHistory();
    static void handleClearLogs();
    static void handleSleep();
    static void handleRestart();
    static void handleNotFound();
    static bool requireAuthentication(bool apiRequest = false);
    static bool hasValidSession(bool refreshActivity = true);
    static void invalidateSession();
    static String createSessionToken();
    static bool constantTimeEquals(
        const String& left,
        const String& right
    );
    static String safeRedirect(const String& requested);
    static String urlEncode(const String& value);
    static void sendLoginPage(
        const String& error,
        const String& redirectTarget
    );

    static void sendTemplate(
        const String& path
    );

    static void sendFile(
        const String& path,
        const String& contentType
    );

    static String loadFile(
        const String& path
    );

    static void processTemplate(
        String& page
    );

    static String getIpAddress();
    static String getNetworkMode();
    static String getSensorStatusText();
    static String getSensorStatusClass();
    static String getBatteryStatusText();
    static String getBatteryStatusClass();
    static String getMqttStatusText();
    static String getMqttStatusClass();

    static String htmlEscape(
        const String& value
    );
    static String pinOptions(
        uint8_t selected,
        const uint8_t* pins,
        size_t count
    );
};
