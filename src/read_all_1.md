
_______________ filename: .clang-format _______________
Language: Cpp
BreakBeforeBraces: Attach

BraceWrapping:
  AfterControlStatement: true  # Braces for if, else, for, etc. on same line
  AfterFunction: true        # Braces for functions on same line
  BeforeCatch: true
  BeforeElse: true

# Other common settings you might want:
IndentWidth: 2
TabWidth: 2
UseTab: Never
AllowShortFunctionsOnASingleLine: All
ColumnLimit: 0
_______________ filename: collect.ps1 _______________
# Function to get next available filename by finding the highest existing number
function Get-NextAvailableFilename {
    param (
        [string]$baseFilename = "read_all"
    )
    
    # Get all existing files matching the pattern
    $existingFiles = Get-ChildItem -Path "." -Filter "${baseFilename}_*.md"
    
    # Find the highest number
    $highestNum = 0
    foreach ($file in $existingFiles) {
        if ($file.Name -match "${baseFilename}_(\d+)\.md") {
            $num = [int]$matches[1]
            if ($num -gt $highestNum) {
                $highestNum = $num
            }
        }
    }
    
    # Use next number after the highest found
    $nextNum = $highestNum + 1
    return "${baseFilename}_${nextNum}.md"
}

# Get unique filename
$outputFile = Get-NextAvailableFilename

# Define excluded extensions
$excludedExtensions = @(
    ".json",
    ".jpg", ".jpeg", ".png", ".gif", ".bmp", 
    ".tiff", ".ico", ".svg", ".webp",
    ".md"
)

# Initialize empty string for content
$markdownContent = ""

# Get all files from specified directories
$files = Get-ChildItem -Path ".\" -File -Recurse | 
    Where-Object { $excludedExtensions -notcontains $_.Extension }

foreach ($file in $files) {
    # Create a visible separator with filename
    $separator = "_______________ filename: $($file.Name) _______________"
    $markdownContent += [char]10+$separator
    $markdownContent += [char]10+(Get-Content $file.FullName -Raw)
}

# Write content to file
$markdownContent | Out-File -FilePath $outputFile -Encoding UTF8

Write-Host "Files have been combined into $outputFile"
Write-Host "Excluded file types: $($excludedExtensions -join ', ')"
Write-Host "Total files processed: $($files.Count)"
_______________ filename: DisplayManager.cpp _______________
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
_______________ filename: EnvironmentSensor.cpp _______________
// EnvironmentSensors.cpp
#include "EnvironmentSensor.h"

bool EnvironmentSensors::begin() {
  Wire.begin(); // Start I2C

  // Initialize BME280
  bmeFound = bme.begin(0x76); // Try first address
  if (!bmeFound) {
    bmeFound = bme.begin(0x77); // Try alternate address
  }
  if (!bmeFound) {
    Serial.println("Could not find BME280 sensor!");
  }

  // Initialize BH1750
  lightMeterFound = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (!lightMeterFound) {
    Serial.println("Could not find BH1750 sensor!");
  } else {
    lightMeter.setMTreg(64);     // Set measurement time to 400ms
    lightMeter.readLightLevel(); // Read once to start measurement

    Serial.println("BH1750 sensor found!");
  }

  return bmeFound ||
         lightMeterFound; // Return true if at least one sensor works
}

void EnvironmentSensors::update() {
  if (bmeFound) {
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F; // Convert to hPa
  }

  if (lightMeterFound) {
    lightLevel = lightMeter.readLightLevel();
  }
}

// Getter methods
float EnvironmentSensors::getTemperature() const {
  return temperature;
}

float EnvironmentSensors::getHumidity() const {
  return humidity;
}

float EnvironmentSensors::getPressure() const {
  return pressure;
}

float EnvironmentSensors::getLightLevel() const {
  return lightLevel;
}

// Status methods
bool EnvironmentSensors::hasBME280() const {
  return bmeFound;
}

bool EnvironmentSensors::hasBH1750() const {
  return lightMeterFound;
}
_______________ filename: GlobalVars.cpp _______________
const char *lastActiveRuleName = "none";
int lastActiveRuleSocket = 0;
char lastActiveRuleTimeStr[12] = "--:-- xxx";
_______________ filename: HomeP1Device.cpp _______________
// HomeP1Device.cpp
#include "HomeP1Device.h"

HomeP1Device::HomeP1Device(const char *ip)
    : baseUrl("http://" + String(ip)), lastImportPower(0), lastExportPower(0),
      lastTotalImport(0), lastTotalExport(0), lastReadTime(0),
      lastReadSuccess(false) {
}
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

void HomeP1Device::update() {
  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadSuccess = getPowerData(lastImportPower, lastExportPower);
    lastReadTime = millis();
  }
}

bool HomeP1Device::getPowerData(float &importPower, float &exportPower) {
  http.begin(client, baseUrl + "/api/v1/data");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      float power = doc["active_power_w"].as<float>();
      lastTotalImport = doc["total_power_import_kwh"].as<float>();
      lastTotalExport = doc["total_power_export_kwh"].as<float>();

      Serial.printf("Received P1 power data: %.2f W\n", power);
      Serial.printf("Today total import: %.2f kWh\n", lastTotalImport);
      Serial.printf("Today total export: %.2f kWh\n", lastTotalExport);

      importPower = max(power, 0);
      exportPower = max(-power, 0);

      lastImportPower = importPower;
      lastExportPower = exportPower;

      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

float HomeP1Device::getCurrentImport() const {
  return lastImportPower;
}

float HomeP1Device::getCurrentExport() const {
  return lastExportPower;
}

float HomeP1Device::getTotalImport() const {
  return lastTotalImport;
}

float HomeP1Device::getTotalExport() const {
  return lastTotalExport;
}

float HomeP1Device::getNetPower() const {
  return lastImportPower - lastExportPower;
}

bool HomeP1Device::isConnected() const {
  return lastReadSuccess;
}
_______________ filename: HomeSocketDevice.cpp _______________
#include "HomeSocketDevice.h"

HomeSocketDevice::HomeSocketDevice(const char *ip)
    : baseUrl("http://" + String(ip)), lastKnownState(false), lastReadTime(0),
      lastReadSuccess(false), consecutiveFailures(0), deviceIP(ip),
      lastLogTime(0) {
  Serial.printf("Initializing socket device at IP: %s\n", ip);
}

void HomeSocketDevice::readStateInfo() {
  unsigned long currentTime = millis();

  // Only try updating if enough time has passed since last attempt
  if (currentTime - lastReadTime < READ_INTERVAL) {
    return;
  }

  // Calculate backoff time based on failures (max 60 seconds)
  unsigned long backoffTime = min(consecutiveFailures * 5000UL, 60000UL);
  if (currentTime - lastReadTime < backoffTime) {
    return;
  }

  // Regular status check
  bool previousState = lastKnownState;
  if (!getState()) {
    consecutiveFailures++;
    if (currentTime - lastLogTime >= 30000) {
      Serial.printf("PowerSocket > %s > Status > Offline (retry in %lu sec)\n",
                    deviceIP.c_str(), backoffTime / 1000);
      lastLogTime = currentTime;
    }
  } else {
    if (consecutiveFailures > 0) {
      Serial.printf("PowerSocket > %s > Status > Back online\n",
                    deviceIP.c_str());
      lastLogTime = currentTime;
    }
    consecutiveFailures = 0;

    // If the state changed from our last known state, log it
    if (previousState != lastKnownState) {
      Serial.printf("PowerSocket > %s > State changed from %s to %s\n",
                    deviceIP.c_str(), previousState ? "ON" : "OFF",
                    lastKnownState ? "ON" : "OFF");
    }
  }

  lastReadTime = currentTime;
  delay(100);
}

bool HomeSocketDevice::makeHttpRequest(const String &endpoint,
                                       const String &method,
                                       const String &payload,
                                       String &response) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClient newClient;
  HTTPClient http;
  newClient.setTimeout(5000);

  String fullUrl = baseUrl + "/api/v1/state";

  if (!http.begin(newClient, fullUrl)) {
    newClient.stop();
    return false;
  }

  http.setTimeout(5000);
  http.setReuse(false);

  int httpCode;
  if (method == "GET") {
    httpCode = http.GET();
  } else if (method == "PUT") {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.PUT(payload);
  }

  bool success = (httpCode == HTTP_CODE_OK);
  if (success) {
    response = http.getString();
  }

  http.end();
  newClient.stop();
  return success;
}

