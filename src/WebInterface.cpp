// WebServer.cpp
#include "WebInterface.h"
#include "Constants.h"
#include "GlobalVars.h"
#include "PowerHistory.h"

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
      server.setContentLength(cachedFiles[i].size);
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

    StaticJsonDocument<768> doc;

    // Power data
    doc["import_power"] = cached.import_power;
    doc["export_power"] = cached.export_power;

    // Daily totals
    if (p1Meter && config.yesterday > 0 && config.yesterdayImport > 0) {
      float dailyImport = p1Meter->getTotalImport() - config.yesterdayImport;
      float dailyExport = p1Meter->getTotalExport() - config.yesterdayExport;
      // Sanity check
      if (dailyImport >= 0 && dailyImport < 100 && dailyExport >= 0 && dailyExport < 100) {
        doc["daily_import"] = dailyImport;
        doc["daily_export"] = dailyExport;
      }
    }

    // Environment
    doc["temperature"] = cached.temperature;
    doc["humidity"] = cached.humidity;
    doc["light"] = cached.light;

    // Phone presence
    doc["phone_present"] = (phoneCheck && phoneCheck->isDevicePresent());

    // Switches
    JsonArray switches = doc.createNestedArray("switches");
    for (int i = 0; i < NUM_SOCKETS; i++) {
      JsonObject sw = switches.createNestedObject();
      sw["state"] = cached.socket_states[i];
      sw["duration"] = cached.socket_durations[i] / 1000;
      sw["online"] = (sockets[i] != nullptr) ? sockets[i]->isConnected() : false;
    }

    // Rule info
    doc["last_rule"] = lastActiveRuleName;
    doc["last_rule_time"] = lastActiveRuleTimeStr;

    // System info
    doc["ip"] = WiFi.localIP().toString();
    doc["free_ram"] = ESP.getFreeHeap() / 1024;
    doc["uptime"] = millis() / 1000;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // History endpoints
  server.on("/history/minute", HTTP_GET, [this]() {
    server.sendHeader("Content-Type", "application/json");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", powerHistory.getMinuteDataJson());
  });

  server.on("/history/hour", HTTP_GET, [this]() {
    server.sendHeader("Content-Type", "application/json");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", powerHistory.getHourDataJson());
  });

  server.on("/history/day", HTTP_GET, [this]() {
    server.sendHeader("Content-Type", "application/json");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", powerHistory.getDayDataJson());
  });

  server.on("/history/month", HTTP_GET, [this]() {
    server.sendHeader("Content-Type", "application/json");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", powerHistory.getMonthDataJson());
  });

  // Debug endpoint to fix daily totals
  server.on("/debug/fixdaily", HTTP_GET, [this]() {
    if (p1Meter) {
      StaticJsonDocument<128> doc;
      doc["day"] = timeSync.getTime().dayOfYear;
      doc["import"] = p1Meter->getTotalImport();
      doc["export"] = p1Meter->getTotalExport();

      File file = SPIFFS.open("/daily_totals.json", "w");
      if (file) {
        serializeJson(doc, file);
        file.close();

        // Update config
        config.yesterday = timeSync.getTime().dayOfYear;
        config.yesterdayImport = p1Meter->getTotalImport();
        config.yesterdayExport = p1Meter->getTotalExport();

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
      }
    } else {
      server.send(500, "text/plain", "P1 meter not ready");
    }
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
    lastWebUpdate = now;
    lastClientCheck = now;
  } else if (now - lastClientCheck >= 1000) {
    lastClientCheck = now;
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Web > Status: No active clients (uptime: %lus)\n",
                    (now - lastWebUpdate) / 1000);
    }
  }

  // Only reset if really needed (increase to 2 minutes)
  if (now - lastWebUpdate > 120000) {
    Serial.println("Web > Watchdog: Server inactive, attempting reset");
    server.close();
    delay(100);
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

  // Actually set the socket state
  if (sockets[switchNumber] != nullptr) {
    sockets[switchNumber]->setState(state);
    lastStateChangeTime[switchNumber] = millis();
    Serial.printf("Web > Socket %d set to %s\n", switchNumber + 1, state ? "ON" : "OFF");
  }

  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"success\":true}");
}
