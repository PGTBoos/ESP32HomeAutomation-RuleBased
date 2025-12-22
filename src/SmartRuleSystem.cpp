#define DEBUG_RULES 0

#include "SmartRuleSystem.h"
#include "GlobalVars.h"
#include <cstdint>
char SmartRuleSystem::timeBuffer[6];

int SmartRuleSystem::dailyRandom[10] = {0};
int SmartRuleSystem::dailyRandom60[5] = {0};
int SmartRuleSystem::dailyRandom24[3] = {0};
int SmartRuleSystem::lastGeneratedDay = -1;

// WGS84 ellipsoid parameters for accurate Earth radius calculation
// Earth is an oblate spheroid: fatter at equator, flattened at poles
const float WGS84_A = 6378137.0;    // Equatorial radius in meters
const float WGS84_B = 6356752.3142; // Polar radius in meters
int SmartRuleSystem::sunriseMinutes = 0;
int SmartRuleSystem::sunsetMinutes = 0;
int SmartRuleSystem::lastSunCalcDay = -1;
float SmartRuleSystem::configLatitude = 52.37; // Default: Amsterdam
float SmartRuleSystem::configLongitude = 4.90;
int SmartRuleSystem::configElevation = 0;
char SmartRuleSystem::sunriseBuffer[6] = "00:00";
char SmartRuleSystem::sunsetBuffer[6] = "00:00";

SmartRuleSystem::SmartRuleSystem() : sockets(NUM_SOCKETS) {
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (::sockets[i]) {
      sockets[i].physicalState = ::sockets[i]->getCurrentState();
      sockets[i].virtualState = physicalToDecision(sockets[i].physicalState);
    }
  }
}

void SmartRuleSystem::addRule(int socketNumber,
                              const char *ruleName, // Add this
                              std::function<RuleDecision()> evaluate,
                              std::function<bool()> timeWindow) {
  Rule rule = {
      .socketNumber = socketNumber,
      .evaluate = evaluate,
      .timeWindow = timeWindow,
      .name = ruleName // Add this
  };
  rules.push_back(rule);
}

void SmartRuleSystem::generateDailyRandoms() {
  TimeSync::TimeData t = timeSync.getTime();

  if (lastGeneratedDay == t.dayOfYear) {
    return; // Already generated for today
  }

  // Seed with day + week for consistency
  srand(t.dayOfYear + t.weekNum * 7);

  // Generate all randoms for the day
  for (int i = 0; i < 10; i++) {
    dailyRandom[i] = rand() % 100;
  }

  for (int i = 0; i < 5; i++) {
    dailyRandom60[i] = rand() % 60;
  }

  for (int i = 0; i < 3; i++) {
    dailyRandom24[i] = rand() % 24;
  }

  lastGeneratedDay = t.dayOfYear;

  Serial.printf("Generated daily randoms for day %d:\n", t.dayOfYear);
  Serial.printf("Random 0-99: ");
  for (int i = 0; i < 10; i++)
    Serial.printf("%d ", dailyRandom[i]);
  Serial.printf("\nRandom 0-59: ");
  for (int i = 0; i < 5; i++)
    Serial.printf("%d ", dailyRandom60[i]);
  Serial.printf("\nRandom 0-23: ");
  for (int i = 0; i < 3; i++)
    Serial.printf("%d ", dailyRandom24[i]);
  Serial.println();
}

int SmartRuleSystem::getDailyRandom(int index) {
  generateDailyRandoms();
  if (index >= 0 && index < 10) {
    return dailyRandom[index];
  }
  return 0;
}

int SmartRuleSystem::getDailyRandom60(int index) {
  generateDailyRandoms();
  if (index >= 0 && index < 5) {
    return dailyRandom60[index];
  }
  return 0;
}

int SmartRuleSystem::getDailyRandom24(int index) {
  generateDailyRandoms();
  if (index >= 0 && index < 3) {
    return dailyRandom24[index];
  }
  return 0;
}

const char *SmartRuleSystem::addMinutesToTime(const char *baseTime, int minutesToAdd) {
  static char timeBuffer[6];

  int hour, minute;
  sscanf(baseTime, "%d:%d", &hour, &minute);

  minute += minutesToAdd;
  hour += minute / 60;
  minute = minute % 60;
  hour = hour % 24;

#if DEBUG_RULES
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
#endif
  return timeBuffer;
}

const char *SmartRuleSystem::addHoursToTime(const char *baseTime, int hoursToAdd) {
  static char timeBuffer[6];

  int hour, minute;
  sscanf(baseTime, "%d:%d", &hour, &minute);

  hour = (hour + hoursToAdd) % 24;

#if DEBUG_RULES
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
#endif
  return timeBuffer;
}

