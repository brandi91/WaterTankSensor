#include "measurement_history.h"
#include "sleep_manager.h"

#include <LittleFS.h>
#include <math.h>

#include "battery.h"
#include "logger.h"
#include "sensor.h"
#include "settings.h"
#include "time_manager.h"

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
        uint32_t timestamp = 0;
        String timeSource;
        int fillPercent = 0;
        float waterLevelCm = 0.0f;
        float distanceCm = 0.0f;
        float batteryVoltage = 0.0f;
        int batteryPercent = -1;
        bool batteryValid = false;
        String sensorStatus;
        bool simulated = false;
    };

    MeasurementRecord records[MAX_HISTORY_ENTRIES];
    size_t recordCount = 0;
    uint32_t nextSequence = 1;

    bool extractToken(
        const String& object,
        const String& key,
        String& value
    )
    {
        const String marker = "\"" + key + "\":";
        const int markerIndex = object.indexOf(marker);

        if (markerIndex < 0)
        {
            return false;
        }

        int start = markerIndex + marker.length();

        while (
            start < object.length() &&
            object.charAt(start) == ' '
        )
        {
            start++;
        }

        int end = start;

        while (
            end < object.length() &&
            object.charAt(end) != ',' &&
            object.charAt(end) != '}'
        )
        {
            end++;
        }

        value = object.substring(start, end);
        value.trim();
        return !value.isEmpty();
    }

    bool extractString(
        const String& object,
        const String& key,
        String& value
    )
    {
        const String marker = "\"" + key + "\":\"";
        const int markerIndex = object.indexOf(marker);

        if (markerIndex < 0)
        {
            return false;
        }

        const int start = markerIndex + marker.length();
        const int end = object.indexOf('"', start);

        if (end < 0)
        {
            return false;
        }

        value = object.substring(start, end);
        return true;
    }

    bool parseRecord(
        const String& object,
        MeasurementRecord& record
    )
    {
        String token;

        if (
            !extractToken(object, "sequence", token) ||
            token.toInt() < 1
        )
        {
            return false;
        }
        record.sequence = token.toInt();

        if (!extractToken(object, "timestamp", token))
        {
            return false;
        }
        record.timestamp =
            static_cast<uint32_t>(strtoul(token.c_str(), nullptr, 10));

        if (
            !extractString(object, "timeSource", record.timeSource) ||
            !extractToken(object, "fillPercent", token)
        )
        {
            return false;
        }
        record.fillPercent = token.toInt();

        if (!extractToken(object, "waterLevelCm", token))
        {
            return false;
        }
        record.waterLevelCm = token.toFloat();

        if (!extractToken(object, "distanceCm", token))
        {
            return false;
        }
        record.distanceCm = token.toFloat();

        if (!extractToken(object, "batteryVoltage", token))
        {
            return false;
        }
        record.batteryVoltage = token.toFloat();

        if (!extractToken(object, "batteryPercent", token))
        {
            return false;
        }
        record.batteryPercent = token.toInt();

        if (!extractToken(object, "batteryValid", token))
        {
            return false;
        }
        record.batteryValid = token == "true";

        if (
            !extractString(object, "sensorStatus", record.sensorStatus) ||
            !extractToken(object, "simulated", token)
        )
        {
            return false;
        }
        record.simulated = token == "true";

        return
            record.fillPercent >= 0 &&
            record.fillPercent <= 100 &&
            isfinite(record.waterLevelCm) &&
            record.waterLevelCm >= 0.0f &&
            isfinite(record.distanceCm) &&
            record.distanceCm >= 0.0f &&
            isfinite(record.batteryVoltage) &&
            record.batteryVoltage >= 0.0f &&
            record.batteryPercent >= -1 &&
            record.batteryPercent <= 100;
    }

    void appendRecordJson(
        String& json,
        const MeasurementRecord& record
    )
    {
        json += "{\"sequence\":";
        json += String(record.sequence);
        json += ",\"timestamp\":";
        json += String(record.timestamp);
        json += ",\"timeSource\":\"";
        json += record.timeSource;
        json += "\",\"fillPercent\":";
        json += String(record.fillPercent);
        json += ",\"waterLevelCm\":";
        json += String(record.waterLevelCm, 1);
        json += ",\"distanceCm\":";
        json += String(record.distanceCm, 1);
        json += ",\"batteryVoltage\":";
        json += String(record.batteryVoltage, 2);
        json += ",\"batteryPercent\":";
        json += String(record.batteryPercent);
        json += ",\"batteryValid\":";
        json += record.batteryValid ? "true" : "false";
        json += ",\"sensorStatus\":\"";
        json += record.sensorStatus;
        json += "\",\"simulated\":";
        json += record.simulated ? "true" : "false";
        json += "}";
    }

    bool rewriteHistory()
    {
        File file = LittleFS.open(HISTORY_TEMP_PATH, "w");

        if (!file)
        {
            Logger::error("History temporary file open failed");
            return false;
        }

        bool writeSucceeded = file.print("[") == 1;

        for (
            size_t index = 0;
            writeSucceeded && index < recordCount;
            index++
        )
        {
            String object;
            object.reserve(280);

            if (index > 0)
            {
                writeSucceeded = file.print(",") == 1;
            }

            appendRecordJson(object, records[index]);
            writeSucceeded =
                writeSucceeded &&
                file.print(object) == object.length();
        }

        writeSucceeded =
            writeSucceeded &&
            file.print("]") == 1;
        file.flush();
        writeSucceeded =
            writeSucceeded &&
            file.getWriteError() == 0;
        file.close();

        if (!writeSucceeded)
        {
            LittleFS.remove(HISTORY_TEMP_PATH);
            Logger::error("History file write failed");
            return false;
        }

        LittleFS.remove(HISTORY_PATH);

        if (!LittleFS.rename(HISTORY_TEMP_PATH, HISTORY_PATH))
        {
            LittleFS.remove(HISTORY_TEMP_PATH);
            Logger::error("History file replacement failed");
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

        File file = LittleFS.open(HISTORY_PATH, "r");

        if (!file)
        {
            Logger::warning("History file open failed");
            return;
        }

        if (file.size() > MAX_HISTORY_FILE_SIZE)
        {
            file.close();
            Logger::warning("History file exceeds size limit");
            return;
        }

        String content = file.readString();
        file.close();
        content.trim();

        if (
            !content.startsWith("[") ||
            !content.endsWith("]")
        )
        {
            Logger::warning("History file is malformed");
            return;
        }

        int position = 0;

        while (recordCount < MAX_HISTORY_ENTRIES)
        {
            const int start = content.indexOf('{', position);

            if (start < 0)
            {
                break;
            }

            const int end = content.indexOf('}', start);

            if (end < 0)
            {
                Logger::warning("History record is malformed");
                break;
            }

            MeasurementRecord record;

            if (parseRecord(content.substring(start, end + 1), record))
            {
                records[recordCount++] = record;
                nextSequence =
                    max(nextSequence, record.sequence + 1);
            }

            position = end + 1;
        }
    }
}

