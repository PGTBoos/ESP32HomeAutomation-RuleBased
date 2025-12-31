// WebInterface.h
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "GlobalVars.h"

using WebServer = ::WebServer;

class WebInterface
{
private:
    struct CachedData
    {
        float import_power = 0;
        float export_power = 0;
        float temperature = 0;
        float humidity = 0;
        float light = 0;
        bool socket_states[NUM_SOCKETS] = {};
        bool socket_online[NUM_SOCKETS] = {};
        unsigned long socket_durations[NUM_SOCKETS] = {0};
    };

    WebServer server;
    unsigned long lastCheck = 0;
    static const unsigned long CHECK_INTERVAL = 30000;

    CachedData cached;

    void updateCache();
    void handleSwitch(int switchNumber);

public:
    // Removed manual buffer allocation
    WebInterface() : server(8080) {}

    void begin();
    void update();

    // Simplified destructor as there is no dynamic memory to clean up
    ~WebInterface() {}
};