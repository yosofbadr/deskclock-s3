#pragma once

#include <stdint.h>

#include "lvgl.h"

namespace DeskClock {
namespace UiWidgets {

lv_obj_t *button(lv_obj_t *parent, const char *text, int32_t width, int32_t height);
lv_obj_t *modalPanel(lv_obj_t *parent, int32_t width, int32_t height);
void setTextColor(lv_obj_t *obj, uint32_t color);

} // namespace UiWidgets
} // namespace DeskClock
