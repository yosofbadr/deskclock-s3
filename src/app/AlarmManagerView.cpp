#include "AlarmManagerView.h"

#include <stdint.h>
#include <stdio.h>

#include "AlarmService.h"
#include "GestureTextMenu.h"
#include "TimeService.h"
#include "TimeSetupView.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

enum class Screen : uint8_t {
  List,
  Edit,
  DeleteConfirm,
};

enum class EditAction : uint8_t {
  Back = 0,
  Save,
  Hour,
  Minute,
  Recurrence,
  Date,
  Enabled,
  Delete,
};

constexpr uint8_t kVisibleRows = 5;
constexpr int32_t kRowHeight = 26;
constexpr int32_t kRowsTop = 38;

lv_obj_t *panel = nullptr;
const lv_font_t *view_font = nullptr;
int32_t panel_width = 0;
Screen screen = Screen::List;
Alarm editing_alarm;
bool editing_existing_alarm = false;
uint8_t editing_alarm_id = 0;
uint8_t selected_index = 1;
lv_obj_t *title_label = nullptr;
lv_obj_t *hint_label = nullptr;
lv_obj_t *rows[kVisibleRows] = {};
GestureTextMenu::TouchState touch_state;

uint8_t days_in_month(uint16_t year, uint8_t month)
{
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
    return 29;
  }
  return days[month - 1];
}

