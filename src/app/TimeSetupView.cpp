#include "TimeSetupView.h"

#include <Preferences.h>
#include <stdint.h>
#include <stdio.h>

#include "NetworkSetupView.h"
#include "SettingsService.h"
#include "TimeService.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

lv_obj_t *panel = nullptr;
lv_obj_t *value_label = nullptr;
DateTime editing_time;
bool use_24_hour_time = true;

void savePreferences()
{
  Preferences preferences;
  if (preferences.begin("deskclock", false)) {
    preferences.putBool("time24", use_24_hour_time);
    preferences.end();
  }
}

void refresh()
{
  if (value_label == nullptr) {
    return;
  }
  char time_text[12];
  TimeSetupView::formatTime(time_text, sizeof(time_text), editing_time.hour, editing_time.minute);
  SettingsSnapshot settings = SettingsService::snapshot();
  char buffer[96];
  snprintf(buffer,
           sizeof(buffer),
           "%04u-%02u-%02u\n%s  Format: %s\nTZ: %s",
           editing_time.year,
           editing_time.month,
           editing_time.day,
           time_text,
           use_24_hour_time ? "24h" : "12h",
           settings.timezone_label);
  lv_label_set_text(value_label, buffer);
}

void close_event(lv_event_t *)
{
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void save_event(lv_event_t *)
{
  editing_time.second = 0;
  if (TimeService::setManualTime(editing_time)) {
    SettingsService::setConfigured(true);
  }
  savePreferences();
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void toggle_format_event(lv_event_t *)
{
  use_24_hour_time = !use_24_hour_time;
  savePreferences();
  refresh();
}

void cycle_timezone_event(lv_event_t *)
{
  SettingsService::cycleTimezone();
  refresh();
}

void adjust_time_event(lv_event_t *event)
{
  const int32_t delta = static_cast<int32_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  int32_t total = static_cast<int32_t>(editing_time.hour) * 60 + editing_time.minute + delta;
  while (total < 0) {
    total += 24 * 60;
  }
  total %= 24 * 60;
  editing_time.hour = static_cast<uint8_t>(total / 60);
  editing_time.minute = static_cast<uint8_t>(total % 60);
  refresh();
}

} // namespace

namespace TimeSetupView {

void loadPreferences()
{
  Preferences preferences;
  if (preferences.begin("deskclock", true)) {
    if (preferences.isKey("time24")) {
      use_24_hour_time = preferences.getBool("time24", true);
    }
    preferences.end();
  }
}

bool use24HourFormat()
{
  return use_24_hour_time;
}

void formatTime(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
  if (use_24_hour_time) {
    snprintf(buffer, size, "%02u:%02u", hour, minute);
    return;
  }
  const bool pm = hour >= 12;
  uint8_t display_hour = hour % 12;
  if (display_hour == 0) {
    display_hour = 12;
  }
  snprintf(buffer, size, "%u:%02u %s", display_hour, minute, pm ? "PM" : "AM");
}

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font)
{
  panel = UiWidgets::modalPanel(parent, width, height);

  lv_obj_t *title = lv_label_create(panel);
  lv_obj_set_style_text_font(title, font, 0);
  UiWidgets::setTextColor(title, 0x1F2933);
  lv_label_set_text(title, "Time setup");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  value_label = lv_label_create(panel);
  lv_obj_set_style_text_font(value_label, font, 0);
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
  UiWidgets::setTextColor(value_label, 0x1F2933);
  lv_label_set_text(value_label, "---- -- --");
  lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -36);

  lv_obj_t *minus_hour = UiWidgets::button(panel, "-1h", 58, 36);
  lv_obj_align(minus_hour, LV_ALIGN_LEFT_MID, 20, 18);
  lv_obj_add_event_cb(minus_hour, adjust_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(-60)));
  lv_obj_t *minus_minute = UiWidgets::button(panel, "-1m", 58, 36);
  lv_obj_align(minus_minute, LV_ALIGN_LEFT_MID, 88, 18);
  lv_obj_add_event_cb(minus_minute, adjust_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(-1)));
  lv_obj_t *plus_minute = UiWidgets::button(panel, "+1m", 58, 36);
  lv_obj_align(plus_minute, LV_ALIGN_RIGHT_MID, -88, 18);
  lv_obj_add_event_cb(plus_minute, adjust_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  lv_obj_t *plus_hour = UiWidgets::button(panel, "+1h", 58, 36);
  lv_obj_align(plus_hour, LV_ALIGN_RIGHT_MID, -20, 18);
  lv_obj_add_event_cb(plus_hour, adjust_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(60)));

  lv_obj_t *format_button = UiWidgets::button(panel, "12/24h", 76, 34);
  lv_obj_align(format_button, LV_ALIGN_BOTTOM_LEFT, 14, -12);
  lv_obj_add_event_cb(format_button, toggle_format_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *timezone_button = UiWidgets::button(panel, "TZ", 46, 34);
  lv_obj_align(timezone_button, LV_ALIGN_BOTTOM_LEFT, 88, -12);
  lv_obj_add_event_cb(timezone_button, cycle_timezone_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *wifi_button = UiWidgets::button(panel, "Wi-Fi", 58, 34);
  lv_obj_align(wifi_button, LV_ALIGN_BOTTOM_LEFT, 140, -12);
  lv_obj_add_event_cb(wifi_button, [](lv_event_t *) { NetworkSetupView::open(); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *save_button = UiWidgets::button(panel, "Set time", 92, 34);
  lv_obj_align(save_button, LV_ALIGN_BOTTOM_MID, 38, -12);
  lv_obj_add_event_cb(save_button, save_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *close_button = UiWidgets::button(panel, "Close", 74, 34);
  lv_obj_align(close_button, LV_ALIGN_BOTTOM_RIGHT, -14, -12);
  lv_obj_add_event_cb(close_button, close_event, LV_EVENT_CLICKED, nullptr);
}

void open()
{
  TimeSnapshot snapshot = TimeService::snapshot();
  if (snapshot.now.valid) {
    editing_time = snapshot.now;
  } else {
    editing_time.year = 2026;
    editing_time.month = 1;
    editing_time.day = 1;
    editing_time.hour = 12;
    editing_time.minute = 0;
    editing_time.second = 0;
    editing_time.valid = true;
  }
  refresh();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
}

} // namespace TimeSetupView
} // namespace DeskClock
