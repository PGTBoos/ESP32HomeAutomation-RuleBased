#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <Arduino.h>
#include "TimeSync.h"

class HomeSocketDevice;

class DisplayManager
{
private:
    U8G2_SH1106_128X64_NONAME_F_HW_I2C display;
    bool displayFound = false;
    int currentPage = 0;
    unsigned long lastPageChange = 0;
    const unsigned long PAGE_DURATION = 3500;

    void showPowerPage(float importPower, float exportPower, float totalImport, float totalExport);
    void showEnvironmentPage(float temp, float humidity, float light);
    void showSwitchesPage(const bool switches[], const String switchTimes[], HomeSocketDevice *const sockets[]);
    void showInfoPage();

public:
    DisplayManager() : display(U8G2_R0, /* reset= */ U8X8_PIN_NONE) {}
    bool begin();

    // Basic display update (without info page)
public:
    void showStartupProgress(const char *message, bool success = false);
    void updateDisplay(float importPower, float exportPower, float totalImport, float totalExport,
                       float temp, float humidity, float light,
                       const bool switches[], const String switchTimes[], HomeSocketDevice *const sockets[]);
};

#endif