#include "ClockFace.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "AlarmService.h"
#include "AlarmAlertView.h"
#include "BrightnessSettingsView.h"
#include "NetworkSetupView.h"
#include "SettingsService.h"
#include "TimeService.h"
#include "TimeSetupView.h"
#include "UiWidgets.h"
#include "lvgl.h"

namespace {

lv_obj_t *time_label = nullptr;
lv_obj_t *date_label = nullptr;
lv_obj_t *seconds_label = nullptr;
lv_obj_t *status_label = nullptr;
lv_obj_t *next_alarm_label = nullptr;
lv_obj_t *sync_dot = nullptr;
lv_obj_t *setup_hint_label = nullptr;
lv_obj_t *alarm_manager_panel = nullptr;
DeskClock::Alarm editing_alarm;
bool editing_existing_alarm = false;
uint8_t editing_alarm_id = 0;

const lv_font_t *time_font()
{
#if LV_FONT_MONTSERRAT_48
  return &lv_font_montserrat_48;
#else
  return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *body_font()
{
#if LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#else
  return LV_FONT_DEFAULT;
#endif
}

void set_text_color(lv_obj_t *obj, uint32_t color)
{
  DeskClock::UiWidgets::setTextColor(obj, color);
}

const char *weekday_name(uint8_t week)
{
  static constexpr const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return week < 7 ? names[week] : "---";
}

const char *month_name(uint8_t month)
{
  static constexpr const char *names[] = {
      "---", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return month <= 12 ? names[month] : "---";
}

bool is_leap_year(uint16_t year)
{
  return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

uint8_t days_in_month(uint16_t year, uint8_t month)
{
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  if (month == 2 && is_leap_year(year)) {
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

uint32_t sync_dot_color(DeskClock::SyncState state, bool blink)
{
  switch (state) {
  case DeskClock::SyncState::SyncedRecently:
    return blink ? 0x10B981 : 0x6EE7B7; // green
  case DeskClock::SyncState::LocalRetained:
    return blink ? 0xF59E0B : 0xFCD34D; // amber
  case DeskClock::SyncState::Unreliable:
  default:
    return blink ? 0xEF4444 : 0xFCA5A5; // red
  }
}

const char *status_text(const DeskClock::TimeSnapshot &snapshot)
{
  if (!snapshot.rtc_available) {
    return "rtc unavailable";
  }
  if (!snapshot.now.valid) {
    return "time not set";
  }
  if (snapshot.bootstrapped_from_compile_time) {
    return "rtc build seed";
  }
  switch (snapshot.sync_state) {
  case DeskClock::SyncState::SyncedRecently:
    return "synced";
  case DeskClock::SyncState::LocalRetained:
    return "rtc local";
  case DeskClock::SyncState::Unreliable:
  default:
    return "time not set";
  }
}

void realign_time_details()
{
  lv_obj_update_layout(time_label);
  lv_obj_align_to(date_label, time_label, LV_ALIGN_OUT_BOTTOM_LEFT, 4, 2);
  lv_obj_align_to(seconds_label, time_label, LV_ALIGN_OUT_RIGHT_MID, 10, 8);
}

void format_time(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
  DeskClock::TimeSetupView::formatTime(buffer, size, hour, minute);
}

void update_next_alarm_label(const DeskClock::DateTime &now)
{
  DeskClock::AlarmOccurrence next = DeskClock::AlarmService::nextAlarm(now);
  if (!next.exists) {
    lv_label_set_text(next_alarm_label, "Alarms: tap to add");
    return;
  }

  char alarm_time[12];
  format_time(alarm_time, sizeof(alarm_time), next.at.hour, next.at.minute);
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "Alarm %s %s", alarm_time, DeskClock::AlarmService::recurrenceLabel(next.alarm.recurrence));
  lv_label_set_text(next_alarm_label, buffer);
}

void refresh_alarm_manager();
void refresh_alarm_editor();
void append_wifi_password_event(lv_event_t *event);

DeskClock::Alarm *find_alarm_by_id(uint8_t id, DeskClock::Alarm *alarms, size_t count)
{
  for (size_t index = 0; index < count; ++index) {
    if (alarms[index].id == id) {
      return &alarms[index];
    }
  }
  return nullptr;
}

lv_obj_t *create_button(lv_obj_t *parent, const char *text, int32_t width, int32_t height)
{
  return DeskClock::UiWidgets::button(parent, text, width, height);
}

void close_alarm_manager_event(lv_event_t *)
{
  lv_obj_add_flag(alarm_manager_panel, LV_OBJ_FLAG_HIDDEN);
}

void toggle_alarm_event(lv_event_t *event)
{
  const uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  DeskClock::Alarm alarms[DeskClock::kMaxAlarms];
  const size_t count = DeskClock::AlarmService::copyAlarms(alarms, DeskClock::kMaxAlarms);
  DeskClock::Alarm *alarm = find_alarm_by_id(id, alarms, count);
  if (alarm == nullptr) {
    return;
  }
  DeskClock::AlarmService::setEnabled(id, !alarm->enabled);
  refresh_alarm_manager();
}

void delete_alarm_event(lv_event_t *event)
{
  const uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  DeskClock::AlarmService::removeAlarm(id);
  refresh_alarm_manager();
}

void begin_alarm_editor(const DeskClock::Alarm &alarm, bool existing)
{
  editing_alarm = alarm;
  editing_existing_alarm = existing;
  editing_alarm_id = existing ? alarm.id : 0;
  refresh_alarm_editor();
}

void new_alarm_event(lv_event_t *)
{
  DeskClock::Alarm alarm;
  alarm.enabled = true;
  alarm.recurrence = DeskClock::AlarmRecurrence::Daily;
  const DeskClock::TimeSnapshot snapshot = DeskClock::TimeService::snapshot();
  if (snapshot.now.valid) {
    const uint16_t total_minutes = static_cast<uint16_t>(snapshot.now.hour) * 60U + snapshot.now.minute + 5U;
    alarm.hour = static_cast<uint8_t>((total_minutes / 60U) % 24U);
    alarm.minute = static_cast<uint8_t>(total_minutes % 60U);
  } else {
    alarm.hour = 7;
    alarm.minute = 0;
  }
  begin_alarm_editor(alarm, false);
}

void edit_alarm_event(lv_event_t *event)
{
  const uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  DeskClock::Alarm alarms[DeskClock::kMaxAlarms];
  const size_t count = DeskClock::AlarmService::copyAlarms(alarms, DeskClock::kMaxAlarms);
  DeskClock::Alarm *alarm = find_alarm_by_id(id, alarms, count);
  if (alarm != nullptr) {
    begin_alarm_editor(*alarm, true);
  }
}

void adjust_alarm_time_event(lv_event_t *event)
{
  const int32_t delta = static_cast<int32_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  int32_t total = static_cast<int32_t>(editing_alarm.hour) * 60 + editing_alarm.minute + delta;
  while (total < 0) {
    total += 24 * 60;
  }
  total %= 24 * 60;
  editing_alarm.hour = static_cast<uint8_t>(total / 60);
  editing_alarm.minute = static_cast<uint8_t>(total % 60);
  refresh_alarm_editor();
}

void adjust_alarm_date_event(lv_event_t *event)
{
  const int8_t delta = static_cast<int8_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  adjust_date_by_days(editing_alarm.year, editing_alarm.month, editing_alarm.day, delta);
  refresh_alarm_editor();
}

void cycle_alarm_recurrence_event(lv_event_t *)
{
  switch (editing_alarm.recurrence) {
  case DeskClock::AlarmRecurrence::Once:
    editing_alarm.recurrence = DeskClock::AlarmRecurrence::Daily;
    break;
  case DeskClock::AlarmRecurrence::Daily:
    editing_alarm.recurrence = DeskClock::AlarmRecurrence::Weekdays;
    break;
  case DeskClock::AlarmRecurrence::Weekdays:
    editing_alarm.recurrence = DeskClock::AlarmRecurrence::Weekends;
    break;
  case DeskClock::AlarmRecurrence::Weekends:
  default:
    editing_alarm.recurrence = DeskClock::AlarmRecurrence::Once;
    DeskClock::DateTime now = DeskClock::TimeService::snapshot().now;
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
    break;
  }
  refresh_alarm_editor();
}

void save_alarm_editor_event(lv_event_t *)
{
  if (editing_alarm.recurrence == DeskClock::AlarmRecurrence::Once && editing_alarm.year == 0) {
    DeskClock::DateTime now = DeskClock::TimeService::snapshot().now;
    editing_alarm.year = now.valid ? now.year : 2026;
    editing_alarm.month = now.valid ? now.month : 1;
    editing_alarm.day = now.valid ? now.day : 1;
  }
  if (editing_existing_alarm) {
    DeskClock::AlarmService::updateAlarm(editing_alarm_id, editing_alarm);
  } else {
    DeskClock::AlarmService::addAlarm(editing_alarm);
  }
  refresh_alarm_manager();
}

void cancel_alarm_editor_event(lv_event_t *)
{
  refresh_alarm_manager();
}

void open_alarm_manager_event(lv_event_t *)
{
  refresh_alarm_manager();
  lv_obj_clear_flag(alarm_manager_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(alarm_manager_panel);
}

void draw_alarm_manager_header(const char *title_text)
{
  lv_obj_t *title = lv_label_create(alarm_manager_panel);
  lv_obj_set_style_text_font(title, body_font(), 0);
  set_text_color(title, 0x1F2933);
  lv_label_set_text(title, title_text);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  lv_obj_t *close_button = create_button(alarm_manager_panel, "Close", 74, 34);
  lv_obj_align(close_button, LV_ALIGN_TOP_RIGHT, -10, 8);
  lv_obj_add_event_cb(close_button, close_alarm_manager_event, LV_EVENT_CLICKED, nullptr);
}

void refresh_alarm_manager()
{
  if (alarm_manager_panel == nullptr) {
    return;
  }

  lv_obj_clean(alarm_manager_panel);
  draw_alarm_manager_header("Alarms");

  lv_obj_t *add_button = create_button(alarm_manager_panel, "+ alarm", 94, 34);
  lv_obj_align(add_button, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_add_event_cb(add_button, new_alarm_event, LV_EVENT_CLICKED, nullptr);
  if (DeskClock::AlarmService::count() >= DeskClock::kMaxAlarms) {
    lv_obj_add_state(add_button, LV_STATE_DISABLED);
  }

  DeskClock::Alarm alarms[DeskClock::kMaxAlarms];
  const size_t count = DeskClock::AlarmService::copyAlarms(alarms, DeskClock::kMaxAlarms);
  if (count == 0) {
    lv_obj_t *empty = lv_label_create(alarm_manager_panel);
    lv_label_set_text(empty, "No saved alarms");
    set_text_color(empty, 0x52616F);
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, 6);
    return;
  }

  for (size_t index = 0; index < count; ++index) {
    const int32_t y = 50 + static_cast<int32_t>(index) * 35;
    char alarm_time[12];
    format_time(alarm_time, sizeof(alarm_time), alarms[index].hour, alarms[index].minute);
    char row_text[48];
    snprintf(
        row_text,
        sizeof(row_text),
        "%u  %s  %s  %s",
        alarms[index].id,
        alarm_time,
        DeskClock::AlarmService::recurrenceLabel(alarms[index].recurrence),
        alarms[index].enabled ? "on" : "off");

    lv_obj_t *row = lv_label_create(alarm_manager_panel);
    lv_label_set_text(row, row_text);
    set_text_color(row, alarms[index].enabled ? 0x1F2933 : 0x829AB1);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 12, y + 8);

    lv_obj_t *edit = create_button(alarm_manager_panel, "Edit", 50, 28);
    lv_obj_align(edit, LV_ALIGN_TOP_RIGHT, -130, y);
    lv_obj_add_event_cb(edit, edit_alarm_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(alarms[index].id)));

    lv_obj_t *toggle = create_button(alarm_manager_panel, alarms[index].enabled ? "Off" : "On", 50, 28);
    lv_obj_align(toggle, LV_ALIGN_TOP_RIGHT, -72, y);
    lv_obj_add_event_cb(toggle, toggle_alarm_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(alarms[index].id)));

    lv_obj_t *del = create_button(alarm_manager_panel, "Del", 50, 28);
    lv_obj_align(del, LV_ALIGN_TOP_RIGHT, -14, y);
    lv_obj_add_event_cb(del, delete_alarm_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(alarms[index].id)));
  }
}

void refresh_alarm_editor()
{
  if (alarm_manager_panel == nullptr) {
    return;
  }
  lv_obj_clean(alarm_manager_panel);
  draw_alarm_manager_header(editing_existing_alarm ? "Edit alarm" : "New alarm");

  char alarm_time[12];
  format_time(alarm_time, sizeof(alarm_time), editing_alarm.hour, editing_alarm.minute);
  char details[96];
  if (editing_alarm.recurrence == DeskClock::AlarmRecurrence::Once) {
    snprintf(
        details,
        sizeof(details),
        "%s\n%04u-%02u-%02u once",
        alarm_time,
        editing_alarm.year,
        editing_alarm.month,
        editing_alarm.day);
  } else {
    snprintf(details, sizeof(details), "%s\n%s", alarm_time, DeskClock::AlarmService::recurrenceLabel(editing_alarm.recurrence));
  }
  lv_obj_t *value = lv_label_create(alarm_manager_panel);
  lv_obj_set_style_text_font(value, time_font(), 0);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
  set_text_color(value, 0x1F2933);
  lv_label_set_text(value, details);
  lv_obj_align(value, LV_ALIGN_CENTER, 0, -22);

  lv_obj_t *minus_hour = create_button(alarm_manager_panel, "-1h", 58, 34);
  lv_obj_align(minus_hour, LV_ALIGN_LEFT_MID, 20, 48);
  lv_obj_add_event_cb(minus_hour, adjust_alarm_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(-60)));
  lv_obj_t *minus_minute = create_button(alarm_manager_panel, "-1m", 58, 34);
  lv_obj_align(minus_minute, LV_ALIGN_LEFT_MID, 88, 48);
  lv_obj_add_event_cb(minus_minute, adjust_alarm_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(-1)));
  lv_obj_t *plus_minute = create_button(alarm_manager_panel, "+1m", 58, 34);
  lv_obj_align(plus_minute, LV_ALIGN_RIGHT_MID, -88, 48);
  lv_obj_add_event_cb(plus_minute, adjust_alarm_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  lv_obj_t *plus_hour = create_button(alarm_manager_panel, "+1h", 58, 34);
  lv_obj_align(plus_hour, LV_ALIGN_RIGHT_MID, -20, 48);
  lv_obj_add_event_cb(plus_hour, adjust_alarm_time_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(60)));

