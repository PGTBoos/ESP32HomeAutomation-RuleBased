<<<<<<< HEAD
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

    struct FileCache
    {
        String path;
        uint8_t *data = nullptr;
        size_t size = 0;
    };

    WebServer server;
    unsigned long lastCheck = 0;
    static const size_t BUFFER_SIZE = 1024;
    static const int CHUNK_DELAY = 5;
    static const unsigned long CHECK_INTERVAL = 30000;
    static const unsigned long ERROR_COOLDOWN = 5000;
    static const int MAX_CACHED_FILES = 2;

    uint8_t *buffer;
    CachedData cached;
    FileCache cachedFiles[MAX_CACHED_FILES];

    void updateCache();
    String getContentType(const String &path);
    bool serveFromCache(const String &path);
    void cacheFile(const String &path, File &file);
    bool serveFile(const String &path);
    void handleSwitch(int switchNumber);

public:
    WebInterface() : server(8080), buffer(new uint8_t[BUFFER_SIZE]) {}
    void begin();
    void update();
    ~WebInterface()
    {
        delete[] buffer;
        for (int i = 0; i < MAX_CACHED_FILES; i++)
        {
            if (cachedFiles[i].data != nullptr)
            {
                delete[] cachedFiles[i].data;
            }
        }
    }
=======
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

    struct FileCache
    {
        String path;
        uint8_t *data = nullptr;
        size_t size = 0;
    };

    WebServer server;
    unsigned long lastCheck = 0;
    static const size_t BUFFER_SIZE = 1024;
    static const int CHUNK_DELAY = 5;
    static const unsigned long CHECK_INTERVAL = 30000;
    static const unsigned long ERROR_COOLDOWN = 5000;
    static const int MAX_CACHED_FILES = 2;

    uint8_t *buffer;
    CachedData cached;
    FileCache cachedFiles[MAX_CACHED_FILES];

    void updateCache();
    String getContentType(const String &path);
    bool serveFromCache(const String &path);
    void cacheFile(const String &path, File &file);
    bool serveFile(const String &path);
    void handleSwitch(int switchNumber);

public:
    WebInterface() : server(8080), buffer(new uint8_t[BUFFER_SIZE]) {}
    void begin();
    void update();
    ~WebInterface()
    {
        delete[] buffer;
        for (int i = 0; i < MAX_CACHED_FILES; i++)
        {
            if (cachedFiles[i].data != nullptr)
            {
                delete[] cachedFiles[i].data;
            }
        }
    }
>>>>>>> 4b002c6a95035d8e01148e2a819261d17acea3df
};