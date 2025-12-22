#define DEBUG_HOME_SOCKET_DEVICE 0
#include "HomeSocketDevice.h"

HomeSocketDevice::HomeSocketDevice(const char *ip, int socketNum)
    : baseUrl("http://" + String(ip)), lastKnownState(false), lastReadTime(0),
      lastReadSuccess(false), consecutiveFailures(0), deviceIP(ip),
      lastLogTime(0), socketNumber(socketNum) {
  Serial.printf("Initializing socket %d at IP: %s\n", socketNum, ip);
}

void HomeSocketDevice::readStateInfo() {
  unsigned long currentTime = millis();
  unsigned long timeSinceLastRead = currentTime - lastReadTime;

  // Disconnected: use exponential backoff (2s, 4s, 6s... max 120s)
  if (consecutiveFailures > 0) {
    unsigned long backoffTime = min(consecutiveFailures * 2000UL, 120000UL);
    if (timeSinceLastRead < backoffTime) {
      return;
    }
  } else {
    // Connected: regular polling interval
    if (timeSinceLastRead < READ_INTERVAL) {
      return;
    }
  }

  // Regular status check
  bool previousState = lastKnownState;
  if (!getState()) {
    consecutiveFailures++;
    if (currentTime - lastLogTime >= 30000) {
      unsigned long nextBackoff = min((consecutiveFailures) * 2000UL, 120000UL);
      Serial.printf("Socket %d > %s > Offline (retry in %lu sec)\n", socketNumber, deviceIP.c_str(), nextBackoff / 1000);
      lastLogTime = currentTime;
    }
  } else {
    if (consecutiveFailures > 0) {
      Serial.printf("Socket %d > %s > Back online\n", socketNumber, deviceIP.c_str());
      lastLogTime = currentTime;
    }
    consecutiveFailures = 0;

    // If the state changed from our last known state, log it
    if (previousState != lastKnownState) {
      Serial.printf("Socket %d > %s > State changed from %s to %s\n",
                    socketNumber, deviceIP.c_str(),
                    previousState ? "ON" : "OFF",
                    lastKnownState ? "ON" : "OFF");
    }
  }

  lastReadTime = currentTime;
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
  newClient.setTimeout(5000); // Increased from 1200 to 5000
  // NO setConnectTimeout() - this causes socket exhaustion!

  String fullUrl = baseUrl + endpoint; // FIXED: was hardcoded to "/api/v1/state"

  if (!http.begin(newClient, fullUrl)) {
    newClient.stop();
    delay(50);
    return false;
  }

  http.setTimeout(5000); // Increased from 1200 to 5000
  http.setReuse(false);

  int httpCode;
  if (method == "GET") {
    httpCode = http.GET();
  } else if (method == "PUT") {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.PUT(payload);
  } else {
    http.end();
    newClient.stop();
    delay(50);
    return false;
  }

  bool success = (httpCode == HTTP_CODE_OK);
  if (success) {
    response = http.getString();
  }

  http.end();
  newClient.stop();
  delay(100); // ADDED: Critical delay to let socket close properly
  return success;
}

bool HomeSocketDevice::getState() {
  String response;
  if (!makeHttpRequest("/api/v1/state", "GET", "", response)) {
#if DEBUG_HOME_SOCKET_DEVICE
    Serial.printf("Socket %d > %s/api/v1/state > Get > HTTP error\n",
                  socketNumber, deviceIP.c_str());
#endif
    lastReadSuccess = false;
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
#if DEBUG_HOME_SOCKET_DEVICE
    Serial.printf("Socket %d > %s/api/v1/state > Get > JSON error\n",
                  socketNumber, deviceIP.c_str());
#endif
    lastReadSuccess = false;
    return false;
  }

  lastKnownState = doc["power_on"] | false;
#if DEBUG_HOME_SOCKET_DEVICE
  Serial.printf("Socket %d > %s/api/v1/state > Get > is %s\n",
                socketNumber, deviceIP.c_str(), lastKnownState ? "on" : "off");
#endif
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
  // FIXED: Was passing empty string, now passing jsonString
  if (!makeHttpRequest("/api/v1/state", "PUT", jsonString, response)) {
    Serial.printf("Socket %d > %s > Disconnected\n",
                  socketNumber, deviceIP.c_str());
    lastReadSuccess = false;
    return false;
  }

  lastKnownState = state;
  Serial.printf("PowerSocket %d > %s/api/v1/state > Put > turn %s\n",
                socketNumber, deviceIP.c_str(), state ? "on" : "off");
  return true;
}