void SmartRuleSystem::update() {
  // 1. Get current physical states
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (::sockets[i]) {
      sockets[i].physicalState = ::sockets[i]->getCurrentState();
    }
  }

  // 2. Evaluate all rules and store final decisions
  for (const auto &rule : rules) {
    int socketIndex = rule.socketNumber - 1;
    if (socketIndex < 0 || socketIndex >= NUM_SOCKETS || !::sockets[socketIndex]) {
      continue;
    }

    // Get rule decision
    RuleDecision decision = rule.evaluate();

#if DEBUG_RULES
    // allow logging of skipped rules too when debugging
    if (decision == RuleDecision::Skip) {
      Serial.printf("Socket %d: Rule '%s' evaluated to SKIP\n",
                    rule.socketNumber,
                    rule.name);
    }
#endif

    // Only store non-SKIP decisions (lets later rules override earlier ones)
    if (decision != RuleDecision::Skip) {
#if DEBUG_RULES
      // Debug mode: log every evaluation
      Serial.printf("Socket %d: Rule '%s' evaluated to %s\n",
                    rule.socketNumber,
                    rule.name,
                    decision == RuleDecision::On ? "ON" : "OFF");
#else
      // Normal mode: only log when state changes
      if (sockets[socketIndex].virtualState != decision) {
        Serial.printf("Socket %d: Rule '%s' -> %s\n",
                      rule.socketNumber,
                      rule.name,
                      decision == RuleDecision::On ? "ON" : "OFF");
      }
#endif
      sockets[socketIndex].virtualState = decision;

      // Track the last active rule that turns something ON
      if (decision == RuleDecision::On) {
        lastActiveRuleName = rule.name;
        lastActiveRuleSocket = rule.socketNumber;
      }
    }
  }

  // 3. Apply the decisions
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (!::sockets[i])
      continue;

    // The socket must be connected to the wall outlet if not returning here
    if (!::sockets[i]->isConnected())
      continue;

    // Only apply if we have a non-SKIP decision
    if (sockets[i].virtualState != RuleDecision::Skip) {
      bool targetState = (sockets[i].virtualState == RuleDecision::On);

      // Only change state if it's different
      if (targetState != sockets[i].physicalState) {
        if (::sockets[i]->setState(targetState)) {
          sockets[i].physicalState = targetState;
          sockets[i].lastStateChange = millis();
          lastActiveRuleTime = millis();                 // Update time
          lastActiveRuleState = sockets[i].virtualState; // Update state

          TimeSync::TimeData currentTime = timeSync.getTime();
          static char timeStr[12]; // Static buffer to store the formatted time
          snprintf(lastActiveRuleTimeStr, sizeof(lastActiveRuleTimeStr), "%02d:%02d %s",
                   currentTime.hour, currentTime.minute, (targetState ? "On" : "Off"));
          Serial.printf("Rule applied at %s: %s\n", lastActiveRuleTimeStr, lastActiveRuleName);
          addRuleToHistory(lastActiveRuleName, lastActiveRuleTimeStr);
        }
      }
    }
  }
}

void SmartRuleSystem::pollPhysicalStates() {
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (::sockets[i]) {
      ::sockets[i]->readStateInfo(); // First actively read the current state
      sockets[i].physicalState = ::sockets[i]->getCurrentState();
    }
  }
}

void SmartRuleSystem::detectManualChanges() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (!::sockets[i])
      continue;

    auto &socket = sockets[i];
    RuleDecision currentPhysical = physicalToDecision(socket.physicalState);

    if (currentPhysical != socket.virtualState) {
      socket.virtualState = currentPhysical;
      socket.lastManualChange = now;
      socket.lastStateChange = now;
    }
  }
}

void SmartRuleSystem::evaluateRules() {
  // Track applied rules per socket
  std::vector<RuleDecision> lastDecisions(NUM_SOCKETS, RuleDecision::Skip);

  // Evaluate each rule in order
  for (const auto &rule : rules) {
    int socketIndex = rule.socketNumber - 1;
    if (socketIndex < 0 || socketIndex >= NUM_SOCKETS || !::sockets[socketIndex]) {
      continue;
    }

    // Get rule decision
    RuleDecision decision = rule.evaluate();

    Serial.printf("Socket %d: Rule evaluated to %s\n",
                  rule.socketNumber,
                  decision == RuleDecision::On ? "ON" : decision == RuleDecision::Off ? "OFF"
                                                                                      : "SKIP");

    if (decision == RuleDecision::On) {
      lastActiveRuleName = rule.name;
      lastActiveRuleSocket = rule.socketNumber;
    }

    // Only update if we get a non-skip decision
    if (decision != RuleDecision::Skip) {
      lastDecisions[socketIndex] = decision;
      sockets[socketIndex].virtualState = decision;

      // print if its a workday
      // Serial.printf("It is a workday %s\n", timeSync.isWorkday() ? "true" : "false");
      Serial.printf("Socket %d: Virtual state updated to %s\n",
                    socketIndex + 1,
                    decision == RuleDecision::On ? "ON" : "OFF");
    }
  }
}