bool HomeSocketDevice::getState() {
  String response;
  if (!makeHttpRequest("/api/v1/state", "GET", "", response)) {
    Serial.printf("PowerSocket > %s/api/v1/state > Get > HTTP error\n",
                  deviceIP.c_str());
    lastReadSuccess = false;
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.printf("PowerSocket > %s/api/v1/state > Get > JSON error\n",
                  deviceIP.c_str());
    lastReadSuccess = false;
    return false;
  }

  lastKnownState = doc["power_on"] | false;
  Serial.printf("PowerSocket > %s/api/v1/state > Get > is %s\n",
                deviceIP.c_str(), lastKnownState ? "on" : "off");
  lastReadSuccess = true;
  return true;
}

bool HomeSocketDevice::setState(bool state) {
  Serial.printf("setState(%s) called for socket %s\n", state ? "true" : "false",
                deviceIP.c_str());
  StaticJsonDocument<128> doc;
  doc["power_on"] = state;
  String jsonString;
  serializeJson(doc, jsonString);

  String response;
  if (!makeHttpRequest("/api/v1/state", "PUT", jsonString, response)) {
    Serial.printf("PowerSocket > %s/api/v1/state > Put > HTTP error\n",
                  deviceIP.c_str());
    return false;
  }

  lastKnownState = state;
  Serial.printf("PowerSocket > %s/api/v1/state > Put > turn %s\n",
                deviceIP.c_str(), state ? "on" : "off");
  return true;
}
_______________ filename: HomeSystem.code-workspace _______________
{
    "folders": [
		{
			"name": "HomeSystem",
			"path": ".."
		}
	]
}
_______________ filename: main.cpp _______________
#include "main.h"

// Global variable definitions
TimingControl timing;
Config config;
DisplayManager display;
EnvironmentSensors sensors;
HomeP1Device *p1Meter = nullptr;
// SimpleRuleEngine ruleEngine;

HomeSocketDevice *sockets[NUM_SOCKETS] = {nullptr, nullptr, nullptr, nullptr};
unsigned long lastStateChangeTime[NUM_SOCKETS] = {0, 0, 0, 0};
bool switchForceOff[NUM_SOCKETS] = {false, false, false, false};
SmartRuleSystem ruleSystem;
TimeSync timeSync;
WebInterface webServer;
NetworkCheck *phoneCheck = nullptr;

unsigned long lastTimeDisplay = 0;

bool loadConfiguration() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return false;
  }

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  // Print raw config file content
  Serial.println("\nRaw config file content:");
  Serial.println("------------------------");
  while (configFile.available()) {
    Serial.write(configFile.read());
  }
  Serial.println("\n------------------------");

  // Reset file pointer to start
  configFile.seek(0);

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }

  // Load configuration
  config.wifi_ssid = doc["wifi_ssid"].as<String>();
  config.wifi_password = doc["wifi_password"].as<String>();
  config.p1_ip = doc["p1_ip"].as<String>();

  for (int i = 0; i < NUM_SOCKETS; i++) {
    String key = "socket_" + String(i + 1);
    config.socket_ip[i] = doc[key].as<String>();
    Serial.printf("Loaded %s: %s\n", key.c_str(), config.socket_ip[i].c_str());
  }

  // config.socket_1 = doc["socket_1"].as<String>();
  // config.socket_2 = doc["socket_2"].as<String>();
  // config.socket_3 = doc["socket_3"].as<String>();
  config.power_on_threshold = doc["power_on_threshold"] | 1000.0f;
  config.power_off_threshold = doc["power_off_threshold"] | 990.0f;
  config.min_on_time = doc["min_on_time"] | 300UL;
  config.min_off_time = doc["min_off_time"] | 300UL;
  config.max_on_time = doc["max_on_time"] | 1800UL;
  config.phone_ip = doc["phone_ip"].as<String>();

  return true;
}

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
  // give the ip stack som time
  delay(200);
  yield();
  delay(300);
}

bool canChangeState(int switchIndex, bool newState) {
  unsigned long currentTime = millis();
  unsigned long timeSinceChange =
      currentTime - lastStateChangeTime[switchIndex];

  if (newState) { // Turning ON
    if (switchForceOff[switchIndex] && timeSinceChange < config.min_off_time) {
      return false;
    }
    switchForceOff[switchIndex] = false;
  } else { // Turning OFF
    if (timeSinceChange < config.min_on_time) {
      return false;
    }
  }

  return true;
}

void checkMaxOnTime() {
  unsigned long currentTime = millis();

  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (sockets[i] && sockets[i]->getCurrentState() &&
        (currentTime - lastStateChangeTime[i]) > config.max_on_time) {
      sockets[i]->setState(false);
      switchForceOff[i] = true;
      lastStateChangeTime[i] = currentTime;
    }
  }
}

void updateDisplay() {
  if (!p1Meter) {
    Serial.println("P1Meter object not initialized");
    p1Meter = new HomeP1Device(config.p1_ip.c_str());
    return;
  }

  bool switchStates[NUM_SOCKETS];
  String switchTimes[NUM_SOCKETS];

  for (int i = 0; i < NUM_SOCKETS; i++) {
    switchStates[i] = sockets[i] ? sockets[i]->getCurrentState() : false;
    switchTimes[i] = String(millis() - lastStateChangeTime[i]);
  }

  display.updateDisplay(p1Meter->getCurrentImport(),
                        p1Meter->getCurrentExport(), p1Meter->getTotalImport(),
                        p1Meter->getTotalExport(), sensors.getTemperature(),
                        sensors.getHumidity(), sensors.getLightLevel(),
                        switchStates, switchTimes,
                        sockets // Add the sockets array
  );
}

void setupRules() {
  auto &rs = ruleSystem;

  static char morningEndTime[6];
  static char eveningEndTime[6];
  static char weekendEndTime[6];
  static char nightOffTime[6];

  snprintf(morningEndTime, 6, "%s", rs.addMinutesToTime("07:45", rs.getDailyRandom60(0) % 36));
  snprintf(eveningEndTime, 6, "%s", rs.addMinutesToTime("23:00", rs.getDailyRandom(0) % 24));
  snprintf(weekendEndTime, 6, "%s", rs.addMinutesToTime("23:00", rs.getDailyRandom60(1) % 54));
  snprintf(nightOffTime, 6, "%s", rs.addMinutesToTime("23:55", rs.getDailyRandom(1) % 5));

  // Morning rule (weekdays) - varies end time by 0-35 minutes after 07:45
  rs.addRule(1, "Good morning",
             rs.period("07:10", "07:44", // Ends at 07:44 to ensure off by 07:45
                       rs.allOf({rs.lightBelow(5), rs.isWorkday()})));

  // Morning OFF rule - EXACTLY at 07:45 as departure reminder
  rs.addRule(1, "Leave for car",
             rs.offAfter("07:45", 2,       // 2 minute window to ensure it turns off
                         rs.isWorkday())); // Only on workdays

  // Evening rule (weekdays)
  rs.addRule(1, "Evening",
             rs.period("17:15", eveningEndTime, // Changed to period()
                       rs.allOf({rs.lightBelow(5), rs.isWorkday()})));

  rs.addRule(1, "Good night",
             rs.offAfter(eveningEndTime, 2, // 2 minute window to ensure it turns off
                         rs.isWorkday()));  // Only on workdays

  // Weekend rule with phone presence
  rs.addRule(1, "Weekend",
             rs.period("19:00", weekendEndTime, // Changed to period()
                       rs.allOf({rs.lightBelow(5), rs.phoneNotPresent(), rs.isWeekend()})));

  // Add explicit weekend off rule
  rs.addRule(1, "Weekend night",
             rs.offAfter(weekendEndTime, 2, // Turns OFF at calculated end time
                         rs.isWeekend()));

  // Late night off rule
  rs.addRule(1, "Night off",
             rs.offAfter(nightOffTime, 5));
  // Smart solar heater rule that maintains different ON/OFF thresholds
  // this one is quite complex and uses the P1Meter object
  rs.addRule(3, "Solar Heater",
             [&]() { // Replace the entire rule evaluation with a new lambda
               // First check time window - exit early if outside window
               if (!timeSync.isTimeBetween("07:00", "19:00")) {
                 return RuleDecision::Skip; // Exit immediately outside time window
               }

               // Check rate limiting
               unsigned long now = millis();
               if (now - lastHeaterCheck < HEATER_CHECK_INTERVAL) {
                 return RuleDecision::Skip; // Exit if too soon
               }
               lastHeaterCheck = now;

               // Only do heater control if we passed time window and rate limit
               return rs.solarHeaterControl(
                   1020,  // Export threshold
                   5,     // Import threshold
                   60000, // Minimum ON time
                   30000, // Minimum OFF time
                   []() { // Phone presence check
                     return phoneCheck && !phoneCheck->isDevicePresent();
                   })();
             });

  rs.addRule(4, "TV ambient on",
             rs.onCondition(rs.allOf({rs.phonePresent(), rs.after("18:00"), rs.lightAbove(11)})));

  rs.addRule(4, "TV ambient off",
             rs.offCondition(rs.lightBelow(5)));
}

