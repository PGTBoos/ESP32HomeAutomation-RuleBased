# ESP32HomeAutomation-RuleBased
ESP32 home automation powered by a fluent rule engine. Declarative rules for socket control based on time, light levels, phone presence, and solar production. Built with a clean DSL that reads like English.

Lets start below with how my rule system works 

# SmartRuleSystem Wiki

* [**Core Concepts**](#core-concepts)
* [**Configuration**](#configuration)
  * [setLocation](#setlocation)
* [**Rule Actions**](#rule-actions)
  * [period](#period)
  * [boolPeriod](#boolperiod)
  * [onAfter](#onafter)
  * [offAfter](#offafter)
  * [onCondition](#oncondition)
  * [offCondition](#offcondition)
  * [onConditionDelayed](#onconditiondelayed)
  * [offConditionDelayed](#offconditiondelayed)
  * [delayedOnOff](#delayedonoff)
  * [solarHeaterControl](#solarheatercontrol)
* [**Conditions**](#conditions)
  * [Light Sensor](#light-sensor)
    * [lightBelow](#lightbelow)
    * [lightAbove](#lightabove)
  * [Temperature](#temperature)
    * [temperatureAbove](#temperatureabove)
    * [temperatureBelow](#temperaturebelow)
  * [Humidity](#humidity)
    * [humidityAbove](#humidityabove)
    * [humidityBelow](#humiditybelow)
  * [Air Pressure](#air-pressure)
    * [pressureAbove](#pressureabove)
    * [pressureBelow](#pressurebelow)
  * [Phone Presence](#phone-presence)
    * [phonePresent](#phonepresent)
    * [phoneNotPresent](#phonenotpresent)
  * [Day of Week](#day-of-week)
    * [isWorkday](#isworkday)
    * [isWeekend](#isweekend)
    * [isMonday](#ismonday)
    * [isTuesday](#istuesday)
    * [isWednesday](#iswednesday)
    * [isThursday](#isthursday)
    * [isFriday](#isfriday)
    * [isSaturday](#issaturday)
    * [isSunday](#issunday)
  * [Sun Position](#sun-position)
    * [sunUp](#sunup)
    * [sunDown](#sundown)
    * [beforeSunrise](#beforesunrise)
    * [afterSunrise](#aftersunrise)
    * [beforeSunset](#beforesunset)
    * [afterSunset](#aftersunset)
  * [Power / Solar](#power--solar)
    * [powerSolarActive](#powersolaractive)
    * [powerProducing](#powerproducing)
    * [powerConsuming](#powerconsuming)
    * [powerProductionAbove](#powerproductionabove)
    * [powerProductionBelow](#powerproductionbelow)
  * [Socket State](#socket-state)
    * [socketIsOn](#socketison)
    * [socketIsOff](#socketisoff)
  * [Duration](#duration)
    * [hasBeenOnFor](#hasbeenonfor)
    * [hasBeenOffFor](#hasbeenofffor)
  * [Time Window](#time-window)
    * [timeWindowBetween](#timewindowbetween)
* [**Combinators**](#combinators)
  * [allOf](#allof)
  * [anyOf](#anyof)
  * [notOf](#notof)
* [**Time Utilities**](#time-utilities)
  * [getSunriseTime](#getsunrisetime)
  * [getSunsetTime](#getsunsettime)
  * [rndTime](#rndtime)
  * [addMinutesToTime](#addminutestotime)
  * [addHoursToTime](#addhourstotime)
  * [getDailyRandom](#getdailyrandom)
  * [getDailyRandom60](#getdailyrandom60)
  * [getDailyRandom24](#getdailyrandom24)
* [**System Functions**](#system-functions)
  * [addRule](#addrule)
  * [update](#update)
  * [pollPhysicalStates](#pollphysicalstates)
  * [detectManualChanges](#detectmanualchanges) (_internal use_)
  * [evaluateRules](#evaluaterules) (_internal use_)
  * [applyVirtualState](#applyvirtualstate) (_internal use_)
  * [getSocketState](#getsocketstate)
  * [clearRules](#clearrules)
* [**Example Usage**](#example-usage)

---

## Core Concepts

The system uses three possible outcomes for any rule evaluation:

| Decision | Meaning |
| --- | --- |
| `On` | Turn the socket on |
| `Off` | Turn the socket off |
| `Skip` | Don't change anything, let other rules decide |

---

## Core Concepts

The system uses three possible outcomes for any rule evaluation:

| Decision | Meaning |
| --- | --- |
| `On` | Turn the socket on |
| `Off` | Turn the socket off |
| `Skip` | Don't change anything, let other rules decide |

Rules are evaluated in order.  
Later rules can override earlier ones if they return `On` or `Off`.  
This code uses a simple time format `HH:MM`  
Some functions use seconds where its just an int and 600 sec is 5 minutes.

---

## Configuration

Setup function for sun position calculations. Call this once at startup before using any sun-based conditions. Timezone is automatically handled by TimeSync (including DST).

### setLocation

```cpp
setLocation(latitude, longitude, elevationMeters)
```

Sets your geographic location for sunrise/sunset calculations. Use decimal degrees format (right-click in Google Maps to copy coordinates). Negative values for South latitude and West longitude. Elevation defaults to 0 and is optional.

The calculation uses the WGS84 ellipsoid model (same as GPS) to account for Earth's oblate shape, giving accurate results regardless of your latitude. Timezone offset is automatically obtained from TimeSync, which already handles CET/CEST transitions.

```cpp
// Netherlands (below sea level treated as 0)
SmartRuleSystem::setLocation(52.37, 4.90, 0);

// Swiss Alps (elevation affects sunrise/sunset by ~10 min at 1500m)
SmartRuleSystem::setLocation(46.82, 8.40, 1500);
```

---

## Rule Actions

These functions return a `RuleDecision` and define _when_ and _how_ a socket should change state.

### period

```cpp
period(startTime, endTime, condition)
```

Active during a time window. Turns **on** if condition is met, returns **skip** if not. Automatically turns **off** in the last 2 minutes of the period.

**Use case:** Morning light that only activates when it's dark.

---

### boolPeriod

```cpp
boolPeriod(startTime, endTime, condition)
```

Active during a time window. Returns **on** when condition is true, **off** when false. Unlike `period`, this actively turns off when condition fails (no skip).

**Use case:** Light that follows a sensor throughout the entire period.

---

### onAfter

```cpp
onAfter(timeStr, durationMins, condition)
```

Turns **on** after the specified time if condition is met. Returns **skip** otherwise. The optional `durationMins` creates a time window.

**Use case:** Turn on evening lights after 18:00 when phone is home.

---

### offAfter

```cpp
offAfter(timeStr, durationMins, condition)
```

Turns **off** after the specified time if condition is met. Returns **skip** otherwise.

**Use case:** Force lights off after 23:30.

---

### onCondition

```cpp
onCondition(condition)
```

Turns **on** when condition is true, **skip** otherwise. No time restrictions.

**Use case:** Simple condition-based activation without time constraints.

---

### offCondition

```cpp
offCondition(condition)
```

Turns **off** when condition is true, **skip** otherwise.

**Use case:** Turn off when phone leaves home.

---

### onConditionDelayed

```cpp
onConditionDelayed(condition, delaySeconds)
```

Turns **on** only after the condition has been continuously true for the specified delay.

**Use case:** Prevent flickering by requiring stable conditions before switching.

---

### offConditionDelayed

```cpp
offConditionDelayed(condition, delaySeconds)
```

Turns **off** only after the condition has been continuously true for the specified delay.

**Use case:** Keep lights on briefly after motion stops.

---

### delayedOnOff

```cpp
delayedOnOff(startTime, endTime, onDelayMinutes, offDelayMinutes, condition)
```

Hysteresis control within a time window. Waits `onDelayMinutes` before turning on and `offDelayMinutes` before turning off.

**Use case:** Prevent rapid switching when conditions fluctuate near thresholds.

---

### solarHeaterControl

```cpp
solarHeaterControl(exportThreshold, importThreshold, minOnTime, minOffTime, extraCondition)
```

Specialized rule for solar-powered devices. Turns on when exporting enough power, turns off when importing too much. Respects minimum on/off times to protect equipment.

**Use case:** Water heater that runs on excess solar production.

---

## Conditions

These functions return `bool` and are used as parameters in rule actions.

### Light Sensor

Requires BH1750 sensor.

| Function | Description |
| --- | --- |
| `lightBelow(threshold)` | True when light level \< threshold (lux) |
| `lightAbove(threshold)` | True when light level > threshold (lux) |

---

### Temperature

Requires BME280 sensor. Returns false if sensor not found.

| Function | Description |
| --- | --- |
| `temperatureAbove(threshold)` | True when temperature > threshold (°C) |
| `temperatureBelow(threshold)` | True when temperature \< threshold (°C) |

---

### Humidity

Requires BME280 sensor. Returns false if sensor not found.

| Function | Description |
| --- | --- |
| `humidityAbove(threshold)` | True when relative humidity > threshold (%) |
| `humidityBelow(threshold)` | True when relative humidity \< threshold (%) |

---

### Air Pressure

Requires BME280 sensor. Returns false if sensor not found. Pressure is in hPa (hectopascal). Standard sea level pressure is 1013.25 hPa. Falling pressure often indicates incoming bad weather.

| Function | Description |
| --- | --- |
| `pressureAbove(threshold)` | True when pressure > threshold (hPa) |
| `pressureBelow(threshold)` | True when pressure \< threshold (hPa) |

---

### Phone Presence

| Function | Description |
| --- | --- |
| `phonePresent()` | True when phone is detected on the network |
| `phoneNotPresent()` | True when phone is not detected |

---

### Day of Week

| Function | Description |
| --- | --- |
| `isWorkday()` | True Monday through Friday |
| `isWeekend()` | True Saturday and Sunday |
| `isMonday()` | True on Monday |
| `isTuesday()` | True on Tuesday |
| `isWednesday()` | True on Wednesday |
| `isThursday()` | True on Thursday |
| `isFriday()` | True on Friday |
| `isSaturday()` | True on Saturday |
| `isSunday()` | True on Sunday |

---

### Sun Position

Requires `setLocation()` to be called at startup. Sun times are calculated once per day using the NOAA solar position algorithm with WGS84 ellipsoid for accurate Earth radius. Timezone is automatically obtained from TimeSync, so DST transitions are handled correctly.

| Function | Description |
| --- | --- |
| `sunUp()` | True when sun is above the horizon |
| `sunDown()` | True when sun is below the horizon |
| `beforeSunrise(minutes)` | True during the X minutes before sunrise |
| `afterSunrise(minutes)` | True during the X minutes after sunrise |
| `beforeSunset(minutes)` | True during the X minutes before sunset |
| `afterSunset(minutes)` | True during the X minutes after sunset |

The before/after functions create a time window. For example, `beforeSunset(30)` is true starting 30 minutes before sunset and ending at sunset.

---

### Power / Solar

Requires P1 meter.

| Function | Description |
| --- | --- |
| `powerSolarActive()` | True when solar is producing |
| `powerProducing()` | True when exporting to grid |
| `powerConsuming()` | True when importing from grid |
| `powerProductionAbove(threshold)` | True when export > threshold (watts) |
| `powerProductionBelow(threshold)` | True when export \< threshold (watts) |

---

### Socket State

Check the state of other sockets. Useful for creating dependent rules or chaining behavior. Socket numbers are 1-indexed (same as `addRule`).

| Function | Description |
| --- | --- |
| `socketIsOn(socketNumber)` | True if the specified socket is currently on |
| `socketIsOff(socketNumber)` | True if the specified socket is currently off |

---

### Duration

Check how long a socket has been in its current state. Uses the existing `lastStateChange` tracking. Useful for timeout rules or ensuring minimum on/off periods.

| Function | Description |
| --- | --- |
| `hasBeenOnFor(socketNumber, minutes)` | True if socket has been on for at least X minutes |
| `hasBeenOffFor(socketNumber, minutes)` | True if socket has been off for at least X minutes |

---

### Time Window

| Function | Description |
| --- | --- |
| `timeWindowBetween(start, end)` | True when current time is within the window |

The `timeWindowBetween()` function has special behavior: it tracks when the window ends and performs a one-shot turn-off of all associated devices when exiting the window. This is primarily used as the `timeWindow` parameter in `addRule()`.

```cpp
// Used as timeWindow parameter for automatic turn-off at window end
rs.addRule(1, "Evening light",
    rs.onCondition(rs.lightBelow(10)),
    rs.timeWindowBetween("18:00", "23:00"));  // Auto-off at 23:00
```

---

## Combinators

Combine multiple conditions with logic operators.

### allOf

```cpp
allOf({condition1, condition2, ...})
```

Returns true only if **all** conditions are true (AND logic).

---

### anyOf

```cpp
anyOf({condition1, condition2, ...})
```

Returns true if **any** condition is true (OR logic).

---

### notOf

```cpp
notOf(condition)
```

Inverts a condition (NOT logic).

---

## Time Utilities

Helper functions for working with time values.

### getSunriseTime / getSunsetTime

```cpp
getSunriseTime()  // Returns "HH:MM"
getSunsetTime()   // Returns "HH:MM"
```

Get today's sunrise or sunset time as a string. Compatible with other time functions like `period()` and `addMinutesToTime()`.

```cpp
// Display today's sun times
Serial.printf("Sunrise: %s, Sunset: %s\n", 
              SmartRuleSystem::getSunriseTime(), 
              SmartRuleSystem::getSunsetTime());

// Use with existing functions
rs.period(rs.addMinutesToTime(rs.getSunsetTime(), -30), "23:00", condition);
```

---

### rndTime

```cpp
rndTime(baseTime, maxMinutes, extraSeed)
```

Generates a random time offset from a base time. The randomness is consistent per day (same seed produces same result all day).

**Use case:** Simulate natural lighting patterns that vary slightly each day.

---

### addMinutesToTime / addHoursToTime

```cpp
addMinutesToTime(baseTime, minutesToAdd)
addHoursToTime(baseTime, hoursToAdd)
```

Simple time arithmetic. Returns a new time string in "HH:MM" format.

---

### Daily Randoms

```cpp
getDailyRandom(index)    // Returns 0-99
getDailyRandom60(index)  // Returns 0-59
getDailyRandom24(index)  // Returns 0-23
```

Pre-generated random numbers that stay consistent throughout the day. Useful for randomized schedules that don't change mid-day.

---

## System Functions

### addRule

```cpp
addRule(socketNumber, ruleName, evaluateFunction, timeWindow)
```

Registers a new rule for a socket. Rules are evaluated in the order they are added.

---

### update

```cpp
update()
```

Call this in your main loop. Polls physical states, evaluates all rules, and applies changes. This is the main function that drives the rule system.

---

### pollPhysicalStates

```cpp
pollPhysicalStates()
```

Manually refresh all socket states from the hardware. Reads the current on/off state from each physical socket device. Called automatically by `update()`, but can be called separately if needed.

---

### detectManualChanges

```cpp
detectManualChanges()
```

Detects if someone manually changed a socket (e.g., pressed the physical button). Updates the virtual state to match the physical state and records the time of the manual change.

---

### evaluateRules

```cpp
evaluateRules()
```

Manually trigger evaluation of all rules. Updates virtual states based on rule decisions. Normally called as part of `update()`.

---

### applyVirtualState

```cpp
applyVirtualState()
```

Applies the virtual state decisions to the physical sockets. Respects minimum on/off times configured in the system. Normally called as part of `update()`.

---

### getSocketState

```cpp
getSocketState(socketIndex)
```

Returns the physical state (true/false) of a socket by its index (0-indexed). Returns false if the index is out of range.

```cpp
// Check if socket 0 is on
bool isOn = ruleSystem.getSocketState(0);
```

---

### clearRules

```cpp
clearRules()
```

Removes all registered rules.

---

## Example Usage

```cpp
void setup() {
  // ... existing setup code ...

  // Configure location for sun calculations (after timeSync.begin())
  SmartRuleSystem::setLocation(52.37, 4.90, 0);   // Amsterdam

  setupRules();
}

void setupRules() {
  auto &rs = ruleSystem;

  // Morning light on workdays, only when dark
  rs.addRule(1, "Morning",
      rs.period("07:00", "08:30",
          rs.allOf({rs.isWorkday(), rs.lightBelow(10)})));

  // Evening light when home and dark
  rs.addRule(1, "Evening",
      rs.period("17:00", "23:00",
          rs.allOf({rs.phonePresent(), rs.lightBelow(15)})));

  // Solar water heater
  rs.addRule(2, "Solar Heater",
      rs.solarHeaterControl(500, 100, 300000, 60000,
          rs.allOf({rs.isWorkday(), rs.lightAbove(50)})));

  // Force off late at night
  rs.addRule(1, "Night Off",
      rs.offAfter("23:30", 0, []() { return true; }));

  // Sun-based lighting
  rs.addRule(3, "Sunset light",
      rs.onCondition(rs.allOf({rs.beforeSunset(30), rs.lightBelow(15)})));

  // Climate control
  rs.addRule(4, "Heating",
      rs.onCondition(rs.allOf({
          rs.temperatureBelow(18.0),
          rs.phonePresent(),
          rs.sunDown()
      })));

  // Auto-off safety rule
  rs.addRule(4, "Heating timeout",
      rs.offCondition(rs.hasBeenOnFor(4, 180)));  // Max 3 hours

  // Socket chaining
  rs.addRule(5, "Follow main light",
      rs.onCondition(rs.socketIsOn(1)));
}
```