void SmartRuleSystem::applyVirtualState() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_SOCKETS; i++) {
    if (!::sockets[i])
      continue;

    auto &socket = sockets[i];
    if (socket.virtualState == RuleDecision::Skip)
      continue;

    bool targetState = decisionToPhysical(socket.virtualState);
    if (targetState == socket.physicalState) {
#if DEBUG_RULES
      Serial.printf("Socket %d: No state change needed (current = %s)\n", i + 1, socket.physicalState ? "ON" : "OFF");
#endif
      socket.virtualState = RuleDecision::Skip;
      continue;
    }

    unsigned long timeSinceChange = now - socket.lastStateChange;
    // Only check min_off_time when turning ON
    if (targetState && timeSinceChange < config.min_off_time) {
      // Calculate and show remaining time
      unsigned long remainingTime = config.min_off_time - timeSinceChange;
      Serial.printf("Socket %d: Waiting %lu ms before turning ON (min off time: %lu ms)\n",
                    i + 1, remainingTime, config.min_off_time);
      // Don't reset the virtual state - keep it as ON
      continue;
    }

    Serial.printf("Socket %d: Applying state change from %s to %s\n",
                  i + 1, socket.physicalState ? "ON" : "OFF", targetState ? "ON" : "OFF");

    if (::sockets[i]->setState(targetState)) {
      socket.physicalState = targetState;
      socket.lastStateChange = now;
      socket.virtualState = RuleDecision::Skip;
      Serial.printf("Socket %d: State change successful\n", i + 1);
    } else {
      socket.physicalState = ::sockets[i]->getCurrentState(); // Update real state after failure
      Serial.printf("Socket %d: State change failed, updated physical state to %s\n",
                    i + 1, socket.physicalState ? "ON" : "OFF");
    }
  }
}

std::function<RuleDecision()> SmartRuleSystem::delayedOnOff(const char *startTime, const char *endTime, int onDelayMinutes, int offDelayMinutes, std::function<bool()> condition) {

  std::string stateKey = std::string(startTime) + "-" + std::string(endTime);

  return [this, stateKey, startTime, endTime, onDelayMinutes, offDelayMinutes, condition]() {
    // First check if we're in the time window
    bool isInTimeRange = timeSync.isTimeBetween(startTime, endTime);
    if (!isInTimeRange) {
      return RuleDecision::Skip;
    }

    auto &state = this->delayedStates[stateKey];
    bool currentCondition = condition();
    unsigned long now = millis();

    // Detect condition change
    if (currentCondition != state.lastCondition) {
      state.conditionChangeTime = now;
      state.lastCondition = currentCondition;
    }

    // Calculate elapsed time since condition change (in minutes)
    unsigned long elapsedMinutes = (now - state.conditionChangeTime) / 60000;

    if (currentCondition && elapsedMinutes >= onDelayMinutes) {
      // Condition has been true for onDelayMinutes
      return RuleDecision::On;
    } else if (!currentCondition && elapsedMinutes >= offDelayMinutes) {
      // Condition has been false for offDelayMinutes
      return RuleDecision::Off;
    }

    // During delay period, maintain previous state
    return RuleDecision::Skip;
  };
}

std::function<bool()> SmartRuleSystem::timeWindowBetween(const char *start, const char *end) {
  return [this, start, end]() {
    std::string key = std::string(start) + "-" + std::string(end);
    bool isInWindow = timeSync.isTimeBetween(start, end);
    auto &windowState = this->activeTimeWindows[key];

    // If we're in the time window and not already active
    if (isInWindow && !windowState.isActive) {
      int endHour, endMin;
      sscanf(end, "%d:%d", &endHour, &endMin);
      windowState.endTimeMillis = calculateEndTime(endHour, endMin);
      windowState.isActive = true;
      Serial.printf("Time window activated: %s to %s\n", start, end);
      return true;
    }

    // If we were active and just now went outside the window - do the ONE SHOT
    // turn off
    if (windowState.isActive &&
        (!isInWindow || millis() > windowState.endTimeMillis)) {
      Serial.printf("Time window ended: %s to %s - turning off devices\n",
                    start, end);
      // Do one-shot turn off using the state change from active to inactive
      for (const auto &rule : rules) {
        if (rule.timeWindow) {
          int socketIndex = rule.socketNumber - 1;
          if (socketIndex >= 0 && socketIndex < NUM_SOCKETS &&
              ::sockets[socketIndex]) {
            ::sockets[socketIndex]->setState(false);
          }
        }
      }
      windowState.isActive = false;
      return false;
    }

    return windowState.isActive;
  };
}

std::function<RuleDecision()> SmartRuleSystem::solarHeaterControl(float exportThreshold, float importThreshold, unsigned long minOnTime, unsigned long minOffTime, std::function<bool()> extraCondition) {

  return [=]() {
    static bool deviceIsOn = false;
    static unsigned long lastStateChangeTime = 0;
    unsigned long currentTime = millis();

    // Evaluate condition once and store result
    bool conditionMet = extraCondition();

// Debug output
#if DEBUG_RULES
    Serial.println("\n----- Heater Rule Evaluation -----");
    Serial.printf("Current state: %s\n", deviceIsOn ? "ON" : "OFF");
    Serial.printf("Extra condition met: %s\n", conditionMet ? "YES" : "NO");
    Serial.printf("Export power: %.2f W (threshold: %.2f W)\n", p1Meter ? p1Meter->getCurrentExport() : -1, exportThreshold);
    Serial.printf("Time since last change: %lu ms\n", currentTime - lastStateChangeTime);
#endif

    // First handle the case when we're ON
    if (deviceIsOn) {
      // Check if we need to turn OFF because condition is no longer met
      if (!conditionMet && (currentTime - lastStateChangeTime >= minOnTime)) {
        deviceIsOn = false;
        lastStateChangeTime = currentTime;
        Serial.println("Solar Heater Control: Turning OFF - condition not met");
        return RuleDecision::Off;
      }

      // Check if we're importing too much power (grid consumption)
      if (p1Meter && p1Meter->getCurrentImport() > importThreshold &&
          (currentTime - lastStateChangeTime >= minOnTime)) {
        deviceIsOn = false;
        lastStateChangeTime = currentTime;
        Serial.println("Solar Heater Control: Turning OFF - import threshold exceeded");
        return RuleDecision::Off;
      }

      // Otherwise stay ON
      return RuleDecision::On;
    }
    // Now handle when we're OFF
    else {
      // Only consider turning ON if the condition is met
      if (conditionMet) {
        // Check if we have enough export power AND minimum off time has elapsed
        if (p1Meter && p1Meter->getCurrentExport() > exportThreshold &&
            (currentTime - lastStateChangeTime >= minOffTime)) {
          deviceIsOn = true;
          lastStateChangeTime = currentTime;
          Serial.println("Solar Heater Control: Turning ON - export threshold met");
          return RuleDecision::On;
        }
      }

      // Stay OFF by default
      return RuleDecision::Off;
    }
  };
}