void setup() {
  // Initialize WiFi and disable persistent settings
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Start serial communication
  Serial.begin(115200);
  delay(100); // Give serial time to initialize

  // Initialize SPIFFS for configuration storage
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }

  // Load configuration from SPIFFS
  if (!loadConfiguration()) {
    Serial.println("Using default configuration");
  }

  // Initialize I2C for sensors and display
  bool wireInitialized = false;
  for (int i = 0; i < 3; i++) { // Retry I2C initialization up to 3 times
    Wire.end();                 // Ensure a clean start
    delay(50);
    if (Wire.begin()) {
      wireInitialized = true;
      break;
    }
  }

  if (!wireInitialized) {
    Serial.println("FATAL: Failed to initialize I2C after 3 attempts!");
    return;
  }

  // Initialize display first
  bool displayOK = false;
  if (display.begin()) {
    Serial.println("Display initialized successfully");
    displayOK = true;
    delay(500); // Show the "Starting.!!" message briefly
  } else {
    Serial.println("Display not connected or initialization failed!");
  }

  // Now show progress messages using your exact text
  if (displayOK) {
    display.showStartupProgress("I2C OK", true);
    delay(200);

    display.showStartupProgress("Config OK", true);
    delay(200);
  }

  // Initialize environmental sensors
  bool sensorsOK = sensors.begin();
  if (displayOK) {
    display.showStartupProgress("BME280 OK", sensorsOK);
    delay(200);
  }
  if (sensorsOK) {
    Serial.println("Environmental sensors initialized successfully");
  } else {
    Serial.println("Environmental sensors not connected or initialization failed!");
  }

  // Connect to WiFi
  connectWiFi();
  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  if (displayOK) {
    display.showStartupProgress("Wifi OK", wifiOK);
    delay(200);
  }

  // Initialize time synchronization
  bool timeOK = false;
  if (WiFi.status() == WL_CONNECTED) {
    timeOK = timeSync.begin();
    if (displayOK) {
      String msg = "NTP " + timeSync.getCurrentTime();
      display.showStartupProgress(msg.c_str(), timeOK);
      delay(200);
    }
  }

  // Initialize web server
  webServer.begin();
  if (displayOK) {
    display.showStartupProgress("WEB OK", true);
    delay(200);
  }

  // Initialize sockets
  int socketsInitialized = 0;
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (config.socket_ip[i] != "" && config.socket_ip[i] != "0" &&
        config.socket_ip[i] != "null") {
      sockets[i] = new HomeSocketDevice(config.socket_ip[i].c_str());
      socketsInitialized++;
      Serial.printf("Socket %d initialized at: %s\n", i + 1,
                    config.socket_ip[i].c_str());
    }
  }

  if (displayOK) {
    char socketMsg[12];
    snprintf(socketMsg, sizeof(socketMsg), "Sockets %d", socketsInitialized);
    display.showStartupProgress(socketMsg, socketsInitialized > 0);
    delay(200);
  }

  // Initialize SmartRuleSystem and synchronize socket states
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (sockets[i]) {
      // Use the public interface to set the physical state
      ruleSystem.pollPhysicalStates(); // Update physical states
      Serial.printf("Socket %d: Physical State = %s, Virtual State = %s\n",
                    i + 1, sockets[i]->getCurrentState() ? "On" : "Off",
                    ruleSystem.getSocketState(i) ? "On" : "Off");
    }
  }

  // Set up rules
  setupRules();
  if (displayOK) {
    display.showStartupProgress("Rules Loaded", true);
    delay(200);
  }

  // Initialize phone presence check (if configured)
  bool phoneOK = false;
  if (config.phone_ip != "" && config.phone_ip != "0" &&
      config.phone_ip != "null") {
    phoneCheck = new NetworkCheck(config.phone_ip.c_str());
    phoneOK = true;
    Serial.println("Phone check initialized at: " + config.phone_ip);
  }

  if (displayOK) {
    display.showStartupProgress("Phone found", phoneOK);
    delay(300);
  }

  // Show completion message
  if (displayOK) {
    display.showStartupProgress("Ready", true);
    delay(800);

    // Clear the display and let normal operation begin
    // The first updateDisplay call will happen in the main loop
  }

  Serial.println("Setup complete!");
}

void reconnectWiFi() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED &&
      (currentMillis - timing.lastWiFiCheck >= timing.WIFI_CHECK_INTERVAL ||
       timing.lastWiFiCheck == 0)) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    timing.lastWiFiCheck = currentMillis;
  }
}

static int yesterday;
static uint16_t operationOrder = 0;
static unsigned long lastRuleCheck = 0;

