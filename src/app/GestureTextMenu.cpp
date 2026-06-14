#include "GestureTextMenu.h"

#include <Arduino.h>
#include <stdint.h>
#include <stdlib.h>

namespace DeskClock {
namespace GestureTextMenu {
namespace {

constexpr int16_t kSwipeThresholdPx = 32;
constexpr int16_t kAxisBiasPx = 10;
constexpr int16_t kTapSlopPx = 18;

struct HandlerContext {
  TouchState *state = nullptr;
  Callback callback = nullptr;
  void *user_data = nullptr;
};

void get_pointer(lv_point_t &point)
{
  lv_indev_t *indev = lv_indev_active();
  if (indev == nullptr) {
    point.x = 0;
    point.y = 0;
    return;
  }
  lv_indev_get_point(indev, &point);
}

Input classify_drag(const lv_point_t &start, const lv_point_t &end)
{
  const int16_t dx = static_cast<int16_t>(end.x - start.x);
  const int16_t dy = static_cast<int16_t>(end.y - start.y);
  const int16_t abs_dx = static_cast<int16_t>(abs(dx));
  const int16_t abs_dy = static_cast<int16_t>(abs(dy));

  if (abs_dx <= kTapSlopPx && abs_dy <= kTapSlopPx) {
    return Input::Tap;
  }
  if (abs_dy >= kSwipeThresholdPx && abs_dy > abs_dx + kAxisBiasPx) {
    return dy > 0 ? Input::Down : Input::Up;
  }
  if (abs_dx >= kSwipeThresholdPx && abs_dx > abs_dy + kAxisBiasPx) {
    return dx > 0 ? Input::Right : Input::Left;
  }
  return Input::None;
}

void event_handler(lv_event_t *event)
{
  auto *context = static_cast<HandlerContext *>(lv_event_get_user_data(event));
  if (context == nullptr || context->state == nullptr) {
    return;
  }

  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_DELETE) {
    delete context;
    return;
  }

  if (code == LV_EVENT_PRESSED) {
    get_pointer(context->state->start);
    context->state->last = context->state->start;
    context->state->active = true;
    return;
  }

  if (code == LV_EVENT_PRESSING && context->state->active) {
    get_pointer(context->state->last);
    return;
  }

  if (code == LV_EVENT_PRESS_LOST) {
    context->state->active = false;
    return;
  }

  if (code != LV_EVENT_RELEASED || !context->state->active) {
    return;
  }

  get_pointer(context->state->last);
  const Input input = classify_drag(context->state->start, context->state->last);
  context->state->active = false;

  if (input != Input::None && context->callback != nullptr) {
    context->callback(input, context->user_data);
  }
}

} // namespace

void attach(lv_obj_t *obj, TouchState *state, Callback callback, void *user_data)
{
  if (obj == nullptr || state == nullptr) {
    return;
  }

  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

  auto *context = new HandlerContext;
  context->state = state;
  context->callback = callback;
  context->user_data = user_data;
  lv_obj_add_event_cb(obj, event_handler, LV_EVENT_PRESSED, context);
  lv_obj_add_event_cb(obj, event_handler, LV_EVENT_PRESSING, context);
  lv_obj_add_event_cb(obj, event_handler, LV_EVENT_RELEASED, context);
  lv_obj_add_event_cb(obj, event_handler, LV_EVENT_PRESS_LOST, context);
  lv_obj_add_event_cb(obj, event_handler, LV_EVENT_DELETE, context);
}

void reset(TouchState *state)
{
  if (state != nullptr) {
    state->active = false;
    state->start = {0, 0};
    state->last = {0, 0};
  }
}

uint8_t wrapIndex(uint8_t selected, int8_t delta, uint8_t count)
{
  if (count == 0) {
    return 0;
  }
  int16_t next = static_cast<int16_t>(selected) + delta;
  while (next < 0) {
    next += count;
  }
  return static_cast<uint8_t>(next % count);
}

uint8_t firstVisibleIndex(uint8_t selected, uint8_t count, uint8_t visible_count)
{
  if (count <= visible_count) {
    return 0;
  }

  const uint8_t half = static_cast<uint8_t>(visible_count / 2U);
  int16_t first = static_cast<int16_t>(selected) - half;
  if (first < 0) {
    first = 0;
  }
  const uint8_t max_first = static_cast<uint8_t>(count - visible_count);
  if (first > max_first) {
    first = max_first;
  }
  return static_cast<uint8_t>(first);
}

} // namespace GestureTextMenu
} // namespace DeskClock
