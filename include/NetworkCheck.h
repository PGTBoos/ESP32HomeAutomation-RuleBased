// NetworkCheck.h
#ifndef NETWORK_CHECK_H
#define NETWORK_CHECK_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>

class NetworkCheck
{
private:
    String deviceIP;
    bool lastKnownState;
    unsigned long lastCheckTime;
    const unsigned long CHECK_INTERVAL = 60000; // Check every minute
    int consecutiveFailures;

    bool pingDevice();

public:
    NetworkCheck(const char *ip);
    bool isDevicePresent();
};

#endif