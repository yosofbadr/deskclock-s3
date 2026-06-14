#include "BrightnessService.h"

#include <Arduino.h>
#include <Preferences.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"

namespace DeskClock {
namespace {

constexpr const char *kPreferencesNamespace = "deskclock";
constexpr const char *kDayBrightnessKey = "bday";
constexpr const char *kNightBrightnessKey = "bnight";
constexpr const char *kNightStartKey = "nstart";
constexpr const char *kDayStartKey = "dstart";
constexpr uint8_t kBrightnessPresets[] = {160, 184, 208, 232, 255};
constexpr uint8_t kBrightnessPresetCount = sizeof(kBrightnessPresets) / sizeof(kBrightnessPresets[0]);
constexpr uint8_t kMinimumVisibleBrightness = kBrightnessPresets[0];

portMUX_TYPE brightness_mux = portMUX_INITIALIZER_UNLOCKED;
BrightnessSettings current_settings;
uint8_t current_brightness = 255;
bool has_applied = false;

uint8_t clamp_brightness(uint8_t value)
{
  if (value < kMinimumVisibleBrightness) {
    return kMinimumVisibleBrightness;
  }
  return value;
}

bool is_night_hour(uint8_t hour, const BrightnessSettings &settings)
{
  if (settings.night_start_hour == settings.day_start_hour) {
    return false;
  }
  if (settings.night_start_hour < settings.day_start_hour) {
    return hour >= settings.night_start_hour && hour < settings.day_start_hour;
  }
  return hour >= settings.night_start_hour || hour < settings.day_start_hour;
}

uint8_t next_preset_value(uint8_t current, int8_t delta)
{
  if (delta >= 0) {
    for (uint8_t preset : kBrightnessPresets) {
      if (preset > current + 4U) {
        return preset;
      }
    }
    return kBrightnessPresets[0];
  }

  for (int index = static_cast<int>(kBrightnessPresetCount) - 1; index >= 0; --index) {
    if (kBrightnessPresets[index] + 4U < current) {
      return kBrightnessPresets[index];
    }
  }
  return kBrightnessPresets[kBrightnessPresetCount - 1];
}

void apply_brightness(uint8_t brightness)
{
  brightness = clamp_brightness(brightness);
  if (has_applied && brightness == current_brightness) {
    return;
  }
  current_brightness = brightness;
  has_applied = true;
  const uint16_t duty = static_cast<uint16_t>(0xffU - brightness);
  setUpduty(duty);
  Serial.printf("BrightnessService: applying brightness=%u duty=%u\n", brightness, duty);
}

bool sanitize_settings(BrightnessSettings &settings)
{
  const BrightnessSettings original = settings;
  settings.day_brightness = clamp_brightness(settings.day_brightness);
  settings.night_brightness = clamp_brightness(settings.night_brightness);
  settings.night_start_hour %= 24U;
  settings.day_start_hour %= 24U;
  return original.day_brightness != settings.day_brightness ||
         original.night_brightness != settings.night_brightness ||
         original.night_start_hour != settings.night_start_hour ||
         original.day_start_hour != settings.day_start_hour;
}

void save_settings(const BrightnessSettings &settings)
{
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) {
    Serial.println("BrightnessService: failed to open preferences for write");
    return;
  }
  preferences.putUChar(kDayBrightnessKey, settings.day_brightness);
  preferences.putUChar(kNightBrightnessKey, settings.night_brightness);
  preferences.putUChar(kNightStartKey, settings.night_start_hour);
  preferences.putUChar(kDayStartKey, settings.day_start_hour);
  preferences.end();
}

} // namespace

namespace BrightnessService {

void begin()
{
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    if (preferences.isKey(kDayBrightnessKey)) {
      current_settings.day_brightness = preferences.getUChar(kDayBrightnessKey, current_settings.day_brightness);
    }
    if (preferences.isKey(kNightBrightnessKey)) {
      current_settings.night_brightness = preferences.getUChar(kNightBrightnessKey, current_settings.night_brightness);
    }
    if (preferences.isKey(kNightStartKey)) {
      current_settings.night_start_hour = preferences.getUChar(kNightStartKey, current_settings.night_start_hour) % 24U;
    }
    if (preferences.isKey(kDayStartKey)) {
      current_settings.day_start_hour = preferences.getUChar(kDayStartKey, current_settings.day_start_hour) % 24U;
    }
    preferences.end();
  }

  if (sanitize_settings(current_settings)) {
    Serial.println("BrightnessService: sanitized saved brightness settings");
    save_settings(current_settings);
  }

  Serial.printf(
      "BrightnessService: loaded day=%u night=%u night_start=%u day_start=%u\n",
      current_settings.day_brightness,
      current_settings.night_brightness,
      current_settings.night_start_hour,
      current_settings.day_start_hour);
  apply_brightness(current_settings.day_brightness);
}

void loop(const DateTime &now)
{
  BrightnessSettings copy = settings();
  const uint8_t desired = (now.valid && is_night_hour(now.hour, copy)) ? copy.night_brightness : copy.day_brightness;
  apply_brightness(desired);
}

BrightnessSettings settings()
{
  BrightnessSettings copy;
  portENTER_CRITICAL(&brightness_mux);
  copy = current_settings;
  portEXIT_CRITICAL(&brightness_mux);
  return copy;
}

void updateSettings(const BrightnessSettings &settings)
{
  BrightnessSettings sanitized = settings;
  sanitize_settings(sanitized);

  portENTER_CRITICAL(&brightness_mux);
  current_settings = sanitized;
  portEXIT_CRITICAL(&brightness_mux);

  save_settings(sanitized);
  Serial.printf(
      "BrightnessService: day=%u night=%u night_start=%u day_start=%u\n",
      sanitized.day_brightness,
      sanitized.night_brightness,
      sanitized.night_start_hour,
      sanitized.day_start_hour);
}

uint8_t currentBrightness()
{
  return current_brightness;
}

uint8_t presetCount()
{
  return kBrightnessPresetCount;
}

uint8_t presetValue(uint8_t index)
{
  return kBrightnessPresets[index % kBrightnessPresetCount];
}

uint8_t nextPresetValue(uint8_t current, int8_t delta)
{
  return next_preset_value(current, delta);
}

uint8_t cyclePreset(int8_t delta)
{
  BrightnessSettings updated = settings();
  const uint8_t next = next_preset_value(currentBrightness(), delta);
  updated.day_brightness = next;
  updated.night_brightness = next;
  updateSettings(updated);
  apply_brightness(next);
  return next;
}

} // namespace BrightnessService
} // namespace DeskClock
