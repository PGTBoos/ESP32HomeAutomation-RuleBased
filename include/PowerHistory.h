#ifndef POWER_HISTORY_H
#define POWER_HISTORY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

struct PowerDataPoint
{
    float import_wh;    // Import in Wh (or W for minute data)
    float export_wh;    // Export in Wh (or W for minute data)
    uint32_t timestamp; // Unix timestamp or millis
};

class PowerHistory
{
public:
    PowerHistory();

    // Call these from main loop at appropriate intervals
    void updateMinute(float importW, float exportW);  // Call every minute
    void updateHour(float importWh, float exportWh);  // Call every hour
    void updateDay(float importKwh, float exportKwh); // Call at midnight

    // Get data for web interface
    String getMinuteDataJson();
    String getHourDataJson();
    String getDayDataJson();
    String getMonthDataJson();

    // Load/save daily data from SPIFFS
    void loadFromSpiffs();
    void saveToSpiffs();

    // Timing helpers
    bool shouldUpdateMinute();
    bool shouldUpdateHour();
    bool shouldUpdateDay();

    // Get current totals for hour/day accumulation
    void resetHourAccumulator();
    void resetDayAccumulator();
    void addToHourAccumulator(float importW, float exportW, unsigned long deltaMs);
    void addToDayAccumulator(float importWh, float exportWh);

private:
    // Circular buffers
    static const int MINUTE_POINTS = 60;
    static const int HOUR_POINTS = 24;
    static const int DAY_POINTS = 7;
    static const int MONTH_POINTS = 30;

    PowerDataPoint minuteData[MINUTE_POINTS];
    PowerDataPoint hourData[HOUR_POINTS];
    PowerDataPoint dayData[DAY_POINTS];
    PowerDataPoint monthData[MONTH_POINTS];

    int minuteIndex = 0;
    int hourIndex = 0;
    int dayIndex = 0;
    int monthIndex = 0;

    int minuteCount = 0;
    int hourCount = 0;
    int dayCount = 0;
    int monthCount = 0;

    // Timing
    unsigned long lastMinuteUpdate = 0;
    unsigned long lastHourUpdate = 0;
    int lastDayOfYear = -1;
    int lastHourOfDay = -1;

    // Accumulators for averaging/summing
    float hourImportAccum = 0;
    float hourExportAccum = 0;
    int hourSampleCount = 0;

    float dayImportAccum = 0;
    float dayExportAccum = 0;

    // Helper to create JSON array from circular buffer
    String bufferToJson(PowerDataPoint *buffer, int count, int currentIndex, int maxSize, const char *unit);
};

extern PowerHistory powerHistory;

#endif
