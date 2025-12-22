#ifndef SMART_RULE_SYSTEM_H
#define SMART_RULE_SYSTEM_H
#include <map>
#include <string>
#include <functional>
#include <vector>
#include "GlobalVars.h"
#include "TimeSync.h"
#include "HomeSocketDevice.h"
#include "EnvironmentSensor.h"
#include "NetworkCheck.h"
#include "Constants.h"

extern TimeSync timeSync;
extern EnvironmentSensors sensors;
extern NetworkCheck *phoneCheck;
extern HomeSocketDevice *sockets[];
extern HomeP1Device *p1Meter;

enum class RuleDecision
{
    Skip,
    On,
    Off
};

class SmartRuleSystem
{
public:
    static int sunriseMinutes;    // Minutes since midnight, local time
    static int sunsetMinutes;     // Minutes since midnight, local time
    static int lastSunCalcDay;    // Day of year when last calculated
    static float configLatitude;  // Decimal degrees (negative = South)
    static float configLongitude; // Decimal degrees (negative = West)
    static int configElevation;   // Meters above sea level
    // String buffers for "HH:MM" format (matches your existing timeBuffer pattern)
    static char sunriseBuffer[6];
    static char sunsetBuffer[6];
    // ============================================================================
    // PUBLIC METHODS - add to public section
    // ============================================================================

    // --- Location configuration (call once at startup) ---
    // Format: decimal degrees from Google Maps, elevation in meters
    // Timezone is automatically obtained from TimeSync (handles DST)
    // Example: setLocation(52.37, 4.90, 0);  // Amsterdam
    static void setLocation(float lat, float lon, int elevationMeters = 0);

    // --- Sun time getters (return "HH:MM" strings) ---
    static const char *getSunriseTime();
    static const char *getSunsetTime();

    // --- Sun-based conditions ---
    static std::function<bool()> sunUp();
    static std::function<bool()> sunDown();
    static std::function<bool()> beforeSunrise(int minutes);
    static std::function<bool()> afterSunrise(int minutes);
    static std::function<bool()> beforeSunset(int minutes);
    static std::function<bool()> afterSunset(int minutes);

    // --- Temperature conditions (requires BME280) ---
    static std::function<bool()> temperatureAbove(float threshold);
    static std::function<bool()> temperatureBelow(float threshold);

    // --- Humidity conditions (requires BME280) ---
    static std::function<bool()> humidityAbove(float threshold);
    static std::function<bool()> humidityBelow(float threshold);

    // --- Air pressure conditions (requires BME280) ---
    static std::function<bool()> pressureAbove(float threshold);
    static std::function<bool()> pressureBelow(float threshold);

    // --- Socket state conditions ---
    // Check state of other sockets (1-indexed, same as addRule)
    std::function<bool()> socketIsOn(int socketNumber);
    std::function<bool()> socketIsOff(int socketNumber);

    // --- Duration conditions ---
    // Check how long a socket has been in current state
    std::function<bool()> hasBeenOnFor(int socketNumber, unsigned long minutes);
    std::function<bool()> hasBeenOffFor(int socketNumber, unsigned long minutes);

    struct SocketState
    {
        bool physicalState = false;
        RuleDecision virtualState = RuleDecision::Skip;
        unsigned long lastManualChange = 0;
        static constexpr unsigned long MANUAL_COOLDOWN = 300000; // 5 min
        unsigned long lastStateChange = 0;
    };

    struct Rule
    {
        int socketNumber;
        std::function<RuleDecision()> evaluate;
        std::function<bool()> timeWindow;
        const char *name; // Add this
    };

    SmartRuleSystem();

    // Rule management
    void addRule(int socketNumber,
                 const char *ruleName, // Add this
                 std::function<RuleDecision()> evaluate,
                 std::function<bool()> timeWindow = nullptr);
    void update();

    std::function<RuleDecision()> delayedOnOff(const char *startTime, const char *endTime, int onDelayMinutes, int offDelayMinutes, std::function<bool()> condition = []()
                                                                                                                                    { return true; });

    // Condition builders
    // static std::function<bool()> timeWindowBetween(const char *start, const char *end);
    std::function<bool()> timeWindowBetween(const char *start, const char *end);

    std::function<RuleDecision()> period(const char *startTime, const char *endTime, std::function<bool()> condition = []()
                                                                                     { return true; });

    std::function<RuleDecision()> boolPeriod(const char *startTime, const char *endTime, std::function<bool()> condition);

    std::function<RuleDecision()> onAfter(const char *timeStr, int durationMins = 0, std::function<bool()> condition = []()
                                                                                     { return true; });

