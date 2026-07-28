#include "wifi_manager.h"

#include <WiFi.h>

#include "config.h"
#include "logger.h"
#include "settings.h"

WifiState WifiManager::state = WifiState::Disabled;

unsigned long WifiManager::connectionStartedAt = 0;
unsigned long WifiManager::lastReconnectAttempt = 0;

void WifiManager::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    state = WifiState::Disconnected;

    Logger::info("Wi-Fi Manager initialized");
}

bool WifiManager::connect()
{
    if (Settings::data.wifiSSID.isEmpty())
    {
        Logger::warning("Wi-Fi SSID is empty");
        state = WifiState::ConnectionFailed;
        return false;
    }

    if (WiFi.isConnected())
    {
        state = WifiState::Connected;
        return true;
    }

    startConnection();
    return true;
}

void WifiManager::startConnection()
{
    Logger::info(
        "Connecting to Wi-Fi: " +
        Settings::data.wifiSSID
    );

    WiFi.begin(
        Settings::data.wifiSSID.c_str(),
        Settings::data.wifiPassword.c_str()
    );

    connectionStartedAt = millis();
    state = WifiState::Connecting;
}

void WifiManager::loop()
{
    const unsigned long now = millis();

    if (WiFi.isConnected())
    {
        if (state != WifiState::Connected)
        {
            state = WifiState::Connected;

            Logger::info("Wi-Fi connected");

            Logger::info(
                "IP address: " +
                WiFi.localIP().toString()
            );

            Logger::info(
                "Signal strength: " +
                String(WiFi.RSSI()) +
                " dBm"
            );
        }

        return;
    }

    if (state == WifiState::Connecting)
    {
        if (
            now - connectionStartedAt >=
            WIFI_CONNECT_TIMEOUT_MS
        )
        {
            state = WifiState::ConnectionFailed;
            lastReconnectAttempt = now;

            Logger::warning("Wi-Fi connection timed out");

            WiFi.disconnect();
        }

        return;
    }

    if (
        state == WifiState::ConnectionFailed ||
        state == WifiState::Disconnected
    )
    {
        if (
            now - lastReconnectAttempt >=
            WIFI_RECONNECT_INTERVAL_MS
        )
        {
            lastReconnectAttempt = now;
            startConnection();
        }
    }
}

void WifiManager::disconnect()
{
    WiFi.disconnect(true, false);

    state = WifiState::Disabled;

    Logger::info("Wi-Fi disconnected");
}

bool WifiManager::isConnected()
{
    return WiFi.isConnected();
}

WifiState WifiManager::getState()
{
    return state;
}

String WifiManager::getIpAddress()
{
    if (!WiFi.isConnected())
    {
        return "";
    }

    return WiFi.localIP().toString();
}

String WifiManager::getSsid()
{
    if (!WiFi.isConnected())
    {
        return "";
    }

    return WiFi.SSID();
}

int32_t WifiManager::getRssi()
{
    if (!WiFi.isConnected())
    {
        return 0;
    }

    return WiFi.RSSI();
}