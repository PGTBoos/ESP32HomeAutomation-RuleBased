#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>

constexpr uint8_t NUM_SOCKETS = 4;

// Day definitions
static const uint8_t MONDAY = 0b00000001;
static const uint8_t TUESDAY = 0b00000010;
static const uint8_t WEDNESDAY = 0b00000100;
static const uint8_t THURSDAY = 0b00001000;
static const uint8_t FRIDAY = 0b00010000;
static const uint8_t SATURDAY = 0b00100000;
static const uint8_t SUNDAY = 0b01000000;

// Combined patterns
static const uint8_t WEEKDAYS = MONDAY | TUESDAY | WEDNESDAY | THURSDAY | FRIDAY;
static const uint8_t WEEKEND = SATURDAY | SUNDAY;
static const uint8_t EVERYDAY = WEEKDAYS | WEEKEND;

#endif