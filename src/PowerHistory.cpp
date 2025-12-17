#include "PowerHistory.h"
#include "TimeSync.h"

extern TimeSync timeSync;

PowerHistory powerHistory;

PowerHistory::PowerHistory() {
    // Initialize all buffers to zero
    memset(minuteData, 0, sizeof(minuteData));
    memset(hourData, 0, sizeof(hourData));
    memset(dayData, 0, sizeof(dayData));
    memset(monthData, 0, sizeof(monthData));
}

bool PowerHistory::shouldUpdateMinute() {
    unsigned long now = millis();
    if (now - lastMinuteUpdate >= 60000) { // 60 seconds
        lastMinuteUpdate = now;
        return true;
    }
    return false;
}

bool PowerHistory::shouldUpdateHour() {
    TimeSync::TimeData t = timeSync.getTime();
    if (lastHourOfDay != t.hour) {
        lastHourOfDay = t.hour;
        return true;
    }
    return false;
}

bool PowerHistory::shouldUpdateDay() {
    TimeSync::TimeData t = timeSync.getTime();
    if (lastDayOfYear != -1 && lastDayOfYear != t.dayOfYear) {
        lastDayOfYear = t.dayOfYear;
        return true;
    }
    if (lastDayOfYear == -1) {
        lastDayOfYear = t.dayOfYear;
    }
    return false;
}

void PowerHistory::updateMinute(float importW, float exportW) {
    minuteData[minuteIndex].import_wh = importW;
    minuteData[minuteIndex].export_wh = exportW;
    minuteData[minuteIndex].timestamp = millis();
    
    minuteIndex = (minuteIndex + 1) % MINUTE_POINTS;
    if (minuteCount < MINUTE_POINTS) minuteCount++;
    
    // Add to hour accumulator (convert W to Wh: W * (1/60) hour)
    addToHourAccumulator(importW, exportW, 60000);
    
    Serial.printf("PowerHistory > Minute update: Import=%.1fW, Export=%.1fW\n", importW, exportW);
}

void PowerHistory::updateHour(float importWh, float exportWh) {
    hourData[hourIndex].import_wh = importWh;
    hourData[hourIndex].export_wh = exportWh;
    hourData[hourIndex].timestamp = millis();
    
    hourIndex = (hourIndex + 1) % HOUR_POINTS;
    if (hourCount < HOUR_POINTS) hourCount++;
    
    // Add to day accumulator
    addToDayAccumulator(importWh, exportWh);
    
    Serial.printf("PowerHistory > Hour update: Import=%.1fWh, Export=%.1fWh\n", importWh, exportWh);
}

void PowerHistory::updateDay(float importKwh, float exportKwh) {
    // Update 7-day buffer
    dayData[dayIndex].import_wh = importKwh;
    dayData[dayIndex].export_wh = exportKwh;
    dayData[dayIndex].timestamp = millis();
    
    dayIndex = (dayIndex + 1) % DAY_POINTS;
    if (dayCount < DAY_POINTS) dayCount++;
    
    // Update 30-day buffer
    monthData[monthIndex].import_wh = importKwh;
    monthData[monthIndex].export_wh = exportKwh;
    monthData[monthIndex].timestamp = millis();
    
    monthIndex = (monthIndex + 1) % MONTH_POINTS;
    if (monthCount < MONTH_POINTS) monthCount++;
    
    Serial.printf("PowerHistory > Day update: Import=%.2fkWh, Export=%.2fkWh\n", importKwh, exportKwh);
    
    // Save to SPIFFS
    saveToSpiffs();
}

void PowerHistory::addToHourAccumulator(float importW, float exportW, unsigned long deltaMs) {
    // Convert W to Wh: W * (hours) = W * (ms / 3600000)
    float hours = deltaMs / 3600000.0f;
    hourImportAccum += importW * hours;
    hourExportAccum += exportW * hours;
    hourSampleCount++;
}

void PowerHistory::addToDayAccumulator(float importWh, float exportWh) {
    dayImportAccum += importWh;
    dayExportAccum += exportWh;
}

void PowerHistory::resetHourAccumulator() {
    float importWh = hourImportAccum;
    float exportWh = hourExportAccum;
    
    hourImportAccum = 0;
    hourExportAccum = 0;
    hourSampleCount = 0;
    
    // Store the hour data
    updateHour(importWh, exportWh);
}

