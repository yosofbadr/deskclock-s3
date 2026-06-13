#include "ClockFace.h"

#include <stdint.h>
#include <stdio.h>

#include "AlarmManagerView.h"
#include "AlarmAlertView.h"
#include "BrightnessSettingsView.h"
#include "ClockDisplayFormatter.h"
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

void realign_time_details()
{
  lv_obj_update_layout(time_label);
  lv_obj_align_to(date_label, time_label, LV_ALIGN_OUT_BOTTOM_LEFT, 4, 2);
  lv_obj_align_to(seconds_label, time_label, LV_ALIGN_OUT_RIGHT_MID, 10, 8);
}

void update_next_alarm_label(const DeskClock::DateTime &now)
{
  char buffer[40];
  DeskClock::ClockDisplayFormatter::nextAlarmText(buffer, sizeof(buffer), now);
  lv_label_set_text(next_alarm_label, buffer);
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
    lv_label_set_text(status_label, DeskClock::ClockDisplayFormatter::statusText(snapshot));
    lv_label_set_text(next_alarm_label, "Alarms: tap to add");
    lv_obj_set_style_bg_color(sync_dot, lv_color_hex(DeskClock::ClockDisplayFormatter::syncDotColor(DeskClock::SyncState::Unreliable, blink)), 0);
    DeskClock::AlarmAlertView::update(snapshot.now);
    realign_time_details();
    return;
  }

  char time_buffer[12];
  DeskClock::ClockDisplayFormatter::timeText(time_buffer, sizeof(time_buffer), snapshot.now.hour, snapshot.now.minute);
  lv_label_set_text(time_label, time_buffer);

  char date_buffer[20];
  DeskClock::ClockDisplayFormatter::dateText(date_buffer, sizeof(date_buffer), snapshot.now);
  lv_label_set_text(date_label, date_buffer);

  char seconds_buffer[16];
  snprintf(seconds_buffer, sizeof(seconds_buffer), ":%02u  local", snapshot.now.second);
  lv_label_set_text(seconds_label, seconds_buffer);

  lv_label_set_text(status_label, DeskClock::ClockDisplayFormatter::statusText(snapshot));
  update_next_alarm_label(snapshot.now);
  lv_obj_set_style_bg_color(sync_dot, lv_color_hex(DeskClock::ClockDisplayFormatter::syncDotColor(snapshot.sync_state, blink)), 0);
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
  lv_obj_add_event_cb(next_alarm_label, [](lv_event_t *) { DeskClock::AlarmManagerView::open(); }, LV_EVENT_CLICKED, nullptr);

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

  DeskClock::AlarmManagerView::create(screen, width, height, time_font());

  DeskClock::AlarmAlertView::create(screen, width, height, time_font());

  lv_timer_create(update_clock_from_time_service, 250, nullptr);
  update_clock_from_time_service(nullptr);
}
