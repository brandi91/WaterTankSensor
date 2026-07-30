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
    static String getSubnetMask();
    static String getGatewayAddress();
    static String getDnsAddress(uint8_t index);
    static String getHostname();
    static String getMdnsName();
    static String getSsid();
    static int32_t getRssi();

private:
    static WifiState state;
    static unsigned long connectionStartedAt;
    static unsigned long lastReconnectAttempt;
    static bool networkConfigurationValid;
    static bool mdnsRunning;
    static String hostname;

    static void startConnection();
    static bool configureNetwork();
    static void startMdns();
    static String buildHostname(
        const String& deviceName
    );
};