void adjust_date_by_days(uint16_t &year, uint8_t &month, uint8_t &day, int8_t delta)
{
  if (year < 2024 || month < 1 || month > 12 || day < 1 || day > days_in_month(year, month)) {
    year = 2026;
    month = 1;
    day = 1;
  }
  while (delta > 0) {
    const uint8_t month_days = days_in_month(year, month);
    if (day < month_days) {
      day++;
    } else {
      day = 1;
      month++;
      if (month > 12) {
        month = 1;
        year++;
        if (year > 2099) {
          year = 2024;
        }
      }
    }
    delta--;
  }
  while (delta < 0) {
    if (day > 1) {
      day--;
    } else {
      if (month > 1) {
        month--;
      } else {
        month = 12;
        year = year <= 2024 ? 2099 : static_cast<uint16_t>(year - 1U);
      }
      day = days_in_month(year, month);
    }
    delta++;
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

Alarm *find_alarm_by_id(uint8_t id, Alarm *alarms, size_t count)
{
  for (size_t index = 0; index < count; ++index) {
    if (alarms[index].id == id) {
      return &alarms[index];
    }
  }
  return nullptr;
}

void refresh();

void close_panel()
{
  if (panel != nullptr) {
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  }
}

void create_default_alarm()
{
  Alarm alarm;
  alarm.enabled = true;
  alarm.recurrence = AlarmRecurrence::Daily;
  DateTime now = TimeService::snapshot().now;
  if (now.valid) {
    const uint16_t total_minutes = static_cast<uint16_t>(now.hour) * 60U + now.minute + 5U;
    alarm.hour = static_cast<uint8_t>((total_minutes / 60U) % 24U);
    alarm.minute = static_cast<uint8_t>(total_minutes % 60U);
  } else {
    alarm.hour = 7;
    alarm.minute = 0;
  }
  editing_alarm = alarm;
  editing_existing_alarm = false;
  editing_alarm_id = 0;
  screen = Screen::Edit;
  selected_index = 2;
  refresh();
}

bool load_alarm_for_row(uint8_t row_index)
{
  if (row_index < 2) {
    return false;
  }
  Alarm alarms[kMaxAlarms];
  const size_t count = AlarmService::copyAlarms(alarms, kMaxAlarms);
  const size_t alarm_index = static_cast<size_t>(row_index - 2U);
  if (alarm_index >= count) {
    return false;
  }
  editing_alarm = alarms[alarm_index];
  editing_existing_alarm = true;
  editing_alarm_id = editing_alarm.id;
  return true;
}

uint8_t list_row_count()
{
  const size_t count = AlarmService::count();
  return static_cast<uint8_t>(2U + (count == 0 ? 1U : count));
}

uint8_t edit_row_count()
{
  return editing_existing_alarm ? 8 : 7;
}

uint8_t row_count()
{
  switch (screen) {
  case Screen::List:
    return list_row_count();
  case Screen::Edit:
    return edit_row_count();
  case Screen::DeleteConfirm:
    return 2;
  }
  return 0;
}

void save_editing_alarm()
{
  if (editing_alarm.recurrence == AlarmRecurrence::Once && editing_alarm.year == 0) {
    DateTime now = TimeService::snapshot().now;
    editing_alarm.year = now.valid ? now.year : 2026;
    editing_alarm.month = now.valid ? now.month : 1;
    editing_alarm.day = now.valid ? now.day : 1;
  }
  if (editing_existing_alarm) {
    AlarmService::updateAlarm(editing_alarm_id, editing_alarm);
  } else {
    AlarmService::addAlarm(editing_alarm);
  }
  screen = Screen::List;
  selected_index = 1;
  refresh();
}

EditAction edit_action_for_index(uint8_t index)
{
  return static_cast<EditAction>(index);
}

void adjust_edit_action(EditAction action, int8_t delta)
{
  switch (action) {
  case EditAction::Back:
  case EditAction::Save:
  case EditAction::Delete:
    return;
  case EditAction::Hour:
    editing_alarm.hour = wrap_value(editing_alarm.hour, delta, 0, 23);
    break;
  case EditAction::Minute:
    editing_alarm.minute = wrap_value(editing_alarm.minute, delta, 0, 55, 5);
    break;
  case EditAction::Recurrence:
    editing_alarm.recurrence = static_cast<AlarmRecurrence>(GestureTextMenu::wrapIndex(static_cast<uint8_t>(editing_alarm.recurrence), delta, 4));
    if (editing_alarm.recurrence == AlarmRecurrence::Once && editing_alarm.year == 0) {
      DateTime now = TimeService::snapshot().now;
      editing_alarm.year = now.valid ? now.year : 2026;
      editing_alarm.month = now.valid ? now.month : 1;
      editing_alarm.day = now.valid ? now.day : 1;
    }
    break;
  case EditAction::Date:
    if (editing_alarm.recurrence == AlarmRecurrence::Once) {
      adjust_date_by_days(editing_alarm.year, editing_alarm.month, editing_alarm.day, delta);
    }
    break;
  case EditAction::Enabled:
    editing_alarm.enabled = !editing_alarm.enabled;
    break;
  }
  refresh();
}

void activate_edit_action(EditAction action)
{
  switch (action) {
  case EditAction::Back:
    screen = Screen::List;
    selected_index = 1;
    refresh();
    return;
  case EditAction::Save:
    save_editing_alarm();
    return;
  case EditAction::Delete:
    if (editing_existing_alarm) {
      screen = Screen::DeleteConfirm;
      selected_index = 0;
      refresh();
    }
    return;
  case EditAction::Hour:
  case EditAction::Minute:
  case EditAction::Recurrence:
  case EditAction::Date:
  case EditAction::Enabled:
    adjust_edit_action(action, 1);
    return;
  }
}

void activate_list_row()
{
  if (selected_index == 0) {
    close_panel();
    return;
  }
  if (selected_index == 1) {
    if (AlarmService::count() < kMaxAlarms) {
      create_default_alarm();
    }
    return;
  }
  if (load_alarm_for_row(selected_index)) {
    screen = Screen::Edit;
    selected_index = 2;
    refresh();
  }
}

void adjust_list_row(int8_t)
{
  if (selected_index < 2) {
    return;
  }
  Alarm alarms[kMaxAlarms];
  const size_t count = AlarmService::copyAlarms(alarms, kMaxAlarms);
  const size_t alarm_index = static_cast<size_t>(selected_index - 2U);
  if (alarm_index >= count) {
    return;
  }
  AlarmService::setEnabled(alarms[alarm_index].id, !alarms[alarm_index].enabled);
  refresh();
}

void activate_delete_row()
{
  if (selected_index == 0) {
    screen = Screen::Edit;
    selected_index = 7;
    refresh();
    return;
  }
  if (selected_index == 1 && editing_existing_alarm) {
    AlarmService::removeAlarm(editing_alarm_id);
    screen = Screen::List;
    selected_index = 1;
    refresh();
  }
}

void draw_title(const char *text)
{
  if (title_label != nullptr) {
    lv_label_set_text(title_label, text);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 22, 8);
  }
  if (hint_label != nullptr) {
    lv_label_set_text(hint_label, "swipe select  left/right edit  tap enter");
    lv_obj_align(hint_label, LV_ALIGN_TOP_RIGHT, -22, 13);
  }
}

void format_list_row(uint8_t index, char *row, size_t size, uint32_t &color)
{
  color = 0x0F172A;
  if (index == 0) {
    snprintf(row, size, "Back");
    return;
  }
  if (index == 1) {
    if (AlarmService::count() >= kMaxAlarms) {
      color = 0x94A3B8;
      snprintf(row, size, "New alarm   full (%u/%u)", static_cast<unsigned>(AlarmService::count()), static_cast<unsigned>(kMaxAlarms));
    } else {
      snprintf(row, size, "New alarm");
    }
    return;
  }

  Alarm alarms[kMaxAlarms];
  const size_t count = AlarmService::copyAlarms(alarms, kMaxAlarms);
  if (count == 0) {
    color = 0x64748B;
    snprintf(row, size, "No saved alarms");
    return;
  }

  const size_t alarm_index = static_cast<size_t>(index - 2U);
  if (alarm_index >= count) {
    snprintf(row, size, "--");
    return;
  }

  char alarm_time[12];
  TimeSetupView::formatTime(alarm_time, sizeof(alarm_time), alarms[alarm_index].hour, alarms[alarm_index].minute);
  color = alarms[alarm_index].enabled ? 0x0F172A : 0x94A3B8;
  snprintf(row,
           size,
           "%u  %s  %s  %s",
           alarms[alarm_index].id,
           alarm_time,
           AlarmService::recurrenceLabel(alarms[alarm_index].recurrence),
           alarms[alarm_index].enabled ? "on" : "off");
}

void format_edit_row(uint8_t index, char *row, size_t size, uint32_t &color)
{
  color = 0x0F172A;
  char alarm_time[12];
  TimeSetupView::formatTime(alarm_time, sizeof(alarm_time), editing_alarm.hour, editing_alarm.minute);

  switch (edit_action_for_index(index)) {
  case EditAction::Back:
    snprintf(row, size, "Back");
    break;
  case EditAction::Save:
    color = 0x0F766E;
    snprintf(row, size, "Save  %s  %s  %s", alarm_time, AlarmService::recurrenceLabel(editing_alarm.recurrence), editing_alarm.enabled ? "on" : "off");
    break;
  case EditAction::Hour:
    snprintf(row, size, "Hour       %02u", editing_alarm.hour);
    break;
  case EditAction::Minute:
    snprintf(row, size, "Minute     %02u", editing_alarm.minute);
    break;
  case EditAction::Recurrence:
    snprintf(row, size, "Repeat     %s", AlarmService::recurrenceLabel(editing_alarm.recurrence));
    break;
  case EditAction::Date:
    if (editing_alarm.recurrence == AlarmRecurrence::Once) {
      snprintf(row, size, "Date       %04u-%02u-%02u", editing_alarm.year, editing_alarm.month, editing_alarm.day);
    } else {
      color = 0x94A3B8;
      snprintf(row, size, "Date       not used");
    }
    break;
  case EditAction::Enabled:
    snprintf(row, size, "Enabled    %s", editing_alarm.enabled ? "on" : "off");
    break;
  case EditAction::Delete:
    color = 0xB91C1C;
    snprintf(row, size, "Delete this alarm");
    break;
  }
}

void format_delete_row(uint8_t index, char *row, size_t size, uint32_t &color)
{
  color = index == 1 ? 0xB91C1C : 0x0F172A;
  if (index == 0) {
    snprintf(row, size, "Cancel");
    return;
  }
  char alarm_time[12];
  TimeSetupView::formatTime(alarm_time, sizeof(alarm_time), editing_alarm.hour, editing_alarm.minute);
  snprintf(row, size, "Delete %s %s", alarm_time, AlarmService::recurrenceLabel(editing_alarm.recurrence));
}

void format_row(uint8_t index, char *row, size_t size, uint32_t &color)
{
  switch (screen) {
  case Screen::List:
    format_list_row(index, row, size, color);
    return;
  case Screen::Edit:
    format_edit_row(index, row, size, color);
    return;
  case Screen::DeleteConfirm:
    format_delete_row(index, row, size, color);
    return;
  }
}

const char *title_for_screen()
{
  switch (screen) {
  case Screen::List:
    return "Alarms";
  case Screen::Edit:
    return editing_existing_alarm ? "Edit alarm" : "New alarm";
  case Screen::DeleteConfirm:
    return "Delete alarm?";
  }
  return "Alarms";
}

uint32_t menu_color(uint32_t color)
{
  switch (color) {
  case 0x0F172A:
    return 0xCBD5E1;
  case 0x0F766E:
    return 0x34D399;
  case 0xB91C1C:
    return 0xF87171;
  case 0xB45309:
    return 0xFBBF24;
  case 0x64748B:
  case 0x94A3B8:
    return color;
  default:
    return color;
  }
}

void set_row(uint8_t slot, uint8_t index, bool selected)
{
  char value[96];
  char line[112];
  uint32_t color = 0x0F172A;
  format_row(index, value, sizeof(value), color);
  snprintf(line, sizeof(line), "%s %.96s", selected ? ">" : " ", value);
  lv_label_set_text(rows[slot], line);
  lv_obj_set_style_text_color(rows[slot], lv_color_hex(selected ? 0xFFFFFF : menu_color(color)), 0);
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
  draw_title(title_for_screen());

  const uint8_t count = row_count();
  if (count == 0) {
    selected_index = 0;
    return;
  }
  if (selected_index >= count) {
    selected_index = static_cast<uint8_t>(count - 1U);
  }

  const uint8_t first = GestureTextMenu::firstVisibleIndex(selected_index, count, kVisibleRows);
  for (uint8_t slot = 0; slot < kVisibleRows; ++slot) {
    const uint8_t index = static_cast<uint8_t>(first + slot);
    if (index < count) {
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
    AlarmManagerView::moveSelection(-1);
    break;
  case GestureTextMenu::Input::Down:
    AlarmManagerView::moveSelection(1);
    break;
  case GestureTextMenu::Input::Left:
    AlarmManagerView::adjustSelected(-1);
    break;
  case GestureTextMenu::Input::Right:
    AlarmManagerView::adjustSelected(1);
    break;
  case GestureTextMenu::Input::Tap:
    AlarmManagerView::activateSelected();
    break;
  case GestureTextMenu::Input::None:
  default:
    break;
  }
}

} // namespace

namespace AlarmManagerView {

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
  screen = Screen::List;
  selected_index = 1;
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
  selected_index = GestureTextMenu::wrapIndex(selected_index, delta, row_count());
  refresh();
}

void adjustSelected(int8_t delta)
{
  if (!isOpen()) {
    return;
  }
  switch (screen) {
  case Screen::List:
    adjust_list_row(delta);
    break;
  case Screen::Edit:
    adjust_edit_action(edit_action_for_index(selected_index), delta);
    break;
  case Screen::DeleteConfirm:
    moveSelection(delta);
    break;
  }
}

void activateSelected()
{
  if (!isOpen()) {
    return;
  }
  switch (screen) {
  case Screen::List:
    activate_list_row();
    break;
  case Screen::Edit:
    activate_edit_action(edit_action_for_index(selected_index));
    break;
  case Screen::DeleteConfirm:
    activate_delete_row();
    break;
  }
}

} // namespace AlarmManagerView
} // namespace DeskClock
