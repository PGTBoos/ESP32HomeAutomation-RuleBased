// TimeSync.h
#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

class TimeSync
{
private:
    const char *ntpServer = "nl.pool.ntp.org";    // Netherlands NTP pool
    const char *ntpServer2 = "0.nl.pool.ntp.org"; // Specific Dutch server
    const char *ntpServer3 = "1.nl.pool.ntp.org"; // Backup Dutch server
    const long gmtOffset_sec = 3600;              // Netherlands is UTC+1
    // int gmtOffset_sec = 3600;                     // CET is UTC+1 (3600 seconds)
    int daylightOffset_sec = 0; // Starts with no DST offset       // DST when applicable
    bool timeInitialized = false;

    // Netherlands timezone: UTC+1 (CET) or UTC+2 (CEST)
    const int cetOffset = 3600;  // CET is UTC+1
    const int cestOffset = 7200; // CEST is UTC+2

    bool isDST(const tm *timeinfo);
    unsigned long lastResyncAttempt = 0;
    static const unsigned long RESYNC_COOLDOWN = 300000; // 5 min delay before next NTP server resync attempt

public:
    int getTimezoneOffsetMinutes() const;
    TimeSync() {}
    bool begin();
    void getCurrentHourMinute(int &hour, int &minute);
    String getCurrentTime();
    int getCurrentDayOfWeek();

    bool isWorkday();
    bool isWeekend();
    bool isSunday();
    bool isMonday();
    bool isTuesday();
    bool isWednesday();
    bool isThursday();
    bool isFriday();
    bool isSaturday();

    bool isTimeBetween(const char *startTime, const char *endTime);
    int getCurrentMinutes();
    bool isTimeSet() const { return timeInitialized; }

    int getDayOfWeek();  // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    int getWeekNumber(); // 1-53
    int getMonth();      // 1-12
    int getYear();       // Full year (e.g., 2024)

    // Optional: helper method to get all time components at once
    struct TimeData
    {
        int year;      // Full year (2024)
        int month;     // 1-12
        int dayOfWeek; // 1-7 (Mon-Sun)
        int weekNum;   // 1-53
        int hour;      // 0-23
        int minute;    // 0-59
        int dayOfYear; // 0-365
    };
    TimeData getTime(); // One function to get everything
};

#endif