//[private after function]
std::function<bool()> SmartRuleSystem::after(const char *timeStr, int durationMins) {
  return [timeStr, durationMins]() {
    // Parse the input time string (HH:MM)
    int targetHour, targetMinute;
    sscanf(timeStr, "%d:%d", &targetHour, &targetMinute);
    int targetMinutes = targetHour * 60 + targetMinute;

    // Get the current time
    TimeSync::TimeData currentTime = timeSync.getTime();
    int currentMinutes = currentTime.hour * 60 + currentTime.minute;

    // Calculate the end time if a duration is provided
    int endMinutes = targetMinutes + durationMins;

    // Handle overnight periods (e.g., 23:30 + 60 minutes = 00:30)
    if (endMinutes >= 1440) { // 1440 minutes = 24 hours
      endMinutes -= 1440;
    }

    // Check if the current time is after the target time
    if (durationMins == 0) {
      // No duration: return true if current time is after target time
      return currentMinutes >= targetMinutes;
    } else {
      // With duration: return true if current time is within the window
      if (targetMinutes <= endMinutes) {
        // Normal case: window is within the same day
        return currentMinutes >= targetMinutes && currentMinutes < endMinutes;
      } else {
        // Overnight case: window spans midnight
        return currentMinutes >= targetMinutes || currentMinutes < endMinutes;
      }
    }
  };
}

