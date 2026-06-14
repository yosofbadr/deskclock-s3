#include "ClockFace.h"

#include <stdint.h>
#include <stdio.h>

#include "AlarmAlertView.h"
#include "AlarmManagerView.h"
#include "BrightnessSettingsView.h"
#include "ClockDisplayFormatter.h"
#include "NetworkSetupView.h"
#include "SettingsService.h"
#include "TimeService.h"
#include "TimeSetupView.h"
#include "UiWidgets.h"
#include "lvgl.h"

namespace {

struct VisualTheme {
  const char *name;
  uint32_t screen_bg;
  uint32_t stage_bg;
  uint32_t floor_bg;
  uint32_t window_bg;
  uint32_t card_bg;
  uint32_t card_border;
  uint32_t text;
  uint32_t muted;
  uint32_t faint;
  uint32_t accent;
  uint32_t accent_2;
  uint32_t mascot;
  uint32_t mascot_detail;
};

constexpr VisualTheme kThemes[] = {
    {"meadow", 0xF7F1E3, 0xFBF4E6, 0xEBDCC4, 0xFFF8E8, 0xFFFDF7, 0xE7D7BB, 0x293241, 0x6B7280, 0xAFA38E, 0xD97706, 0x2F5F8F, 0xFFFFFF, 0xE58A3A},
    {"blush", 0xFFF1F4, 0xFFF8F4, 0xF7D9D9, 0xFFFDFB, 0xFFFDFC, 0xF3C8D2, 0x51313A, 0x8D5D6B, 0xC9A3AC, 0xD84E6C, 0xB95C7B, 0xFFFFFF, 0xE96B84},
    {"dusk", 0x0B1020, 0x15172C, 0x211B33, 0x0F1630, 0x181A31, 0x3D355A, 0xF7F2FF, 0xB9A8D9, 0x6D5B8F, 0x9B5DE5, 0xF15BB5, 0xF5EEFF, 0xC084FC},
};

constexpr uint8_t kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);

lv_obj_t *root_screen = nullptr;
lv_obj_t *left_date_card = nullptr;
lv_obj_t *calendar_card = nullptr;
lv_obj_t *stage_card = nullptr;
lv_obj_t *right_status_card = nullptr;
lv_obj_t *right_alarm_card = nullptr;
lv_obj_t *right_message_card = nullptr;
lv_obj_t *floor_panel = nullptr;
lv_obj_t *window_panel = nullptr;
lv_obj_t *window_glow = nullptr;
lv_obj_t *plant_pot = nullptr;
lv_obj_t *plant_leaf_left = nullptr;
lv_obj_t *plant_leaf_right = nullptr;
lv_obj_t *mascot_ear_left = nullptr;
lv_obj_t *mascot_ear_right = nullptr;
lv_obj_t *mascot_head = nullptr;
lv_obj_t *mascot_eye_left = nullptr;
lv_obj_t *mascot_eye_right = nullptr;
lv_obj_t *mascot_mouth = nullptr;
lv_obj_t *sync_dot = nullptr;

lv_obj_t *time_label = nullptr;
lv_obj_t *ampm_label = nullptr;
lv_obj_t *seconds_label = nullptr;
lv_obj_t *greeting_label = nullptr;
lv_obj_t *setup_hint_label = nullptr;
lv_obj_t *weekday_label = nullptr;
lv_obj_t *month_label = nullptr;
lv_obj_t *day_label = nullptr;
lv_obj_t *calendar_month_label = nullptr;
lv_obj_t *calendar_weekday_labels[7] = {};
lv_obj_t *calendar_day_labels[42] = {};
lv_obj_t *status_label = nullptr;
lv_obj_t *weather_label = nullptr;
lv_obj_t *next_alarm_label = nullptr;
lv_obj_t *message_label = nullptr;
lv_obj_t *theme_name_label = nullptr;

