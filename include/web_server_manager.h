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

    static void registerRoutes();

    static void handleRoot();
    static void handleStatus();
    static void handleSave();
    static void handleMeasure();
    static void handlePublishMqttDiscovery();
    static void handleResetBatteryEstimate();
    static void handleSleep();
    static void handleRestart();
    static void handleNotFound();

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

    static String processTemplate(
        String page
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
};
