#pragma once

#include "AlarmService.h"
#include "TimeService.h"

namespace DeskClock {
struct AlarmToneSettings {
  bool enabled = true;
  uint8_t volume = 85;
};

namespace AlarmToneService {

void begin();
void loop(const ActiveAlarmAlert &alert);
void testTone();
bool available();
AlarmToneSettings settings();
void updateSettings(const AlarmToneSettings &settings);

} // namespace AlarmToneService
} // namespace DeskClock
