#include "SettingsService.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>

namespace DeskClock {
namespace {

constexpr const char *kPreferencesNamespace = "deskclock";
constexpr const char *kConfiguredKey = "configured";
constexpr const char *kTimezoneKey = "tz";
constexpr const char *kThemeKey = "theme";
constexpr const char *kTimezoneLabels[] = {"Local", "UTC", "Manual"};
constexpr uint8_t kTimezoneCount = sizeof(kTimezoneLabels) / sizeof(kTimezoneLabels[0]);
constexpr uint8_t kThemeCount = 3;

bool configured = false;
uint8_t timezone_index = 0;
uint8_t theme_index = 0;

void save()
{
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) {
    Serial.println("SettingsService: failed to open preferences for write");
    return;
  }
  preferences.putBool(kConfiguredKey, configured);
  preferences.putUChar(kTimezoneKey, timezone_index);
  preferences.putUChar(kThemeKey, theme_index);
  preferences.end();
}

} // namespace

namespace SettingsService {

void begin()
{
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    if (preferences.isKey(kConfiguredKey)) {
      configured = preferences.getBool(kConfiguredKey, false);
    }
    if (preferences.isKey(kTimezoneKey)) {
      timezone_index = preferences.getUChar(kTimezoneKey, 0) % kTimezoneCount;
    }
    if (preferences.isKey(kThemeKey)) {
      theme_index = preferences.getUChar(kThemeKey, 0) % kThemeCount;
    }
    preferences.end();
  }
}

SettingsSnapshot snapshot()
{
  SettingsSnapshot result;
  result.configured = configured;
  result.timezone_label = kTimezoneLabels[timezone_index % kTimezoneCount];
  result.theme_index = theme_index % kThemeCount;
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

void cycleTheme()
{
  theme_index = static_cast<uint8_t>((theme_index + 1U) % kThemeCount);
  save();
}

uint8_t themeCount()
{
  return kThemeCount;
}

} // namespace SettingsService
} // namespace DeskClock