const lv_font_t *time_font()
{
#if LV_FONT_MONTSERRAT_48
  return &lv_font_montserrat_48;
#else
  return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *large_font()
{
#if LV_FONT_MONTSERRAT_28
  return &lv_font_montserrat_28;
#elif LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
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

const lv_font_t *small_font()
{
#if LV_FONT_MONTSERRAT_12
  return &lv_font_montserrat_12;
#else
  return LV_FONT_DEFAULT;
#endif
}

const VisualTheme &theme()
{
  const DeskClock::SettingsSnapshot settings = DeskClock::SettingsService::snapshot();
  return kThemes[settings.theme_index % kThemeCount];
}

void set_text_color(lv_obj_t *obj, uint32_t color)
{
  if (obj != nullptr) {
    DeskClock::UiWidgets::setTextColor(obj, color);
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

const char *weekday_name(uint8_t week)
{
  static constexpr const char *names[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  return week < 7 ? names[week] : "DAY";
}

const char *month_name(uint8_t month)
{
  static constexpr const char *names[] = {"---", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  return month <= 12 ? names[month] : "---";
}

const char *greeting_for_hour(uint8_t hour)
{
  if (hour < 5) {
    return "Rest well";
  }
  if (hour < 12) {
    return "Good morning";
  }
  if (hour < 18) {
    return "Good afternoon";
  }
  return "Good evening";
}

lv_obj_t *plain_obj(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, int32_t height)
{
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, width, height);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

lv_obj_t *card(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, int32_t height)
{
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, width, height);
  lv_obj_set_style_radius(obj, 16, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

void style_card(lv_obj_t *obj, uint32_t bg, uint32_t border)
{
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
}

void style_soft_shape(lv_obj_t *obj, uint32_t bg, int32_t radius)
{
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
}

void format_primary_time(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
  if (DeskClock::TimeSetupView::use24HourFormat()) {
    snprintf(buffer, size, "%02u:%02u", hour, minute);
    return;
  }

  uint8_t display_hour = hour % 12U;
  if (display_hour == 0) {
    display_hour = 12;
  }
  snprintf(buffer, size, "%u:%02u", display_hour, minute);
}

void realign_time_details()
{
  if (time_label == nullptr || ampm_label == nullptr || seconds_label == nullptr) {
    return;
  }
  lv_obj_update_layout(time_label);
  lv_obj_align(time_label, LV_ALIGN_CENTER, -10, -18);
  lv_obj_align_to(ampm_label, time_label, LV_ALIGN_OUT_RIGHT_MID, 6, -16);
  lv_obj_align_to(seconds_label, ampm_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
}

void update_next_alarm_label(const DeskClock::DateTime &now)
{
  char buffer[44];
  DeskClock::ClockDisplayFormatter::nextAlarmText(buffer, sizeof(buffer), now);
  lv_label_set_text(next_alarm_label, buffer);
}

void update_calendar_cards(const DeskClock::DateTime &now)
{
  const VisualTheme &t = theme();
  if (!now.valid) {
    lv_label_set_text(weekday_label, "SET TIME");
    lv_label_set_text(month_label, "CLOCK");
    lv_label_set_text(day_label, "--");
    lv_label_set_text(calendar_month_label, "calendar");
    for (lv_obj_t *cell : calendar_day_labels) {
      lv_label_set_text(cell, "");
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    }
    return;
  }

  lv_label_set_text(weekday_label, weekday_name(now.week));
  lv_label_set_text(month_label, month_name(now.month));
  char day_buffer[4];
  snprintf(day_buffer, sizeof(day_buffer), "%u", now.day);
  lv_label_set_text(day_label, day_buffer);

  char month_buffer[16];
  snprintf(month_buffer, sizeof(month_buffer), "%s %04u", month_name(now.month), now.year);
  lv_label_set_text(calendar_month_label, month_buffer);

  const uint8_t month_days = days_in_month(now.year, now.month);
  const uint8_t first_weekday = static_cast<uint8_t>((now.week + 7U - ((now.day - 1U) % 7U)) % 7U);
  for (uint8_t index = 0; index < 42; ++index) {
    const int16_t day_number = static_cast<int16_t>(index) - static_cast<int16_t>(first_weekday) + 1;
    lv_obj_t *cell = calendar_day_labels[index];
    if (day_number < 1 || day_number > month_days) {
      lv_label_set_text(cell, "");
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
      continue;
    }

    char buffer[4];
    snprintf(buffer, sizeof(buffer), "%d", day_number);
    lv_label_set_text(cell, buffer);
    const bool selected = day_number == now.day;
    lv_obj_set_style_radius(cell, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cell, lv_color_hex(t.accent), 0);
    lv_obj_set_style_bg_opa(cell, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    set_text_color(cell, selected ? t.card_bg : t.text);
  }
}

void open_time_setup_event(lv_event_t *)
{
  DeskClock::TimeSetupView::open();
}

void open_brightness_event(lv_event_t *)
{
  DeskClock::BrightnessSettingsView::open();
}

void open_alarms_event(lv_event_t *)
{
  DeskClock::AlarmManagerView::open();
}

void apply_theme_to_static_objects()
{
  const VisualTheme &t = theme();
  if (root_screen == nullptr) {
    return;
  }

  lv_obj_set_style_bg_color(root_screen, lv_color_hex(t.screen_bg), 0);
  lv_obj_set_style_bg_opa(root_screen, LV_OPA_COVER, 0);

  style_card(left_date_card, t.card_bg, t.card_border);
  style_card(calendar_card, t.card_bg, t.card_border);
  style_card(stage_card, t.stage_bg, t.card_border);
  style_card(right_status_card, t.card_bg, t.card_border);
  style_card(right_alarm_card, t.card_bg, t.card_border);
  style_card(right_message_card, t.card_bg, t.card_border);

  style_soft_shape(floor_panel, t.floor_bg, 0);
  style_soft_shape(window_panel, t.window_bg, 10);
  style_soft_shape(window_glow, t.card_bg, 12);
  style_soft_shape(plant_pot, t.accent, 6);
  style_soft_shape(plant_leaf_left, t.accent_2, LV_RADIUS_CIRCLE);
  style_soft_shape(plant_leaf_right, t.accent_2, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_ear_left, t.mascot, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_ear_right, t.mascot, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_head, t.mascot, 24);
  style_soft_shape(mascot_eye_left, t.text, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_eye_right, t.text, LV_RADIUS_CIRCLE);

  set_text_color(time_label, t.accent_2);
  set_text_color(ampm_label, t.accent);
  set_text_color(seconds_label, t.accent_2);
  set_text_color(greeting_label, t.text);
  set_text_color(setup_hint_label, t.accent);
  set_text_color(weekday_label, t.accent);
  set_text_color(month_label, t.text);
  set_text_color(day_label, t.text);
  set_text_color(calendar_month_label, t.text);
  set_text_color(status_label, t.muted);
  set_text_color(weather_label, t.text);
  set_text_color(next_alarm_label, t.text);
  set_text_color(message_label, t.text);
  set_text_color(theme_name_label, t.faint);
  set_text_color(mascot_mouth, t.text);

  for (lv_obj_t *label : calendar_weekday_labels) {
    set_text_color(label, t.faint);
  }
  for (lv_obj_t *label : calendar_day_labels) {
    set_text_color(label, t.text);
  }

  if (setup_hint_label != nullptr) {
    lv_obj_set_style_bg_color(setup_hint_label, lv_color_hex(t.card_bg), 0);
    lv_obj_set_style_bg_opa(setup_hint_label, LV_OPA_80, 0);
    lv_obj_set_style_radius(setup_hint_label, 10, 0);
    lv_obj_set_style_pad_hor(setup_hint_label, 8, 0);
    lv_obj_set_style_pad_ver(setup_hint_label, 3, 0);
  }
}

void update_clock_from_time_service(lv_timer_t *)
{
  const DeskClock::TimeSnapshot snapshot = DeskClock::TimeService::snapshot();
  const bool blink = snapshot.now.valid ? ((snapshot.now.second % 2U) == 0U) : true;
  const VisualTheme &t = theme();

  if (theme_name_label != nullptr) {
    char theme_buffer[24];
    snprintf(theme_buffer, sizeof(theme_buffer), "%s theme", t.name);
    lv_label_set_text(theme_name_label, theme_buffer);
  }
  if (setup_hint_label != nullptr) {
    if (DeskClock::SettingsService::snapshot().configured) {
      lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(setup_hint_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (!snapshot.now.valid) {
    lv_label_set_text(time_label, "--:--");
    lv_label_set_text(ampm_label, "");
    lv_label_set_text(seconds_label, "set");
    lv_label_set_text(greeting_label, "Set the time to start");
    lv_label_set_text(status_label, DeskClock::ClockDisplayFormatter::statusText(snapshot));
    lv_label_set_text(weather_label, "Weather\nopt-in");
    lv_label_set_text(next_alarm_label, "Tap to add alarm");
    lv_label_set_text(message_label, "Hold BOOT\nfor setup");
    lv_obj_set_style_bg_color(sync_dot, lv_color_hex(DeskClock::ClockDisplayFormatter::syncDotColor(DeskClock::SyncState::Unreliable, blink)), 0);
    update_calendar_cards(snapshot.now);
    DeskClock::AlarmAlertView::update(snapshot.now);
    realign_time_details();
    return;
  }

  char time_buffer[12];
  format_primary_time(time_buffer, sizeof(time_buffer), snapshot.now.hour, snapshot.now.minute);
  lv_label_set_text(time_label, time_buffer);

  if (DeskClock::TimeSetupView::use24HourFormat()) {
    lv_label_set_text(ampm_label, "");
  } else {
    lv_label_set_text(ampm_label, snapshot.now.hour >= 12 ? "PM" : "AM");
  }

  char seconds_buffer[8];
  snprintf(seconds_buffer, sizeof(seconds_buffer), ":%02u", snapshot.now.second);
  lv_label_set_text(seconds_label, seconds_buffer);

  char greeting_buffer[48];
  snprintf(greeting_buffer, sizeof(greeting_buffer), "* %s", greeting_for_hour(snapshot.now.hour));
  lv_label_set_text(greeting_label, greeting_buffer);

  lv_label_set_text(status_label, DeskClock::ClockDisplayFormatter::statusText(snapshot));
  lv_label_set_text(weather_label, "Weather\n--°");
  lv_label_set_text(message_label, "You got this!\nsettings");
  update_next_alarm_label(snapshot.now);
  update_calendar_cards(snapshot.now);
  lv_obj_set_style_bg_color(sync_dot, lv_color_hex(DeskClock::ClockDisplayFormatter::syncDotColor(snapshot.sync_state, blink)), 0);

  DeskClock::AlarmAlertView::update(snapshot.now);
  realign_time_details();
}

} // namespace

extern "C" void clock_face_refresh_theme(void)
{
  apply_theme_to_static_objects();
  update_clock_from_time_service(nullptr);
}

extern "C" void clock_face_create(void)
{
  DeskClock::TimeSetupView::loadPreferences();
  root_screen = lv_screen_active();
  lv_obj_clean(root_screen);

  const int32_t width = lv_display_get_horizontal_resolution(nullptr);
  const int32_t height = lv_display_get_vertical_resolution(nullptr);
  const int32_t margin = 8;
  const int32_t gap = 8;
  const int32_t left_width = 108;
  const int32_t right_width = 132;
  const int32_t right_x = width - margin - right_width;
  const int32_t center_x = margin + left_width + gap;
  const int32_t center_width = right_x - gap - center_x;
  const int32_t content_height = height - (margin * 2);

  left_date_card = card(root_screen, margin, margin, left_width, 56);
  calendar_card = card(root_screen, margin, margin + 62, left_width, content_height - 62);
  stage_card = card(root_screen, center_x, margin, center_width, content_height);
  right_status_card = card(root_screen, right_x, margin, right_width, 48);
  right_alarm_card = card(root_screen, right_x, margin + 54, right_width, 48);
  right_message_card = card(root_screen, right_x + 12, margin + 108, right_width - 12, content_height - 108);

  lv_obj_add_flag(right_alarm_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(right_alarm_card, open_alarms_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(right_message_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(right_message_card, open_brightness_event, LV_EVENT_CLICKED, nullptr);

  window_panel = plain_obj(stage_card, 78, 12, center_width - 148, 86);
  window_glow = plain_obj(stage_card, 94, 24, center_width - 180, 56);
  floor_panel = plain_obj(stage_card, 0, content_height - 48, center_width, 48);

  // Lightweight decorative desk/plant/mascot shapes. They keep the bundled
  // firmware themeable without shipping branded character artwork.
  plant_pot = plain_obj(stage_card, 34, content_height - 48, 28, 22);
  plant_leaf_left = plain_obj(stage_card, 26, content_height - 70, 24, 14);
  plant_leaf_right = plain_obj(stage_card, 48, content_height - 72, 24, 14);
  mascot_ear_left = plain_obj(stage_card, center_width - 70, content_height - 78, 16, 44);
  mascot_ear_right = plain_obj(stage_card, center_width - 44, content_height - 78, 16, 44);
  mascot_head = plain_obj(stage_card, center_width - 80, content_height - 48, 62, 42);
  mascot_eye_left = plain_obj(mascot_head, 18, 18, 6, 6);
  mascot_eye_right = plain_obj(mascot_head, 39, 18, 6, 6);
  mascot_mouth = lv_label_create(mascot_head);
  lv_obj_set_style_text_font(mascot_mouth, small_font(), 0);
  lv_label_set_text(mascot_mouth, "x");
  lv_obj_align(mascot_mouth, LV_ALIGN_CENTER, 0, 8);

  weekday_label = lv_label_create(left_date_card);
  lv_obj_set_style_text_font(weekday_label, small_font(), 0);
  lv_label_set_text(weekday_label, "WEDNESDAY");
  lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 0, 8);

  month_label = lv_label_create(left_date_card);
  lv_obj_set_style_text_font(month_label, small_font(), 0);
  lv_label_set_text(month_label, "MAY");
  lv_obj_align(month_label, LV_ALIGN_TOP_MID, 0, 25);
  lv_obj_add_flag(month_label, LV_OBJ_FLAG_HIDDEN);

  day_label = lv_label_create(left_date_card);
  lv_obj_set_style_text_font(day_label, large_font(), 0);
  lv_label_set_text(day_label, "22");
  lv_obj_align(day_label, LV_ALIGN_BOTTOM_MID, 0, 2);

  calendar_month_label = lv_label_create(calendar_card);
  lv_obj_set_style_text_font(calendar_month_label, small_font(), 0);
  lv_label_set_text(calendar_month_label, "MAY 2024");
  lv_obj_align(calendar_month_label, LV_ALIGN_TOP_MID, 0, 5);

  static constexpr const char *weekdays[] = {"S", "M", "T", "W", "T", "F", "S"};
  for (uint8_t index = 0; index < 7; ++index) {
    calendar_weekday_labels[index] = lv_label_create(calendar_card);
    lv_obj_set_style_text_font(calendar_weekday_labels[index], small_font(), 0);
    lv_label_set_text(calendar_weekday_labels[index], weekdays[index]);
    lv_obj_set_size(calendar_weekday_labels[index], 13, 12);
    lv_obj_set_style_text_align(calendar_weekday_labels[index], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(calendar_weekday_labels[index], 8 + (index * 13), 21);
  }

  for (uint8_t row = 0; row < 6; ++row) {
    for (uint8_t column = 0; column < 7; ++column) {
      const uint8_t index = static_cast<uint8_t>((row * 7U) + column);
      calendar_day_labels[index] = lv_label_create(calendar_card);
      lv_obj_set_style_text_font(calendar_day_labels[index], small_font(), 0);
      lv_obj_set_size(calendar_day_labels[index], 13, 12);
      lv_obj_set_style_text_align(calendar_day_labels[index], LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(calendar_day_labels[index], 8 + (column * 13), 34 + (row * 10));
      lv_label_set_text(calendar_day_labels[index], "");
    }
  }

  time_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(time_label, time_font(), 0);
  lv_label_set_text(time_label, "--:--");
  lv_obj_align(time_label, LV_ALIGN_CENTER, -10, -18);

  ampm_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(ampm_label, body_font(), 0);
  lv_label_set_text(ampm_label, "");

  seconds_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(seconds_label, body_font(), 0);
  lv_label_set_text(seconds_label, "set");

  greeting_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(greeting_label, body_font(), 0);
  lv_label_set_text(greeting_label, "* Good morning");
  lv_obj_align(greeting_label, LV_ALIGN_BOTTOM_MID, -8, -24);

  setup_hint_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(setup_hint_label, small_font(), 0);
  lv_label_set_text(setup_hint_label, "setup time / Wi-Fi");
  lv_obj_align(setup_hint_label, LV_ALIGN_TOP_MID, 0, 7);
  lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(setup_hint_label, open_time_setup_event, LV_EVENT_CLICKED, nullptr);
  if (DeskClock::SettingsService::snapshot().configured) {
    lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_HIDDEN);
  }

  sync_dot = plain_obj(right_status_card, right_width - 22, 10, 10, 10);
  lv_obj_set_style_radius(sync_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(sync_dot, LV_OPA_COVER, 0);

  weather_label = lv_label_create(right_status_card);
  lv_obj_set_style_text_font(weather_label, small_font(), 0);
  lv_label_set_text(weather_label, "Weather\n--°");
  lv_obj_align(weather_label, LV_ALIGN_LEFT_MID, 12, 1);

  status_label = lv_label_create(right_status_card);
  lv_obj_set_style_text_font(status_label, small_font(), 0);
  lv_label_set_text(status_label, "time setup");
  lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -12, -5);
  lv_obj_add_flag(status_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status_label, open_time_setup_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *alarm_title = lv_label_create(right_alarm_card);
  lv_obj_set_style_text_font(alarm_title, small_font(), 0);
  lv_label_set_text(alarm_title, "NEXT ALARM");
  lv_obj_align(alarm_title, LV_ALIGN_TOP_LEFT, 12, 7);

  next_alarm_label = lv_label_create(right_alarm_card);
  lv_obj_set_style_text_font(next_alarm_label, small_font(), 0);
  lv_obj_set_width(next_alarm_label, right_width - 22);
  lv_label_set_long_mode(next_alarm_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(next_alarm_label, "Tap to add alarm");
  lv_obj_align(next_alarm_label, LV_ALIGN_BOTTOM_LEFT, 12, -7);
  lv_obj_add_flag(next_alarm_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(next_alarm_label, open_alarms_event, LV_EVENT_CLICKED, nullptr);

  message_label = lv_label_create(right_message_card);
  lv_obj_set_style_text_font(message_label, small_font(), 0);
  lv_label_set_text(message_label, "You got this!\nsettings");
  lv_obj_align(message_label, LV_ALIGN_LEFT_MID, 12, 0);

  theme_name_label = lv_label_create(right_message_card);
  lv_obj_set_style_text_font(theme_name_label, small_font(), 0);
  lv_label_set_text(theme_name_label, "theme");
  lv_obj_align(theme_name_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);

  apply_theme_to_static_objects();
  realign_time_details();

  DeskClock::BrightnessSettingsView::create(root_screen, width, height, body_font());
  DeskClock::TimeSetupView::create(root_screen, width, height, body_font());
  DeskClock::NetworkSetupView::create(root_screen, width, height, body_font());
  DeskClock::AlarmManagerView::create(root_screen, width, height, time_font());
  DeskClock::AlarmAlertView::create(root_screen, width, height, time_font());

  lv_timer_create(update_clock_from_time_service, 250, nullptr);
  update_clock_from_time_service(nullptr);
}
