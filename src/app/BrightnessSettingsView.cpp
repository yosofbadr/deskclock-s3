#include "BrightnessSettingsView.h"

#include <stdint.h>
#include <stdio.h>

#include "AlarmToneService.h"
#include "BrightnessService.h"
#include "GestureTextMenu.h"
#include "SettingsService.h"
#include "UiWidgets.h"

extern "C" void clock_face_refresh_theme(void);

namespace DeskClock {
namespace {

enum class Action : uintptr_t {
  Back = 0,
  Save,
  DayBrightness,
  NightBrightness,
  NightStart,
  DayStart,
  Audio,
  Volume,
  Theme,
  TestTone,
};

constexpr uint8_t kRowCount = 10;
constexpr uint8_t kVisibleRows = 5;
constexpr int32_t kRowHeight = 26;
constexpr int32_t kRowsTop = 38;

lv_obj_t *panel = nullptr;
const lv_font_t *view_font = nullptr;
int32_t panel_width = 0;
BrightnessSettings editing_brightness;
AlarmToneSettings editing_tone;
uint8_t selected_index = 2;
lv_obj_t *title_label = nullptr;
lv_obj_t *hint_label = nullptr;
lv_obj_t *rows[kVisibleRows] = {};
GestureTextMenu::TouchState touch_state;

void refresh();

uint8_t wrap_value(uint8_t value, int8_t delta, uint8_t min_value, uint8_t max_value, uint8_t step)
{
  if (value < min_value || value > max_value) {
    value = min_value;
  }
  const int16_t next = static_cast<int16_t>(value) + static_cast<int16_t>(delta) * step;
  if (next > max_value) {
    return min_value;
  }
  if (next < min_value) {
    return max_value;
  }
  return static_cast<uint8_t>(next);
}

void apply_and_close()
{
  BrightnessService::updateSettings(editing_brightness);
  AlarmToneService::updateSettings(editing_tone);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void adjust_theme(int8_t delta)
{
  SettingsSnapshot settings = SettingsService::snapshot();
  const uint8_t count = SettingsService::themeCount();
  if (count == 0) {
    return;
  }
  const uint8_t next = GestureTextMenu::wrapIndex(settings.theme_index, delta, count);
  while (SettingsService::snapshot().theme_index != next) {
    SettingsService::cycleTheme();
  }
  clock_face_refresh_theme();
}

Action action_for_index(uint8_t index)
{
  return static_cast<Action>(index);
}

void adjust_action(Action action, int8_t delta)
{
  switch (action) {
  case Action::Back:
  case Action::Save:
  case Action::TestTone:
    return;
  case Action::DayBrightness:
    editing_brightness.day_brightness = BrightnessService::nextPresetValue(editing_brightness.day_brightness, delta);
    break;
  case Action::NightBrightness:
    editing_brightness.night_brightness = BrightnessService::nextPresetValue(editing_brightness.night_brightness, delta);
    break;
  case Action::NightStart:
    editing_brightness.night_start_hour = wrap_value(editing_brightness.night_start_hour, delta, 0, 23, 1);
    break;
  case Action::DayStart:
    editing_brightness.day_start_hour = wrap_value(editing_brightness.day_start_hour, delta, 0, 23, 1);
    break;
  case Action::Audio:
    editing_tone.enabled = !editing_tone.enabled;
    break;
  case Action::Volume:
    editing_tone.volume = wrap_value(editing_tone.volume, delta, 10, 100, 10);
    break;
  case Action::Theme:
    adjust_theme(delta);
    break;
  }
  refresh();
}

void activate_action(Action action)
{
  switch (action) {
  case Action::Back:
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    return;
  case Action::Save:
    apply_and_close();
    return;
  case Action::TestTone:
    AlarmToneService::updateSettings(editing_tone);
    AlarmToneService::testTone();
    return;
  case Action::DayBrightness:
  case Action::NightBrightness:
  case Action::NightStart:
  case Action::DayStart:
  case Action::Audio:
  case Action::Volume:
  case Action::Theme:
    adjust_action(action, 1);
    return;
  }
}

void format_row(uint8_t index, char *row, size_t size)
{
  SettingsSnapshot settings = SettingsService::snapshot();
  switch (action_for_index(index)) {
  case Action::Back:
    snprintf(row, size, "Back");
    break;
  case Action::Save:
    snprintf(row, size, "Save display/audio settings");
    break;
  case Action::DayBrightness:
    snprintf(row, size, "Day brightness    %u", editing_brightness.day_brightness);
    break;
  case Action::NightBrightness:
    snprintf(row, size, "Night brightness  %u", editing_brightness.night_brightness);
    break;
  case Action::NightStart:
    snprintf(row, size, "Night starts      %02u:00", editing_brightness.night_start_hour);
    break;
  case Action::DayStart:
    snprintf(row, size, "Day starts        %02u:00", editing_brightness.day_start_hour);
    break;
  case Action::Audio:
    snprintf(row, size, "Alarm audio       %s", editing_tone.enabled ? "on" : "off");
    break;
  case Action::Volume:
    snprintf(row, size, "Alarm volume      %u", editing_tone.volume);
    break;
  case Action::Theme:
    snprintf(row, size, "Theme             %u/%u", static_cast<unsigned>(settings.theme_index + 1U), static_cast<unsigned>(SettingsService::themeCount()));
    break;
  case Action::TestTone:
    snprintf(row, size, "Test alarm tone");
    break;
  }
}

void draw_title()
{
  if (title_label != nullptr) {
    lv_label_set_text(title_label, "Display / audio");
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 22, 8);
  }
  if (hint_label != nullptr) {
    lv_label_set_text(hint_label, "swipe select  left/right edit  tap save/back");
    lv_obj_align(hint_label, LV_ALIGN_TOP_RIGHT, -22, 13);
  }
}

void set_row(uint8_t slot, uint8_t index, bool selected)
{
  char value[96];
  char line[112];
  format_row(index, value, sizeof(value));
  snprintf(line, sizeof(line), "%s %.96s", selected ? ">" : " ", value);
  lv_label_set_text(rows[slot], line);
  const bool save = action_for_index(index) == Action::Save;
  const bool destructive = false;
  uint32_t color = selected ? 0xFFFFFF : (save ? 0x34D399 : (destructive ? 0xF87171 : 0xCBD5E1));
  lv_obj_set_style_text_color(rows[slot], lv_color_hex(color), 0);
  lv_obj_set_style_bg_color(rows[slot], lv_color_hex(selected ? 0x334155 : 0x020617), 0);
  lv_obj_set_style_bg_opa(rows[slot], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(rows[slot], 2, 0);
  lv_obj_set_style_pad_left(rows[slot], 8, 0);
}

void refresh()
{
  if (panel == nullptr) {
    return;
  }
  draw_title();
  if (selected_index >= kRowCount) {
    selected_index = 2;
  }
  const uint8_t first = GestureTextMenu::firstVisibleIndex(selected_index, kRowCount, kVisibleRows);
  for (uint8_t slot = 0; slot < kVisibleRows; ++slot) {
    const uint8_t index = static_cast<uint8_t>(first + slot);
    if (index < kRowCount) {
      lv_obj_clear_flag(rows[slot], LV_OBJ_FLAG_HIDDEN);
      set_row(slot, index, index == selected_index);
    } else {
      lv_obj_add_flag(rows[slot], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void handle_input(GestureTextMenu::Input input, void *)
{
  switch (input) {
  case GestureTextMenu::Input::Up:
    BrightnessSettingsView::moveSelection(-1);
    break;
  case GestureTextMenu::Input::Down:
    BrightnessSettingsView::moveSelection(1);
    break;
  case GestureTextMenu::Input::Left:
    BrightnessSettingsView::adjustSelected(-1);
    break;
  case GestureTextMenu::Input::Right:
    BrightnessSettingsView::adjustSelected(1);
    break;
  case GestureTextMenu::Input::Tap:
    BrightnessSettingsView::activateSelected();
    break;
  case GestureTextMenu::Input::None:
  default:
    break;
  }
}

} // namespace

namespace BrightnessSettingsView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font)
{
  view_font = font != nullptr ? font : LV_FONT_DEFAULT;
  panel_width = width;
  panel = UiWidgets::fullScreenPanel(parent, width, height);
  GestureTextMenu::attach(panel, &touch_state, handle_input, nullptr);

  title_label = lv_label_create(panel);
  lv_obj_set_style_text_font(title_label, view_font, 0);
  UiWidgets::setTextColor(title_label, 0xFFFFFF);
  lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 22, 8);

  hint_label = lv_label_create(panel);
  lv_obj_set_style_text_font(hint_label, LV_FONT_DEFAULT, 0);
  UiWidgets::setTextColor(hint_label, 0x94A3B8);
  lv_obj_align(hint_label, LV_ALIGN_TOP_RIGHT, -22, 13);

  for (uint8_t slot = 0; slot < kVisibleRows; ++slot) {
    rows[slot] = lv_label_create(panel);
    lv_obj_set_style_text_font(rows[slot], view_font, 0);
    lv_obj_set_size(rows[slot], panel_width - 44, kRowHeight - 2);
    lv_label_set_long_mode(rows[slot], LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(rows[slot], 22, kRowsTop + static_cast<int32_t>(slot) * kRowHeight);
  }
  refresh();
}

void open()
{
  editing_brightness = BrightnessService::settings();
  editing_tone = AlarmToneService::settings();
  selected_index = 2;
  GestureTextMenu::reset(&touch_state);
  refresh();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
}

bool isOpen()
{
  return panel != nullptr && !lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void moveSelection(int8_t delta)
{
  if (!isOpen()) {
    return;
  }
  selected_index = GestureTextMenu::wrapIndex(selected_index, delta, kRowCount);
  refresh();
}

void adjustSelected(int8_t delta)
{
  if (!isOpen()) {
    return;
  }
  adjust_action(action_for_index(selected_index), delta);
}

void activateSelected()
{
  if (!isOpen()) {
    return;
  }
  activate_action(action_for_index(selected_index));
}

} // namespace BrightnessSettingsView
} // namespace DeskClock
