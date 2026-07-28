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

    static void registerRoutes();

    static void handleRoot();
    static void handleSave();
    static void handleRestart();
    static void handleNotFound();

    static String buildPage();
    static String htmlEscape(const String& value);
};