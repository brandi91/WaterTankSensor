#pragma once

#include <Arduino.h>

class Logger
{
public:
    static void begin();
    static void loop();

    static void info(const String &message);
    static void warning(const String &message);
    static void error(const String &message);

    static void enablePersistentLogging();
    static void flushPersistentLogs();
    static String getPersistentLogsJson();
    static void clearPersistentLogs();
    static uint32_t getWakeCycleId();

private:
    static void print(const String &level, const String &message);
    static void persist(
        const String& level,
        const String& message
    );
};
