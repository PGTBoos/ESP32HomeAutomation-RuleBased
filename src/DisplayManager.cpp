// DisplayManager.cpp
#include "DisplayManager.h"

#include <GlobalVars.h>
#include <WiFi.h>
#include <timeSync.h>

#include "HomeSocketDevice.h"

// Add method for showing startup progress
void DisplayManager::showStartupProgress(const char *message, bool success) {
  if (!displayFound)
    return;

  static int lineCount = 0;
  static bool firstCall = true;
  static bool dotsDrawn = false;
  static const int totalSteps = 9; // Count of initialization steps

  // Clear and draw all dots on first call
  if (firstCall) {
    display.clearBuffer();
    display.setFont(u8g2_font_profont10_tr);
    display.setDrawColor(1);
    display.setFontPosTop();

    // Draw dots for all expected steps
    for (int i = 0; i < totalSteps; i++) {
      int y = 8 + (i * 9);
      display.drawStr(0, y, ".");
      delay(500);
    }
    display.sendBuffer();

    lineCount = 0;
    firstCall = false;
    dotsDrawn = true;
    delay(500); // Brief pause to show the dots
  }

  // Overwrite the dot with the actual message
  if (dotsDrawn && lineCount < totalSteps) {
    display.clearBuffer();

    // Redraw all previous completed messages
    static String completedMessages[10];
    completedMessages[lineCount] = String(message);

    for (int i = 0; i <= lineCount; i++) {
      int y = 8 + (i * 9);
      if (i < lineCount) {
        display.drawStr(0, y, completedMessages[i].c_str());
      } else {
        display.drawStr(0, y, message);
      }
    }

    // Draw remaining dots
    for (int i = lineCount + 1; i < totalSteps; i++) {
      int y = 8 + (i * 9);
      display.drawStr(0, y, ".");
    }
    delay(1000);
    display.sendBuffer();
    lineCount++;
  }

  // Special handling for "Ready" message - reset everything for normal operation
  if (strcmp(message, "Ready") == 0) {
    delay(3500);
    display.clearBuffer();
    display.sendBuffer();
    // Reset static variables for next boot
    lineCount = 0;
    firstCall = true;
    dotsDrawn = false;
  }
}

// Assuming timeSync is a global or class member variable
bool DisplayManager::begin() {
  Serial.println("\nInitializing \nOLED display...");

  if (!display.begin()) {
    Serial.println("SH1106 allocation failed");
    return false;
  }
  display.setContrast(64);
  // Setup display parameters
  display.setDisplayRotation(U8G2_R1);
  display.setFont(u8g2_font_profont10_tr); // Default font for labels
  display.setDrawColor(1);
  display.setFontPosTop();
  display.clearBuffer();

  // Draw initial test pattern
  display.drawStr(5, 8, "Starting.!!");
  display.drawFrame(0, 0, display.getWidth(), display.getHeight());
  display.sendBuffer();

  displayFound = true;
  currentPage = 0;
  lastPageChange = millis() - PAGE_DURATION;
  Serial.println("Display initialized successfully!");

  return true;
}

void DisplayManager::showPowerPage(float importPower, float exportPower, float totalImport, float totalExport) {
  if (!displayFound)
    return;

  display.clearBuffer();

  // Title
  display.setFont(u8g2_font_profont10_tr);
  display.drawBox(0, 0, 64, 12);
  display.setDrawColor(0);
  display.drawStr(2, 2, "Power");
  display.setDrawColor(1);

  // Function to format power value
  auto formatPower = [](float power) -> String {
    if (abs(power) >= 1000) {
      // Display in kW with 2 decimals
      return String(power / 1000.0, 2) + " kW";
    } else {
      // Display in Watt with no decimals
      return String((int)power) + " Watt";
    }
  };

  // Import
  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 17, "Import:");
  display.setFont(u8g2_font_7x14_tr);
  display.setCursor(0, 27);
  display.print(formatPower(importPower));

  // Export
  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 45, "Export:");
  display.setFont(u8g2_font_7x14_tr);
  display.setCursor(0, 55);
  display.print(formatPower(exportPower));

  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 73, "Total:");

  // daily import and export
  float dailyImport = (config.yesterday > 0)
                          ? (totalImport - config.yesterdayImport)
                          : totalImport;
  float dailyExport = (config.yesterday > 0)
                          ? (totalExport - config.yesterdayExport)
                          : totalExport;

  // Daily import (used)
  display.setFont(u8g2_font_profont10_tr);
  String totalImportStr = "-" + String(dailyImport, 1) + " kWh";
  int totalWidth = display.getStrWidth(totalImportStr.c_str());
  display.setCursor(64 - totalWidth, 83);
  display.print(totalImportStr);

  // Daily export (produced)
  String totalExportStr = "+" + String(dailyExport, 1) + " kWh";
  totalWidth = display.getStrWidth(totalExportStr.c_str());
  display.setCursor(64 - totalWidth, 93);
  display.print(totalExportStr);

  display.sendBuffer();
}

