// TimeSync.cpp
#include "TimeSync.h"

bool TimeSync::isDST(const tm *timeinfo) {
  // Dutch DST rules:
  // Starts last Sunday in March at 01:00 UTC (02:00 local)
  // Ends last Sunday in October at 01:00 UTC (02:00 local)

  if (timeinfo->tm_mon < 2 || timeinfo->tm_mon > 9)
    return false; // Jan, Feb, Nov, Dec
  if (timeinfo->tm_mon > 2 && timeinfo->tm_mon < 9)
    return true; // Apr-Sep

  // March: check if after last Sunday
  if (timeinfo->tm_mon == 2) {
    // Find last Sunday in March
    tm march = *timeinfo;
    march.tm_mday = 31;
    mktime(&march); // Normalize
    int lastSunday = march.tm_mday - march.tm_wday;
    return (timeinfo->tm_mday > lastSunday) ||
           (timeinfo->tm_mday == lastSunday && timeinfo->tm_hour >= 2);
  }

  // October: check if before last Sunday
  if (timeinfo->tm_mon == 9) {
    // Find last Sunday in October
    tm oct = *timeinfo;
    oct.tm_mday = 31;
    mktime(&oct); // Normalize
    int lastSunday = oct.tm_mday - oct.tm_wday;
    return (timeinfo->tm_mday < lastSunday) ||
           (timeinfo->tm_mday == lastSunday && timeinfo->tm_hour < 2);
  }

  return false;
}

bool TimeSync::begin() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - cannot sync time");
    return false;
  }

  // First configure with CET (will be adjusted by isDST)
  configTime(gmtOffset_sec, 0, ntpServer, ntpServer2, ntpServer3);

  Serial.println("Attempting to sync with Dutch NTP servers...");

  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  const int retry_count = 20;  // Number of retries
  const int retry_delay = 500; // ms between retries

  while (!getLocalTime(&timeinfo) && ++retry < retry_count) {
    Serial.printf("NTP Sync attempt %d/%d\n", retry, retry_count);
    if (retry == 5) {
      Serial.println("Initial NTP servers not responding, trying backup servers...");
    }
    delay(retry_delay);
  }

  if (getLocalTime(&timeinfo)) {
    // Apply DST configuration if needed
    if (isDST(&timeinfo)) {
      setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
      tzset();
      daylightOffset_sec = 3600; // Set DST offset
    } else {
      setenv("TZ", "CET-1", 1);
      tzset();
      daylightOffset_sec = 0; // No DST offset
    }

    char time_str[25];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    Serial.println("✓ Time synchronized successfully!");
    Serial.printf("Current time: %s\n", time_str);
    Serial.printf("Timezone: %s (UTC+%d)\n",
                  isDST(&timeinfo) ? "CEST" : "CET",
                  (gmtOffset_sec + daylightOffset_sec) / 3600);
    Serial.printf("DST is %s\n", isDST(&timeinfo) ? "active" : "not active");

    timeInitialized = true;
    return true;
  } else {
    Serial.println("× Failed to sync time after multiple attempts");
    Serial.println("Diagnostic information:");
    Serial.printf("WiFi status: %d\n", WiFi.status());
    Serial.printf("WiFi SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.println("Please check:");
    Serial.println("1. WiFi connection is stable");
    Serial.println("2. NTP ports (123 UDP) aren't blocked");
    Serial.println("3. DNS resolution is working");
    timeInitialized = false;
    return false;
  }
}

int TimeSync::getCurrentDayOfWeek() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return -1;
  }
  return timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday;
}

bool TimeSync::isWorkday() {
  int dayOfWeek = getCurrentDayOfWeek();
  return dayOfWeek >= 1 && dayOfWeek <= 5;
}

bool TimeSync::isWeekend() {
  int dayOfWeek = getCurrentDayOfWeek();
  return dayOfWeek == 6 || dayOfWeek == 7;
}
bool TimeSync::isSunday() { return getCurrentDayOfWeek() == 7; }
bool TimeSync::isMonday() { return getCurrentDayOfWeek() == 1; }
bool TimeSync::isTuesday() { return getCurrentDayOfWeek() == 2; }
bool TimeSync::isWednesday() { return getCurrentDayOfWeek() == 3; }
bool TimeSync::isThursday() { return getCurrentDayOfWeek() == 4; }
bool TimeSync::isFriday() { return getCurrentDayOfWeek() == 5; }
bool TimeSync::isSaturday() { return getCurrentDayOfWeek() == 6; }

