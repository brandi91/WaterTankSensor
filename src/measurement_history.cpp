#include "measurement_history.h"

#include <LittleFS.h>

#include "battery.h"
#include "logger.h"
#include "sensor.h"

namespace
{
    constexpr const char* HISTORY_PATH =
        "/measurement_history.json";

    constexpr const char* HISTORY_TEMP_PATH =
        "/measurement_history.tmp";

    constexpr size_t MAX_HISTORY_ENTRIES = 10;
    constexpr size_t MAX_HISTORY_FILE_SIZE = 8192;

    struct MeasurementRecord
    {
        uint32_t sequence = 0;
        uint32_t wakeCycle = 0;
        uint32_t uptimeSeconds = 0;
        int fillPercent = 0;
        float waterLevelCm = 0.0f;
        float distanceCm = 0.0f;
        float batteryVoltage = 0.0f;
        int batteryPercent = 0;
        String sensorStatus;
    };

    MeasurementRecord records[MAX_HISTORY_ENTRIES];
    size_t recordCount = 0;
    uint32_t nextSequence = 1;

    bool extractNumber(
        const String& object,
        const String& key,
        float& value
    )
    {
        const String marker =
            "\"" + key + "\":";

        const int markerIndex =
            object.indexOf(marker);

        if (markerIndex < 0)
        {
            return false;
        }

        int valueStart =
            markerIndex + marker.length();

        while (
            valueStart < object.length() &&
            object.charAt(valueStart) == ' '
        )
        {
            valueStart++;
        }

        int valueEnd = valueStart;

        while (valueEnd < object.length())
        {
            const char character =
                object.charAt(valueEnd);

            if (
                character != '-' &&
                character != '.' &&
                (
                    character < '0' ||
                    character > '9'
                )
            )
            {
                break;
            }

            valueEnd++;
        }

        if (valueEnd == valueStart)
        {
            return false;
        }

        value =
            object.substring(
                valueStart,
                valueEnd
            ).toFloat();

        return true;
    }

    bool extractString(
        const String& object,
        const String& key,
        String& value
    )
    {
        const String marker =
            "\"" + key + "\":\"";

        const int markerIndex =
            object.indexOf(marker);

        if (markerIndex < 0)
        {
            return false;
        }

        const int valueStart =
            markerIndex + marker.length();

        const int valueEnd =
            object.indexOf(
                '"',
                valueStart
            );

        if (valueEnd < 0)
        {
            return false;
        }

        value =
            object.substring(
                valueStart,
                valueEnd
            );

        return true;
    }

    bool parseRecord(
        const String& object,
        MeasurementRecord& record
    )
    {
        float sequence = 0.0f;
        float wakeCycle = 0.0f;
        float uptimeSeconds = 0.0f;
        float fillPercent = 0.0f;
        float waterLevelCm = 0.0f;
        float distanceCm = 0.0f;
        float batteryVoltage = 0.0f;
        float batteryPercent = 0.0f;
        String sensorStatus;

        if (
            !extractNumber(
                object,
                "sequence",
                sequence
            ) ||
            !extractNumber(
                object,
                "wakeCycle",
                wakeCycle
            ) ||
            !extractNumber(
                object,
                "uptimeSeconds",
                uptimeSeconds
            ) ||
            !extractNumber(
                object,
                "fillPercent",
                fillPercent
            ) ||
            !extractNumber(
                object,
                "waterLevelCm",
                waterLevelCm
            ) ||
            !extractNumber(
                object,
                "distanceCm",
                distanceCm
            ) ||
            !extractNumber(
                object,
                "batteryVoltage",
                batteryVoltage
            ) ||
            !extractNumber(
                object,
                "batteryPercent",
                batteryPercent
            ) ||
            !extractString(
                object,
                "sensorStatus",
                sensorStatus
            )
        )
        {
            return false;
        }

        if (
            sequence < 1.0f ||
            fillPercent < 0.0f ||
            fillPercent > 100.0f ||
            batteryPercent < 0.0f ||
            batteryPercent > 100.0f ||
            waterLevelCm < 0.0f ||
            distanceCm < 0.0f ||
            batteryVoltage < 0.0f
        )
        {
            return false;
        }

        record.sequence =
            static_cast<uint32_t>(sequence);
        record.wakeCycle =
            static_cast<uint32_t>(wakeCycle);
        record.uptimeSeconds =
            static_cast<uint32_t>(uptimeSeconds);
        record.fillPercent =
            static_cast<int>(fillPercent);
        record.waterLevelCm = waterLevelCm;
        record.distanceCm = distanceCm;
        record.batteryVoltage = batteryVoltage;
        record.batteryPercent =
            static_cast<int>(batteryPercent);
        record.sensorStatus = sensorStatus;

        return true;
    }

    void appendRecordJson(
        String& json,
        const MeasurementRecord& record
    )
    {
        json += "{\"sequence\":";
        json += String(record.sequence);
        json += ",\"wakeCycle\":";
        json += String(record.wakeCycle);
        json += ",\"uptimeSeconds\":";
        json += String(record.uptimeSeconds);
        json += ",\"fillPercent\":";
        json += String(record.fillPercent);
        json += ",\"waterLevelCm\":";
        json += String(record.waterLevelCm, 1);
        json += ",\"distanceCm\":";
        json += String(record.distanceCm, 1);
        json += ",\"batteryVoltage\":";
        json += String(record.batteryVoltage, 2);
        json += ",\"batteryPercent\":";
        json += String(record.batteryPercent);
        json += ",\"sensorStatus\":\"";
        json += record.sensorStatus;
        json += "\"}";
    }

