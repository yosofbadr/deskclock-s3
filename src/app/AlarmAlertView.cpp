#include "AlarmAlertView.h"

#include <stdio.h>

#include "AlarmService.h"
#include "TimeService.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

lv_obj_t *alert_panel = nullptr;
lv_obj_t *alert_time_label = nullptr;

void dismiss_alert_event(lv_event_t *)
{
  AlarmService::dismissActiveAlert();
}

void snooze_alert_event(lv_event_t *)
{
  AlarmService::snoozeActiveAlert(TimeService::snapshot().now);
}

} // namespace

namespace AlarmAlertView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *title_font)
{
  alert_panel = lv_obj_create(parent);
  lv_obj_set_size(alert_panel, width - 32, height - 32);
  lv_obj_center(alert_panel);
  lv_obj_set_style_radius(alert_panel, 22, 0);
  lv_obj_set_style_bg_color(alert_panel, lv_color_hex(0xFEE2E2), 0);
  lv_obj_set_style_bg_opa(alert_panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(alert_panel, lv_color_hex(0xDC2626), 0);
  lv_obj_set_style_border_width(alert_panel, 4, 0);
  lv_obj_clear_flag(alert_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(alert_panel, LV_OBJ_FLAG_HIDDEN);

  alert_time_label = lv_label_create(alert_panel);
  lv_obj_set_style_text_font(alert_time_label, title_font, 0);
  UiWidgets::setTextColor(alert_time_label, 0x991B1B);
  lv_label_set_text(alert_time_label, "Alarm");
  lv_obj_align(alert_time_label, LV_ALIGN_CENTER, 0, -34);

  lv_obj_t *snooze_button = UiWidgets::button(alert_panel, "Snooze 10m", 128, 46);
  lv_obj_align(snooze_button, LV_ALIGN_BOTTOM_LEFT, 14, -14);
  lv_obj_add_event_cb(snooze_button, snooze_alert_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *dismiss_button = UiWidgets::button(alert_panel, "Dismiss", 128, 46);
  lv_obj_align(dismiss_button, LV_ALIGN_BOTTOM_RIGHT, -14, -14);
  lv_obj_add_event_cb(dismiss_button, dismiss_alert_event, LV_EVENT_CLICKED, nullptr);
}

void update(const DateTime &now)
{
  if (alert_panel == nullptr || alert_time_label == nullptr) {
    return;
  }

  ActiveAlarmAlert alert = AlarmService::activeAlert();
  if (!alert.active) {
    lv_obj_add_flag(alert_panel, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  char buffer[40];
  snprintf(buffer, sizeof(buffer), "Alarm %02u:%02u", alert.alarm.hour, alert.alarm.minute);
  lv_label_set_text(alert_time_label, buffer);
  lv_obj_clear_flag(alert_panel, LV_OBJ_FLAG_HIDDEN);

  if (now.valid && (now.second % 2U) == 0U) {
    lv_obj_set_style_bg_color(alert_panel, lv_color_hex(0xFEE2E2), 0);
  } else {
    lv_obj_set_style_bg_color(alert_panel, lv_color_hex(0xFFF7ED), 0);
  }
}

} // namespace AlarmAlertView
} // namespace DeskClock
