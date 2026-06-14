#pragma once

#include "lvgl.h"

namespace DeskClock {
namespace BrightnessSettingsView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font);
void open();
bool isOpen();
void moveSelection(int8_t delta);
void adjustSelected(int8_t delta);
void activateSelected();

} // namespace BrightnessSettingsView
} // namespace DeskClock
