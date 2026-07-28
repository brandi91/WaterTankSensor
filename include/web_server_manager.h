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

    static void registerRoutes();

    static void handleRoot();
    static void handleSave();
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

    static String htmlEscape(
        const String& value
    );
};