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

    if (WifiManager::isConnected())
    {
        if (state != WifiState::Connected)
        {
            state = WifiState::Connected;

            Logger::info("Wi-Fi connected");
            Logger::info(
                "IP address: " +
                WifiManager::getIpAddress()
            );

            Logger::info(
                "Signal strength: " +
                String(WifiManager::getRssi()) +
                " dBm"
            );
        }

        return;
    }

    if (state == WifiState::Connecting)
    {
        if (now - connectionStartedAt >= WIFI_CONNECT_TIMEOUT_MS)
        {
            state = WifiState::ConnectionFailed;
            lastReconnectAttempt = now;

            Logger::warning(
                "Wi-Fi connection timed out"
            );

            WifiManager::disconnect();
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
    WifiManager::disconnect(true, false);

    state = WifiState::Disabled;

    Logger::info("Wi-Fi disconnected");
}

bool WifiManager::isConnected()
{
    return WifiManager::isConnected();
}

WifiState WifiManager::getState()
{
    return state;
}

String WifiManager::getIpAddress()
{
    if (!WifiManager::isConnected())
    {
        return "";
    }

    return WifiManager::localIP().toString();
}

String WifiManager::getSsid()
{
    if (!WifiManager::isConnected())
    {
        return "";
    }

    return WifiManager::SSID();
}

int32_t WifiManager::getRssi()
{
    if (!WifiManager::isConnected())
    {
        return 0;
    }

    return WifiManager::RSSI();
}