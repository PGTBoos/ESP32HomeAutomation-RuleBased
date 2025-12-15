# ESP32HomeAutomation-RuleBased
ESP32 home automation powered by a fluent rule engine. Declarative rules for socket control based on time, light levels, phone presence, and solar production. Built with a clean DSL that reads like English. For about 30 Euro (or less, thanks to [uncle Ali](https://nl.aliexpress.com/item/1005005520419040.html?spm=a2g0o.productlist.main.34.35f05K9b5K9bFv&algo_pvid=1fe82635-a72e-4060-8650-93a687c4832c&algo_exp_id=1fe82635-a72e-4060-8650-93a687c4832c-33&pdp_ext_f=%7B%22order%22%3A%2214%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21EUR%214.42%213.54%21%21%215.07%214.06%21%402103890117658255801394317e47b6%2112000033397389571%21sea%21NL%21929538575%21X%211%210%21n_tag%3A-29919%3Bd%3A2d2b1a90%3Bm03_new_user%3A-29895&curPageLogUid=mWPBu79V3ewX&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005005520419040%7C_p_origin_prod%3A) )

### A typical day:    
Wake up, light turns on if it's not light enough until you go to work.  
At daytime, too much solar power?  Then you turn on a heater (or cooler).  
At some lux level, you turn on the main light, and at 6, the TV backlight flips on.  
Between 23:00 / 24:00, randomly turn off a main light (or only when you're not at home).  
But you're still watching TV, so the backlight turns off only if all other lights are off.  
Or do you want sun up, sun down? This code has your back.  
As it contains location-based sun up / down triggers.  
And yes, it is NTP time synced, handling winter and summer time fine.  
Some actions keep a switch for a duration under control.  
While others allow for manual changes (or delayed actions).    



| Overview | Schematic |
|--|--|
| ![split your breadboards its esp32 time](https://github.com/PGTBoos/ESP32HomeAutomation-RuleBased/blob/main/IMG_esp32home.jpg) | ![schematic](https://github.com/PGTBoos/ESP32HomeAutomation-RuleBased/blob/main/wiring.svg) |




# SmartRuleSystem Wiki

A flexible rule-based automation system for ESP32 smart home control.  
Rules evaluate conditions and time windows to control WiFi sockets automatically.


---

## Table of Contents

* [**Core Concepts**](#core-concepts)
* [**Configuration**](#configuration)
* [**Rule Actions**](#rule-actions)
* [**Conditions**](#conditions)
* [**Combinators**](#combinators)
* [**Time Utilities**](#time-utilities)
* [**System Functions**](#system-functions)
* [**Example Usage**](#example-usage)

---

## Core Concepts

| Decision | Meaning |
|----------|---------|
| `On` | Turn the socket on |
| `Off` | Turn the socket off |
| `Skip` | Don't change anything, let other rules decide |

Rules are evaluated in order. Later rules can override earlier ones if they return `On` or `Off`.  
Time format: `HH:MM`. Duration parameters use seconds (600 = 5 minutes).

---

## Configuration

<details>
<summary><b>setLocation</b> - Configure sun position calculations</summary>

```cpp
setLocation(latitude, longitude, elevationMeters)
```

Sets your geographic location for sunrise/sunset calculations. Use decimal degrees format (right-click in Google Maps to copy coordinates). Negative values for South latitude and West longitude. Elevation defaults to 0 and is optional.

```cpp
SmartRuleSystem::setLocation(52.37, 4.90, 0);    // Amsterdam
SmartRuleSystem::setLocation(46.82, 8.40, 1500); // Swiss Alps
```

</details>

---

## Rule Actions

<details>
<summary><b>period</b> - Active during a time window</summary>

```cpp
period(startTime, endTime, condition)
```

Turns **on** if condition is met, returns **skip** if not. Automatically turns **off** in the last 2 minutes of the period.

**Use case:** Morning light that only activates when it's dark.

</details>

<details>
<summary><b>boolPeriod</b> - On/Off based on condition in time window</summary>

```cpp
boolPeriod(startTime, endTime, condition)
```

Returns **on** when condition is true, **off** when false. Unlike `period`, this actively turns off when condition fails (no skip).

**Use case:** Light that follows a sensor throughout the entire period.

</details>

<details>
<summary><b>onAfter</b> - Turn on after specified time</summary>

```cpp
onAfter(timeStr, durationMins, condition)
```

Turns **on** after the specified time if condition is met. Returns **skip** otherwise. The optional `durationMins` creates a time window.

**Use case:** Turn on evening lights after 18:00 when phone is home.

</details>

<details>
<summary><b>offAfter</b> - Turn off after specified time</summary>

```cpp
offAfter(timeStr, durationMins, condition)
```

Turns **off** after the specified time if condition is met. Returns **skip** otherwise.

**Use case:** Force lights off after 23:30.

</details>

<details>
<summary><b>onCondition</b> - Turn on when condition is true</summary>

```cpp
onCondition(condition)
```

Turns **on** when condition is true, **skip** otherwise. No time restrictions.

**Use case:** Simple condition-based activation without time constraints.

</details>

<details>
<summary><b>offCondition</b> - Turn off when condition is true</summary>

```cpp
offCondition(condition)
```

Turns **off** when condition is true, **skip** otherwise.

**Use case:** Turn off when light level drops.

</details>

<details>
<summary><b>onConditionDelayed</b> - Turn on after stable condition</summary>

```cpp
onConditionDelayed(condition, delaySeconds)
```

Turns **on** only after the condition has been continuously true for the specified delay.

**Use case:** Prevent flickering by requiring stable conditions before switching.

</details>

<details>
<summary><b>offConditionDelayed</b> - Turn off after stable condition</summary>

```cpp
offConditionDelayed(condition, delaySeconds)
```

Turns **off** only after the condition has been continuously true for the specified delay.

**Use case:** Keep lights on briefly after motion stops.

</details>

<details>
<summary><b>delayedOnOff</b> - Hysteresis control</summary>

```cpp
delayedOnOff(startTime, endTime, onDelayMinutes, offDelayMinutes, condition)
```

Hysteresis control within a time window. Waits `onDelayMinutes` before turning on and `offDelayMinutes` before turning off.

**Use case:** Prevent rapid switching when conditions fluctuate near thresholds.

</details>

<details>
<summary><b>solarHeaterControl</b> - Solar-powered device control</summary>

```cpp
solarHeaterControl(exportThreshold, importThreshold, minOnTime, minOffTime, extraCondition)
```

Specialized rule for solar-powered devices. Turns on when exporting enough power, turns off when importing too much. Respects minimum on/off times to protect equipment.

**Use case:** Water heater that runs on excess solar production.

</details>

---

## Conditions

<details>
<summary><b>Light Sensor</b> - lightBelow, lightAbove</summary>

Requires BH1750 sensor.

| Function | Description |
|----------|-------------|
| `lightBelow(threshold)` | True when light level < threshold (lux) |
| `lightAbove(threshold)` | True when light level > threshold (lux) |

</details>

<details>
<summary><b>Temperature</b> - temperatureAbove, temperatureBelow</summary>

Requires BME280 sensor. Returns false if sensor not found.

| Function | Description |
|----------|-------------|
| `temperatureAbove(threshold)` | True when temperature > threshold (°C) |
| `temperatureBelow(threshold)` | True when temperature < threshold (°C) |

</details>

<details>
<summary><b>Humidity</b> - humidityAbove, humidityBelow</summary>

Requires BME280 sensor. Returns false if sensor not found.

| Function | Description |
|----------|-------------|
| `humidityAbove(threshold)` | True when relative humidity > threshold (%) |
| `humidityBelow(threshold)` | True when relative humidity < threshold (%) |

</details>

<details>
<summary><b>Air Pressure</b> - pressureAbove, pressureBelow</summary>

Requires BME280 sensor. Returns false if sensor not found. Pressure is in hPa (hectopascal). Standard sea level pressure is 1013.25 hPa.

| Function | Description |
|----------|-------------|
| `pressureAbove(threshold)` | True when pressure > threshold (hPa) |
| `pressureBelow(threshold)` | True when pressure < threshold (hPa) |

</details>

<details>
<summary><b>Phone Presence</b> - phonePresent, phoneNotPresent</summary>

| Function | Description |
|----------|-------------|
| `phonePresent()` | True when phone is detected on the network |
| `phoneNotPresent()` | True when phone is not detected |

</details>

<details>
<summary><b>Day of Week</b> - isWorkday, isWeekend, isMonday, etc.</summary>

| Function | Description |
|----------|-------------|
| `isWorkday()` | True Monday through Friday |
| `isWeekend()` | True Saturday and Sunday |
| `isMonday()` | True on Monday |
| `isTuesday()` | True on Tuesday |
| `isWednesday()` | True on Wednesday |
| `isThursday()` | True on Thursday |
| `isFriday()` | True on Friday |
| `isSaturday()` | True on Saturday |
| `isSunday()` | True on Sunday |

</details>

<details>
<summary><b>Sun Position</b> - sunUp, sunDown, beforeSunrise, afterSunrise, beforeSunset, afterSunset</summary>

Requires `setLocation()` to be called at startup.

| Function | Description |
|----------|-------------|
| `sunUp()` | True when sun is above the horizon |
| `sunDown()` | True when sun is below the horizon |
| `beforeSunrise(minutes)` | True during the X minutes before sunrise |
| `afterSunrise(minutes)` | True during the X minutes after sunrise |
| `beforeSunset(minutes)` | True during the X minutes before sunset |
| `afterSunset(minutes)` | True during the X minutes after sunset |

The before/after functions create a time window. For example, `beforeSunset(30)` is true starting 30 minutes before sunset and ending at sunset.

</details>

<details>
<summary><b>Power / Solar</b> - powerSolarActive, powerProducing, powerConsuming, etc.</summary>

Requires P1 meter.

| Function | Description |
|----------|-------------|
| `powerSolarActive()` | True when solar is producing |
| `powerProducing()` | True when exporting to grid |
| `powerConsuming()` | True when importing from grid |
| `powerProductionAbove(threshold)` | True when export > threshold (watts) |
| `powerProductionBelow(threshold)` | True when export < threshold (watts) |

</details>

<details>
<summary><b>Socket State</b> - socketIsOn, socketIsOff</summary>

Check the state of other sockets. Socket numbers are 1-indexed.

| Function | Description |
|----------|-------------|
| `socketIsOn(socketNumber)` | True if the specified socket is currently on |
| `socketIsOff(socketNumber)` | True if the specified socket is currently off |

</details>

<details>
<summary><b>Duration</b> - hasBeenOnFor, hasBeenOffFor</summary>

Check how long a socket has been in its current state.

| Function | Description |
|----------|-------------|
| `hasBeenOnFor(socketNumber, minutes)` | True if socket has been on for at least X minutes |
| `hasBeenOffFor(socketNumber, minutes)` | True if socket has been off for at least X minutes |

</details>

<details>
<summary><b>Time Window</b> - timeWindowBetween, after</summary>

| Function | Description |
|----------|-------------|
| `timeWindowBetween(start, end)` | True when current time is within the window |
| `after(time, duration)` | True after specified time (optional duration window) |

</details>

---

## Combinators

<details>
<summary><b>allOf</b> - AND logic</summary>

```cpp
allOf({condition1, condition2, ...})
```

Returns true only if **all** conditions are true.

</details>

<details>
<summary><b>anyOf</b> - OR logic</summary>

```cpp
anyOf({condition1, condition2, ...})
```

Returns true if **any** condition is true.

</details>

<details>
<summary><b>notOf</b> - NOT logic</summary>

```cpp
notOf(condition)
```

Inverts a condition.

</details>

---

## Time Utilities

<details>
<summary><b>getSunriseTime / getSunsetTime</b> - Get today's sun times</summary>

```cpp
getSunriseTime()  // Returns "HH:MM"
getSunsetTime()   // Returns "HH:MM"
```

Get today's sunrise or sunset time as a string. Compatible with other time functions.

</details>

<details>
<summary><b>rndTime</b> - Random time offset</summary>

```cpp
rndTime(baseTime, maxMinutes, extraSeed)
```

Generates a random time offset from a base time. The randomness is consistent per day.

**Use case:** Simulate natural lighting patterns that vary slightly each day.

</details>

<details>
<summary><b>addMinutesToTime / addHoursToTime</b> - Time arithmetic</summary>

```cpp
addMinutesToTime(baseTime, minutesToAdd)
addHoursToTime(baseTime, hoursToAdd)
```

Simple time arithmetic. Returns a new time string in "HH:MM" format.

</details>

<details>
<summary><b>getDailyRandom / getDailyRandom60 / getDailyRandom24</b> - Daily consistent randoms</summary>

```cpp
getDailyRandom(index)    // Returns 0-99
getDailyRandom60(index)  // Returns 0-59
getDailyRandom24(index)  // Returns 0-23
```

Pre-generated random numbers that stay consistent throughout the day.

</details>

---

## System Functions

<details>
<summary><b>addRule</b> - Register a rule</summary>

```cpp
addRule(socketNumber, ruleName, evaluateFunction, timeWindow)
```

Registers a new rule for a socket. Rules are evaluated in the order they are added.

</details>

<details>
<summary><b>update</b> - Main loop function</summary>

```cpp
update()
```

Call this in your main loop. Polls physical states, evaluates all rules, and applies changes.

</details>

<details>
<summary><b>pollPhysicalStates</b> - Refresh socket states</summary>

```cpp
pollPhysicalStates()
```

Manually refresh all socket states from the hardware. Called automatically by `update()`.

</details>

<details>
<summary><b>getSocketState</b> - Get socket state</summary>

```cpp
getSocketState(socketIndex)
```

Returns the physical state (true/false) of a socket by its index (0-indexed).

</details>

<details>
<summary><b>clearRules</b> - Remove all rules</summary>

```cpp
clearRules()
```

Removes all registered rules.

</details>

---

## Example Usage  
This repo is based on a state machine to control all devices.  
Similar to continuous flow coding as in a PLC.  
Although the rule system has its own logic, which I show below.  
And it can be easily extended to include a lot of rules.  
Without conflicting with the main state machine loop code.  


```cpp
void setup() {
  SmartRuleSystem::setLocation(52.37, 4.90, 0);  // Amsterdam
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

  // TV ambient: on when home + after 18:00 + main light on (>20 lux)
  rs.addRule(4, "TV ambient on",
      rs.onCondition(rs.allOf({rs.phonePresent(), rs.after("18:00"), rs.lightAbove(20)})));

  // TV ambient: off when main light turned off (<10 lux)
  rs.addRule(4, "TV ambient off",
      rs.offCondition(rs.lightBelow(10)));

  // Solar water heater
  rs.addRule(2, "Solar Heater",
      rs.solarHeaterControl(500, 100, 300000, 60000,
          []() { return true; }));

  // Force off late at night
  rs.addRule(1, "Night Off",
      rs.offAfter("23:30"));

  // Sunset-triggered light
  rs.addRule(3, "Sunset light",
      rs.onCondition(rs.allOf({rs.beforeSunset(30), rs.lightBelow(15)})));

  // Heating with auto-timeout (max 3 hours)
  rs.addRule(4, "Heating",
      rs.onCondition(rs.allOf({
          rs.temperatureBelow(18.0),
          rs.phonePresent(),
          rs.sunDown()
      })));
  rs.addRule(4, "Heating timeout",
      rs.offCondition(rs.hasBeenOnFor(4, 180)));
}
```

---

## Hardware Requirements

| Component | Purpose | Required |
|-----------|---------|----------|
| ESP32 | Main controller | Yes |
| WiFi Sockets | Controllable devices (HomeWizard, etc.) | Yes |
| BH1750 | Light sensor | Optional |
| BME280 | Temperature/Humidity/Pressure | Optional |
| P1 Meter | Power monitoring (Dutch smart meter) | Optional |
| OLED Display (SH1106 128x64)| Status display |
| (also futering a basic website)| |
| Phone on static ip WiFi | Presence detection via ping | Optional |

Price estimate ~ 30 to 40 Euro's total.

So far this code controls 4 home connect switches, but you can code more.



