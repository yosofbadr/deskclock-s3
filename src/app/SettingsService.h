#pragma once

namespace DeskClock {

struct SettingsSnapshot {
  bool configured = false;
  const char *timezone_label = "Local";
};

namespace SettingsService {

void begin();
SettingsSnapshot snapshot();
void setConfigured(bool configured);
void cycleTimezone();

} // namespace SettingsService
} // namespace DeskClock
