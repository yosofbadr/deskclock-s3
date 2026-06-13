#include "AlarmManagerView.h"

#include <stdint.h>
#include <stdio.h>

#include "AlarmService.h"
#include "TimeService.h"
#include "TimeSetupView.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

lv_obj_t *panel = nullptr;
const lv_font_t *view_font = nullptr;
Alarm editing_alarm;
bool editing_existing_alarm = false;
uint8_t editing_alarm_id = 0;
uint8_t pending_delete_alarm_id = 0;
lv_obj_t *hour_roller = nullptr;
lv_obj_t *minute_roller = nullptr;

uint8_t days_in_month(uint16_t year, uint8_t month)
{
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
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
      }
    }
    delta--;
  }
  while (delta < 0) {
    if (day > 1) {
      day--;
    } else if (month > 1) {
      month--;
      day = days_in_month(year, month);
    } else if (year > 2024) {
      year--;
      month = 12;
      day = 31;
    }
    delta++;
  }
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

void refresh_manager();
void refresh_editor();
void refresh_delete_confirmation();

void close_event(lv_event_t *)
{
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void draw_header(const char *title_text)
{
  lv_obj_t *title = lv_label_create(panel);
  lv_obj_set_style_text_font(title, view_font, 0);
  UiWidgets::setTextColor(title, 0x1F2933);
  lv_label_set_text(title, title_text);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  lv_obj_t *close_button = UiWidgets::button(panel, "Close", 74, 34);
  lv_obj_align(close_button, LV_ALIGN_TOP_RIGHT, -10, 8);
  lv_obj_add_event_cb(close_button, close_event, LV_EVENT_CLICKED, nullptr);
}

void begin_editor(const Alarm &alarm, bool existing)
{
  editing_alarm = alarm;
  editing_existing_alarm = existing;
  editing_alarm_id = existing ? alarm.id : 0;
  refresh_editor();
}

void toggle_event(lv_event_t *event)
{
  const uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  Alarm alarms[kMaxAlarms];
  const size_t count = AlarmService::copyAlarms(alarms, kMaxAlarms);
  Alarm *alarm = find_alarm_by_id(id, alarms, count);
  if (alarm != nullptr) {
    AlarmService::setEnabled(id, !alarm->enabled);
    refresh_manager();
  }
}

void delete_event(lv_event_t *event)
{
  pending_delete_alarm_id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  refresh_delete_confirmation();
}

void confirm_delete_event(lv_event_t *)
{
  if (pending_delete_alarm_id != 0) {
    AlarmService::removeAlarm(pending_delete_alarm_id);
    pending_delete_alarm_id = 0;
  }
  refresh_manager();
}

void cancel_delete_event(lv_event_t *)
{
  pending_delete_alarm_id = 0;
  refresh_manager();
}

void new_event(lv_event_t *)
{
  Alarm alarm;
  alarm.enabled = true;
  alarm.recurrence = AlarmRecurrence::Daily;
  const TimeSnapshot snapshot = TimeService::snapshot();
  if (snapshot.now.valid) {
    const uint16_t total_minutes = static_cast<uint16_t>(snapshot.now.hour) * 60U + snapshot.now.minute + 5U;
    alarm.hour = static_cast<uint8_t>((total_minutes / 60U) % 24U);
    alarm.minute = static_cast<uint8_t>(total_minutes % 60U);
  } else {
    alarm.hour = 7;
    alarm.minute = 0;
  }
  begin_editor(alarm, false);
}

void edit_event(lv_event_t *event)
{
  const uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  Alarm alarms[kMaxAlarms];
  const size_t count = AlarmService::copyAlarms(alarms, kMaxAlarms);
  Alarm *alarm = find_alarm_by_id(id, alarms, count);
  if (alarm != nullptr) {
    begin_editor(*alarm, true);
  }
}

void time_roller_event(lv_event_t *)
{
  if (hour_roller == nullptr || minute_roller == nullptr) {
    return;
  }
  editing_alarm.hour = static_cast<uint8_t>(lv_roller_get_selected(hour_roller));
  editing_alarm.minute = static_cast<uint8_t>(lv_roller_get_selected(minute_roller));
}

void adjust_date_event(lv_event_t *event)
{
  const int8_t delta = static_cast<int8_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  adjust_date_by_days(editing_alarm.year, editing_alarm.month, editing_alarm.day, delta);
  refresh_editor();
}

void set_once_date_from_current_time()
{
  DateTime now = TimeService::snapshot().now;
  if (now.valid) {
    editing_alarm.year = now.year;
    editing_alarm.month = now.month;
    editing_alarm.day = now.day;
    if (editing_alarm.hour < now.hour || (editing_alarm.hour == now.hour && editing_alarm.minute <= now.minute)) {
      adjust_date_by_days(editing_alarm.year, editing_alarm.month, editing_alarm.day, 1);
    }
  } else {
    editing_alarm.year = 2026;
    editing_alarm.month = 1;
    editing_alarm.day = 1;
  }
}

void recurrence_button_event(lv_event_t *event)
{
  time_roller_event(nullptr);
  const AlarmRecurrence recurrence = static_cast<AlarmRecurrence>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  editing_alarm.recurrence = recurrence;
  if (editing_alarm.recurrence == AlarmRecurrence::Once) {
    set_once_date_from_current_time();
  }
  refresh_editor();
}

void save_editor_event(lv_event_t *)
{
  time_roller_event(nullptr);
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
  refresh_manager();
}

void refresh_manager()
{
  if (panel == nullptr) {
    return;
  }
  lv_obj_clean(panel);
  draw_header("Alarms");

  lv_obj_t *add_button = UiWidgets::button(panel, "+ alarm", 94, 34);
  lv_obj_align(add_button, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_add_event_cb(add_button, new_event, LV_EVENT_CLICKED, nullptr);
  if (AlarmService::count() >= kMaxAlarms) {
    lv_obj_add_state(add_button, LV_STATE_DISABLED);
  }

  Alarm alarms[kMaxAlarms];
  const size_t count = AlarmService::copyAlarms(alarms, kMaxAlarms);
  if (count == 0) {
    lv_obj_t *empty = lv_label_create(panel);
    lv_label_set_text(empty, "No saved alarms");
    UiWidgets::setTextColor(empty, 0x52616F);
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, 6);
    return;
  }

  for (size_t index = 0; index < count; ++index) {
    const int32_t y = 50 + static_cast<int32_t>(index) * 35;
    char alarm_time[12];
    TimeSetupView::formatTime(alarm_time, sizeof(alarm_time), alarms[index].hour, alarms[index].minute);
    char row_text[48];
    snprintf(row_text, sizeof(row_text), "%u  %s  %s  %s", alarms[index].id, alarm_time, AlarmService::recurrenceLabel(alarms[index].recurrence), alarms[index].enabled ? "on" : "off");

    lv_obj_t *row = lv_label_create(panel);
    lv_label_set_text(row, row_text);
    UiWidgets::setTextColor(row, alarms[index].enabled ? 0x1F2933 : 0x829AB1);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 12, y + 8);

    lv_obj_t *edit = UiWidgets::button(panel, "Edit", 50, 28);
    lv_obj_align(edit, LV_ALIGN_TOP_RIGHT, -130, y);
    lv_obj_add_event_cb(edit, edit_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(alarms[index].id)));
    lv_obj_t *toggle = UiWidgets::button(panel, alarms[index].enabled ? "Off" : "On", 50, 28);
    lv_obj_align(toggle, LV_ALIGN_TOP_RIGHT, -72, y);
    lv_obj_add_event_cb(toggle, toggle_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(alarms[index].id)));
    lv_obj_t *del = UiWidgets::button(panel, "Del", 50, 28);
    lv_obj_align(del, LV_ALIGN_TOP_RIGHT, -14, y);
    lv_obj_add_event_cb(del, delete_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(alarms[index].id)));
  }
}

