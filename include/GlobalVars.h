// GlobalVars.h
#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include "Constants.h"
#include "HomeP1Device.h"
#include "HomeSocketDevice.h"
#include "EnvironmentSensor.h"
#include "TimeSync.h"
// #include "RulesEngine.h"
#include "SmartRuleSystem.h"
// class SimpleRuleEngine; // Forward declaration
// extern SimpleRuleEngine ruleEngine;

#include "DisplayManager.h"
#include "NetworkCheck.h"

// External variable declarations
extern HomeP1Device *p1Meter;

// Define constants

extern unsigned long lastStateChangeTime[NUM_SOCKETS];
extern HomeSocketDevice *sockets[NUM_SOCKETS];
extern EnvironmentSensors sensors; // Make sure this matches your actual class name
extern DisplayManager display;
extern TimeSync timeSync;
extern NetworkCheck *phoneCheck;

struct RuleHistoryEntry
{
    char name[32];
    char time[12];
};
extern RuleHistoryEntry ruleHistory[4];
extern int ruleHistoryIndex;

void addRuleToHistory(const char *name, const char *time);

// Config structure
struct Config
{
    String wifi_ssid;
    String wifi_password;
    String p1_ip;
    String socket_ip[NUM_SOCKETS];
    String phone_ip;

    float yesterdayImport;
    float yesterdayExport;
    int yesterday;

    float power_on_threshold;
    float power_off_threshold;
    unsigned long min_on_time;
    unsigned long min_off_time;
    unsigned long max_on_time;
};

extern Config config;

// Timing control structure
struct TimingControl
{
    const unsigned long ENV_SENSOR_INTERVAL = 500;    // 10 seconds
    const unsigned long LIGHT_SENSOR_INTERVAL = 500;  // 10 seconds
    const unsigned long DISPLAY_INTERVAL = 1500;      // 1.5 second
    const unsigned long P1_INTERVAL = 30000;          // 30 second
    const unsigned long SOCKET_INTERVAL = 5000;       // 5 seconds
    const unsigned long WIFI_CHECK_INTERVAL = 30000;  // 30 seconds
    const unsigned long PHONE_CHECK_INTERVAL = 60000; // 60 seconds

    unsigned long lastEnvSensorUpdate = 0;
    unsigned long lastLightSensorUpdate = 0;
    unsigned long lastDisplayUpdate = 0;
    unsigned long lastP1Update = 0;
    unsigned long lastSocketUpdate = 0; // for a time interval to update the socket array (as group) each socket will have individual timers too
    unsigned long lastSocketUpdates[NUM_SOCKETS] = {0};
    unsigned long lastWiFiCheck = 0;
    unsigned long lastPhoneCheck = 0;
};

// create a public variable for last rule used, so it can be exchanged with const char *ruleName in the addRule function by default it should contain "none"
extern const char *lastActiveRuleName;
extern int lastActiveRuleSocket;
extern char lastActiveRuleTimeStr[12];

extern TimingControl timing;
#endif