void DisplayManager::showEnvironmentPage(float temp, float humidity,
                                         float light) {
  if (!displayFound)
    return;

  display.clearBuffer();

  // Title
  display.setFont(u8g2_font_profont10_tr);
  display.drawBox(0, 0, 64, 12);
  display.setDrawColor(0);
  display.drawStr(2, 2, "Environment");
  display.setDrawColor(1);

  // Temperature
  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 17, "Temperature:");
  display.setFont(u8g2_font_7x14_tr);
  display.setCursor(0, 27);
  display.print(temp, 1);
  display.print(" C'");

  // Humidity
  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 45, "Humidity:");
  display.setFont(u8g2_font_7x14_tr);
  display.setCursor(0, 55);
  display.print(humidity, 0);
  display.print(" %");

  // Light
  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 73, "Light:");
  display.setFont(u8g2_font_7x14_tr);
  display.setCursor(0, 83);
  display.print(light, 0);
  display.print(" Lux");

  display.sendBuffer();
}
void DisplayManager::showSwitchesPage(const bool switches[],
                                      const String switchTimes[],
                                      HomeSocketDevice *const sockets[]) {
  if (!displayFound)
    return;
  display.clearBuffer();

  // Title
  display.setFont(u8g2_font_profont10_tr);
  display.drawBox(0, 0, 64, 12);
  display.setDrawColor(0);
  display.drawStr(2, 2, "Switches");

  display.setDrawColor(1);

  if (phoneCheck && phoneCheck->isDevicePresent()) {
    display.drawStr(0, 17, "Phone found");
  } else {
    display.drawStr(0, 17, "No phone");
  }

  auto drawSwitch = [&](int x, bool state, bool isOnline) {
    const int y = 35;
    const int radius = 6;

    if (!isOnline) {
      display.drawCircle(x, y, radius);
      display.drawLine(x - 4, y - 4, x + 4, y + 4);
      display.drawLine(x + 4, y - 4, x - 4, y + 4);
    } else if (state) {
      display.drawDisc(x, y, radius);
    } else {
      display.drawCircle(x, y, radius);
    }
  };

  const int diameter = 12;
  const int spacing = 5; // Increased from 2 to 5
  const int startX = 6;

  for (int i = 0; i < NUM_SOCKETS; i++) {
    int x = startX + (i * (diameter + spacing));
    // Add null pointer check before dereferencing
    bool isConnected = (sockets[i] != nullptr) ? sockets[i]->isConnected() : false;
    drawSwitch(x, switches[i], isConnected);
  }

  display.setFont(u8g2_font_profont10_tr);
  display.setCursor(0, 60);
  display.print(lastActiveRuleName);

  // Display the time when the rule was last applied on the next line
  display.setCursor(0, 70);
  display.print(lastActiveRuleTimeStr);

  display.sendBuffer();
}
void DisplayManager::showInfoPage() {
  if (!displayFound)
    return;

  display.clearBuffer();

  // Title
  display.setFont(u8g2_font_profont10_tr);
  display.drawBox(0, 0, 64, 12);
  display.setDrawColor(0);
  display.drawStr(2, 2, "System Info");
  display.setDrawColor(1);

  // Time
  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 17, "Time:");
  display.setFont(u8g2_font_7x14_tr);
  display.setCursor(0, 27);
  display.print(timeSync.getCurrentTime()); // Get time directly from TimeSync

  // WiFi Status
  display.setFont(u8g2_font_profont10_tr);
  display.drawStr(0, 45, "WiFi:");
  display.setFont(u8g2_font_7x14_tr);
  display.setCursor(30, 45); // Moved cursor right to align with "WiFi:"
  if (WiFi.status() == WL_CONNECTED) {
    display.print("Yes");

    // IP Address
    display.setFont(u8g2_font_robot_de_niro_tn);
    IPAddress ip = WiFi.localIP();

    // Calculate positions with 5 pixels per character
    const int charWidth = 5;
    const int blockWidth = charWidth * 3 + 1; // Each block is 3 digits
    const int startX = 0;
    const int y = 60; // Moved up to be closer to WiFi status

    // Print numbers
    display.setDrawColor(1);
    display.setCursor(startX, y);
    display.printf("%03d", ip[0]);

    display.setCursor(startX + blockWidth, y);
    display.printf("%03d", ip[1]);

    display.setCursor(startX + (blockWidth * 2), y);
    display.printf("%03d", ip[2]);

    display.setCursor(startX + (blockWidth * 3), y);
    display.printf("%03d", ip[3]);
  } else {
    display.print("Off");
  }

  // RAM Info
  display.setFont(u8g2_font_profont10_tr);
  uint32_t totalRam = ESP.getHeapSize() / 1024; // Total RAM in KB
  uint32_t freeRam = ESP.getFreeHeap() / 1024;  // Free RAM in KB
  display.drawStr(0, 75, "RAM:");
  display.setFont(u8g2_font_robot_de_niro_tn);
  display.setCursor(25,
                    75); // Moved from 25 to 30 to give more space after "RAM:"
  display.printf("%d", totalRam);
  display.setCursor(48,
                    75); // Moved from 45 to 55 to accommodate larger numbers
  display.printf("(%d)", freeRam);

  display.sendBuffer();
}

void DisplayManager::updateDisplay(float importPower, float exportPower,
                                   float totalImport, float totalExport,
                                   float temp, float humidity, float light,
                                   const bool switches[],
                                   const String switchTimes[],
                                   HomeSocketDevice *const sockets[]) {

  Serial.printf("UpdateDisplay called - currentPage: %d\n", currentPage);
  // Rotate pages every PAGE_DURATION milliseconds
  if (millis() - lastPageChange >= PAGE_DURATION) {
    currentPage = (currentPage + 1) % 4; // Cycle through 4 pages
    lastPageChange = millis();
  } else {
    return;
  }

  // Show current page
  switch (currentPage) {
  case 0:
    showPowerPage(importPower, exportPower, totalImport, totalExport);
    break;
  case 1:
    showEnvironmentPage(temp, humidity, light);
    break;
  case 2:
    showSwitchesPage(switches, switchTimes, sockets);
    break;
  case 3:
    showInfoPage(); // Now using the parameter-less version
    break;
  }

  // // if mobile phone is on the network in the top bar of the display in the
  // right corner create a small black square (remind that the display is 90
  // degree turned) if (phoneCheck->isPhoneConnected()) {
  //   display.drawBox(120, 0, 4, 4);
  //   display.sendBuffer();
}