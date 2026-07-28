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

private:
    static void print(const String &level, const String &message);
};