void TimeSync::getCurrentHourMinute(int &hour, int &minute) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    Serial.printf("Current time: %02d:%02d\n", hour, minute);
  } else {
    hour = 12;
    minute = 0;
    Serial.println("âš  Could not get current time, using default (12:00)");
    timeInitialized = false; // Mark time as not synchronized
  }
}

String TimeSync::getCurrentTime() {
  // Try to get time using our TimeData structure for consistency with rules
  TimeData currentTime = getTime();

  // Check if time was successfully obtained
  if (currentTime.year == 0) {
    Serial.println("⚠ Failed to obtain time");
    timeInitialized = false;

    // Check cooldown period
    unsigned long now = millis();
    if (now - lastResyncAttempt < RESYNC_COOLDOWN) {
      Serial.println("× Skipping resync - in cooldown period");
      return "TimeFail";
    }

    // Only attempt resync if we have WiFi
    if (WiFi.status() == WL_CONNECTED) {
      lastResyncAttempt = now;
      Serial.println("Attempting time resync...");

      if (begin()) { // Try to resync
        // Get time again after resync
        currentTime = getTime();
        if (currentTime.year != 0) {
          Serial.println("✓ Time resync successful");
          char timeString[9];
          sprintf(timeString, "%02d:%02d:%02d", currentTime.hour, currentTime.minute, 0);
          return String(timeString);
        }
      }
      Serial.println("× Time resync failed");
    } else {
      Serial.println("× Cannot resync time - WiFi not connected");
    }
    return "TimeFail";
  }

  char timeString[9];
  sprintf(timeString, "%02d:%02d:%02d", currentTime.hour, currentTime.minute, 0);
  return String(timeString);
}

bool TimeSync::isTimeBetween(const char *startTime, const char *endTime) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("âš  Failed to get time for comparison");
    timeInitialized = false; // Mark time as not synchronized
    return false;
  }

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  // Parse start time (format "HH:MM")
  int startHour, startMin;
  sscanf(startTime, "%d:%d", &startHour, &startMin);
  int startMinutes = startHour * 60 + startMin;

  // Parse end time
  int endHour, endMin;
  sscanf(endTime, "%d:%d", &endHour, &endMin);
  int endMinutes = endHour * 60 + endMin;

  // Debug time information
  Serial.printf("Time check - Current: %02d:%02d (%d min), ", timeinfo.tm_hour,
                timeinfo.tm_min, currentMinutes);
  Serial.printf("Start: %02d:%02d (%d min), ", startHour, startMin,
                startMinutes);
  Serial.printf("End: %02d:%02d (%d min)\n", endHour, endMin, endMinutes);

  if (endMinutes < startMinutes) { // Handles overnight periods
    bool isInRange =
        currentMinutes >= startMinutes || currentMinutes <= endMinutes;
    Serial.printf("Overnight period check: %s\n", isInRange ? "true" : "false");
    return isInRange;
  }

  bool isInRange =
      currentMinutes >= startMinutes && currentMinutes <= endMinutes;
  Serial.printf("Same-day period check: %s\n", isInRange ? "true" : "false");
  return isInRange;
}

int TimeSync::getCurrentMinutes() {
  int hour, minute;
  getCurrentHourMinute(hour, minute);
  return hour * 60 + minute;
}

int TimeSync::getTimezoneOffsetMinutes() const {
  return (gmtOffset_sec + daylightOffset_sec) / 60;
}

TimeSync::TimeData TimeSync::getTime() {
  TimeData t = {0};
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {
    t.year = timeinfo.tm_year + 1900;
    t.month = timeinfo.tm_mon + 1;
    // Convert to 1-7 where Monday=1 and Sunday=7
    t.dayOfWeek = timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday;
    t.hour = timeinfo.tm_hour;
    t.minute = timeinfo.tm_min;
    t.weekNum = ((timeinfo.tm_yday + 7 - timeinfo.tm_wday) / 7) + 1;
    // day of the year
    t.dayOfYear = timeinfo.tm_yday;
  } else {
    Serial.println("Failed to get time");
  }
  return t;
}