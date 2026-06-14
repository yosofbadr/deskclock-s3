#pragma once

#include <stdint.h>

namespace DeskClock {

struct SettingsSnapshot {
  bool configured = false;
  const char *timezone_label = "Local";
  const char *timezone_posix = "CET-1CEST,M3.5.0/2,M10.5.0/3";
  uint8_t timezone_index = 0;
  uint8_t theme_index = 0;
};

namespace SettingsService {

void begin();
SettingsSnapshot snapshot();
void setConfigured(bool configured);
void cycleTimezone();
void setTimezoneIndex(uint8_t index);
uint8_t timezoneCount();
const char *timezoneLabel(uint8_t index);
const char *timezonePosix(uint8_t index);
void cycleTheme();
uint8_t themeCount();

} // namespace SettingsService
} // namespace DeskClock
