#pragma once

#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

namespace DeskClock {
namespace TimeSetupView {

void loadPreferences();
bool use24HourFormat();
void formatTime(char *buffer, size_t size, uint8_t hour, uint8_t minute);
void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font);
void open();
bool isOpen();
void moveSelection(int8_t delta);
void adjustSelected(int8_t delta);
void activateSelected();

} // namespace TimeSetupView
} // namespace DeskClock
