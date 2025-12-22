#ifndef HOME_SOCKET_DEVICE_H
#define HOME_SOCKET_DEVICE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>

class HomeSocketDevice
{
private:
    HTTPClient http;
    WiFiClient client;
    String baseUrl;
    bool lastKnownState;
    unsigned long lastReadTime;
    const unsigned long READ_INTERVAL = 1000;
    bool lastReadSuccess;

    int consecutiveFailures;
    String deviceIP; // Store IP for better logging
    bool makeHttpRequest(const String &endpoint, const String &method, const String &payload, String &response);
    int socketNumber;
    unsigned long lastLogTime; // For controlling log frequency

public:
    HomeSocketDevice(const char *ip, int socketNum);
    void readStateInfo();
    bool setState(bool state);
    bool getState();
    bool isConnected() const { return consecutiveFailures == 0; }
    bool getCurrentState() const { return lastKnownState; }
};

#endif