void loop() {
  unsigned long currentMillis = millis();

  // Use static counter to sequence for ALL operations

  File file;

  switch (operationOrder) {
  case 0:
    file = SPIFFS.open("/daily_totals.json", "r");
    if (file) {
      StaticJsonDocument<128> doc;
      DeserializationError error = deserializeJson(doc, file);
      file.close();

      if (!error) {
        config.yesterday = doc["day"] | 0;
        config.yesterdayImport = doc["import"] | 0.0f;
        config.yesterdayExport = doc["export"] | 0.0f;

        Serial.println("\nLoaded previous day totals:");
        Serial.printf("Day: %d\n", config.yesterday);
        Serial.printf("Import: %.2f kWh\n", config.yesterdayImport);
        Serial.printf("Export: %.2f kWh\n", config.yesterdayExport);
      } else {
        Serial.println("Error parsing daily totals file");
        config.yesterday = 0;
        config.yesterdayImport = 0;
        config.yesterdayExport = 0;
      }
    } else {
      Serial.println("No previous day totals found");
      config.yesterday = 0;
      config.yesterdayImport = 0;
      config.yesterdayExport = 0;
    }

    //    Serial.println (ruleEngine.isWeekday(SimpleRuleEngine::WEEKDAYS));

    // print if it it is a weekday
    // print if it it is a weekday

    operationOrder = 5;

    break;

  case 5: // Environmental sensor (I2C) - Temperature/Humidity
    if (currentMillis - timing.lastEnvSensorUpdate >=
        timing.ENV_SENSOR_INTERVAL) {
      sensors.update(); // Assuming this method exists, if not we use
                        // sensors.update()
      timing.lastEnvSensorUpdate = currentMillis;
      yield();
      delay(1);
    }
    operationOrder = 10;
    break;

  case 10:
    if (WiFi.status() != WL_CONNECTED) {
      reconnectWiFi();
      yield();
      delay(8000);
      reconnectWiFi();
      // operationOrder = 10; // Go back to start if no WiFi
    } else {
      operationOrder = 12;
    }
    break;

  case 12: // Light sensor (I2C)
    if (currentMillis - timing.lastLightSensorUpdate >=
        timing.LIGHT_SENSOR_INTERVAL) {
      sensors.update(); // Assuming this method exists, if not we use
                        // sensors.update()
      timing.lastLightSensorUpdate = currentMillis;
      yield();
      delay(1);
    }
    operationOrder = 20;
    break;

  case 20: // Display update (I2C)

    if (currentMillis - timing.lastDisplayUpdate >= timing.DISPLAY_INTERVAL) {
      Serial.printf("Display case 20 - time since last update: %lu ms\n",
                    currentMillis - timing.lastDisplayUpdate);
      Serial.println("Updating display...");
      updateDisplay();
      timing.lastDisplayUpdate = currentMillis;
      yield();
      delay(17);
      Serial.println("Display update complete");
    }
    operationOrder = 30;
    break;

  case 30: // P1 meter (Network)
    if (p1Meter &&
        (currentMillis - timing.lastP1Update >= timing.P1_INTERVAL)) {
      p1Meter->update();
      Serial.printf("****** P1 meter update - Import: %.2f W, Export: %.2f W\n",
                    p1Meter->getCurrentImport(), p1Meter->getCurrentExport()); // it reporst zero here as well ??
      timing.lastP1Update = currentMillis;
      yield();
      delay(49);
    }
    operationOrder = 40;
    break;

  case 40: // Socket updates (Network)
    for (int i = 0; i < NUM_SOCKETS; i++) {
      if (sockets[i] && (currentMillis - timing.lastSocketUpdates[i] >=
                         timing.SOCKET_INTERVAL)) {
        sockets[i]->readStateInfo();
        timing.lastSocketUpdates[i] = currentMillis;
        // Handle socket-specific logic
        if (i == 0 && p1Meter) {
          // updateSwitch1Logic();
        } else if (i == 1) {
          //  updateSwitch2Logic();
        } else if (i == 2) {
          // updateSwitch3Logic();
        }
        yield();
        delay(50);
      }
    }
    operationOrder = 70;
    break;

  case 70: // Max on time check (no I2C or network)
    // checkMaxOnTime();
    operationOrder = 80;
    break;

  case 80: // Web server (Network)
    webServer.update();
    operationOrder = 90; // Back to start
    yield();
    break;

  case 90: // Phone presence check
    if (phoneCheck && (currentMillis - timing.lastPhoneCheck >=
                       timing.PHONE_CHECK_INTERVAL)) {
      if (phoneCheck->isDevicePresent()) {
        Serial.println("Phone is detected");
        // Add your logic for when phone is present
      } else {
        Serial.println("Phone is not detected");
        // Add your logic for when phone is absent
      }
      timing.lastPhoneCheck = currentMillis;
      operationOrder = 100;
      yield();
      delay(50); // Give some time between network operations
    } else {
      operationOrder = 100;
    }
    break;

  case 100: {
    if (!p1Meter) {
      operationOrder = 1000;
      break;
    }

    static int lastSavedDay = config.yesterday;
    int currentDay = timeSync.getTime().dayOfYear;

    if (lastSavedDay == 0) {
      lastSavedDay = currentDay;
      config.yesterday = currentDay;
      config.yesterdayImport = p1Meter->getTotalImport();
      config.yesterdayExport = p1Meter->getTotalExport();

      // Save initial values
      StaticJsonDocument<128> doc;
      doc["day"] = currentDay;
      doc["import"] = config.yesterdayImport;
      doc["export"] = config.yesterdayExport;

      File file = SPIFFS.open("/daily_totals.json", "w");
      if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.printf(
            "Initialized day totals - Day: %d, Import: %.3f, "
            "Export: %.3f\n",
            currentDay, config.yesterdayImport,
            config.yesterdayExport);
      }

      Serial.println("NEW DAY DETECTED - Updating rules with fresh random numbers");
      ruleSystem.clearRules(); // Clear existing rules
      setupRules();            // Setup rules with new daily randoms
    }

    // Only check for day change - remove the exact midnight check
    if (currentDay != lastSavedDay) {
      StaticJsonDocument<128> doc;
      doc["day"] = currentDay;
      doc["import"] = p1Meter->getTotalImport();
      doc["export"] = p1Meter->getTotalExport();

      File file = SPIFFS.open("/daily_totals.json", "w");
      if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.printf("Saved day %d totals to SPIFFS:\n", currentDay);
        Serial.printf("Import: %.2f kWh\n", doc["import"].as<float>());
        Serial.printf("Export: %.2f kWh\n", doc["export"].as<float>());

        // Update config values and lastSavedDay
        config.yesterday = currentDay;
        config.yesterdayImport = doc["import"].as<float>();
        config.yesterdayExport = doc["export"].as<float>();
        lastSavedDay = currentDay;
      }
    }
    operationOrder = 1000;
    break;
  }

    // lets do switching logic above 1000

  case 1000:
    if (millis() - lastRuleCheck < 1000) {
      operationOrder = 5;
      break;
    }
    lastRuleCheck = millis();
    ruleSystem.update();
    operationOrder = 5;
    break;

  default:
    Serial.printf("ERROR: Invalid operation order: %d\n", operationOrder);
    operationOrder = 5; // Reset to beginning
    break;
  }
}

_______________ filename: NetworkCheck.cpp _______________
// NetworkCheck.cpp
#include "NetworkCheck.h"

NetworkCheck::NetworkCheck(const char *ip)
    : deviceIP(ip), lastKnownState(false), lastCheckTime(0),
      consecutiveFailures(0) {
  Serial.printf("Network > %s > Check initialized\n", ip);
  // Force first check to happen immediately by setting lastCheckTime far in the past
  lastCheckTime = millis() - CHECK_INTERVAL - 1;
}

bool NetworkCheck::isDevicePresent() {
  unsigned long currentTime = millis();
  if (currentTime - lastCheckTime < CHECK_INTERVAL) {
    return lastKnownState;
  }

  lastCheckTime = currentTime;
  bool pingResult = pingDevice();

  if (pingResult) {
    if (!lastKnownState) { // Device just became available
      Serial.printf("Network > %s > Device detected\n", deviceIP.c_str());
    }
    consecutiveFailures = 0;
  } else {
    consecutiveFailures++;
    if (lastKnownState) { // Device just became unavailable
      Serial.printf("Network > %s > Device lost\n", deviceIP.c_str());
    }
  }

  lastKnownState = pingResult;
  return pingResult;
}

bool NetworkCheck::pingDevice() {
  bool success = Ping.ping(deviceIP.c_str(), 1); // 1 ping attempt
  if (success) {
    Serial.printf("Network > %s > Ping response: %.2fms\n", deviceIP.c_str(),
                  Ping.averageTime());
  }
  return success;
}
_______________ filename: SmartRuleSystem.cpp _______________
#include "SmartRuleSystem.h"
#include "GlobalVars.h"
#include <cstdint>
char SmartRuleSystem::timeBuffer[6];

int SmartRuleSystem::dailyRandom[10] = {0};
int SmartRuleSystem::dailyRandom60[5] = {0};
int SmartRuleSystem::dailyRandom24[3] = {0};
int SmartRuleSystem::lastGeneratedDay = -1;

SmartRuleSystem::SmartRuleSystem() : sockets(NUM_SOCKETS) {
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (::sockets[i]) {
      sockets[i].physicalState = ::sockets[i]->getCurrentState();
      sockets[i].virtualState = physicalToDecision(sockets[i].physicalState);
    }
  }
}

void SmartRuleSystem::addRule(int socketNumber,
                              const char *ruleName, // Add this
                              std::function<RuleDecision()> evaluate,
                              std::function<bool()> timeWindow) {
  Rule rule = {
      .socketNumber = socketNumber,
      .evaluate = evaluate,
      .timeWindow = timeWindow,
      .name = ruleName // Add this
  };
  rules.push_back(rule);
}

void SmartRuleSystem::generateDailyRandoms() {
  TimeSync::TimeData t = timeSync.getTime();

  if (lastGeneratedDay == t.dayOfYear) {
    return; // Already generated for today
  }

  // Seed with day + week for consistency
  srand(t.dayOfYear + t.weekNum * 7);

  // Generate all randoms for the day
  for (int i = 0; i < 10; i++) {
    dailyRandom[i] = rand() % 100;
  }

  for (int i = 0; i < 5; i++) {
    dailyRandom60[i] = rand() % 60;
  }

  for (int i = 0; i < 3; i++) {
    dailyRandom24[i] = rand() % 24;
  }

  lastGeneratedDay = t.dayOfYear;

  Serial.printf("Generated daily randoms for day %d:\n", t.dayOfYear);
  Serial.printf("Random 0-99: ");
  for (int i = 0; i < 10; i++)
    Serial.printf("%d ", dailyRandom[i]);
  Serial.printf("\nRandom 0-59: ");
  for (int i = 0; i < 5; i++)
    Serial.printf("%d ", dailyRandom60[i]);
  Serial.printf("\nRandom 0-23: ");
  for (int i = 0; i < 3; i++)
    Serial.printf("%d ", dailyRandom24[i]);
  Serial.println();
}

int SmartRuleSystem::getDailyRandom(int index) {
  generateDailyRandoms();
  if (index >= 0 && index < 10) {
    return dailyRandom[index];
  }
  return 0;
}

int SmartRuleSystem::getDailyRandom60(int index) {
  generateDailyRandoms();
  if (index >= 0 && index < 5) {
    return dailyRandom60[index];
  }
  return 0;
}

int SmartRuleSystem::getDailyRandom24(int index) {
  generateDailyRandoms();
  if (index >= 0 && index < 3) {
    return dailyRandom24[index];
  }
  return 0;
}

