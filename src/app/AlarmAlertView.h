#pragma once

#include "TimeService.h"
#include "lvgl.h"

namespace DeskClock {
namespace AlarmAlertView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *title_font);
void update(const DateTime &now);

} // namespace AlarmAlertView
} // namespace DeskClock
