#include "TimeSetupView.h"

#include <Preferences.h>
#include <stdint.h>
#include <stdio.h>

#include "GestureTextMenu.h"
#include "NetworkSetupView.h"
#include "SettingsService.h"
#include "TimeService.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

enum class Action : uintptr_t {
  Back = 0,
  Save,
  Year,
  Month,
  Day,
  Hour,
  Minute,
  Format,
  Timezone,
};

constexpr uint8_t kRowCount = 9;
constexpr uint8_t kVisibleRows = 5;
constexpr int32_t kRowHeight = 26;
constexpr int32_t kRowsTop = 38;

lv_obj_t *panel = nullptr;
const lv_font_t *view_font = nullptr;
int32_t panel_width = 0;
DateTime editing_time;
bool use_24_hour_time = true;
uint8_t selected_index = 2;
lv_obj_t *title_label = nullptr;
lv_obj_t *hint_label = nullptr;
lv_obj_t *rows[kVisibleRows] = {};
GestureTextMenu::TouchState touch_state;

void refresh();

void savePreferences()
{
  Preferences preferences;
  if (preferences.begin("deskclock", false)) {
    preferences.putBool("time24", use_24_hour_time);
    preferences.end();
  }
}

uint8_t days_in_month(uint16_t year, uint8_t month)
{
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  if (month == 2 && (((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U))) {
    return 29;
  }
  return days[month - 1];
}

void clamp_day_to_month()
{
  const uint8_t month_days = days_in_month(editing_time.year, editing_time.month);
  if (editing_time.day > month_days) {
    editing_time.day = month_days;
  }
}

uint8_t wrap_value(uint8_t value, int8_t delta, uint8_t min_value, uint8_t max_value, uint8_t step = 1)
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

uint16_t wrap_year(uint16_t year, int8_t delta)
{
  if (year < 2024 || year > 2099) {
    year = 2026;
  }
  int16_t next = static_cast<int16_t>(year) + delta;
  if (next > 2099) {
    next = 2024;
  } else if (next < 2024) {
    next = 2099;
  }
  return static_cast<uint16_t>(next);
}

void adjust_date_by_days(int8_t delta)
{
  if (editing_time.year < 2024 || editing_time.month < 1 || editing_time.month > 12 || editing_time.day < 1 ||
      editing_time.day > days_in_month(editing_time.year, editing_time.month)) {
    editing_time.year = 2026;
    editing_time.month = 1;
    editing_time.day = 1;
  }

  while (delta > 0) {
    const uint8_t month_days = days_in_month(editing_time.year, editing_time.month);
    if (editing_time.day < month_days) {
      editing_time.day++;
    } else {
      editing_time.day = 1;
      editing_time.month++;
      if (editing_time.month > 12) {
        editing_time.month = 1;
        editing_time.year++;
        if (editing_time.year > 2099) {
          editing_time.year = 2024;
        }
      }
    }
    delta--;
  }

  while (delta < 0) {
    if (editing_time.day > 1) {
      editing_time.day--;
    } else {
      if (editing_time.month > 1) {
        editing_time.month--;
      } else {
        editing_time.month = 12;
        editing_time.year = editing_time.year <= 2024 ? 2099 : static_cast<uint16_t>(editing_time.year - 1U);
      }
      editing_time.day = days_in_month(editing_time.year, editing_time.month);
    }
    delta++;
  }
}

void close_panel()
{
  GestureTextMenu::reset(&touch_state);
  if (panel != nullptr) {
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  }
}

void save_and_close()
{
  editing_time.second = 0;
  editing_time.valid = true;
  if (TimeService::setManualTime(editing_time)) {
    SettingsService::setConfigured(true);
  }
  savePreferences();
  close_panel();
}

void adjust_timezone(int8_t delta)
{
  const SettingsSnapshot settings = SettingsService::snapshot();
  const uint8_t count = SettingsService::timezoneCount();
  if (count == 0) {
    return;
  }
  SettingsService::setTimezoneIndex(GestureTextMenu::wrapIndex(settings.timezone_index, delta, count));
}

void adjust_action(Action action, int8_t delta)
{
  switch (action) {
  case Action::Back:
  case Action::Save:
    return;
  case Action::Year:
    editing_time.year = wrap_year(editing_time.year, delta);
    clamp_day_to_month();
    break;
  case Action::Month:
    editing_time.month = wrap_value(editing_time.month, delta, 1, 12);
    clamp_day_to_month();
    break;
  case Action::Day:
    adjust_date_by_days(delta);
    break;
  case Action::Hour:
    editing_time.hour = wrap_value(editing_time.hour, delta, 0, 23);
    break;
  case Action::Minute:
    editing_time.minute = wrap_value(editing_time.minute, delta, 0, 55, 5);
    break;
  case Action::Format:
    use_24_hour_time = !use_24_hour_time;
    savePreferences();
    break;
  case Action::Timezone:
    adjust_timezone(delta);
    break;
  }
  refresh();
}

Action action_for_index(uint8_t index)
{
  return static_cast<Action>(index);
}

void activate_action(Action action)
{
  switch (action) {
  case Action::Back:
    close_panel();
    return;
  case Action::Save:
    save_and_close();
    return;
  case Action::Year:
  case Action::Month:
  case Action::Day:
  case Action::Hour:
  case Action::Minute:
  case Action::Format:
  case Action::Timezone:
    adjust_action(action, 1);
    return;
  }
}

void format_row(uint8_t index, char *row, size_t size)
{
  SettingsSnapshot settings = SettingsService::snapshot();
  char formatted_time[12];
  TimeSetupView::formatTime(formatted_time, sizeof(formatted_time), editing_time.hour, editing_time.minute);

  switch (action_for_index(index)) {
  case Action::Back:
    snprintf(row, size, "Back");
    break;
  case Action::Save:
    snprintf(row, size, "Save   %04u-%02u-%02u %s", editing_time.year, editing_time.month, editing_time.day, formatted_time);
    break;
  case Action::Year:
    snprintf(row, size, "Year        %04u", editing_time.year);
    break;
  case Action::Month:
    snprintf(row, size, "Month       %02u", editing_time.month);
    break;
  case Action::Day:
    snprintf(row, size, "Day         %02u", editing_time.day);
    break;
  case Action::Hour:
    snprintf(row, size, "Hour        %02u", editing_time.hour);
    break;
  case Action::Minute:
    snprintf(row, size, "Minute      %02u", editing_time.minute);
    break;
  case Action::Format:
    snprintf(row, size, "Format      %s", use_24_hour_time ? "24h" : "12h");
    break;
  case Action::Timezone:
    snprintf(row, size, "Timezone    %s", settings.timezone_label);
    break;
  }
}

void draw_title()
{
  if (title_label != nullptr) {
    lv_label_set_text(title_label, "Time");
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
  lv_obj_set_style_text_color(rows[slot], lv_color_hex(selected ? 0xFFFFFF : 0xCBD5E1), 0);
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
    TimeSetupView::moveSelection(-1);
    break;
  case GestureTextMenu::Input::Down:
    TimeSetupView::moveSelection(1);
    break;
  case GestureTextMenu::Input::Left:
    TimeSetupView::adjustSelected(-1);
    break;
  case GestureTextMenu::Input::Right:
    TimeSetupView::adjustSelected(1);
    break;
  case GestureTextMenu::Input::Tap:
    TimeSetupView::activateSelected();
    break;
  case GestureTextMenu::Input::None:
  default:
    break;
  }
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

} // namespace TimeSetupView
} // namespace DeskClock
