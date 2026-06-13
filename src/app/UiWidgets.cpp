#include "UiWidgets.h"

namespace DeskClock {
namespace UiWidgets {

lv_obj_t *button(lv_obj_t *parent, const char *text, int32_t width, int32_t height)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_size(button, width, height);
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  return button;
}

lv_obj_t *modalPanel(lv_obj_t *parent, int32_t width, int32_t height)
{
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_size(panel, width - 28, height - 28);
  lv_obj_center(panel);
  lv_obj_set_style_radius(panel, 18, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0xCBD5E1), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  return panel;
}

void setTextColor(lv_obj_t *obj, uint32_t color)
{
  lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

} // namespace UiWidgets
} // namespace DeskClock
