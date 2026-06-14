#pragma once

#include <stdint.h>
#include "lvgl.h"

namespace DeskClock {
namespace SystemMenuView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font);
void open();
void close();
bool isOpen();
void moveSelection(int8_t delta);
void adjustSelected(int8_t delta);
void activateSelected();

} // namespace SystemMenuView
} // namespace DeskClock
