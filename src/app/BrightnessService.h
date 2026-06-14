#pragma once

#include <stdint.h>

#include "TimeService.h"

namespace DeskClock {

struct BrightnessSettings {
  uint8_t day_brightness = 220;
  uint8_t night_brightness = 160;
  uint8_t night_start_hour = 22;
  uint8_t day_start_hour = 7;
};

namespace BrightnessService {

void begin();
void loop(const DateTime &now);
BrightnessSettings settings();
void updateSettings(const BrightnessSettings &settings);
uint8_t currentBrightness();
uint8_t presetCount();
uint8_t presetValue(uint8_t index);
uint8_t nextPresetValue(uint8_t current, int8_t delta);
uint8_t cyclePreset(int8_t delta = 1);

} // namespace BrightnessService
} // namespace DeskClock