const char *SmartRuleSystem::addMinutesToTime(const char *baseTime, int minutesToAdd) {
  static char timeBuffer[6];

  int hour, minute;
  sscanf(baseTime, "%d:%d", &hour, &minute);

  minute += minutesToAdd;
  hour += minute / 60;
  minute = minute % 60;
  hour = hour % 24;

  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
  return timeBuffer;
}

const char *SmartRuleSystem::addHoursToTime(const char *baseTime, int hoursToAdd) {
  static char timeBuffer[6];

  int hour, minute;
  sscanf(baseTime, "%d:%d", &hour, &minute);

  hour = (hour + hoursToAdd) % 24;

  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
  return timeBuffer;
}

void SmartRuleSystem::update() {
  // 1. Get current physical states
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (::sockets[i]) {
      sockets[i].physicalState = ::sockets[i]->getCurrentState();
    }
  }

  // 2. Evaluate all rules and store final decisions
  for (const auto &rule : rules) {
    int socketIndex = rule.socketNumber - 1;
    if (socketIndex < 0 || socketIndex >= NUM_SOCKETS || !::sockets[socketIndex]) {
      continue;
    }

    // Get rule decision
    RuleDecision decision = rule.evaluate();

    Serial.printf("Socket %d: Rule '%s' evaluated to %s\n",
                  rule.socketNumber,
                  rule.name,
                  decision == RuleDecision::On ? "ON" : decision == RuleDecision::Off ? "OFF"
                                                                                      : "SKIP");

    // Only store non-SKIP decisions (lets later rules override earlier ones)
    if (decision != RuleDecision::Skip) {
      sockets[socketIndex].virtualState = decision;

      // Track the last active rule that turns something ON
      if (decision == RuleDecision::On) {
        lastActiveRuleName = rule.name;
        lastActiveRuleSocket = rule.socketNumber;
      }
    }
  }

  // 3. Apply the decisions
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (!::sockets[i])
      continue;

    // Only apply if we have a non-SKIP decision
    if (sockets[i].virtualState != RuleDecision::Skip) {
      bool targetState = (sockets[i].virtualState == RuleDecision::On);

      // Only change state if it's different
      if (targetState != sockets[i].physicalState) {
        if (::sockets[i]->setState(targetState)) {
          sockets[i].physicalState = targetState;
          sockets[i].lastStateChange = millis();
          lastActiveRuleTime = millis();                 // Update time
          lastActiveRuleState = sockets[i].virtualState; // Update state

          TimeSync::TimeData currentTime = timeSync.getTime();
          static char timeStr[12]; // Static buffer to store the formatted time
          snprintf(lastActiveRuleTimeStr, sizeof(lastActiveRuleTimeStr), "%02d:%02d %s",
                   currentTime.hour, currentTime.minute, (targetState ? "On" : "Off"));
          Serial.printf("Rule applied at %s: %s\n", lastActiveRuleTimeStr, lastActiveRuleName);
        }
      }
    }
  }
}

void SmartRuleSystem::pollPhysicalStates() {
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (::sockets[i]) {
      ::sockets[i]->readStateInfo(); // First actively read the current state
      sockets[i].physicalState = ::sockets[i]->getCurrentState();
    }
  }
}

void SmartRuleSystem::detectManualChanges() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (!::sockets[i])
      continue;

    auto &socket = sockets[i];
    RuleDecision currentPhysical = physicalToDecision(socket.physicalState);

    if (currentPhysical != socket.virtualState) {
      socket.virtualState = currentPhysical;
      socket.lastManualChange = now;
      socket.lastStateChange = now;
    }
  }
}

void SmartRuleSystem::evaluateRules() {
  // Track applied rules per socket
  std::vector<RuleDecision> lastDecisions(NUM_SOCKETS, RuleDecision::Skip);

  // Evaluate each rule in order
  for (const auto &rule : rules) {
    int socketIndex = rule.socketNumber - 1;
    if (socketIndex < 0 || socketIndex >= NUM_SOCKETS || !::sockets[socketIndex]) {
      continue;
    }

    // Get rule decision
    RuleDecision decision = rule.evaluate();

    Serial.printf("Socket %d: Rule evaluated to %s\n",
                  rule.socketNumber,
                  decision == RuleDecision::On ? "ON" : decision == RuleDecision::Off ? "OFF"
                                                                                      : "SKIP");

    if (decision == RuleDecision::On) {
      lastActiveRuleName = rule.name;
      lastActiveRuleSocket = rule.socketNumber;
    }

    // Only update if we get a non-skip decision
    if (decision != RuleDecision::Skip) {
      lastDecisions[socketIndex] = decision;
      sockets[socketIndex].virtualState = decision;

      // print if its a workday
      Serial.printf("It is a workday %s\n", timeSync.isWorkday() ? "true" : "false");
      Serial.printf("Socket %d: Virtual state updated to %s\n",
                    socketIndex + 1,
                    decision == RuleDecision::On ? "ON" : "OFF");
    }
  }
}

void SmartRuleSystem::applyVirtualState() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (!::sockets[i])
      continue;

    auto &socket = sockets[i];
    if (socket.virtualState == RuleDecision::Skip)
      continue;

    bool targetState = decisionToPhysical(socket.virtualState);
    if (targetState == socket.physicalState) {
      Serial.printf("Socket %d: No state change needed (current = %s)\n",
                    i + 1, socket.physicalState ? "ON" : "OFF");
      socket.virtualState = RuleDecision::Skip;
      continue;
    }

    unsigned long timeSinceChange = now - socket.lastStateChange;
    // Only check min_off_time when turning ON
    if (targetState && timeSinceChange < config.min_off_time) {
      // Calculate and show remaining time
      unsigned long remainingTime = config.min_off_time - timeSinceChange;
      Serial.printf("Socket %d: Waiting %lu ms before turning ON (min off time: %lu ms)\n",
                    i + 1, remainingTime, config.min_off_time);
      // Don't reset the virtual state - keep it as ON
      continue;
    }

    Serial.printf("Socket %d: Applying state change from %s to %s\n",
                  i + 1, socket.physicalState ? "ON" : "OFF", targetState ? "ON" : "OFF");

    if (::sockets[i]->setState(targetState)) {
      socket.physicalState = targetState;
      socket.lastStateChange = now;
      socket.virtualState = RuleDecision::Skip;
      Serial.printf("Socket %d: State change successful\n", i + 1);
    } else {
      socket.physicalState = ::sockets[i]->getCurrentState(); // Update real state after failure
      Serial.printf("Socket %d: State change failed, updated physical state to %s\n",
                    i + 1, socket.physicalState ? "ON" : "OFF");
    }
  }
}

std::function<RuleDecision()> SmartRuleSystem::delayedOnOff(
    const char *startTime,
    const char *endTime,
    int onDelayMinutes,
    int offDelayMinutes,
    std::function<bool()> condition) {

  std::string stateKey = std::string(startTime) + "-" + std::string(endTime);

  return [this, stateKey, startTime, endTime, onDelayMinutes, offDelayMinutes, condition]() {
    // First check if we're in the time window
    bool isInTimeRange = timeSync.isTimeBetween(startTime, endTime);
    if (!isInTimeRange) {
      return RuleDecision::Skip;
    }

    auto &state = this->delayedStates[stateKey];
    bool currentCondition = condition();
    unsigned long now = millis();

    // Detect condition change
    if (currentCondition != state.lastCondition) {
      state.conditionChangeTime = now;
      state.lastCondition = currentCondition;
    }

    // Calculate elapsed time since condition change (in minutes)
    unsigned long elapsedMinutes = (now - state.conditionChangeTime) / 60000;

    if (currentCondition && elapsedMinutes >= onDelayMinutes) {
      // Condition has been true for onDelayMinutes
      return RuleDecision::On;
    } else if (!currentCondition && elapsedMinutes >= offDelayMinutes) {
      // Condition has been false for offDelayMinutes
      return RuleDecision::Off;
    }

    // During delay period, maintain previous state
    return RuleDecision::Skip;
  };
}