bool MeasurementHistory::initialized = false;
bool MeasurementHistory::filesystemReady = false;

void MeasurementHistory::begin()
{
    if (initialized)
    {
        return;
    }

    if (!LittleFS.begin(true))
    {
        filesystemReady = false;
        Logger::error("Measurement history: LittleFS unavailable");
        return;
    }

    filesystemReady = true;
    initialized = true;
    loadHistory();
}

bool MeasurementHistory::addCurrentMeasurement()
{
    if (!SleepManager::canStartNormalWork())
    {
        return false;
    }

    if (!initialized)
    {
        Logger::warning(
            "Measurement history unavailable: not initialized"
        );
        return false;
    }

    const bool sensorValid = Sensor::isValid();
    const bool simulated = Sensor::isSimulated();
    const float waterLevel = Sensor::getWaterLevelCm();
    const float distance = Sensor::getDistanceCm();
    const int fillPercent = Sensor::getPercentage();
    const bool calculatedValuesValid =
        isfinite(waterLevel) &&
        isfinite(distance) &&
        waterLevel >= 0.0f &&
        waterLevel <= Settings::data.tankHeight &&
        distance >= 0.0f &&
        fillPercent >= 0 &&
        fillPercent <= 100;

    if (
        !filesystemReady ||
        !sensorValid ||
        !calculatedValuesValid
    )
    {
        Logger::warning(
            "Measurement history rejected: fs=" +
            String(filesystemReady ? "ready" : "unavailable") +
            ", sensorValid=" +
            String(sensorValid ? "true" : "false") +
            ", simulated=" +
            String(simulated ? "true" : "false") +
            ", count=" +
            String(recordCount)
        );
        return false;
    }

    MeasurementRecord record;
    record.sequence = nextSequence++;
    record.timestamp =
        static_cast<uint32_t>(TimeManager::now());
    record.timeSource =
        TimeManager::getStorageTimeSource();
    record.fillPercent = fillPercent;
    record.waterLevelCm = waterLevel;
    record.distanceCm = distance;
    record.batteryVoltage = Battery::getVoltage();
    record.batteryPercent = Battery::getPercentage();
    record.batteryValid = Battery::isValid();
    record.sensorStatus = simulated ? "Simulated" : "OK";
    record.simulated = simulated;

    if (recordCount == MAX_HISTORY_ENTRIES)
    {
        for (size_t index = 1; index < recordCount; index++)
        {
            records[index - 1] = records[index];
        }

        recordCount--;
    }

    records[recordCount++] = record;

    if (!rewriteHistory())
    {
        loadHistory();
        Logger::warning(
            "Measurement history entry was not stored; count=" +
            String(recordCount)
        );
        return false;
    }

    return true;
}

String MeasurementHistory::toJson()
{
    String json;
    json.reserve(96 + recordCount * 280);
    json += "{\"count\":";
    json += String(recordCount);
    json += ",\"measurements\":[";

    for (size_t index = 0; index < recordCount; index++)
    {
        if (index > 0)
        {
            json += ",";
        }

        appendRecordJson(json, records[index]);
    }

    json += "]}";
    return json;
}

void MeasurementHistory::clear()
{
    recordCount = 0;
    nextSequence = 1;

    if (filesystemReady)
    {
        LittleFS.remove(HISTORY_PATH);
        LittleFS.remove(HISTORY_TEMP_PATH);
    }
}

size_t MeasurementHistory::count()
{
    return recordCount;
}
