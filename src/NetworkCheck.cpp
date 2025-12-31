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