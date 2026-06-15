#include "SystemMenuView.h"

#include <Arduino.h>
#include <stdio.h>

#include "AlarmManagerView.h"
#include "AlarmToneService.h"
#include "BoardPowerService.h"
#include "BrightnessService.h"
#include "BrightnessSettingsView.h"
#include "GestureTextMenu.h"
#include "NetworkService.h"
#include "NetworkSetupView.h"
#include "SettingsService.h"
#include "TimeSetupView.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

constexpr uint8_t kMenuItemCount = 9;
constexpr uint8_t kVisibleRows = 5;
constexpr int32_t kRowHeight = 26;
constexpr int32_t kRowsTop = 38;

lv_obj_t *panel = nullptr;
lv_obj_t *title_label = nullptr;
lv_obj_t *hint_label = nullptr;
lv_obj_t *rows[kVisibleRows] = {};
const lv_font_t *menu_font = nullptr;
uint8_t selected_index = 1;
GestureTextMenu::TouchState touch_state;

void cycle_brightness()
{
  BrightnessService::cyclePreset(1);
}

void adjust_brightness(int8_t delta)
{
  BrightnessService::cyclePreset(delta);
}

void adjust_timezone(int8_t delta)
{
  const SettingsSnapshot settings = SettingsService::snapshot();
  const uint8_t count = SettingsService::timezoneCount();
  if (count == 0) {
    return;
  }
  const uint8_t next = GestureTextMenu::wrapIndex(settings.timezone_index, delta, count);
  SettingsService::setTimezoneIndex(next);
}

void shutdown_board()
{
  Serial.println("SystemMenuView: power off requested");
  BoardPowerService::releaseBatteryPowerHold();
}

void format_item(uint8_t index, char *buffer, size_t size)
{
  SettingsSnapshot settings = SettingsService::snapshot();
  NetworkSnapshot network = NetworkService::snapshot();
  AlarmToneSettings tone = AlarmToneService::settings();

  switch (index) {
  case 0:
    snprintf(buffer, size, "Back to clock");
    break;
  case 1:
    snprintf(buffer, size, "Time setup / format");
    break;
  case 2:
    snprintf(buffer, size, "Wi-Fi       %s", network.status);
    break;
  case 3:
    snprintf(buffer, size, "Timezone   %s", settings.timezone_label);
    break;
  case 4:
    snprintf(buffer, size, "Alarms");
    break;
  case 5:
    snprintf(buffer, size, "Display / audio");
    break;
  case 6:
    snprintf(buffer, size, "Brightness %u", BrightnessService::currentBrightness());
    break;
  case 7:
    snprintf(buffer, size, "Theme      %u/%u in display", static_cast<unsigned>(settings.theme_index + 1U), static_cast<unsigned>(SettingsService::themeCount()));
    break;
  case 8:
    snprintf(buffer, size, "Power off  audio %s", tone.enabled ? "on" : "off");
    break;
  default:
    snprintf(buffer, size, "--");
    break;
  }
}

