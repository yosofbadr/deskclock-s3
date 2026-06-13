#include "SettingsService.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>

namespace DeskClock {
namespace {

constexpr const char *kPreferencesNamespace = "deskclock";
constexpr const char *kConfiguredKey = "configured";
constexpr const char *kTimezoneKey = "tz";
constexpr const char *kTimezoneLabels[] = {"Local", "UTC", "Manual"};
constexpr uint8_t kTimezoneCount = sizeof(kTimezoneLabels) / sizeof(kTimezoneLabels[0]);

bool configured = false;
uint8_t timezone_index = 0;

void save()
{
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) {
    Serial.println("SettingsService: failed to open preferences for write");
    return;
  }
  preferences.putBool(kConfiguredKey, configured);
  preferences.putUChar(kTimezoneKey, timezone_index);
  preferences.end();
}

} // namespace

namespace SettingsService {

void begin()
{
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    configured = preferences.getBool(kConfiguredKey, false);
    timezone_index = preferences.getUChar(kTimezoneKey, 0) % kTimezoneCount;
    preferences.end();
  }
}

SettingsSnapshot snapshot()
{
  SettingsSnapshot result;
  result.configured = configured;
  result.timezone_label = kTimezoneLabels[timezone_index % kTimezoneCount];
  return result;
}

void setConfigured(bool value)
{
  configured = value;
  save();
}

void cycleTimezone()
{
  timezone_index = static_cast<uint8_t>((timezone_index + 1U) % kTimezoneCount);
  save();
}

} // namespace SettingsService
} // namespace DeskClock
