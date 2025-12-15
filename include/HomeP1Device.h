// HomeP1Device.h
#ifndef HOME_P1_DEVICE_H
#define HOME_P1_DEVICE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>

class HomeP1Device
{
private:
    HTTPClient http;
    WiFiClient client;
    String baseUrl;
    float lastImportPower;
    float lastExportPower;

    float lastTotalImport;
    float lastTotalExport;

    unsigned long lastReadTime;
    const unsigned long READ_INTERVAL = 1000;
    const unsigned long HTTP_TIMEOUT = 5000;
    bool lastReadSuccess;
    bool getPowerData(float &importPower, float &exportPower);
    bool makeRequest(const String &endpoint, const String &method, const String &payload = "");

public:
    HomeP1Device(const char *ip);
    void update();
    float getCurrentImport() const;
    float getCurrentExport() const;
    float getNetPower() const;
    bool isConnected() const;
    float getTotalImport() const;
    float getTotalExport() const;
};

#endif