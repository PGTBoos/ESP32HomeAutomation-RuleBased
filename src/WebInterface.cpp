// WebServer.cpp
#include "WebInterface.h"
#include "Constants.h"
#include "GlobalVars.h"
#include "PowerHistory.h"

void WebInterface::updateCache() {
  if (p1Meter) {
    cached.import_power = p1Meter->getCurrentImport();
    cached.export_power = p1Meter->getCurrentExport();
  }
  cached.temperature = sensors.getTemperature();
  cached.humidity = sensors.getHumidity();
  cached.light = sensors.getLightLevel();

  for (int i = 0; i < NUM_SOCKETS; i++) {
    cached.socket_states[i] = sockets[i] ? sockets[i]->getCurrentState() : false;
    cached.socket_online[i] = sockets[i] ? sockets[i]->isConnected() : false;
    cached.socket_durations[i] = millis() - lastStateChangeTime[i];
  }
}

void WebInterface::begin() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // Serve static files automatically from SPIFFS
  // This replaces getContentType, serveFromCache, cacheFile, and serveFile

  server.serveStatic("/", SPIFFS, "/index.html", "public, max-age=604800");

  // API endpoint for getting data
  server.on("/data", HTTP_GET, [this]() {
    server.sendHeader("Access-Control-Allow-Origin", "*");

    // Using StaticJsonDocument on the stack is much safer than Dynamic
    StaticJsonDocument<2048> doc;

    doc["import_power"] = cached.import_power;
    doc["export_power"] = cached.export_power;

    if (p1Meter && config.yesterday > 0 && config.yesterdayImport > 0) {
      float dailyImport = p1Meter->getTotalImport() - config.yesterdayImport;
      float dailyExport = p1Meter->getTotalExport() - config.yesterdayExport;
      if (dailyImport >= 0 && dailyImport < 100 && dailyExport >= 0 && dailyExport < 100) {
        doc["daily_import"] = dailyImport;
        doc["daily_export"] = dailyExport;
      }
    }

    doc["temperature"] = cached.temperature;
    doc["humidity"] = cached.humidity;
    doc["light"] = cached.light;
    doc["phone_present"] = (phoneCheck && phoneCheck->isDevicePresent());

    JsonArray switches = doc.createNestedArray("switches");
    for (int i = 0; i < NUM_SOCKETS; i++) {
      JsonObject sw = switches.createNestedObject();
      sw["state"] = cached.socket_states[i];
      sw["duration"] = cached.socket_durations[i] / 1000;
      sw["online"] = cached.socket_online[i];
    }

    doc["last_rule"] = lastActiveRuleName;
    doc["last_rule_time"] = lastActiveRuleTimeStr;

    JsonArray ruleHist = doc.createNestedArray("rule_history");
    for (int i = 0; i < 4; i++) {
      int idx = (ruleHistoryIndex + i) % 4;
      if (ruleHistory[idx].name[0] != '\0') {
        if (strcmp(ruleHistory[idx].name, lastActiveRuleName) != 0) {
          JsonObject hist = ruleHist.createNestedObject();
          hist["name"] = ruleHistory[idx].name;
          hist["time"] = ruleHistory[idx].time;
        }
      }
    }

    doc["ip"] = WiFi.localIP().toString();
    doc["free_ram"] = ESP.getFreeHeap() / 1024;
    doc["uptime"] = millis() / 1000;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // History endpoints
  server.on("/history/minute", HTTP_GET, [this]() {
    server.send(200, "application/json", powerHistory.getMinuteDataJson());
  });

  server.on("/history/hour", HTTP_GET, [this]() {
    server.send(200, "application/json", powerHistory.getHourDataJson());
  });

  server.on("/history/day", HTTP_GET, [this]() {
    server.send(200, "application/json", powerHistory.getDayDataJson());
  });

  server.on("/history/month", HTTP_GET, [this]() {
    server.send(200, "application/json", powerHistory.getMonthDataJson());
  });

  // API endpoints for controlling switches
  for (int i = 0; i < NUM_SOCKETS; i++) {
    server.on("/switch/" + String(i + 1), HTTP_POST, [this, i]() { handleSwitch(i); });
  }

  server.begin();
  Serial.println("Web server started");
}

void WebInterface::update() {
  unsigned long now = millis();
  server.handleClient();

  // Periodic Cache Update
  static unsigned long lastCacheUpdate = 0;
  if (now - lastCacheUpdate >= 1000) {
    updateCache();
    lastCacheUpdate = now;
  }
}
void WebInterface::handleSwitch(int switchNumber) {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Body not received");
    return;
  }

  StaticJsonDocument<128> doc;
  deserializeJson(doc, server.arg("plain"));
  bool state = doc["state"];

  if (sockets[switchNumber] != nullptr) {
    sockets[switchNumber]->setState(state);
    lastStateChangeTime[switchNumber] = millis();

    // *** ADD THESE 3 LINES: ***
    cached.socket_states[switchNumber] = state; // Update cache immediately
    cached.socket_durations[switchNumber] = 0;  // Reset duration

    // Force cache update to refresh all other sockets too
    updateCache(); // Get fresh states for everything
  }

  server.send(200, "application/json", "{\"success\":true}");
}
/*
void WebInterface::handleSwitch(int switchNumber) {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Body not received");
    return;
  }

  StaticJsonDocument<128> doc;
  deserializeJson(doc, server.arg("plain"));
  bool state = doc["state"];

  if (sockets[switchNumber] != nullptr) {
    sockets[switchNumber]->setState(state);
    lastStateChangeTime[switchNumber] = millis();
  }

  server.send(200, "application/json", "{\"success\":true}");
}
*/