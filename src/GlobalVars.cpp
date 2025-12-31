<<<<<<< HEAD
#include <GlobalVars.h>
#include <cstring>
const char *lastActiveRuleName = "none";
int lastActiveRuleSocket = 0;
char lastActiveRuleTimeStr[12] = "--:-- xxx";

RuleHistoryEntry ruleHistory[4] = {{"", ""}, {"", ""}, {"", ""}, {"", ""}};
int ruleHistoryIndex = 0;

void addRuleToHistory(const char *name, const char *time) {
  // Don't add if same as most recent
  int lastIdx = (ruleHistoryIndex + 3) % 4;
  if (strcmp(ruleHistory[lastIdx].name, name) == 0) {
    return;
  }

  strncpy(ruleHistory[ruleHistoryIndex].name, name, 31);
  ruleHistory[ruleHistoryIndex].name[31] = '\0';
  strncpy(ruleHistory[ruleHistoryIndex].time, time, 11);
  ruleHistory[ruleHistoryIndex].time[11] = '\0';

  ruleHistoryIndex = (ruleHistoryIndex + 1) % 4;
=======
#include <GlobalVars.h>
#include <cstring>
const char *lastActiveRuleName = "none";
int lastActiveRuleSocket = 0;
char lastActiveRuleTimeStr[12] = "--:-- xxx";

RuleHistoryEntry ruleHistory[4] = {{"", ""}, {"", ""}, {"", ""}, {"", ""}};
int ruleHistoryIndex = 0;

void addRuleToHistory(const char *name, const char *time) {
  // Don't add if same as most recent
  int lastIdx = (ruleHistoryIndex + 3) % 4;
  if (strcmp(ruleHistory[lastIdx].name, name) == 0) {
    return;
  }

  strncpy(ruleHistory[ruleHistoryIndex].name, name, 31);
  ruleHistory[ruleHistoryIndex].name[31] = '\0';
  strncpy(ruleHistory[ruleHistoryIndex].time, time, 11);
  ruleHistory[ruleHistoryIndex].time[11] = '\0';

  ruleHistoryIndex = (ruleHistoryIndex + 1) % 4;
>>>>>>> 4b002c6a95035d8e01148e2a819261d17acea3df
}