std::function<bool()> SmartRuleSystem::timeWindowBetween(const char *start, const char *end) {
  return [this, start, end]() {
    std::string key = std::string(start) + "-" + std::string(end);
    bool isInWindow = timeSync.isTimeBetween(start, end);
    auto &windowState = this->activeTimeWindows[key];

    // If we're in the time window and not already active
    if (isInWindow && !windowState.isActive) {
      int endHour, endMin;
      sscanf(end, "%d:%d", &endHour, &endMin);
      windowState.endTimeMillis = calculateEndTime(endHour, endMin);
      windowState.isActive = true;
      Serial.printf("Time window activated: %s to %s\n", start, end);
      return true;
    }

    // If we were active and just now went outside the window - do the ONE SHOT
    // turn off
    if (windowState.isActive &&
        (!isInWindow || millis() > windowState.endTimeMillis)) {
      Serial.printf("Time window ended: %s to %s - turning off devices\n",
                    start, end);
      // Do one-shot turn off using the state change from active to inactive
      for (const auto &rule : rules) {
        if (rule.timeWindow) {
          int socketIndex = rule.socketNumber - 1;
          if (socketIndex >= 0 && socketIndex < NUM_SOCKETS &&
              ::sockets[socketIndex]) {
            ::sockets[socketIndex]->setState(false);
          }
        }
      }
      windowState.isActive = false;
      return false;
    }

    return windowState.isActive;
  };
}

std::function<RuleDecision()> SmartRuleSystem::solarHeaterControl(
    float exportThreshold,
    float importThreshold,
    unsigned long minOnTime,
    unsigned long minOffTime,
    std::function<bool()> extraCondition) {

  return [=]() {
    static bool deviceIsOn = false;
    static unsigned long lastStateChangeTime = 0;
    unsigned long currentTime = millis();

    // Evaluate condition once and store result
    bool conditionMet = extraCondition();

    // Debug output
    Serial.println("\n----- Heater Rule Evaluation -----");
    Serial.printf("Current state: %s\n", deviceIsOn ? "ON" : "OFF");
    Serial.printf("Extra condition met: %s\n", conditionMet ? "YES" : "NO");
    Serial.printf("Export power: %.2f W (threshold: %.2f W)\n",
                  p1Meter ? p1Meter->getCurrentExport() : -1, exportThreshold);
    Serial.printf("Time since last change: %lu ms\n", currentTime - lastStateChangeTime);

    // First handle the case when we're ON
    if (deviceIsOn) {
      // Check if we need to turn OFF because condition is no longer met
      if (!conditionMet && (currentTime - lastStateChangeTime >= minOnTime)) {
        deviceIsOn = false;
        lastStateChangeTime = currentTime;
        Serial.println("Solar Control: Turning OFF - condition not met");
        return RuleDecision::Off;
      }

      // Check if we're importing too much power (grid consumption)
      if (p1Meter && p1Meter->getCurrentImport() > importThreshold &&
          (currentTime - lastStateChangeTime >= minOnTime)) {
        deviceIsOn = false;
        lastStateChangeTime = currentTime;
        Serial.println("Solar Control: Turning OFF - import threshold exceeded");
        return RuleDecision::Off;
      }

      // Otherwise stay ON
      return RuleDecision::On;
    }
    // Now handle when we're OFF
    else {
      // Only consider turning ON if the condition is met
      if (conditionMet) {
        // Check if we have enough export power AND minimum off time has elapsed
        if (p1Meter && p1Meter->getCurrentExport() > exportThreshold &&
            (currentTime - lastStateChangeTime >= minOffTime)) {
          deviceIsOn = true;
          lastStateChangeTime = currentTime;
          Serial.println("Solar Control: Turning ON - export threshold met");
          return RuleDecision::On;
        }
      }

      // Stay OFF by default
      return RuleDecision::Off;
    }
  };
}

//[private after function]
std::function<bool()> SmartRuleSystem::after(const char *timeStr, int durationMins) {
  return [timeStr, durationMins]() {
    // Parse the input time string (HH:MM)
    int targetHour, targetMinute;
    sscanf(timeStr, "%d:%d", &targetHour, &targetMinute);
    int targetMinutes = targetHour * 60 + targetMinute;

    // Get the current time
    TimeSync::TimeData currentTime = timeSync.getTime();
    int currentMinutes = currentTime.hour * 60 + currentTime.minute;

    // Calculate the end time if a duration is provided
    int endMinutes = targetMinutes + durationMins;

    // Handle overnight periods (e.g., 23:30 + 60 minutes = 00:30)
    if (endMinutes >= 1440) { // 1440 minutes = 24 hours
      endMinutes -= 1440;
    }

    // Check if the current time is after the target time
    if (durationMins == 0) {
      // No duration: return true if current time is after target time
      return currentMinutes >= targetMinutes;
    } else {
      // With duration: return true if current time is within the window
      if (targetMinutes <= endMinutes) {
        // Normal case: window is within the same day
        return currentMinutes >= targetMinutes && currentMinutes < endMinutes;
      } else {
        // Overnight case: window spans midnight
        return currentMinutes >= targetMinutes || currentMinutes < endMinutes;
      }
    }
  };
}

