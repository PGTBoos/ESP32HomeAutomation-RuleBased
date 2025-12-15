#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

#include "GlobalVars.h"
#include "DisplayManager.h"
#include "EnvironmentSensor.h"
#include "HomeP1Device.h"
#include "HomeSocketDevice.h"
#include "TimeSync.h"
#include "WebInterface.h"
#include "NetworkCheck.h"

extern HomeP1Device *p1Meter;
extern HomeSocketDevice *socket1;
extern HomeSocketDevice *socket2;
extern HomeSocketDevice *socket3;
extern HomeSocketDevice *socket4;
extern unsigned long lastStateChangeTime[NUM_SOCKETS];
extern NetworkCheck *phoneCheck;
// Timing control structure

unsigned long lastLightSensorUpdate;
unsigned long lastPhoneCheck;
static unsigned long lastHeaterCheck = 0;
const unsigned long HEATER_CHECK_INTERVAL = 5000;

bool loadConfiguration();
void connectWiFi();
bool canChangeState(int switchIndex, bool newState);
void checkMaxOnTime();
void updateSwitch1Logic();
void updateSwitch2Logic();
void updateSwitch3Logic();
void updateDisplay();
void setup();
void reconnectWiFi();
void loop();

// External variable declarations
extern TimingControl timing;
extern Config config;
extern DisplayManager display;
extern EnvironmentSensors sensors;
extern HomeP1Device *p1Meter;
extern HomeSocketDevice *socket1;
extern HomeSocketDevice *socket2;
extern HomeSocketDevice *socket3;
extern HomeSocketDevice *socket4;
extern TimeSync timeSync;
extern WebInterface webServer;
extern unsigned long lastStateChangeTime[NUM_SOCKETS];
extern bool switchForceOff[NUM_SOCKETS];
extern unsigned long lastTimeDisplay;
extern HomeP1Device *p1Meter;
extern EnvironmentSensors sensors;

void setupRules();
extern SmartRuleSystem ruleSystem;