void refresh_delete_confirmation()
{
  if (panel == nullptr) {
    return;
  }

  lv_obj_clean(panel);
  draw_header("Delete alarm?");

  Alarm alarms[kMaxAlarms];
  const size_t count = AlarmService::copyAlarms(alarms, kMaxAlarms);
  Alarm *alarm = find_alarm_by_id(pending_delete_alarm_id, alarms, count);

  char message[80];
  if (alarm != nullptr) {
    char alarm_time[12];
    TimeSetupView::formatTime(alarm_time, sizeof(alarm_time), alarm->hour, alarm->minute);
    snprintf(message, sizeof(message), "Delete %s %s?", alarm_time, AlarmService::recurrenceLabel(alarm->recurrence));
  } else {
    snprintf(message, sizeof(message), "Delete this alarm?");
  }

  lv_obj_t *label = lv_label_create(panel);
  lv_obj_set_style_text_font(label, view_font, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  UiWidgets::setTextColor(label, 0x1F2933);
  lv_label_set_text(label, message);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -12);

  lv_obj_t *delete_button = UiWidgets::button(panel, "Delete", 94, 38);
  lv_obj_align(delete_button, LV_ALIGN_BOTTOM_MID, -56, -18);
  lv_obj_add_event_cb(delete_button, confirm_delete_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *cancel_button = UiWidgets::button(panel, "Cancel", 94, 38);
  lv_obj_align(cancel_button, LV_ALIGN_BOTTOM_MID, 56, -18);
  lv_obj_add_event_cb(cancel_button, cancel_delete_event, LV_EVENT_CLICKED, nullptr);
}

void refresh_editor()
{
  if (panel == nullptr) {
    return;
  }
  lv_obj_clean(panel);
  hour_roller = nullptr;
  minute_roller = nullptr;
  draw_header(editing_existing_alarm ? "Edit alarm" : "New alarm");

  char alarm_time[12];
  TimeSetupView::formatTime(alarm_time, sizeof(alarm_time), editing_alarm.hour, editing_alarm.minute);
  char details[96];
  if (editing_alarm.recurrence == AlarmRecurrence::Once) {
    snprintf(details, sizeof(details), "%s\n%04u-%02u-%02u once", alarm_time, editing_alarm.year, editing_alarm.month, editing_alarm.day);
  } else {
    snprintf(details, sizeof(details), "%s\n%s", alarm_time, AlarmService::recurrenceLabel(editing_alarm.recurrence));
  }
  lv_obj_t *value = lv_label_create(panel);
  lv_obj_set_style_text_font(value, view_font, 0);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
  UiWidgets::setTextColor(value, 0x1F2933);
  lv_label_set_text(value, details);
  lv_obj_align(value, LV_ALIGN_CENTER, 0, -22);

  static const char *hour_options = "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";
  static const char *minute_options = "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

  hour_roller = lv_roller_create(panel);
  lv_roller_set_options(hour_roller, hour_options, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(hour_roller, 3);
  lv_roller_set_selected(hour_roller, editing_alarm.hour, LV_ANIM_OFF);
  lv_obj_set_size(hour_roller, 74, 82);
  lv_obj_align(hour_roller, LV_ALIGN_CENTER, -48, 42);
  lv_obj_add_event_cb(hour_roller, time_roller_event, LV_EVENT_VALUE_CHANGED, nullptr);

  minute_roller = lv_roller_create(panel);
  lv_roller_set_options(minute_roller, minute_options, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(minute_roller, 3);
  lv_roller_set_selected(minute_roller, editing_alarm.minute, LV_ANIM_OFF);
  lv_obj_set_size(minute_roller, 74, 82);
  lv_obj_align(minute_roller, LV_ALIGN_CENTER, 48, 42);
  lv_obj_add_event_cb(minute_roller, time_roller_event, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *date_down = UiWidgets::button(panel, "D-", 44, 30);
  lv_obj_align(date_down, LV_ALIGN_TOP_MID, -48, 48);
  lv_obj_add_event_cb(date_down, adjust_date_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(-1)));
  lv_obj_t *date_up = UiWidgets::button(panel, "D+", 44, 30);
  lv_obj_align(date_up, LV_ALIGN_TOP_MID, 48, 48);
  lv_obj_add_event_cb(date_up, adjust_date_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  if (editing_alarm.recurrence != AlarmRecurrence::Once) {
    lv_obj_add_state(date_down, LV_STATE_DISABLED);
    lv_obj_add_state(date_up, LV_STATE_DISABLED);
  }

  lv_obj_t *once = UiWidgets::button(panel, "Once", 72, 28);
  lv_obj_align(once, LV_ALIGN_BOTTOM_LEFT, 12, -48);
  lv_obj_add_event_cb(once, recurrence_button_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(AlarmRecurrence::Once)));
  lv_obj_t *daily = UiWidgets::button(panel, "Daily", 72, 28);
  lv_obj_align(daily, LV_ALIGN_BOTTOM_LEFT, 90, -48);
  lv_obj_add_event_cb(daily, recurrence_button_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(AlarmRecurrence::Daily)));
  lv_obj_t *weekdays = UiWidgets::button(panel, "Week", 72, 28);
  lv_obj_align(weekdays, LV_ALIGN_BOTTOM_LEFT, 168, -48);
  lv_obj_add_event_cb(weekdays, recurrence_button_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(AlarmRecurrence::Weekdays)));
  lv_obj_t *weekends = UiWidgets::button(panel, "End", 72, 28);
  lv_obj_align(weekends, LV_ALIGN_BOTTOM_LEFT, 246, -48);
  lv_obj_add_event_cb(weekends, recurrence_button_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(AlarmRecurrence::Weekends)));

  switch (editing_alarm.recurrence) {
  case AlarmRecurrence::Once:
    lv_obj_add_state(once, LV_STATE_CHECKED);
    break;
  case AlarmRecurrence::Daily:
    lv_obj_add_state(daily, LV_STATE_CHECKED);
    break;
  case AlarmRecurrence::Weekdays:
    lv_obj_add_state(weekdays, LV_STATE_CHECKED);
    break;
  case AlarmRecurrence::Weekends:
  default:
    lv_obj_add_state(weekends, LV_STATE_CHECKED);
    break;
  }

  lv_obj_t *save = UiWidgets::button(panel, "Save", 74, 34);
  lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_obj_add_event_cb(save, save_editor_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cancel = UiWidgets::button(panel, "Cancel", 74, 34);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, -14, -12);
  lv_obj_add_event_cb(cancel, [](lv_event_t *) { refresh_manager(); }, LV_EVENT_CLICKED, nullptr);
}

} // namespace

namespace AlarmManagerView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font)
{
  view_font = font;
  panel = UiWidgets::modalPanel(parent, width, height);
  refresh_manager();
}

void open()
{
  refresh_manager();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
}

} // namespace AlarmManagerView
} // namespace DeskClock
