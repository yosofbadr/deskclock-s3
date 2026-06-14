#pragma once

#include <stdint.h>

namespace DeskClock {

struct SettingsSnapshot {
  bool configured = false;
  const char *timezone_label = "Local";
  uint8_t theme_index = 0;
};

namespace SettingsService {

void begin();
SettingsSnapshot snapshot();
void setConfigured(bool configured);
void cycleTimezone();
void cycleTheme();
uint8_t themeCount();

} // namespace SettingsService
} // namespace DeskClock
