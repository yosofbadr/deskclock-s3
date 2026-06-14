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
struct TimezoneOption {
  const char *label;
  const char *posix;
};

constexpr TimezoneOption kTimezones[] = {
    {"Local", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
    {"UTC", "UTC0"},
    {"London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
    {"Dubai", "GST-4"},
    {"New York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
    {"Los Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2"},
};
constexpr uint8_t kTimezoneCount = sizeof(kTimezones) / sizeof(kTimezones[0]);
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
  result.timezone_index = timezone_index % kTimezoneCount;
  result.timezone_label = kTimezones[result.timezone_index].label;
  result.timezone_posix = kTimezones[result.timezone_index].posix;
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

void setTimezoneIndex(uint8_t index)
{
  timezone_index = static_cast<uint8_t>(index % kTimezoneCount);
  save();
}

uint8_t timezoneCount()
{
  return kTimezoneCount;
}

const char *timezoneLabel(uint8_t index)
{
  return kTimezones[index % kTimezoneCount].label;
}

const char *timezonePosix(uint8_t index)
{
  return kTimezones[index % kTimezoneCount].posix;
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
