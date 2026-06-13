#include "BrightnessSettingsView.h"

#include <stdint.h>
#include <stdio.h>

#include "AlarmToneService.h"
#include "BrightnessService.h"
#include "UiWidgets.h"

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
           "Day %u  Night %u\nNight starts %02u:00  Day starts %02u:00\nAudio %s  Vol %u",
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
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  value_label = lv_label_create(panel);
  lv_obj_set_style_text_font(value_label, font, 0);
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
  UiWidgets::setTextColor(value_label, 0x1F2933);
  lv_label_set_text(value_label, "Brightness");
  lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -38);

  const char *labels[] = {"Day-", "Day+", "Night-", "Night+", "N-", "N+", "D-", "D+", "Vol-", "Vol+", "Audio"};
  const int widths[] = {62, 62, 70, 70, 44, 44, 44, 44, 54, 54, 64};
  lv_align_t aligns[] = {LV_ALIGN_LEFT_MID, LV_ALIGN_LEFT_MID, LV_ALIGN_RIGHT_MID, LV_ALIGN_RIGHT_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_TOP_RIGHT, LV_ALIGN_TOP_RIGHT, LV_ALIGN_BOTTOM_LEFT};
  const int xs[] = {14, 84, -92, -14, -98, -48, 12, 62, -74, -14, 14};
  const int ys[] = {-24, -24, -24, -24, -58, -58, -58, -58, 12, 12, -12};
  const int heights[] = {32, 32, 32, 32, 30, 30, 30, 30, 30, 30, 34};
  for (uint8_t i = 0; i < 11; ++i) {
    lv_obj_t *button = UiWidgets::button(panel, labels[i], widths[i], heights[i]);
    lv_obj_align(button, aligns[i], xs[i], ys[i]);
    lv_obj_add_event_cb(button, adjust_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(i + 1)));
  }

  lv_obj_t *test_tone = UiWidgets::button(panel, "Test", 58, 34);
  lv_obj_align(test_tone, LV_ALIGN_BOTTOM_MID, -56, -12);
  lv_obj_add_event_cb(test_tone, test_tone_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *save = UiWidgets::button(panel, "Save", 74, 34);
  lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 24, -12);
  lv_obj_add_event_cb(save, save_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *close = UiWidgets::button(panel, "Close", 74, 34);
  lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, -14, -12);
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
