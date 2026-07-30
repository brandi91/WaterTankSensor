#include "logger.h"

#include <LittleFS.h>

#include "time_manager.h"

namespace
{
    constexpr const char* LOG_PATH =
        "/device_logs.log";

    constexpr const char* LOG_TEMP_PATH =
        "/device_logs.tmp";

    constexpr size_t MAX_LOG_ENTRIES = 50;
    constexpr size_t MAX_PENDING_LOG_ENTRIES = 12;
    constexpr size_t MAX_LOG_MESSAGE_LENGTH = 180;
    constexpr size_t MAX_LOG_FILE_SIZE = 32768;

    RTC_DATA_ATTR uint32_t wakeCycleId = 0;

    bool persistentLoggingEnabled = false;
    bool persistentWriteInProgress = false;

    struct PersistentLogEntry
    {
        uint32_t wakeCycle = 0;
        uint32_t uptimeSeconds = 0;
        uint32_t timestamp = 0;
        String timeSource;
        String level;
        String message;
    };

    PersistentLogEntry pendingEntries[
        MAX_PENDING_LOG_ENTRIES
    ];

    PersistentLogEntry storedEntries[
        MAX_LOG_ENTRIES
    ];

    size_t pendingEntryCount = 0;

    String escapeLogField(
        const String& value
    )
    {
        String escaped;
        escaped.reserve(value.length() + 8);

        for (
            size_t index = 0;
            index < value.length();
            index++
        )
        {
            const char character =
                value.charAt(index);

            switch (character)
            {
                case '\\':
                    escaped += "\\\\";
                    break;

                case '\t':
                    escaped += "\\t";
                    break;

                case '\n':
                    escaped += "\\n";
                    break;

                case '\r':
                    break;

                default:
                    escaped += character;
                    break;
            }
        }

        return escaped;
    }

    String unescapeLogField(
        const String& value
    )
    {
        String unescaped;
        unescaped.reserve(value.length());

        bool escaping = false;

        for (
            size_t index = 0;
            index < value.length();
            index++
        )
        {
            const char character =
                value.charAt(index);

            if (escaping)
            {
                unescaped +=
                    character == 't'
                        ? '\t'
                        : character == 'n'
                            ? '\n'
                            : character;

                escaping = false;
            }
            else if (character == '\\')
            {
                escaping = true;
            }
            else
            {
                unescaped += character;
            }
        }

        if (escaping)
        {
            unescaped += '\\';
        }

        return unescaped;
    }

    bool parseLogLine(
        const String& line,
        PersistentLogEntry& entry
    )
    {
        const int firstSeparator =
            line.indexOf('\t');

        const int secondSeparator =
            line.indexOf(
                '\t',
                firstSeparator + 1
            );

        const int thirdSeparator =
            line.indexOf(
                '\t',
                secondSeparator + 1
            );

        const int fourthSeparator =
            line.indexOf(
                '\t',
                thirdSeparator + 1
            );

        const int fifthSeparator =
            line.indexOf(
                '\t',
                fourthSeparator + 1
            );

        if (
            firstSeparator <= 0 ||
            secondSeparator <=
                firstSeparator + 1 ||
            thirdSeparator <= secondSeparator + 1 ||
            fourthSeparator <= thirdSeparator + 1 ||
            fifthSeparator <= fourthSeparator + 1
        )
        {
            return false;
        }

        const String cycleText =
            line.substring(
                0,
                firstSeparator
            );

        const String uptimeText =
            line.substring(
                firstSeparator + 1,
                secondSeparator
            );

        const String timestampText =
            line.substring(
                secondSeparator + 1,
                thirdSeparator
            );

        const String timeSource =
            line.substring(
                thirdSeparator + 1,
                fourthSeparator
            );

        const String level =
            line.substring(
                fourthSeparator + 1,
                fifthSeparator
            );

        if (
            level != "INFO" &&
            level != "WARNING" &&
            level != "ERROR"
        )
        {
            return false;
        }

        const String message =
            unescapeLogField(
                line.substring(
                    fifthSeparator + 1
                )
            );

        if (message.isEmpty())
        {
            return false;
        }

        entry.wakeCycle =
            static_cast<uint32_t>(
                cycleText.toInt()
            );

        entry.uptimeSeconds =
            static_cast<uint32_t>(
                uptimeText.toInt()
            );

        entry.timestamp =
            static_cast<uint32_t>(
                strtoul(
                    timestampText.c_str(),
                    nullptr,
                    10
                )
            );
        entry.timeSource = timeSource;

        entry.level = level;
        entry.message = message;

        return true;
    }