void PowerHistory::resetDayAccumulator() {
    float importKwh = dayImportAccum / 1000.0f;
    float exportKwh = dayExportAccum / 1000.0f;
    
    dayImportAccum = 0;
    dayExportAccum = 0;
    
    // Store the day data
    updateDay(importKwh, exportKwh);
}

String PowerHistory::bufferToJson(PowerDataPoint* buffer, int count, int currentIndex, int maxSize, const char* unit) {
    StaticJsonDocument<2048> doc;
    JsonArray imports = doc.createNestedArray("import");
    JsonArray exports = doc.createNestedArray("export");
    
    // Read from oldest to newest
    int startIndex = (currentIndex - count + maxSize) % maxSize;
    
    for (int i = 0; i < count; i++) {
        int idx = (startIndex + i) % maxSize;
        imports.add(buffer[idx].import_wh);
        exports.add(buffer[idx].export_wh);
    }
    
    doc["count"] = count;
    doc["unit"] = unit;
    
    String result;
    serializeJson(doc, result);
    return result;
}

String PowerHistory::getMinuteDataJson() {
    return bufferToJson(minuteData, minuteCount, minuteIndex, MINUTE_POINTS, "W");
}

String PowerHistory::getHourDataJson() {
    return bufferToJson(hourData, hourCount, hourIndex, HOUR_POINTS, "Wh");
}

String PowerHistory::getDayDataJson() {
    return bufferToJson(dayData, dayCount, dayIndex, DAY_POINTS, "kWh");
}

String PowerHistory::getMonthDataJson() {
    return bufferToJson(monthData, monthCount, monthIndex, MONTH_POINTS, "kWh");
}

void PowerHistory::saveToSpiffs() {
    StaticJsonDocument<2048> doc;
    
    // Save day data (7 days)
    JsonArray days = doc.createNestedArray("days");
    int startIdx = (dayIndex - dayCount + DAY_POINTS) % DAY_POINTS;
    for (int i = 0; i < dayCount; i++) {
        int idx = (startIdx + i) % DAY_POINTS;
        JsonObject day = days.createNestedObject();
        day["i"] = dayData[idx].import_wh;
        day["e"] = dayData[idx].export_wh;
    }
    
    // Save month data (30 days)
    JsonArray months = doc.createNestedArray("months");
    startIdx = (monthIndex - monthCount + MONTH_POINTS) % MONTH_POINTS;
    for (int i = 0; i < monthCount; i++) {
        int idx = (startIdx + i) % MONTH_POINTS;
        JsonObject month = months.createNestedObject();
        month["i"] = monthData[idx].import_wh;
        month["e"] = monthData[idx].export_wh;
    }
    
    doc["dayIdx"] = dayIndex;
    doc["dayCount"] = dayCount;
    doc["monthIdx"] = monthIndex;
    doc["monthCount"] = monthCount;
    
    File file = SPIFFS.open("/power_history.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("PowerHistory > Saved to SPIFFS");
    } else {
        Serial.println("PowerHistory > Failed to save to SPIFFS");
    }
}

void PowerHistory::loadFromSpiffs() {
    File file = SPIFFS.open("/power_history.json", "r");
    if (!file) {
        Serial.println("PowerHistory > No history file found, starting fresh");
        return;
    }
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("PowerHistory > Failed to parse history file");
        return;
    }
    
    // Load day data
    JsonArray days = doc["days"];
    dayCount = min((int)days.size(), DAY_POINTS);
    dayIndex = doc["dayIdx"] | 0;
    
    int i = 0;
    for (JsonObject day : days) {
        if (i >= DAY_POINTS) break;
        int idx = (dayIndex - dayCount + i + DAY_POINTS) % DAY_POINTS;
        dayData[idx].import_wh = day["i"] | 0.0f;
        dayData[idx].export_wh = day["e"] | 0.0f;
        i++;
    }
    
    // Load month data
    JsonArray months = doc["months"];
    monthCount = min((int)months.size(), MONTH_POINTS);
    monthIndex = doc["monthIdx"] | 0;
    
    i = 0;
    for (JsonObject month : months) {
        if (i >= MONTH_POINTS) break;
        int idx = (monthIndex - monthCount + i + MONTH_POINTS) % MONTH_POINTS;
        monthData[idx].import_wh = month["i"] | 0.0f;
        monthData[idx].export_wh = month["e"] | 0.0f;
        i++;
    }
    
    Serial.printf("PowerHistory > Loaded from SPIFFS: %d days, %d months\n", dayCount, monthCount);
}
