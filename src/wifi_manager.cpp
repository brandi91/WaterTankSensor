#include "wifi_manager.h"

#include <WiFi.h>
#include <ESPmDNS.h>

#include "config.h"
#include "logger.h"
#include "settings.h"

WifiState WifiManager::state = WifiState::Disabled;

unsigned long WifiManager::connectionStartedAt = 0;
unsigned long WifiManager::lastReconnectAttempt = 0;
bool WifiManager::networkConfigurationValid = false;
bool WifiManager::mdnsRunning = false;
String WifiManager::hostname;

void WifiManager::begin()
{
    WiFi.mode(WIFI_STA);

    hostname = buildHostname(
        Settings::data.deviceName
    );

    if (!WiFi.setHostname(hostname.c_str()))
    {
        Logger::error(
            "Failed to set Wi-Fi hostname: " +
            hostname
        );
    }

    WiFi.setAutoReconnect(true);

    networkConfigurationValid =
        configureNetwork();

    state = networkConfigurationValid
        ? WifiState::Disconnected
        : WifiState::ConnectionFailed;

    Logger::info(
        "Wi-Fi Manager initialized with hostname: " +
        hostname
    );
}

bool WifiManager::connect()
{
    if (Settings::data.wifiSSID.isEmpty())
    {
        Logger::warning("Wi-Fi SSID is empty");
        state = WifiState::ConnectionFailed;
        return false;
    }

    if (!networkConfigurationValid)
    {
        Logger::error(
            "Wi-Fi connection blocked: invalid static network configuration"
        );

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

            startMdns();
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
        networkConfigurationValid &&
        (
            state == WifiState::ConnectionFailed ||
            state == WifiState::Disconnected
        )
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
    if (mdnsRunning)
    {
        MDNS.end();
        mdnsRunning = false;
    }

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

String WifiManager::getSubnetMask()
{
    return WiFi.isConnected()
        ? WiFi.subnetMask().toString()
        : "";
}

String WifiManager::getGatewayAddress()
{
    return WiFi.isConnected()
        ? WiFi.gatewayIP().toString()
        : "";
}

String WifiManager::getDnsAddress(
    uint8_t index
)
{
    if (
        !WiFi.isConnected() ||
        index > 1
    )
    {
        return "";
    }

    return WiFi.dnsIP(index).toString();
}

String WifiManager::getHostname()
{
    return hostname;
}

String WifiManager::getMdnsName()
{
    return hostname.isEmpty()
        ? ""
        : hostname + ".local";
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

bool WifiManager::configureNetwork()
{
    if (Settings::data.wifiDhcp)
    {
        Logger::info(
            "Wi-Fi network configuration: DHCP"
        );

        return true;
    }

    IPAddress staticIp;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;

    const bool addressesValid =
        staticIp.fromString(
            Settings::data.wifiStaticIp
        ) &&
        gateway.fromString(
            Settings::data.wifiGateway
        ) &&
        subnet.fromString(
            Settings::data.wifiSubnet
        ) &&
        dns1.fromString(
            Settings::data.wifiDns1
        ) &&
        dns2.fromString(
            Settings::data.wifiDns2
        );

    if (!addressesValid)
    {
        Logger::error(
            "Invalid static Wi-Fi configuration: "
            "IP, gateway, subnet, DNS 1 and DNS 2 "
            "must all be valid IPv4 addresses"
        );

        return false;
    }

    if (
        !WiFi.config(
            staticIp,
            gateway,
            subnet,
            dns1,
            dns2
        )
    )
    {
        Logger::error(
            "Failed to apply static Wi-Fi configuration"
        );

        return false;
    }

    Logger::info(
        "Wi-Fi network configuration: static IP " +
        staticIp.toString()
    );

    return true;
}

void WifiManager::startMdns()
{
    if (mdnsRunning)
    {
        return;
    }

    if (!MDNS.begin(hostname.c_str()))
    {
        Logger::error(
            "Failed to start mDNS for " +
            hostname +
            ".local"
        );

        return;
    }

    mdnsRunning = true;

    Logger::info(
        "mDNS available at http://" +
        hostname +
        ".local"
    );
}

String WifiManager::buildHostname(
    const String& deviceName
)
{
    String result;
    result.reserve(63);

    bool previousWasHyphen = false;

    for (
        size_t index = 0;
        index < deviceName.length() &&
        result.length() < 63;
        index++
    )
    {
        char character = deviceName.charAt(index);

        if (
            character >= 'A' &&
            character <= 'Z'
        )
        {
            character =
                static_cast<char>(
                    character - 'A' + 'a'
                );
        }

        const bool isLowercaseLetter =
            character >= 'a' &&
            character <= 'z';

        const bool isDigit =
            character >= '0' &&
            character <= '9';

        if (
            isLowercaseLetter ||
            isDigit
        )
        {
            result += character;
            previousWasHyphen = false;
        }
        else if (
            !result.isEmpty() &&
            !previousWasHyphen
        )
        {
            result += '-';
            previousWasHyphen = true;
        }
    }

    while (result.endsWith("-"))
    {
        result.remove(
            result.length() - 1
        );
    }

    if (result.isEmpty())
    {
        result = "watertanksensor";
    }

    return result;
}
