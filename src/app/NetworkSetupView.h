#pragma once

#include "lvgl.h"

namespace DeskClock {
namespace NetworkSetupView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font);
void open();

} // namespace NetworkSetupView
} // namespace DeskClock
