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

portMUX_TYPE brightness_mux = portMUX_INITIALIZER_UNLOCKED;
BrightnessSettings current_settings;
uint8_t current_brightness = 255;
bool has_applied = false;

uint8_t clamp_brightness(uint8_t value)
{
  if (value < 5) {
    return 5;
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

void apply_brightness(uint8_t brightness)
{
  brightness = clamp_brightness(brightness);
  if (has_applied && brightness == current_brightness) {
    return;
  }
  current_brightness = brightness;
  has_applied = true;
  setUpduty(static_cast<uint16_t>(0xffU - brightness));
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
  sanitized.day_brightness = clamp_brightness(sanitized.day_brightness);
  sanitized.night_brightness = clamp_brightness(sanitized.night_brightness);
  sanitized.night_start_hour %= 24U;
  sanitized.day_start_hour %= 24U;

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

} // namespace BrightnessService
} // namespace DeskClock