// turns on a rule if a condition is met, but skips if not (does not turn off)
std::function<RuleDecision()> SmartRuleSystem::period(const char *startTime, const char *endTime, std::function<bool()> condition) {
  return [this, startTime, endTime, condition]() {
    // Parse times
    int startHour, startMinute, endHour, endMinute;
    sscanf(startTime, "%d:%d", &startHour, &startMinute);
    sscanf(endTime, "%d:%d", &endHour, &endMinute);

    TimeSync::TimeData currentTime = timeSync.getTime();
    int currentMinutes = currentTime.hour * 60 + currentTime.minute;
    int startMinutes = startHour * 60 + startMinute;
    int endMinutes = endHour * 60 + endMinute;

    // Check if we're in the time range
    bool isInTimeRange;
    if (endMinutes < startMinutes) {
      // Overnight period
      isInTimeRange = currentMinutes >= startMinutes || currentMinutes <= endMinutes;
    } else {
      // Same day period
      isInTimeRange = currentMinutes >= startMinutes && currentMinutes <= endMinutes;
    }

    Serial.printf("Period check: Current: %02d:%02d, Start: %s, End: %s, In Range: %s\n",
                  currentTime.hour, currentTime.minute, startTime, endTime,
                  isInTimeRange ? "true" : "false");

    // If we're outside the time range, return SKIP
    if (!isInTimeRange) {
      Serial.println("Outside time period - returning SKIP");
      return RuleDecision::Skip;
    }

    // Check if we're in the turn-off window (last 2 minutes)
    int turnOffStartMinutes = endMinutes - 2;
    bool isInTurnOffWindow = currentMinutes >= turnOffStartMinutes && currentMinutes <= endMinutes;

    // If in turn-off window, return OFF
    if (isInTurnOffWindow) {
      Serial.println("In turn-off window - returning OFF");
      return RuleDecision::Off;
    }

    // We're in the active period, check condition
    bool conditionMet = condition();
    Serial.printf("In active period, condition: %s\n", conditionMet ? "met" : "not met");

    // Return ON if condition is met, otherwise SKIP
    return conditionMet ? RuleDecision::On : RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::boolPeriod(const char *startTime, const char *endTime, std::function<bool()> condition) {
  return [=]() {
    // Parse times
    int startHour, startMinute, endHour, endMinute;
    sscanf(startTime, "%d:%d", &startHour, &startMinute);
    sscanf(endTime, "%d:%d", &endHour, &endMinute);

    TimeSync::TimeData currentTime = timeSync.getTime();
    int currentMinutes = currentTime.hour * 60 + currentTime.minute;
    int startMinutes = startHour * 60 + startMinute;
    int endMinutes = endHour * 60 + endMinute;

    // Check if we're in the time range
    bool isInTimeRange;
    if (endMinutes < startMinutes) {
      // Overnight period
      isInTimeRange = currentMinutes >= startMinutes || currentMinutes <= endMinutes;
    } else {
      // Same day period
      isInTimeRange = currentMinutes >= startMinutes && currentMinutes <= endMinutes;
    }

    Serial.printf("Period check: Current: %02d:%02d, Start: %s, End: %s, In Range: %s\n",
                  currentTime.hour, currentTime.minute, startTime, endTime,
                  isInTimeRange ? "true" : "false");

    // Outside period -> SKIP (let other rules decide)
    if (!isInTimeRange) {
      Serial.println("Outside time period - returning SKIP");
      return RuleDecision::Skip;
    }

    // Inside period, condition determines ON/OFF
    bool conditionMet = condition();
    Serial.printf("In period, condition: %s\n", conditionMet ? "true" : "false");

    // The socket will handle the actual delays when state changes
    return conditionMet ? RuleDecision::On : RuleDecision::Off;
  };
}

std::function<RuleDecision()> SmartRuleSystem::onAfter(const char *timeStr, int durationMins, std::function<bool()> condition) {
  return [this, timeStr, durationMins, condition]() {
    bool isAfter = after(timeStr, durationMins)();
    bool conditionMet = condition();

    if (isAfter && conditionMet) {
      // Only turn on if both after time and condition is met
      return RuleDecision::On;
    }
    // In all other cases, skip
    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::offAfter(const char *timeStr, int durationMins, std::function<bool()> condition) {
  return [this, timeStr, durationMins, condition]() {
    bool isAfter = after(timeStr, durationMins)();
    bool conditionMet = condition();

    if (isAfter) {
      // After the time, condition determines off/skip
      return conditionMet ? RuleDecision::Off : RuleDecision::Skip;
    }
    // Before the time, don't influence the state
    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::onCondition(std::function<bool()> condition) {
  return [condition]() {
    if (condition())
      return RuleDecision::On;
    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::offCondition(std::function<bool()> condition) {
  return [condition]() {
    if (condition())
      return RuleDecision::Off;
    return RuleDecision::Skip;
  };
}

std::function<bool()> SmartRuleSystem::lightBelow(float threshold) {
  return [=]() { return sensors.getLightLevel() < threshold; };
}

struct DelayState {
  unsigned long startTime = 0;
  bool timing = false;
};

struct DelayState {
  unsigned long startTime = 0;
  bool timing = false;
};

std::function<RuleDecision()> SmartRuleSystem::onConditionDelayed(
    std::function<bool()> condition,
    int delaySeconds) {

  auto state = std::make_shared<DelayState>();

  return [condition, delaySeconds, state]() {
    bool conditionMet = condition();

    if (conditionMet) {
      if (!state->timing) {
        state->startTime = millis();
        state->timing = true;
      }

      if (millis() - state->startTime >= delaySeconds * 1000UL) {
        state->timing = false;
        return RuleDecision::On;
      }
    } else {
      state->timing = false;
    }

    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::offConditionDelayed(
    std::function<bool()> condition,
    int delaySeconds) {

  auto state = std::make_shared<DelayState>();

  return [condition, delaySeconds, state]() {
    bool conditionMet = condition();

    if (conditionMet) {
      if (!state->timing) {
        state->startTime = millis();
        state->timing = true;
      }

      if (millis() - state->startTime >= delaySeconds * 1000UL) {
        state->timing = false;
        return RuleDecision::Off;
      }
    } else {
      state->timing = false;
    }

    return RuleDecision::Skip;
  };
}

std::function<bool()> SmartRuleSystem::lightAbove(float threshold) {
  return [=]() { return sensors.getLightLevel() > threshold; };
}

std::function<bool()> SmartRuleSystem::phoneNotPresent() {
  return []() { return phoneCheck && !phoneCheck->isDevicePresent(); };
}

std::function<bool()> SmartRuleSystem::phonePresent() {
  return []() {
    if (!phoneCheck)
      return false;
    return phoneCheck->isDevicePresent();
  };
}

std::function<bool()> SmartRuleSystem::isWorkday() {
  return []() {
    return timeSync.isWorkday();
  };
}
std::function<bool()> SmartRuleSystem::isWeekend() {
  return []() {
    return timeSync.isWeekend();
  };
}
std::function<bool()> SmartRuleSystem::isMonday() {
  return []() {
    return timeSync.isMonday();
  };
}
std::function<bool()> SmartRuleSystem::isTuesday() {
  return []() {
    return timeSync.isTuesday();
  };
}
std::function<bool()> SmartRuleSystem::isWednesday() {
  return []() {
    return timeSync.isWednesday();
  };
}
std::function<bool()> SmartRuleSystem::isThursday() {
  return []() {
    return timeSync.isThursday();
  };
}
std::function<bool()> SmartRuleSystem::isFriday() {
  return []() {
    return timeSync.isFriday();
  };
}
std::function<bool()> SmartRuleSystem::isSaturday() {
  return []() {
    return timeSync.isSaturday();
  };
}
std::function<bool()> SmartRuleSystem::isSunday() {
  return []() {
    return timeSync.isSunday();
  };
}

// power related rules

std::function<bool()> SmartRuleSystem::powerSolarActive() {
  return []() { return p1Meter && p1Meter->getCurrentExport() > 0; };
}

std::function<bool()> SmartRuleSystem::powerProducing() {
  return []() { return p1Meter && p1Meter->getCurrentExport() > 0; };
}

std::function<bool()> SmartRuleSystem::powerConsuming() {
  return []() { return p1Meter && p1Meter->getCurrentImport() > 0; };
}

std::function<bool()> SmartRuleSystem::powerProductionBelow(float threshold) {
  return [=]() { return p1Meter && p1Meter->getCurrentExport() < threshold; };
}

std::function<bool()> SmartRuleSystem::powerProductionAbove(float threshold) {
  return [=]() { return p1Meter && p1Meter->getCurrentExport() > threshold; };
}

const char *SmartRuleSystem::rndTime(const char *time, int maxMinutes, int extraSeed) {
  TimeSync::TimeData t = timeSync.getTime();
  srand(t.dayOfYear + t.weekNum + extraSeed);
  int addMinutes = rand() % maxMinutes;

  int hour, minute;
  sscanf(time, "%d:%d", &hour, &minute);

  minute += addMinutes;
  hour += minute / 60;
  minute = minute % 60;
  hour = hour % 24;

  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
  Serial.printf("Generated random time: %s (base: %s, added: %d min)\n",
                timeBuffer, time, addMinutes);
  return timeBuffer;
}

// grouping logic

std::function<bool()>
SmartRuleSystem::allOf(std::vector<std::function<bool()>> conditions) {
  return [=]() {
    for (const auto &cond : conditions) {
      if (!cond())
        return false;
    }
    return true;
  };
}

std::function<bool()>
SmartRuleSystem::anyOf(std::vector<std::function<bool()>> conditions) {
  return [=]() {
    for (const auto &cond : conditions) {
      if (cond())
        return true;
    }
    return false;
  };
}

std::function<bool()> SmartRuleSystem::notOf(std::function<bool()> condition) {
  return [=]() { return !condition(); };
}

bool SmartRuleSystem::getSocketState(int socketIndex) const {
  if (socketIndex < 0 || socketIndex >= NUM_SOCKETS)
    return false;
  return sockets[socketIndex].physicalState;
}

RuleDecision SmartRuleSystem::physicalToDecision(bool state) {
  return state ? RuleDecision::On : RuleDecision::Off;
}

bool SmartRuleSystem::decisionToPhysical(RuleDecision decision) {
  return decision == RuleDecision::On;
}

// helper function to calculate end time in milliseconds
unsigned long SmartRuleSystem::calculateEndTime(int hour, int minute) {
  TimeSync::TimeData t = timeSync.getTime();
  unsigned long now = millis();
  int currentHour = t.hour;
  int currentMinute = t.minute;

  // Calculate minutes remaining until end time
  int minutesRemaining = (hour - currentHour) * 60 + (minute - currentMinute);
  if (minutesRemaining < 0)
    minutesRemaining += 24 * 60; // Handle overnight

  return now + (minutesRemaining * 60 * 1000UL);
}
void SmartRuleSystem::clearRules() {
  rules.clear();
  Serial.println("All rules cleared");
}
_______________ filename: TimeSync.cpp _______________
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
    Serial.println("âœ“ Time synchronized successfully!");
    Serial.printf("Current time: %s\n", time_str);
    Serial.printf("Timezone: %s (UTC+%d)\n",
                  isDST(&timeinfo) ? "CEST" : "CET",
                  (gmtOffset_sec + daylightOffset_sec) / 3600);
    Serial.printf("DST is %s\n", isDST(&timeinfo) ? "active" : "not active");

    timeInitialized = true;
    return true;
  } else {
    Serial.println("Ã— Failed to sync time after multiple attempts");
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
    Serial.println("Ã¢Å¡  Could not get current time, using default (12:00)");
    timeInitialized = false; // Mark time as not synchronized
  }
}

String TimeSync::getCurrentTime() {
  // Try to get time using our TimeData structure for consistency with rules
  TimeData currentTime = getTime();

  // Check if time was successfully obtained
  if (currentTime.year == 0) {
    Serial.println("âš  Failed to obtain time");
    timeInitialized = false;

    // Check cooldown period
    unsigned long now = millis();
    if (now - lastResyncAttempt < RESYNC_COOLDOWN) {
      Serial.println("Ã— Skipping resync - in cooldown period");
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
          Serial.println("âœ“ Time resync successful");
          char timeString[9];
          sprintf(timeString, "%02d:%02d:%02d", currentTime.hour, currentTime.minute, 0);
          return String(timeString);
        }
      }
      Serial.println("Ã— Time resync failed");
    } else {
      Serial.println("Ã— Cannot resync time - WiFi not connected");
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
    Serial.println("Ã¢Å¡  Failed to get time for comparison");
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
_______________ filename: WebInterface.cpp _______________
// WebServer.cpp
#include "WebInterface.h"
#include "Constants.h"
#include "GlobalVars.h"
String WebInterface::getContentType(const String &path) {
  if (path.endsWith(".html"))
    return "text/html";
  else if (path.endsWith(".css"))
    return "text/css";
  else if (path.endsWith(".js"))
    return "application/javascript";
  else if (path.endsWith(".json"))
    return "application/json";
  else if (path.endsWith(".ico"))
    return "image/x-icon";
  return "text/plain";
}

bool WebInterface::serveFromCache(const String &path) {
  for (int i = 0; i < MAX_CACHED_FILES; i++) {
    if (cachedFiles[i].path == path && cachedFiles[i].data != nullptr) {
      Serial.printf("Web > Serving %s from RAM cache\n", path.c_str());
      server.sendHeader("Content-Type", getContentType(path));
      server.sendHeader("Content-Length", String(cachedFiles[i].size));
      server.sendHeader("Cache-Control", "no-cache");
      server.send(200, getContentType(path), "");
      server.client().write(cachedFiles[i].data, cachedFiles[i].size);
      return true;
    }
  }
  return false;
}

void WebInterface::cacheFile(const String &path, File &file) {
  static int cacheIndex = 0;

  if (cachedFiles[cacheIndex].data != nullptr) {
    delete[] cachedFiles[cacheIndex].data;
    cachedFiles[cacheIndex].data = nullptr;
  }

  size_t fileSize = file.size();
  cachedFiles[cacheIndex].data = new uint8_t[fileSize];
  if (cachedFiles[cacheIndex].data) {
    file.read(cachedFiles[cacheIndex].data, fileSize);
    cachedFiles[cacheIndex].path = path;
    cachedFiles[cacheIndex].size = fileSize;
    Serial.printf("Web > Cached %s in RAM (%u bytes)\n", path.c_str(),
                  fileSize);

    cacheIndex = (cacheIndex + 1) % MAX_CACHED_FILES;
  }
}

void WebInterface::updateCache() {
  if (p1Meter) {
    cached.import_power = p1Meter->getCurrentImport();
    cached.export_power = p1Meter->getCurrentExport();
  }
  cached.temperature = sensors.getTemperature();
  cached.humidity = sensors.getHumidity();
  cached.light = sensors.getLightLevel();

  for (int i = 0; i < NUM_SOCKETS; i++) {
    cached.socket_states[i] =
        sockets[i] ? sockets[i]->getCurrentState() : false;
    cached.socket_durations[i] = millis() - lastStateChangeTime[i];
  }

  for (int i = 0; i < NUM_SOCKETS; i++) {
    cached.socket_durations[i] = millis() - lastStateChangeTime[i];
  }
}

void WebInterface::begin() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  server.client().setNoDelay(true);

  Serial.printf("Total space: %d bytes\n", SPIFFS.totalBytes());
  Serial.printf("Used space: %d bytes\n", SPIFFS.usedBytes());

  // Serve the main page at root URL
  server.on("/", HTTP_GET, [this]() { serveFile("/data/index.html"); });

  // API endpoint for getting data
  server.on("/data", HTTP_GET, [this]() {
    server.sendHeader("Content-Type", "application/json");
    server.sendHeader("Access-Control-Allow-Origin", "*");

    StaticJsonDocument<512> doc;
    doc["import_power"] = cached.import_power;
    doc["export_power"] = cached.export_power;
    doc["temperature"] = cached.temperature;
    doc["humidity"] = cached.humidity;
    doc["light"] = cached.light;

    JsonArray switches = doc.createNestedArray("switches");
    for (int i = 0; i < NUM_SOCKETS; i++) {
      JsonObject sw = switches.createNestedObject();
      sw["state"] = cached.socket_states[i];
      sw["duration"] = String(cached.socket_durations[i] / 1000) + "s";
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // API endpoints for controlling switches
  for (int i = 0; i < NUM_SOCKETS; i++) {
    server.on("/switch/" + String(i + 1), HTTP_POST,
              [this, i]() { handleSwitch(i); });
  }

  // Handle any other static files
  server.onNotFound([this]() {
    if (!serveFile(server.uri())) {
      server.send(404, "text/plain", "Not found");
    }
  });

  server.begin();
  Serial.println("Web server started on IP: " + WiFi.localIP().toString());
}

void WebInterface::update() {
  static unsigned long lastWebUpdate = 0;
  static unsigned long lastClientCheck = 0;
  unsigned long now = millis();

  // Handle web clients first
  server.handleClient();

  // Only reset if we haven't seen any activity for a longer period
  if (server.client() && server.client().connected()) {
    lastWebUpdate = now; // Reset timeout if we have an active client
    lastClientCheck = now;
  } else if (now - lastClientCheck >=
             1000) { // Check connection status every second
    lastClientCheck = now;
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Web > Status: No active clients (uptime: %lus)\n",
                    (now - lastWebUpdate) / 1000);
    }
  }

  // Only reset if really needed (increase to 2 minutes)
  if (now - lastWebUpdate > 120000) { // 2 minutes
    Serial.println("Web > Watchdog: Server inactive, attempting reset");
    server.close();
    delay(100); // Give it time to close
    server.begin();
    Serial.println("Web > Server reset complete");
    lastWebUpdate = now;
  }

  // Update cache periodically
  static unsigned long lastCacheUpdate = 0;
  if (now - lastCacheUpdate >= 1000) {
    updateCache();
    lastCacheUpdate = now;
  }

  // WiFi check
  if (now - lastCheck >= CHECK_INTERVAL) {
    lastCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Web > WiFi connection lost - attempting reconnect");
      WiFi.reconnect();
    }
  }

  yield();
}

bool WebInterface::serveFile(const String &path) {
  Serial.printf("Web > Attempting to serve: %s\n", path.c_str());

  if (!buffer) {
    Serial.println("Web > Error: Buffer not allocated!");
    return false;
  }

  // Try cache first
  if (serveFromCache(path)) {
    Serial.println("Web > Served from cache successfully");
    return true;
  }

  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.printf("Web > Error: Failed to open %s\n", path.c_str());
    return false;
  }

  size_t fileSize = file.size();
  Serial.printf("Web > Serving file from SPIFFS: %s (%u bytes)\n", path.c_str(),
                fileSize);

  String contentType = getContentType(path);
  server.sendHeader("Content-Type", contentType);
  server.sendHeader("Content-Length", String(fileSize));
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache");
  server.setContentLength(fileSize);
  server.send(200, contentType, "");

  size_t totalBytesSent = 0;
  while (totalBytesSent < fileSize) {
    if (!server.client().connected()) {
      Serial.println("Web > Error: Client disconnected during transfer");
      file.close();
      return false;
    }

    size_t bytesRead =
        file.read(buffer, min(BUFFER_SIZE, fileSize - totalBytesSent));
    if (bytesRead == 0) {
      Serial.println("Web > Error: Failed to read file");
      break;
    }

    size_t bytesWritten = server.client().write(buffer, bytesRead);
    if (bytesWritten != bytesRead) {
      Serial.printf("Web > Warning: Partial write %u/%u bytes\n", bytesWritten,
                    bytesRead);
      delay(50);
      continue;
    }

    totalBytesSent += bytesWritten;
    if (totalBytesSent % (BUFFER_SIZE * 4) == 0) {
      Serial.printf("Web > Progress: %u/%u bytes sent\n", totalBytesSent,
                    fileSize);
    }
    delay(1);
    yield();
  }

  file.close();

  if (totalBytesSent == fileSize) {
    Serial.println("Web > File served successfully");
    // Try to cache the file for next time
    file = SPIFFS.open(path, "r");
    if (file) {
      cacheFile(path, file);
      file.close();
    }
    return true;
  } else {
    Serial.printf("Web > Error: Only sent %u/%u bytes\n", totalBytesSent,
                  fileSize);
    return false;
  }
}

void WebInterface::handleSwitch(int switchNumber) {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Body not received");
    return;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  bool state = doc["state"];
  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"success\":true}");
}