    std::function<RuleDecision()> offAfter(const char *timeStr, int durationMins = 0, std::function<bool()> condition = []()
                                                                                      { return true; });

    std::function<RuleDecision()> onCondition(std::function<bool()> condition);
    std::function<RuleDecision()> offCondition(std::function<bool()> condition);
    // const char *getLastActiveRule() const { return lastActiveRuleName; }

    const char *rndTime(const char *time, int maxMinutes, int extraSeed = 0);
    std::function<RuleDecision()> offConditionDelayed(std::function<bool()> condition, int delaySeconds);
    static std::function<bool()> lightAbove(float threshold);

    static std::function<bool()> lightBelow(float threshold);
    std::function<RuleDecision()> onConditionDelayed(std::function<bool()> condition, int delaySeconds);
    static std::function<bool()> phoneNotPresent();
    static std::function<bool()> phonePresent();
    static std::function<bool()> isWorkday();
    static std::function<bool()> isWeekend();
    static std::function<bool()> isMonday();
    static std::function<bool()> isTuesday();
    static std::function<bool()> isWednesday();
    static std::function<bool()> isThursday();
    static std::function<bool()> isFriday();
    static std::function<bool()> isSaturday();
    static std::function<bool()> isSunday();

    static std::function<bool()> allOf(std::vector<std::function<bool()>> conditions);
    static std::function<bool()> anyOf(std::vector<std::function<bool()>> conditions);
    static std::function<bool()> notOf(std::function<bool()> condition);

    static std::function<bool()> powerSolarActive();
    static std::function<bool()> powerProducing();
    static std::function<bool()> powerConsuming();
    static std::function<bool()> powerProductionBelow(float threshold);
    static std::function<bool()> powerProductionAbove(float threshold);
    int getDailyRandom(int index);   // Get dailyRandom[index] (0-9)
    int getDailyRandom60(int index); // Get dailyRandom60[index] (0-4)
    int getDailyRandom24(int index); // Get dailyRandom24[index] (0-2)

    static std::function<bool()> after(const char *timeStr, int durationMins = 0);

    // Helper functions for time generation
    const char *addMinutesToTime(const char *baseTime, int minutesToAdd);
    const char *addHoursToTime(const char *baseTime, int hoursToAdd);

    std::function<RuleDecision()> solarHeaterControl(
        float exportThreshold,
        float importThreshold,
        unsigned long minOnTime = 30000,
        unsigned long minOffTime = 30000,
        std::function<bool()> extraCondition = []()
        { return true; });

    unsigned long getLastActiveRuleTime() const { return lastActiveRuleTime; }
    RuleDecision getLastActiveRuleState() const { return lastActiveRuleState; }

    bool getSocketState(int socketIndex) const;

    void pollPhysicalStates();

    void clearRules(); // Clear all rules

private:
    struct DelayedState
    {
        bool lastCondition = false;
        unsigned long conditionChangeTime = 0;
    };

    std::map<std::string, DelayedState> delayedStates;

    unsigned long lastActiveRuleTime = 0;
    RuleDecision lastActiveRuleState = RuleDecision::Skip;

    // const char *lastActiveRuleName = "None";
    // int lastActiveRuleSocket = 0;

    static char timeBuffer[6]; // Static buffer for time string
    std::vector<SocketState> sockets;
    std::vector<Rule> rules;

    void detectManualChanges();
    void evaluateRules();
    void applyVirtualState();

    static RuleDecision physicalToDecision(bool state);
    static bool decisionToPhysical(RuleDecision decision);
    static int dailyRandom[10];  // 10 random numbers 0-99
    static int dailyRandom60[5]; // 5 random numbers 0-59 (for minutes)
    static int dailyRandom24[3]; // 3 random numbers 0-23 (for hours)
    static int lastGeneratedDay; // Track which day we generated for

    void generateDailyRandoms(); // Generate all randoms for the day
    int timeWindowCounter = 0;

    struct TimeWindowState
    {
        bool isActive;
        unsigned long endTimeMillis;

        // Default constructor
        TimeWindowState() : isActive(false), endTimeMillis(0) {}

        // Parameterized constructor
        TimeWindowState(bool active, unsigned long endTime)
            : isActive(active), endTimeMillis(endTime) {}
    };
    std::map<std::string, TimeWindowState> activeTimeWindows;

    unsigned long calculateEndTime(int hour, int minute);
    static void calculateDailySunTimes();
    static float getLocalEarthRadius(float latitudeDeg);
};
#endif