    size_t loadLogEntries(
        PersistentLogEntry* entries
    )
    {
        if (!LittleFS.exists(LOG_PATH))
        {
            return 0;
        }

        File file =
            LittleFS.open(
                LOG_PATH,
                "r"
            );

        if (
            !file ||
            file.size() > MAX_LOG_FILE_SIZE
        )
        {
            if (file)
            {
                file.close();
            }

            return 0;
        }

        size_t entryCount = 0;

        while (file.available())
        {
            String line =
                file.readStringUntil('\n');

            line.trim();

            PersistentLogEntry entry;

            if (!parseLogLine(line, entry))
            {
                continue;
            }

            if (entryCount == MAX_LOG_ENTRIES)
            {
                for (
                    size_t index = 1;
                    index < entryCount;
                    index++
                )
                {
                    entries[index - 1] =
                        entries[index];
                }

                entryCount--;
            }

            entries[entryCount] = entry;
            entryCount++;
        }

        file.close();

        return entryCount;
    }

    bool rewriteLogEntries(
        const PersistentLogEntry* entries,
        size_t entryCount
    )
    {
        File temporaryFile =
            LittleFS.open(
                LOG_TEMP_PATH,
                "w"
            );

        if (!temporaryFile)
        {
            return false;
        }

        for (
            size_t index = 0;
            index < entryCount;
            index++
        )
        {
            temporaryFile.print(
                entries[index].wakeCycle
            );
            temporaryFile.print('\t');
            temporaryFile.print(
                entries[index].uptimeSeconds
            );
            temporaryFile.print('\t');
            temporaryFile.print(
                entries[index].timestamp
            );
            temporaryFile.print('\t');
            temporaryFile.print(
                entries[index].timeSource
            );
            temporaryFile.print('\t');
            temporaryFile.print(
                entries[index].level
            );
            temporaryFile.print('\t');
            temporaryFile.println(
                escapeLogField(
                    entries[index].message
                )
            );
        }

        temporaryFile.flush();

        const bool writeSucceeded =
            temporaryFile.getWriteError() == 0;

        temporaryFile.close();

        if (!writeSucceeded)
        {
            LittleFS.remove(LOG_TEMP_PATH);
            return false;
        }

        if (LittleFS.exists(LOG_PATH))
        {
            LittleFS.remove(LOG_PATH);
        }

        if (
            !LittleFS.rename(
                LOG_TEMP_PATH,
                LOG_PATH
            )
        )
        {
            LittleFS.remove(LOG_TEMP_PATH);
            return false;
        }

        return true;
    }

    String jsonEscape(
        const String& value
    )
    {
        String escaped;
        escaped.reserve(value.length() + 8);

        for (
            size_t index = 0;
            index < value.length();
            index++
        )
        {
            const char character =
                value.charAt(index);

            switch (character)
            {
                case '\\':
                    escaped += "\\\\";
                    break;

                case '"':
                    escaped += "\\\"";
                    break;

                case '\n':
                    escaped += "\\n";
                    break;

                case '\r':
                    break;

                case '\t':
                    escaped += "\\t";
                    break;

                default:
                    if (
                        static_cast<uint8_t>(
                            character
                        ) >= 0x20
                    )
                    {
                        escaped += character;
                    }
                    break;
            }
        }

        return escaped;
    }

    bool isMeaningfulInfoMessage(
        const String& message
    )
    {
        static const char* keywords[] =
        {
            "Booting",
            "Wakeup reason",
            "Starting tank measurement",
            "Tank measurement successful",
            "measurement failed",
            "Wi-Fi connected",
            "MQTT connected",
            "MQTT transmission",
            "Discovery",
            "NTP synchronization",
            "Measurement history",
            "deep sleep",
            "Deep sleep",
            "Sleeping for",
            "Settings saved",
            "Configuration saved",
            "Configuration portal"
        };

        for (
            const char* keyword : keywords
        )
        {
            if (message.indexOf(keyword) >= 0)
            {
                return true;
            }
        }

        return false;
    }
}

void Logger::begin()
{
    Serial.begin(115200);

    while (!Serial)
    {
        delay(10);
    }

    wakeCycleId++;

    Serial.println();
    Serial.println(
        "======================================"
    );
    Serial.println(" Logger started");
    Serial.println(
        "======================================"
    );

    enablePersistentLogging();
}

void Logger::loop()
{
}

void Logger::info(
    const String& message
)
{
    print("INFO", message);
}

void Logger::warning(
    const String& message
)
{
    print("WARNING", message);
}

void Logger::error(
    const String& message
)
{
    print("ERROR", message);
}

void Logger::print(
    const String& level,
    const String& message
)
{
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);

    persist(level, message);
}

void Logger::enablePersistentLogging()
{
    if (persistentLoggingEnabled)
    {
        return;
    }

    persistentLoggingEnabled =
        LittleFS.begin(true);
}

