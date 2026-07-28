#pragma once

#include <Arduino.h>

enum class WifiState
{
    Disabled,
    Disconnected,
    Connecting,
    Connected,
    ConnectionFailed
};

class WifiManager
{
public:
    static void begin();
    static void loop();

    static bool connect();
    static void disconnect();

    static bool isConnected();
    static WifiState getState();

    static String getIpAddress();
    static String getSsid();
    static int32_t getRssi();

private:
    static WifiState state;
    static unsigned long connectionStartedAt;
    static unsigned long lastReconnectAttempt;

    static void startConnection();
};