  lv_obj_t *date_down = create_button(alarm_manager_panel, "D-", 44, 30);
  lv_obj_align(date_down, LV_ALIGN_TOP_MID, -48, 48);
  lv_obj_add_event_cb(date_down, adjust_alarm_date_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(-1)));
  lv_obj_t *date_up = create_button(alarm_manager_panel, "D+", 44, 30);
  lv_obj_align(date_up, LV_ALIGN_TOP_MID, 48, 48);
  lv_obj_add_event_cb(date_up, adjust_alarm_date_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  if (editing_alarm.recurrence != DeskClock::AlarmRecurrence::Once) {
    lv_obj_add_state(date_down, LV_STATE_DISABLED);
    lv_obj_add_state(date_up, LV_STATE_DISABLED);
  }

  lv_obj_t *recurrence = create_button(alarm_manager_panel, "Repeat", 82, 34);
  lv_obj_align(recurrence, LV_ALIGN_BOTTOM_LEFT, 14, -12);
  lv_obj_add_event_cb(recurrence, cycle_alarm_recurrence_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *save = create_button(alarm_manager_panel, "Save", 74, 34);
  lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_obj_add_event_cb(save, save_alarm_editor_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cancel = create_button(alarm_manager_panel, "Cancel", 74, 34);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, -14, -12);
  lv_obj_add_event_cb(cancel, cancel_alarm_editor_event, LV_EVENT_CLICKED, nullptr);
}

void open_time_setup_event(lv_event_t *)
{
  DeskClock::TimeSetupView::open();
}

void update_clock_from_time_service(lv_timer_t *)
{
  const DeskClock::TimeSnapshot snapshot = DeskClock::TimeService::snapshot();
  const bool blink = snapshot.now.valid ? ((snapshot.now.second % 2U) == 0U) : true;

  if (!snapshot.now.valid) {
    lv_label_set_text(time_label, "--:--");
    lv_label_set_text(date_label, "Time not set");
    lv_label_set_text(seconds_label, "unreliable");
    lv_label_set_text(status_label, status_text(snapshot));
    lv_label_set_text(next_alarm_label, "Alarms: tap to add");
    lv_obj_set_style_bg_color(sync_dot, lv_color_hex(sync_dot_color(DeskClock::SyncState::Unreliable, blink)), 0);
    DeskClock::AlarmAlertView::update(snapshot.now);
    realign_time_details();
    return;
  }

  char time_buffer[12];
  format_time(time_buffer, sizeof(time_buffer), snapshot.now.hour, snapshot.now.minute);
  lv_label_set_text(time_label, time_buffer);

  char date_buffer[20];
  snprintf(
      date_buffer,
      sizeof(date_buffer),
      "%s, %s %u",
      weekday_name(snapshot.now.week),
      month_name(snapshot.now.month),
      snapshot.now.day);
  lv_label_set_text(date_label, date_buffer);

  char seconds_buffer[16];
  snprintf(seconds_buffer, sizeof(seconds_buffer), ":%02u  local", snapshot.now.second);
  lv_label_set_text(seconds_label, seconds_buffer);

  lv_label_set_text(status_label, status_text(snapshot));
  update_next_alarm_label(snapshot.now);
  lv_obj_set_style_bg_color(sync_dot, lv_color_hex(sync_dot_color(snapshot.sync_state, blink)), 0);
  DeskClock::AlarmAlertView::update(snapshot.now);
  realign_time_details();
}

} // namespace

