#include "BrightnessSettingsView.h"

#include <stdint.h>
#include <stdio.h>

#include "AlarmToneService.h"
#include "BrightnessService.h"
#include "SettingsService.h"
#include "UiWidgets.h"

extern "C" void clock_face_refresh_theme(void);

namespace DeskClock {
namespace {

lv_obj_t *panel = nullptr;
lv_obj_t *value_label = nullptr;
BrightnessSettings editing_brightness;
AlarmToneSettings editing_tone;

void refresh()
{
  if (value_label == nullptr) {
    return;
  }
  char buffer[96];
  snprintf(buffer,
           sizeof(buffer),
           "Theme %u/%u | Day %u Night %u | N%02u D%02u | Audio %s Vol %u",
           static_cast<unsigned int>(SettingsService::snapshot().theme_index + 1U),
           static_cast<unsigned int>(SettingsService::themeCount()),
           editing_brightness.day_brightness,
           editing_brightness.night_brightness,
           editing_brightness.night_start_hour,
           editing_brightness.day_start_hour,
           editing_tone.enabled ? "on" : "off",
           editing_tone.volume);
  lv_label_set_text(value_label, buffer);
}

void close_event(lv_event_t *)
{
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void save_event(lv_event_t *)
{
  BrightnessService::updateSettings(editing_brightness);
  AlarmToneService::updateSettings(editing_tone);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void adjust_event(lv_event_t *event)
{
  const uint8_t action = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (action == 1 || action == 2) {
    const int32_t delta = action == 1 ? -20 : 20;
    const int32_t next = static_cast<int32_t>(editing_brightness.day_brightness) + delta;
    editing_brightness.day_brightness = static_cast<uint8_t>(next < 5 ? 5 : (next > 255 ? 255 : next));
  } else if (action == 3 || action == 4) {
    const int32_t delta = action == 3 ? -20 : 20;
    const int32_t next = static_cast<int32_t>(editing_brightness.night_brightness) + delta;
    editing_brightness.night_brightness = static_cast<uint8_t>(next < 5 ? 5 : (next > 255 ? 255 : next));
  } else if (action == 5) {
    editing_brightness.night_start_hour = static_cast<uint8_t>((editing_brightness.night_start_hour + 23U) % 24U);
  } else if (action == 6) {
    editing_brightness.night_start_hour = static_cast<uint8_t>((editing_brightness.night_start_hour + 1U) % 24U);
  } else if (action == 7) {
    editing_brightness.day_start_hour = static_cast<uint8_t>((editing_brightness.day_start_hour + 23U) % 24U);
  } else if (action == 8) {
    editing_brightness.day_start_hour = static_cast<uint8_t>((editing_brightness.day_start_hour + 1U) % 24U);
  } else if (action == 9 || action == 10) {
    const int32_t delta = action == 9 ? -10 : 10;
    const int32_t next = static_cast<int32_t>(editing_tone.volume) + delta;
    editing_tone.volume = static_cast<uint8_t>(next < 5 ? 5 : (next > 100 ? 100 : next));
  } else if (action == 11) {
    editing_tone.enabled = !editing_tone.enabled;
  } else if (action == 12) {
    SettingsService::cycleTheme();
    clock_face_refresh_theme();
  }
  refresh();
}

void test_tone_event(lv_event_t *)
{
  AlarmToneService::updateSettings(editing_tone);
  AlarmToneService::testTone();
}

} // namespace

namespace BrightnessSettingsView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font)
{
  panel = UiWidgets::modalPanel(parent, width, height);

  lv_obj_t *title = lv_label_create(panel);
  lv_obj_set_style_text_font(title, font, 0);
  UiWidgets::setTextColor(title, 0x1F2933);
  lv_label_set_text(title, "Brightness");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 8);

  value_label = lv_label_create(panel);
  lv_obj_set_style_text_font(value_label, font, 0);
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(value_label, width - 185);
  UiWidgets::setTextColor(value_label, 0x1F2933);
  lv_label_set_text(value_label, "Brightness");
  lv_obj_align(value_label, LV_ALIGN_TOP_MID, 42, 8);

  struct ButtonSpec {
    const char *label;
    uint8_t action;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
  };

  const ButtonSpec specs[] = {
      {"Day-", 1, 14, 38, 62, 30},   {"Day+", 2, 82, 38, 62, 30},
      {"Night-", 3, 168, 38, 70, 30}, {"Night+", 4, 244, 38, 70, 30},
      {"Vol-", 9, 442, 38, 54, 30},   {"Vol+", 10, 502, 38, 54, 30},
      {"Audio", 11, 14, 74, 64, 30},  {"Theme", 12, 84, 74, 74, 30},
      {"N-", 5, 168, 74, 44, 30},      {"N+", 6, 218, 74, 44, 30},
      {"D-", 7, 268, 74, 44, 30},      {"D+", 8, 318, 74, 44, 30},
  };

  for (const ButtonSpec &spec : specs) {
    lv_obj_t *button = UiWidgets::button(panel, spec.label, spec.w, spec.h);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, spec.x, spec.y);
    lv_obj_add_event_cb(button, adjust_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(spec.action)));
  }

  lv_obj_t *test_tone = UiWidgets::button(panel, "Test", 58, 30);
  lv_obj_align(test_tone, LV_ALIGN_BOTTOM_MID, -78, -2);
  lv_obj_add_event_cb(test_tone, test_tone_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *save = UiWidgets::button(panel, "Save", 74, 30);
  lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, -2);
  lv_obj_add_event_cb(save, save_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *close = UiWidgets::button(panel, "Close", 74, 30);
  lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, -14, -2);
  lv_obj_add_event_cb(close, close_event, LV_EVENT_CLICKED, nullptr);
}

void open()
{
  editing_brightness = BrightnessService::settings();
  editing_tone = AlarmToneService::settings();
  refresh();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
}

} // namespace BrightnessSettingsView
} // namespace DeskClock