void Logger::persist(
    const String& level,
    const String& message
)
{
    if (
        !persistentLoggingEnabled ||
        persistentWriteInProgress ||
        message.isEmpty()
    )
    {
        return;
    }

    if (
        level == "INFO" &&
        !isMeaningfulInfoMessage(message)
    )
    {
        return;
    }

    if (pendingEntryCount >= MAX_PENDING_LOG_ENTRIES)
    {
        flushPersistentLogs();
    }

    /*
     * A filesystem error can leave the pending queue full. Keep it bounded
     * and preserve the newest diagnostics instead of writing past the array.
     */
    if (pendingEntryCount >= MAX_PENDING_LOG_ENTRIES)
    {
        for (
            size_t index = 1;
            index < pendingEntryCount;
            ++index
        )
        {
            pendingEntries[index - 1] = pendingEntries[index];
        }
        pendingEntryCount--;
    }

    String boundedMessage = message;

    if (
        boundedMessage.length() >
        MAX_LOG_MESSAGE_LENGTH
    )
    {
        boundedMessage.remove(
            MAX_LOG_MESSAGE_LENGTH
        );
    }

    if (pendingEntryCount > 0)
    {
        const PersistentLogEntry& latest =
            pendingEntries[
                pendingEntryCount - 1
            ];

        if (
            latest.level == level &&
            latest.message == boundedMessage
        )
        {
            return;
        }
    }
    else
    {
        const size_t storedCount =
            loadLogEntries(storedEntries);

        if (
            storedCount > 0 &&
            storedEntries[
                storedCount - 1
            ].level == level &&
            storedEntries[
                storedCount - 1
            ].message == boundedMessage
        )
        {
            return;
        }
    }

    pendingEntries[
        pendingEntryCount
    ].wakeCycle =
        wakeCycleId;

    pendingEntries[
        pendingEntryCount
    ].uptimeSeconds =
        millis() / 1000UL;

    pendingEntries[
        pendingEntryCount
    ].timestamp =
        static_cast<uint32_t>(
            TimeManager::now()
        );

    pendingEntries[
        pendingEntryCount
    ].timeSource =
        TimeManager::getStorageTimeSource();

    pendingEntries[
        pendingEntryCount
    ].level = level;

    pendingEntries[
        pendingEntryCount
    ].message =
        boundedMessage;

    pendingEntryCount++;

    if (
        pendingEntryCount ==
        MAX_PENDING_LOG_ENTRIES
    )
    {
        flushPersistentLogs();
    }
}

void Logger::flushPersistentLogs()
{
    if (
        !persistentLoggingEnabled ||
        persistentWriteInProgress ||
        pendingEntryCount == 0
    )
    {
        return;
    }

    persistentWriteInProgress = true;

    size_t entryCount =
        loadLogEntries(storedEntries);

    for (
        size_t pendingIndex = 0;
        pendingIndex < pendingEntryCount;
        pendingIndex++
    )
    {
        if (entryCount == MAX_LOG_ENTRIES)
        {
            for (
                size_t index = 1;
                index < entryCount;
                index++
            )
            {
                storedEntries[index - 1] =
                    storedEntries[index];
            }

            entryCount--;
        }

        storedEntries[entryCount] =
            pendingEntries[pendingIndex];
        entryCount++;
    }

    if (
        rewriteLogEntries(
            storedEntries,
            entryCount
        )
    )
    {
        pendingEntryCount = 0;
    }

    persistentWriteInProgress = false;
}

String Logger::getPersistentLogsJson()
{
    enablePersistentLogging();
    flushPersistentLogs();

    const size_t entryCount =
        persistentLoggingEnabled
            ? loadLogEntries(storedEntries)
            : 0;

    String json;
    json.reserve(
        64 +
        entryCount * 260
    );

    json += "{\"count\":";
    json += String(entryCount);
    json += ",\"logs\":[";

    for (
        size_t reverseIndex = entryCount;
        reverseIndex > 0;
        reverseIndex--
    )
    {
        const size_t index =
            reverseIndex - 1;

        if (reverseIndex < entryCount)
        {
            json += ",";
        }

        json += "{\"wakeCycle\":";
        json += String(
            storedEntries[index].wakeCycle
        );
        json += ",\"uptimeSeconds\":";
        json += String(
            storedEntries[index].uptimeSeconds
        );
        json += ",\"timestamp\":";
        json += String(
            storedEntries[index].timestamp
        );
        json += ",\"timeSource\":\"";
        json += storedEntries[index].timeSource;
        json += "\"";
        json += ",\"level\":\"";
        json += storedEntries[index].level;
        json += "\",\"message\":\"";
        json += jsonEscape(
            storedEntries[index].message
        );
        json += "\"}";
    }

    json += "]}";

    return json;
}

void Logger::clearPersistentLogs()
{
    enablePersistentLogging();

    if (!persistentLoggingEnabled)
    {
        return;
    }

    persistentWriteInProgress = true;
    pendingEntryCount = 0;
    LittleFS.remove(LOG_PATH);
    LittleFS.remove(LOG_TEMP_PATH);
    persistentWriteInProgress = false;
}

uint32_t Logger::getWakeCycleId()
{
    return wakeCycleId;
}
