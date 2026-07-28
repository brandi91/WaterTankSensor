#include "logger.h"

void Logger::begin()
{
    Serial.begin(115200);

    while (!Serial)
    {
        delay(10);
    }

    Serial.println();
    Serial.println("======================================");
    Serial.println(" Logger gestartet");
    Serial.println("======================================");
}

void Logger::loop()
{
    // Für spätere Erweiterungen (Weblog, MQTT usw.)
}

void Logger::info(const String &message)
{
    print("INFO", message);
}

void Logger::warning(const String &message)
{
    print("WARN", message);
}

void Logger::error(const String &message)
{
    print("ERROR", message);
}

void Logger::print(const String &level, const String &message)
{
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");

    Serial.println(message);
}