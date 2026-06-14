#pragma once

#include "lvgl.h"

namespace DeskClock {
namespace GestureTextMenu {

enum class Input {
  None,
  Tap,
  Up,
  Down,
  Left,
  Right,
};

struct TouchState {
  bool active = false;
  bool emitted_swipe = false;
  lv_point_t start = {0, 0};
  lv_point_t last = {0, 0};
};

using Callback = void (*)(Input input, void *user_data);

void attach(lv_obj_t *obj, TouchState *state, Callback callback, void *user_data);
void reset(TouchState *state);

uint8_t wrapIndex(uint8_t selected, int8_t delta, uint8_t count);
uint8_t firstVisibleIndex(uint8_t selected, uint8_t count, uint8_t visible_count);

} // namespace GestureTextMenu
} // namespace DeskClock