    bool rewriteHistory()
    {
        File temporaryFile =
            LittleFS.open(
                HISTORY_TEMP_PATH,
                "w"
            );

        if (!temporaryFile)
        {
            return false;
        }

        temporaryFile.print("[");

        for (
            size_t index = 0;
            index < recordCount;
            index++
        )
        {
            if (index > 0)
            {
                temporaryFile.print(",");
            }

            String object;
            object.reserve(240);

            appendRecordJson(
                object,
                records[index]
            );

            temporaryFile.print(object);
        }

        temporaryFile.print("]");
        temporaryFile.flush();

        const bool writeSucceeded =
            temporaryFile.getWriteError() == 0;

        temporaryFile.close();

        if (!writeSucceeded)
        {
            LittleFS.remove(
                HISTORY_TEMP_PATH
            );

            return false;
        }

        if (LittleFS.exists(HISTORY_PATH))
        {
            LittleFS.remove(HISTORY_PATH);
        }

        if (
            !LittleFS.rename(
                HISTORY_TEMP_PATH,
                HISTORY_PATH
            )
        )
        {
            LittleFS.remove(
                HISTORY_TEMP_PATH
            );

            return false;
        }

        return true;
    }

    void loadHistory()
    {
        recordCount = 0;
        nextSequence = 1;

        if (!LittleFS.exists(HISTORY_PATH))
        {
            return;
        }

        File file =
            LittleFS.open(
                HISTORY_PATH,
                "r"
            );

        if (
            !file ||
            file.size() > MAX_HISTORY_FILE_SIZE
        )
        {
            if (file)
            {
                file.close();
            }

            return;
        }

        String content =
            file.readString();

        file.close();
        content.trim();

        if (
            !content.startsWith("[") ||
            !content.endsWith("]")
        )
        {
            return;
        }

        int searchFrom = 0;

        while (
            recordCount < MAX_HISTORY_ENTRIES
        )
        {
            const int objectStart =
                content.indexOf(
                    '{',
                    searchFrom
                );

            if (objectStart < 0)
            {
                break;
            }

            const int objectEnd =
                content.indexOf(
                    '}',
                    objectStart
                );

            if (objectEnd < 0)
            {
                break;
            }

            MeasurementRecord record;

            if (
                parseRecord(
                    content.substring(
                        objectStart,
                        objectEnd + 1
                    ),
                    record
                )
            )
            {
                records[recordCount] = record;
                recordCount++;

                if (
                    record.sequence >=
                    nextSequence
                )
                {
                    nextSequence =
                        record.sequence + 1;
                }
            }

            searchFrom = objectEnd + 1;
        }
    }
}

bool MeasurementHistory::initialized = false;
bool MeasurementHistory::filesystemReady = false;

void MeasurementHistory::begin()
{
    if (
        initialized &&
        filesystemReady
    )
    {
        return;
    }

    filesystemReady =
        LittleFS.begin(true);

    if (!filesystemReady)
    {
        return;
    }

    initialized = true;
    loadHistory();
}

bool MeasurementHistory::addCurrentMeasurement()
{
    begin();

    if (
        !filesystemReady ||
        !Sensor::isValid() ||
        Sensor::isSimulated()
    )
    {
        return false;
    }

    MeasurementRecord record;

    record.sequence = nextSequence++;
    record.wakeCycle =
        Logger::getWakeCycleId();
    record.uptimeSeconds =
        millis() / 1000UL;
    record.fillPercent =
        Sensor::getPercentage();
    record.waterLevelCm =
        Sensor::getWaterLevelCm();
    record.distanceCm =
        Sensor::getDistanceCm();
    record.batteryVoltage =
        Battery::getVoltage();
    record.batteryPercent =
        Battery::getPercentage();
    record.sensorStatus = "OK";

    if (recordCount == MAX_HISTORY_ENTRIES)
    {
        for (
            size_t index = 1;
            index < recordCount;
            index++
        )
        {
            records[index - 1] =
                records[index];
        }

        recordCount--;
    }

    records[recordCount] = record;
    recordCount++;

    if (!rewriteHistory())
    {
        loadHistory();
        return false;
    }

    return true;
}

String MeasurementHistory::toJson()
{
    begin();

    String json;
    json.reserve(
        96 +
        recordCount * 240
    );

    json += "{\"count\":";
    json += String(recordCount);
    json += ",\"measurements\":[";

    for (
        size_t index = 0;
        index < recordCount;
        index++
    )
    {
        if (index > 0)
        {
            json += ",";
        }

        appendRecordJson(
            json,
            records[index]
        );
    }

    json += "]}";

    return json;
}

void MeasurementHistory::clear()
{
    begin();

    recordCount = 0;
    nextSequence = 1;

    if (!filesystemReady)
    {
        return;
    }

    LittleFS.remove(HISTORY_PATH);
    LittleFS.remove(HISTORY_TEMP_PATH);
}

size_t MeasurementHistory::count()
{
    begin();
    return recordCount;
}
