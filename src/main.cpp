#include "main.h"
#include "PowerHistory.h"

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
             rs.offConditionDelayed(rs.lightBelow(5), 120));
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

  // intitalise power history
  powerHistory.loadFromSpiffs();

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
      operationOrder = 95;
      yield();
      delay(50); // Give some time between network operations
    } else {
      operationOrder = 95;
    }
    break;

  case 95: // Power history updates
  {
    // Update minute data every minute
    if (p1Meter && powerHistory.shouldUpdateMinute()) {
      powerHistory.updateMinute(
          p1Meter->getCurrentImport(),
          p1Meter->getCurrentExport());
    }

    // Update hour data when hour changes
    if (powerHistory.shouldUpdateHour()) {
      powerHistory.resetHourAccumulator();
    }

    // Update day data at midnight
    if (powerHistory.shouldUpdateDay()) {
      powerHistory.resetDayAccumulator();
    }

    operationOrder = 100;
    break;
  }

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