// turns on a rule if a condition is met, but skips if not (does not turn off)
std::function<RuleDecision()> SmartRuleSystem::period(const char *startTime, const char *endTime, std::function<bool()> condition) {
  return [this, startTime, endTime, condition]() {
    // Parse times
    int startHour, startMinute, endHour, endMinute;
    sscanf(startTime, "%d:%d", &startHour, &startMinute);
    sscanf(endTime, "%d:%d", &endHour, &endMinute);

    TimeSync::TimeData currentTime = timeSync.getTime();
    int currentMinutes = currentTime.hour * 60 + currentTime.minute;
    int startMinutes = startHour * 60 + startMinute;
    int endMinutes = endHour * 60 + endMinute;

    // Check if we're in the time range
    bool isInTimeRange;
    if (endMinutes < startMinutes) {
      // Overnight period
      isInTimeRange = currentMinutes >= startMinutes || currentMinutes <= endMinutes;
    } else {
      // Same day period
      isInTimeRange = currentMinutes >= startMinutes && currentMinutes <= endMinutes;
    }

#if DEBUG_RULES
    Serial.printf("Period %s-%s: %s\n", startTime, endTime,
                  isInTimeRange ? "IN RANGE" : "outside");
#endif

    // If we're outside the time range, return SKIP (silent)
    if (!isInTimeRange) {
      return RuleDecision::Skip;
    }

    // Check if we're in the turn-off window (last 2 minutes)
    int turnOffStartMinutes = endMinutes - 2;
    bool isInTurnOffWindow = currentMinutes >= turnOffStartMinutes && currentMinutes <= endMinutes;

    if (isInTurnOffWindow) {
      // This matters - period ending, always log
      Serial.printf("Period %s-%s ending -> OFF\n", startTime, endTime);
      return RuleDecision::Off;
    }

    // We're in the active period, check condition
    bool conditionMet = condition();

#if DEBUG_RULES
    Serial.printf("Period %s-%s active, condition: %s\n",
                  startTime, endTime, conditionMet ? "MET" : "not met");
#endif

    return conditionMet ? RuleDecision::On : RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::boolPeriod(const char *startTime, const char *endTime, std::function<bool()> condition) {
  return [=]() {
    // Parse times
    int startHour, startMinute, endHour, endMinute;
    sscanf(startTime, "%d:%d", &startHour, &startMinute);
    sscanf(endTime, "%d:%d", &endHour, &endMinute);

    TimeSync::TimeData currentTime = timeSync.getTime();
    int currentMinutes = currentTime.hour * 60 + currentTime.minute;
    int startMinutes = startHour * 60 + startMinute;
    int endMinutes = endHour * 60 + endMinute;

    // Check if we're in the time range
    bool isInTimeRange;
    if (endMinutes < startMinutes) {
      // Overnight period
      isInTimeRange = currentMinutes >= startMinutes || currentMinutes <= endMinutes;
    } else {
      // Same day period
      isInTimeRange = currentMinutes >= startMinutes && currentMinutes <= endMinutes;
    }

    Serial.printf("Period check: Current: %02d:%02d, Start: %s, End: %s, In Range: %s\n",
                  currentTime.hour, currentTime.minute, startTime, endTime,
                  isInTimeRange ? "true" : "false");

    // Outside period -> SKIP (let other rules decide)
    if (!isInTimeRange) {
      Serial.println("Outside time period - returning SKIP");
      return RuleDecision::Skip;
    }

    // Inside period, condition determines ON/OFF
    bool conditionMet = condition();
    Serial.printf("In period, condition: %s\n", conditionMet ? "true" : "false");

    // The socket will handle the actual delays when state changes
    return conditionMet ? RuleDecision::On : RuleDecision::Off;
  };
}

std::function<RuleDecision()> SmartRuleSystem::onAfter(const char *timeStr, int durationMins, std::function<bool()> condition) {
  return [this, timeStr, durationMins, condition]() {
    bool isAfter = after(timeStr, durationMins)();
    bool conditionMet = condition();

    if (isAfter && conditionMet) {
      // Only turn on if both after time and condition is met
      return RuleDecision::On;
    }
    // In all other cases, skip
    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::offAfter(const char *timeStr, int durationMins, std::function<bool()> condition) {
  return [this, timeStr, durationMins, condition]() {
    bool isAfter = after(timeStr, durationMins)();
    bool conditionMet = condition();

    if (isAfter) {
      // After the time, condition determines off/skip
      return conditionMet ? RuleDecision::Off : RuleDecision::Skip;
    }
    // Before the time, don't influence the state
    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::onCondition(std::function<bool()> condition) {
  return [condition]() {
    if (condition())
      return RuleDecision::On;
    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::offCondition(std::function<bool()> condition) {
  return [condition]() {
    if (condition())
      return RuleDecision::Off;
    return RuleDecision::Skip;
  };
}

std::function<bool()> SmartRuleSystem::lightBelow(float threshold) {
  return [=]() { return sensors.getLightLevel() < threshold; };
}

struct DelayState {
  unsigned long startTime = 0;
  bool timing = false;
};

std::function<RuleDecision()> SmartRuleSystem::onConditionDelayed(std::function<bool()> condition, int delaySeconds) {

  auto state = std::make_shared<DelayState>();

  return [condition, delaySeconds, state]() {
    bool conditionMet = condition();

    if (conditionMet) {
      if (!state->timing) {
        state->startTime = millis();
        state->timing = true;
      }

      if (millis() - state->startTime >= delaySeconds * 1000UL) {
        state->timing = false;
        return RuleDecision::On;
      }
    } else {
      state->timing = false;
    }

    return RuleDecision::Skip;
  };
}

std::function<RuleDecision()> SmartRuleSystem::offConditionDelayed(std::function<bool()> condition, int delaySeconds) {

  auto state = std::make_shared<DelayState>();

  return [condition, delaySeconds, state]() {
    bool conditionMet = condition();

    if (conditionMet) {
      if (!state->timing) {
        state->startTime = millis();
        state->timing = true;
      }

      if (millis() - state->startTime >= delaySeconds * 1000UL) {
        state->timing = false;
        return RuleDecision::Off;
      }
    } else {
      state->timing = false;
    }

    return RuleDecision::Skip;
  };
}

std::function<bool()> SmartRuleSystem::lightAbove(float threshold) {
  return [=]() { return sensors.getLightLevel() > threshold; };
}

std::function<bool()> SmartRuleSystem::phoneNotPresent() {
  return []() { return phoneCheck && !phoneCheck->isDevicePresent(); };
}

std::function<bool()> SmartRuleSystem::phonePresent() {
  return []() {
    if (!phoneCheck)
      return false;
    return phoneCheck->isDevicePresent();
  };
}

std::function<bool()> SmartRuleSystem::isWorkday() {
  return []() {
    return timeSync.isWorkday();
  };
}
std::function<bool()> SmartRuleSystem::isWeekend() {
  return []() {
    return timeSync.isWeekend();
  };
}
std::function<bool()> SmartRuleSystem::isMonday() {
  return []() {
    return timeSync.isMonday();
  };
}
std::function<bool()> SmartRuleSystem::isTuesday() {
  return []() {
    return timeSync.isTuesday();
  };
}
std::function<bool()> SmartRuleSystem::isWednesday() {
  return []() {
    return timeSync.isWednesday();
  };
}
std::function<bool()> SmartRuleSystem::isThursday() {
  return []() {
    return timeSync.isThursday();
  };
}
std::function<bool()> SmartRuleSystem::isFriday() {
  return []() {
    return timeSync.isFriday();
  };
}
std::function<bool()> SmartRuleSystem::isSaturday() {
  return []() {
    return timeSync.isSaturday();
  };
}
std::function<bool()> SmartRuleSystem::isSunday() {
  return []() {
    return timeSync.isSunday();
  };
}

// power related rules

std::function<bool()> SmartRuleSystem::powerSolarActive() {
  return []() { return p1Meter && p1Meter->getCurrentExport() > 0; };
}
std::function<bool()> SmartRuleSystem::powerProducing() {
  return []() { return p1Meter && p1Meter->getCurrentExport() > 0; };
}
std::function<bool()> SmartRuleSystem::powerConsuming() {
  return []() { return p1Meter && p1Meter->getCurrentImport() > 0; };
}
std::function<bool()> SmartRuleSystem::powerProductionBelow(float threshold) {
  return [=]() { return p1Meter && p1Meter->getCurrentExport() < threshold; };
}
std::function<bool()> SmartRuleSystem::powerProductionAbove(float threshold) {
  return [=]() { return p1Meter && p1Meter->getCurrentExport() > threshold; };
}

const char *SmartRuleSystem::rndTime(const char *time, int maxMinutes, int extraSeed) {
  TimeSync::TimeData t = timeSync.getTime();
  srand(t.dayOfYear + t.weekNum + extraSeed);
  int addMinutes = rand() % maxMinutes;

  int hour, minute;
  sscanf(time, "%d:%d", &hour, &minute);

  minute += addMinutes;
  hour += minute / 60;
  minute = minute % 60;
  hour = hour % 24;

  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
  Serial.printf("Generated random time: %s (base: %s, added: %d min)\n",
                timeBuffer, time, addMinutes);
  return timeBuffer;
}

// ============================================================================
// SUN POSITION CALCULATION
// ============================================================================
void SmartRuleSystem::setLocation(float lat, float lon, int elevationMeters) {
  configLatitude = lat;
  configLongitude = lon;
  // Negative elevation (below sea level) doesn't give meaningful horizon
  // correction - treat as sea level
  configElevation = (elevationMeters > 0) ? elevationMeters : 0;
  lastSunCalcDay = -1; // Force recalculation

  Serial.printf("Location set: lat %.4f, lon %.4f, elevation %dm\n",
                lat, lon, configElevation);
}

float SmartRuleSystem::getLocalEarthRadius(float latitudeDeg) {
  // WGS84 ellipsoid model accounts for Earth being oblate
  // Radius varies from ~6378km at equator to ~6357km at poles
  float lat = latitudeDeg * PI / 180.0;
  float cosLat = cos(lat);
  float sinLat = sin(lat);

  float a2 = WGS84_A * WGS84_A;
  float b2 = WGS84_B * WGS84_B;

  float numerator = pow(a2 * cosLat, 2) + pow(b2 * sinLat, 2);
  float denominator = pow(WGS84_A * cosLat, 2) + pow(WGS84_B * sinLat, 2);

  return sqrt(numerator / denominator);
}

void SmartRuleSystem::calculateDailySunTimes() {
  TimeSync::TimeData t = timeSync.getTime();

  // Only recalculate once per day
  if (lastSunCalcDay == t.dayOfYear) {
    return;
  }

  int dayOfYear = t.dayOfYear;
  float lat = configLatitude;
  float lon = configLongitude;

  // Fractional year in radians
  float gamma = 2.0 * PI * (dayOfYear - 1) / 365.0;

  // Equation of time (minutes) - corrects for elliptical orbit and axial tilt
  float eqTime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma) - 0.014615 * cos(2.0 * gamma) - 0.040849 * sin(2.0 * gamma));

  // Solar declination (radians) - sun's angle relative to equator
  // Ranges from +23.45° (summer) to -23.45° (winter)
  float decl = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma) - 0.006758 * cos(2.0 * gamma) + 0.000907 * sin(2.0 * gamma) - 0.002697 * cos(3.0 * gamma) + 0.00148 * sin(3.0 * gamma);

  // Zenith angle with corrections:
  // 90° base + 0.833° for refraction and sun radius
  // Minus elevation correction (higher = see further over horizon)
  float baseZenithDeg = 90.833;

  float elevationCorrectionDeg = 0.0;
  if (configElevation > 0) {
    float localRadius = getLocalEarthRadius(lat);
    elevationCorrectionDeg = sqrt(2.0 * configElevation / localRadius) * (180.0 / PI);
  }

  float zenithDeg = baseZenithDeg - elevationCorrectionDeg;
  float zenith = zenithDeg * PI / 180.0;

  // Hour angle calculation
  float latRad = lat * PI / 180.0;
  float cosHA = (cos(zenith) / (cos(latRad) * cos(decl))) - tan(latRad) * tan(decl);

  // Handle polar regions
  if (cosHA > 1.0) {
    // Polar night: sun never rises
    sunriseMinutes = -1;
    sunsetMinutes = -1;
    lastSunCalcDay = t.dayOfYear;
    snprintf(sunriseBuffer, sizeof(sunriseBuffer), "--:--");
    snprintf(sunsetBuffer, sizeof(sunsetBuffer), "--:--");
    Serial.println("Sun calculation: Polar night - sun does not rise");
    return;
  }

  if (cosHA < -1.0) {
    // Midnight sun: sun never sets
    sunriseMinutes = 0;
    sunsetMinutes = 1440;
    lastSunCalcDay = t.dayOfYear;
    snprintf(sunriseBuffer, sizeof(sunriseBuffer), "00:00");
    snprintf(sunsetBuffer, sizeof(sunsetBuffer), "23:59");
    Serial.println("Sun calculation: Midnight sun - sun does not set");
    return;
  }

  float haRad = acos(cosHA);
  float haDeg = haRad * 180.0 / PI;

  // Solar noon and sunrise/sunset in minutes from midnight UTC
  float solarNoonUTC = 720.0 - (4.0 * lon) - eqTime;
  float sunriseUTC = solarNoonUTC - (haDeg * 4.0);
  float sunsetUTC = solarNoonUTC + (haDeg * 4.0);

  // Get timezone offset from TimeSync (automatically handles DST)
  int tzOffset = timeSync.getTimezoneOffsetMinutes();

  // Apply timezone offset for local time
  sunriseMinutes = ((int)round(sunriseUTC) + tzOffset) % 1440;
  sunsetMinutes = ((int)round(sunsetUTC) + tzOffset) % 1440;

  if (sunriseMinutes < 0)
    sunriseMinutes += 1440;
  if (sunsetMinutes < 0)
    sunsetMinutes += 1440;

  // Update string buffers for "HH:MM" format
  snprintf(sunriseBuffer, sizeof(sunriseBuffer), "%02d:%02d",
           sunriseMinutes / 60, sunriseMinutes % 60);
  snprintf(sunsetBuffer, sizeof(sunsetBuffer), "%02d:%02d",
           sunsetMinutes / 60, sunsetMinutes % 60);

  lastSunCalcDay = t.dayOfYear;

  Serial.printf("Sun times for day %d (TZ offset %+d min): Sunrise %s, Sunset %s\n",
                t.dayOfYear, tzOffset, sunriseBuffer, sunsetBuffer);
}

// ============================================================================
// SUN TIME GETTERS
// ============================================================================

const char *SmartRuleSystem::getSunriseTime() {
  calculateDailySunTimes();
  return sunriseBuffer;
}

const char *SmartRuleSystem::getSunsetTime() {
  calculateDailySunTimes();
  return sunsetBuffer;
}

// ============================================================================
// SUN-BASED CONDITIONS
// ============================================================================

std::function<bool()> SmartRuleSystem::sunUp() {
  return []() {
    calculateDailySunTimes();

    if (sunriseMinutes < 0)
      return false; // Polar night
    if (sunsetMinutes >= 1440)
      return true; // Midnight sun

    TimeSync::TimeData t = timeSync.getTime();
    int currentMinutes = t.hour * 60 + t.minute;

    return currentMinutes >= sunriseMinutes && currentMinutes < sunsetMinutes;
  };
}

std::function<bool()> SmartRuleSystem::sunDown() {
  return []() {
    calculateDailySunTimes();

    if (sunriseMinutes < 0)
      return true; // Polar night
    if (sunsetMinutes >= 1440)
      return false; // Midnight sun

    TimeSync::TimeData t = timeSync.getTime();
    int currentMinutes = t.hour * 60 + t.minute;

    return currentMinutes < sunriseMinutes || currentMinutes >= sunsetMinutes;
  };
}

std::function<bool()> SmartRuleSystem::beforeSunrise(int minutes) {
  return [minutes]() {
    calculateDailySunTimes();

    if (sunriseMinutes < 0)
      return false; // No sunrise today

    TimeSync::TimeData t = timeSync.getTime();
    int currentMinutes = t.hour * 60 + t.minute;

    int windowStart = sunriseMinutes - minutes;
    if (windowStart < 0)
      windowStart += 1440;

    if (windowStart < sunriseMinutes) {
      return currentMinutes >= windowStart && currentMinutes < sunriseMinutes;
    } else {
      // Window spans midnight
      return currentMinutes >= windowStart || currentMinutes < sunriseMinutes;
    }
  };
}

std::function<bool()> SmartRuleSystem::afterSunrise(int minutes) {
  return [minutes]() {
    calculateDailySunTimes();

    if (sunriseMinutes < 0)
      return false;

    TimeSync::TimeData t = timeSync.getTime();
    int currentMinutes = t.hour * 60 + t.minute;

    int windowEnd = sunriseMinutes + minutes;
    if (windowEnd >= 1440)
      windowEnd -= 1440;

    if (sunriseMinutes < windowEnd) {
      return currentMinutes >= sunriseMinutes && currentMinutes < windowEnd;
    } else {
      return currentMinutes >= sunriseMinutes || currentMinutes < windowEnd;
    }
  };
}

std::function<bool()> SmartRuleSystem::beforeSunset(int minutes) {
  return [minutes]() {
    calculateDailySunTimes();

    if (sunsetMinutes >= 1440)
      return false; // No sunset today

    TimeSync::TimeData t = timeSync.getTime();
    int currentMinutes = t.hour * 60 + t.minute;

    int windowStart = sunsetMinutes - minutes;
    if (windowStart < 0)
      windowStart += 1440;

    if (windowStart < sunsetMinutes) {
      return currentMinutes >= windowStart && currentMinutes < sunsetMinutes;
    } else {
      return currentMinutes >= windowStart || currentMinutes < sunsetMinutes;
    }
  };
}

std::function<bool()> SmartRuleSystem::afterSunset(int minutes) {
  return [minutes]() {
    calculateDailySunTimes();

    if (sunsetMinutes >= 1440)
      return false;

    TimeSync::TimeData t = timeSync.getTime();
    int currentMinutes = t.hour * 60 + t.minute;

    int windowEnd = sunsetMinutes + minutes;
    if (windowEnd >= 1440)
      windowEnd -= 1440;

    if (sunsetMinutes < windowEnd) {
      return currentMinutes >= sunsetMinutes && currentMinutes < windowEnd;
    } else {
      return currentMinutes >= sunsetMinutes || currentMinutes < windowEnd;
    }
  };
}

// ============================================================================
// TEMPERATURE CONDITIONS
// ============================================================================

std::function<bool()> SmartRuleSystem::temperatureAbove(float threshold) {
  return [=]() {
    if (!sensors.hasBME280())
      return false;
    return sensors.getTemperature() > threshold;
  };
}

std::function<bool()> SmartRuleSystem::temperatureBelow(float threshold) {
  return [=]() {
    if (!sensors.hasBME280())
      return false;
    return sensors.getTemperature() < threshold;
  };
}

// ============================================================================
// HUMIDITY CONDITIONS
// ============================================================================

std::function<bool()> SmartRuleSystem::humidityAbove(float threshold) {
  return [=]() {
    if (!sensors.hasBME280())
      return false;
    return sensors.getHumidity() > threshold;
  };
}

std::function<bool()> SmartRuleSystem::humidityBelow(float threshold) {
  return [=]() {
    if (!sensors.hasBME280())
      return false;
    return sensors.getHumidity() < threshold;
  };
}

// ============================================================================
// AIR PRESSURE CONDITIONS
// ============================================================================

std::function<bool()> SmartRuleSystem::pressureAbove(float threshold) {
  return [=]() {
    if (!sensors.hasBME280())
      return false;
    return sensors.getPressure() > threshold;
  };
}

std::function<bool()> SmartRuleSystem::pressureBelow(float threshold) {
  return [=]() {
    if (!sensors.hasBME280())
      return false;
    return sensors.getPressure() < threshold;
  };
}

// ============================================================================
// SOCKET STATE CONDITIONS
// ============================================================================

std::function<bool()> SmartRuleSystem::socketIsOn(int socketNumber) {
  return [this, socketNumber]() {
    int idx = socketNumber - 1;
    if (idx < 0 || idx >= NUM_SOCKETS)
      return false;
    return sockets[idx].physicalState;
  };
}

std::function<bool()> SmartRuleSystem::socketIsOff(int socketNumber) {
  return [this, socketNumber]() {
    int idx = socketNumber - 1;
    if (idx < 0 || idx >= NUM_SOCKETS)
      return true;
    return !sockets[idx].physicalState;
  };
}

// ============================================================================
// DURATION CONDITIONS
// ============================================================================

std::function<bool()> SmartRuleSystem::hasBeenOnFor(int socketNumber, unsigned long minutes) {
  return [this, socketNumber, minutes]() {
    int idx = socketNumber - 1;
    if (idx < 0 || idx >= NUM_SOCKETS)
      return false;

    // Must be currently ON
    if (!sockets[idx].physicalState)
      return false;

    unsigned long elapsedMs = millis() - sockets[idx].lastStateChange;
    unsigned long elapsedMinutes = elapsedMs / 60000;

    return elapsedMinutes >= minutes;
  };
}

std::function<bool()> SmartRuleSystem::hasBeenOffFor(int socketNumber, unsigned long minutes) {
  return [this, socketNumber, minutes]() {
    int idx = socketNumber - 1;
    if (idx < 0 || idx >= NUM_SOCKETS)
      return false;

    // Must be currently OFF
    if (sockets[idx].physicalState)
      return false;

    unsigned long elapsedMs = millis() - sockets[idx].lastStateChange;
    unsigned long elapsedMinutes = elapsedMs / 60000;

    return elapsedMinutes >= minutes;
  };
}

// grouping logic

std::function<bool()> SmartRuleSystem::allOf(std::vector<std::function<bool()>> conditions) {
  return [=]() {
    for (const auto &cond : conditions) {
      if (!cond())
        return false;
    }
    return true;
  };
}
std::function<bool()> SmartRuleSystem::anyOf(std::vector<std::function<bool()>> conditions) {
  return [=]() {
    for (const auto &cond : conditions) {
      if (cond())
        return true;
    }
    return false;
  };
}
std::function<bool()> SmartRuleSystem::notOf(std::function<bool()> condition) {
  return [=]() { return !condition(); };
}

bool SmartRuleSystem::getSocketState(int socketIndex) const {
  if (socketIndex < 0 || socketIndex >= NUM_SOCKETS)
    return false;
  return sockets[socketIndex].physicalState;
}

// inner logic
RuleDecision SmartRuleSystem::physicalToDecision(bool state) {
  return state ? RuleDecision::On : RuleDecision::Off;
}
bool SmartRuleSystem::decisionToPhysical(RuleDecision decision) {
  return decision == RuleDecision::On;
}

// helper function to calculate end time in milliseconds
unsigned long SmartRuleSystem::calculateEndTime(int hour, int minute) {
  TimeSync::TimeData t = timeSync.getTime();
  unsigned long now = millis();
  int currentHour = t.hour;
  int currentMinute = t.minute;

  // Calculate minutes remaining until end time
  int minutesRemaining = (hour - currentHour) * 60 + (minute - currentMinute);
  if (minutesRemaining < 0)
    minutesRemaining += 24 * 60; // Handle overnight

  return now + (minutesRemaining * 60 * 1000UL);
}
void SmartRuleSystem::clearRules() {
  rules.clear();
  Serial.println("All rules cleared");
}