void set_row_text(uint8_t slot, uint8_t item_index, bool selected)
{
  if (rows[slot] == nullptr) {
    return;
  }
  char item[96];
  char line[112];
  format_item(item_index, item, sizeof(item));
  snprintf(line, sizeof(line), "%s %.96s", selected ? ">" : " ", item);
  lv_label_set_text(rows[slot], line);
  lv_obj_set_style_text_color(rows[slot], lv_color_hex(selected ? 0xFFFFFF : 0xCBD5E1), 0);
  lv_obj_set_style_bg_color(rows[slot], lv_color_hex(selected ? 0x334155 : 0x020617), 0);
  lv_obj_set_style_bg_opa(rows[slot], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(rows[slot], 2, 0);
  lv_obj_set_style_pad_left(rows[slot], 8, 0);
}

void refresh()
{
  if (panel == nullptr) {
    return;
  }

  if (selected_index >= kMenuItemCount) {
    selected_index = 0;
  }

  const uint8_t first = GestureTextMenu::firstVisibleIndex(selected_index, kMenuItemCount, kVisibleRows);
  for (uint8_t slot = 0; slot < kVisibleRows; ++slot) {
    const uint8_t item_index = static_cast<uint8_t>(first + slot);
    if (item_index < kMenuItemCount) {
      lv_obj_clear_flag(rows[slot], LV_OBJ_FLAG_HIDDEN);
      set_row_text(slot, item_index, item_index == selected_index);
    } else {
      lv_obj_add_flag(rows[slot], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void handle_menu_input(GestureTextMenu::Input input, void *)
{
  switch (input) {
  case GestureTextMenu::Input::Up:
    DeskClock::SystemMenuView::moveSelection(-1);
    break;
  case GestureTextMenu::Input::Down:
    DeskClock::SystemMenuView::moveSelection(1);
    break;
  case GestureTextMenu::Input::Left:
    DeskClock::SystemMenuView::adjustSelected(-1);
    break;
  case GestureTextMenu::Input::Right:
    DeskClock::SystemMenuView::adjustSelected(1);
    break;
  case GestureTextMenu::Input::Tap:
    DeskClock::SystemMenuView::activateSelected();
    break;
  case GestureTextMenu::Input::None:
  default:
    break;
  }
}

} // namespace

namespace SystemMenuView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font)
{
  menu_font = font != nullptr ? font : LV_FONT_DEFAULT;
  panel = UiWidgets::fullScreenPanel(parent, width, height);
  GestureTextMenu::attach(panel, &touch_state, handle_menu_input, nullptr);

  title_label = lv_label_create(panel);
  lv_obj_set_style_text_font(title_label, menu_font, 0);
  lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(title_label, "Settings");
  lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 22, 8);

  hint_label = lv_label_create(panel);
  lv_obj_set_style_text_font(hint_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(hint_label, lv_color_hex(0x94A3B8), 0);
  lv_label_set_text(hint_label, "swipe select  left/right edit  tap/BOOT enter");
  lv_obj_align(hint_label, LV_ALIGN_TOP_RIGHT, -22, 13);

  for (uint8_t slot = 0; slot < kVisibleRows; ++slot) {
    rows[slot] = lv_label_create(panel);
    lv_obj_set_style_text_font(rows[slot], menu_font, 0);
    lv_obj_set_size(rows[slot], width - 44, kRowHeight - 2);
    lv_obj_set_pos(rows[slot], 22, kRowsTop + (slot * kRowHeight));
    lv_label_set_long_mode(rows[slot], LV_LABEL_LONG_CLIP);
  }
  refresh();
}

void open()
{
  if (panel == nullptr) {
    return;
  }
  selected_index = selected_index >= kMenuItemCount ? 1 : selected_index;
  GestureTextMenu::reset(&touch_state);
  refresh();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
}

void close()
{
  GestureTextMenu::reset(&touch_state);
  if (panel != nullptr) {
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  }
}

bool isOpen()
{
  return panel != nullptr && !lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void moveSelection(int8_t delta)
{
  if (!isOpen()) {
    return;
  }
  selected_index = GestureTextMenu::wrapIndex(selected_index, delta, kMenuItemCount);
  refresh();
}

void adjustSelected(int8_t delta)
{
  if (!isOpen()) {
    return;
  }

  switch (selected_index) {
  case 3:
    adjust_timezone(delta);
    break;
  case 6:
    adjust_brightness(delta);
    break;
  case 7:
    (void)delta;
    break;
  default:
    break;
  }
  refresh();
}

void activateSelected()
{
  if (!isOpen()) {
    open();
    return;
  }

  switch (selected_index) {
  case 0:
    close();
    break;
  case 1:
    close();
    TimeSetupView::open();
    break;
  case 2:
    close();
    NetworkSetupView::open();
    break;
  case 3:
    adjust_timezone(1);
    refresh();
    break;
  case 4:
    close();
    AlarmManagerView::open();
    break;
  case 5:
    close();
    BrightnessSettingsView::open();
    break;
  case 6:
    cycle_brightness();
    refresh();
    break;
  case 7:
    close();
    BrightnessSettingsView::open();
    break;
  case 8:
    shutdown_board();
    break;
  default:
    break;
  }
}

} // namespace SystemMenuView
} // namespace DeskClock