extern "C" void clock_face_create(void)
{
  DeskClock::TimeSetupView::loadPreferences();
  lv_obj_t *screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xF7F9FC), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  const int32_t width = lv_display_get_horizontal_resolution(nullptr);
  const int32_t height = lv_display_get_vertical_resolution(nullptr);
  const int32_t theme_width = (width * 35) / 100;
  const int32_t clock_width = width - theme_width;

  lv_obj_t *theme_panel = lv_obj_create(screen);
  lv_obj_remove_style_all(theme_panel);
  lv_obj_set_size(theme_panel, theme_width, height);
  lv_obj_set_pos(theme_panel, 0, 0);
  lv_obj_set_style_bg_color(theme_panel, lv_color_hex(0xE9EEF5), 0);
  lv_obj_set_style_bg_opa(theme_panel, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(theme_panel, 8, 0);

  lv_obj_t *asset_card = lv_obj_create(theme_panel);
  lv_obj_set_size(asset_card, LV_MIN(theme_width - 24, 132), LV_MIN(height - 28, 132));
  lv_obj_align(asset_card, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(asset_card, 24, 0);
  lv_obj_set_style_bg_color(asset_card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(asset_card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(asset_card, lv_color_hex(0xB8C2CC), 0);
  lv_obj_set_style_border_width(asset_card, 3, 0);
  lv_obj_clear_flag(asset_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(asset_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(asset_card, [](lv_event_t *) { DeskClock::BrightnessSettingsView::open(); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *asset_label = lv_label_create(asset_card);
  lv_obj_set_style_text_font(asset_label, body_font(), 0);
  set_text_color(asset_label, 0x52616F);
  lv_label_set_text(asset_label, "settings\nbrightness\nsound");
  lv_obj_set_style_text_align(asset_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(asset_label);

  lv_obj_t *clock_panel = lv_obj_create(screen);
  lv_obj_remove_style_all(clock_panel);
  lv_obj_set_size(clock_panel, clock_width, height);
  lv_obj_set_pos(clock_panel, theme_width, 0);
  lv_obj_set_style_bg_color(clock_panel, lv_color_hex(0xF7F9FC), 0);
  lv_obj_set_style_bg_opa(clock_panel, LV_OPA_COVER, 0);
  lv_obj_clear_flag(clock_panel, LV_OBJ_FLAG_SCROLLABLE);

  time_label = lv_label_create(clock_panel);
  lv_obj_set_style_text_font(time_label, time_font(), 0);
  set_text_color(time_label, 0x1F2933);
  lv_label_set_text(time_label, "--:--");
  lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 28, -24);

  date_label = lv_label_create(clock_panel);
  lv_obj_set_style_text_font(date_label, body_font(), 0);
  set_text_color(date_label, 0x52616F);
  lv_label_set_text(date_label, "Time not set");
  lv_obj_align_to(date_label, time_label, LV_ALIGN_OUT_BOTTOM_LEFT, 4, 2);

  seconds_label = lv_label_create(clock_panel);
  lv_obj_set_style_text_font(seconds_label, body_font(), 0);
  set_text_color(seconds_label, 0x627D98);
  lv_label_set_text(seconds_label, "unreliable");
  lv_obj_align_to(seconds_label, time_label, LV_ALIGN_OUT_RIGHT_MID, 10, 8);

  setup_hint_label = lv_label_create(clock_panel);
  lv_obj_set_style_text_font(setup_hint_label, body_font(), 0);
  set_text_color(setup_hint_label, 0x2563EB);
  lv_label_set_text(setup_hint_label, "Setup: time / Wi-Fi / format");
  lv_obj_align(setup_hint_label, LV_ALIGN_TOP_MID, 0, 14);
  lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(setup_hint_label, open_time_setup_event, LV_EVENT_CLICKED, nullptr);
  if (DeskClock::SettingsService::snapshot().configured) {
    lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_HIDDEN);
  }

  sync_dot = lv_obj_create(clock_panel);
  lv_obj_remove_style_all(sync_dot);
  lv_obj_set_size(sync_dot, 14, 14);
  lv_obj_set_style_radius(sync_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(sync_dot, lv_color_hex(0xEF4444), 0);
  lv_obj_set_style_bg_opa(sync_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(sync_dot, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(sync_dot, 2, 0);
  lv_obj_align(sync_dot, LV_ALIGN_TOP_RIGHT, -18, 18);

  next_alarm_label = lv_label_create(clock_panel);
  lv_obj_set_style_text_font(next_alarm_label, body_font(), 0);
  set_text_color(next_alarm_label, 0x52616F);
  lv_label_set_text(next_alarm_label, "Alarms: none");
  lv_obj_align(next_alarm_label, LV_ALIGN_BOTTOM_LEFT, 28, -12);
  lv_obj_add_flag(next_alarm_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(next_alarm_label, open_alarm_manager_event, LV_EVENT_CLICKED, nullptr);

  status_label = lv_label_create(clock_panel);
  lv_obj_set_style_text_font(status_label, body_font(), 0);
  set_text_color(status_label, 0x829AB1);
  lv_label_set_text(status_label, "time setup");
  lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -18, -12);
  lv_obj_add_flag(status_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status_label, open_time_setup_event, LV_EVENT_CLICKED, nullptr);

  DeskClock::BrightnessSettingsView::create(screen, width, height, body_font());

  DeskClock::TimeSetupView::create(screen, width, height, body_font());

  DeskClock::NetworkSetupView::create(screen, width, height, body_font());

  alarm_manager_panel = DeskClock::UiWidgets::modalPanel(screen, width, height);
  refresh_alarm_manager();

  DeskClock::AlarmAlertView::create(screen, width, height, time_font());

  lv_timer_create(update_clock_from_time_service, 250, nullptr);
  update_clock_from_time_service(nullptr);
}
