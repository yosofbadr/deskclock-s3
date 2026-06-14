#include "NetworkSetupView.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GestureTextMenu.h"
#include "NetworkService.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

enum class Screen : uint8_t {
  Main,
  Password,
};

enum class MainAction : uint8_t {
  Back = 0,
  Status,
  Network,
  Password,
  Scan,
  Select1,
  Select2,
  Select3,
  Connect,
  Disable,
  Demo,
};

enum class PasswordAction : uint8_t {
  Back = 0,
  Preview,
  Character,
  AddCharacter,
  Mode,
  Space,
  Backspace,
  Clear,
};

constexpr uint8_t kMainRowCount = 11;
constexpr uint8_t kPasswordRowCount = 8;
constexpr uint8_t kVisibleRows = 7;

lv_obj_t *panel = nullptr;
const lv_font_t *view_font = nullptr;
int32_t panel_width = 0;
Screen screen = Screen::Main;
uint8_t selected_index = 4;
uint8_t keyboard_mode = 0;
uint8_t selected_char_index = 0;
lv_obj_t *rows[kVisibleRows] = {};
GestureTextMenu::TouchState touch_state;

void refresh();

const char *keyboard_chars(uint8_t mode)
{
  switch (mode) {
  case 1:
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-_";
  case 2:
    return "0123456789!@#$%&*?";
  case 3:
    return "+=/\\:;,.()[]{}\"'`~";
  case 0:
  default:
    return "abcdefghijklmnopqrstuvwxyz.-_";
  }
}

char selected_char()
{
  const char *chars = keyboard_chars(keyboard_mode);
  const size_t count = strlen(chars);
  if (count == 0) {
    return 'a';
  }
  selected_char_index = static_cast<uint8_t>(selected_char_index % count);
  return chars[selected_char_index];
}

const char *keyboard_mode_label()
{
  switch (keyboard_mode) {
  case 1:
    return "UPPER";
  case 2:
    return "123";
  case 3:
    return "symbols";
  case 0:
  default:
    return "lower";
  }
}

uint8_t row_count()
{
  return screen == Screen::Main ? kMainRowCount : kPasswordRowCount;
}

void set_screen(Screen next, uint8_t selected)
{
  screen = next;
  selected_index = selected;
  GestureTextMenu::reset(&touch_state);
  refresh();
}

MainAction main_action_for_index(uint8_t index)
{
  return static_cast<MainAction>(index);
}

PasswordAction password_action_for_index(uint8_t index)
{
  return static_cast<PasswordAction>(index);
}

void cycle_keyboard_mode(int8_t delta)
{
  keyboard_mode = GestureTextMenu::wrapIndex(keyboard_mode, delta, 4);
  selected_char_index = 0;
}

void adjust_password_char(int8_t delta)
{
  const char *chars = keyboard_chars(keyboard_mode);
  const uint8_t count = static_cast<uint8_t>(strlen(chars));
  if (count == 0) {
    selected_char_index = 0;
    return;
  }
  selected_char_index = GestureTextMenu::wrapIndex(selected_char_index, delta, count);
}

void activate_main_action(MainAction action)
{
  switch (action) {
  case MainAction::Back:
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    break;
  case MainAction::Status:
  case MainAction::Connect:
    NetworkService::connectSelected();
    refresh();
    break;
  case MainAction::Network:
  case MainAction::Scan:
    NetworkService::scanNetworks();
    refresh();
    break;
  case MainAction::Password:
    set_screen(Screen::Password, 2);
    break;
  case MainAction::Select1:
  case MainAction::Select2:
  case MainAction::Select3: {
    const int index = static_cast<int>(action) - static_cast<int>(MainAction::Select1);
    NetworkService::selectScannedNetwork(index);
    refresh();
    break;
  }
  case MainAction::Disable:
    NetworkService::setEnabled(false);
    refresh();
    break;
  case MainAction::Demo:
    NetworkService::selectDemoNetwork();
    refresh();
    break;
  }
}

void adjust_main_action(MainAction action, int8_t delta)
{
  switch (action) {
  case MainAction::Select1:
  case MainAction::Select2:
  case MainAction::Select3:
    activate_main_action(action);
    return;
  case MainAction::Disable:
    if (delta != 0) {
      NetworkService::setEnabled(delta > 0);
      refresh();
    }
    return;
  case MainAction::Status:
  case MainAction::Connect:
    if (delta > 0) {
      NetworkService::connectSelected();
      refresh();
    }
    return;
  case MainAction::Back:
  case MainAction::Network:
  case MainAction::Password:
  case MainAction::Scan:
  case MainAction::Demo:
  default:
    return;
  }
}

void activate_password_action(PasswordAction action)
{
  switch (action) {
  case PasswordAction::Back:
    set_screen(Screen::Main, 3);
    break;
  case PasswordAction::Preview:
    selected_index = 2;
    refresh();
    break;
  case PasswordAction::Character:
  case PasswordAction::AddCharacter:
    NetworkService::appendPasswordChar(selected_char());
    refresh();
    break;
  case PasswordAction::Mode:
    cycle_keyboard_mode(1);
    refresh();
    break;
  case PasswordAction::Space:
    NetworkService::appendPasswordChar(' ');
    refresh();
    break;
  case PasswordAction::Backspace:
    NetworkService::backspacePassword();
    refresh();
    break;
  case PasswordAction::Clear:
    NetworkService::clearPassword();
    refresh();
    break;
  }
}

void adjust_password_action(PasswordAction action, int8_t delta)
{
  switch (action) {
  case PasswordAction::Character:
    adjust_password_char(delta);
    refresh();
    break;
  case PasswordAction::Mode:
    cycle_keyboard_mode(delta);
    refresh();
    break;
  case PasswordAction::Backspace:
    if (delta < 0) {
      NetworkService::backspacePassword();
      refresh();
    }
    break;
  case PasswordAction::AddCharacter:
    if (delta > 0) {
      NetworkService::appendPasswordChar(selected_char());
      refresh();
    }
    break;
  case PasswordAction::Space:
    if (delta > 0) {
      NetworkService::appendPasswordChar(' ');
      refresh();
    }
    break;
  case PasswordAction::Clear:
    if (delta < 0) {
      NetworkService::clearPassword();
      refresh();
    }
    break;
  case PasswordAction::Back:
  case PasswordAction::Preview:
  default:
    break;
  }
}

void draw_title(const char *title_text)
{
  lv_obj_t *title = lv_label_create(panel);
  lv_obj_set_style_text_font(title, view_font, 0);
  UiWidgets::setTextColor(title, 0x0F172A);
  lv_label_set_text(title, title_text);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 7);

  lv_obj_t *hint = lv_label_create(panel);
  lv_obj_set_style_text_font(hint, LV_FONT_DEFAULT, 0);
  UiWidgets::setTextColor(hint, 0x64748B);
  lv_label_set_text(hint, "up/down select  left/right adjust  tap enter");
  lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -12, 11);
}

void format_main_row(uint8_t index, char *row, size_t size, uint32_t &color)
{
  NetworkSnapshot network = NetworkService::snapshot();
  color = 0x0F172A;
  switch (main_action_for_index(index)) {
  case MainAction::Back:
    snprintf(row, size, "Back");
    break;
  case MainAction::Status:
    color = network.connected ? 0x0F766E : 0x334155;
    snprintf(row, size, "Status     %.44s", network.status);
    break;
  case MainAction::Network:
    snprintf(row, size, "Network    %.44s", network.ssid);
    break;
  case MainAction::Password:
    snprintf(row, size, "Password   %.36s", network.password_preview);
    break;
  case MainAction::Scan:
    color = 0x0F766E;
    snprintf(row, size, "Scan networks");
    break;
  case MainAction::Select1:
  case MainAction::Select2:
  case MainAction::Select3: {
    const int network_index = static_cast<int>(main_action_for_index(index)) - static_cast<int>(MainAction::Select1);
    if (network_index < NetworkService::scannedNetworkCount()) {
      snprintf(row, size, "%d  %.48s", network_index + 1, NetworkService::scannedSsid(network_index));
    } else {
      color = 0x94A3B8;
      snprintf(row, size, "%d  --", network_index + 1);
    }
    break;
  }
  case MainAction::Connect:
    color = 0x0F766E;
    snprintf(row, size, "Connect selected");
    break;
  case MainAction::Disable:
    color = 0xB45309;
    snprintf(row, size, "Disable Wi-Fi");
    break;
  case MainAction::Demo:
    color = 0x334155;
    snprintf(row, size, "Demo network");
    break;
  }
}

void format_password_row(uint8_t index, char *row, size_t size, uint32_t &color)
{
  NetworkSnapshot network = NetworkService::snapshot();
  color = 0x0F172A;
  switch (password_action_for_index(index)) {
  case PasswordAction::Back:
    snprintf(row, size, "Back to Wi-Fi");
    break;
  case PasswordAction::Preview:
    color = 0x334155;
    snprintf(row, size, "Password   %.42s", network.password_preview);
    break;
  case PasswordAction::Character:
    color = 0x0F766E;
    snprintf(row, size, "Character  [%c]", selected_char());
    break;
  case PasswordAction::AddCharacter:
    snprintf(row, size, "Add [%c]", selected_char());
    break;
  case PasswordAction::Mode:
    snprintf(row, size, "Mode       %s", keyboard_mode_label());
    break;
  case PasswordAction::Space:
    snprintf(row, size, "Space");
    break;
  case PasswordAction::Backspace:
    snprintf(row, size, "Backspace");
    break;
  case PasswordAction::Clear:
    color = 0xB45309;
    snprintf(row, size, "Clear password");
    break;
  }
}

void format_row(uint8_t index, char *row, size_t size, uint32_t &color)
{
  if (screen == Screen::Main) {
    format_main_row(index, row, size, color);
  } else {
    format_password_row(index, row, size, color);
  }
}

void set_row(uint8_t slot, uint8_t index, bool selected)
{
  char value[96];
  char line[112];
  uint32_t color = 0x0F172A;
  format_row(index, value, sizeof(value), color);
  snprintf(line, sizeof(line), "%s %.96s", selected ? ">" : " ", value);
  lv_label_set_text(rows[slot], line);
  lv_obj_set_style_text_color(rows[slot], lv_color_hex(selected ? 0xFFFFFF : color), 0);
  lv_obj_set_style_bg_color(rows[slot], lv_color_hex(selected ? 0x334155 : 0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(rows[slot], selected ? LV_OPA_90 : LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(rows[slot], 4, 0);
  lv_obj_set_style_pad_left(rows[slot], 4, 0);
}

void refresh()
{
  if (panel == nullptr) {
    return;
  }
  lv_obj_clean(panel);
  draw_title(screen == Screen::Main ? "Wi-Fi setup" : "Wi-Fi password");

  const uint8_t count = row_count();
  if (selected_index >= count) {
    selected_index = 0;
  }
  const uint8_t first = GestureTextMenu::firstVisibleIndex(selected_index, count, kVisibleRows);
  for (uint8_t slot = 0; slot < kVisibleRows; ++slot) {
    const uint8_t index = static_cast<uint8_t>(first + slot);
    rows[slot] = lv_label_create(panel);
    lv_obj_set_style_text_font(rows[slot], LV_FONT_DEFAULT, 0);
    lv_obj_set_width(rows[slot], panel_width - 24);
    lv_obj_set_height(rows[slot], 15);
    lv_label_set_long_mode(rows[slot], LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(rows[slot], 12, 30 + static_cast<int32_t>(slot) * 16);
    if (index < count) {
      set_row(slot, index, index == selected_index);
    } else {
      lv_obj_add_flag(rows[slot], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void handle_input(GestureTextMenu::Input input, void *)
{
  switch (input) {
  case GestureTextMenu::Input::Up:
    NetworkSetupView::moveSelection(-1);
    break;
  case GestureTextMenu::Input::Down:
    NetworkSetupView::moveSelection(1);
    break;
  case GestureTextMenu::Input::Left:
    NetworkSetupView::adjustSelected(-1);
    break;
  case GestureTextMenu::Input::Right:
    NetworkSetupView::adjustSelected(1);
    break;
  case GestureTextMenu::Input::Tap:
    NetworkSetupView::activateSelected();
    break;
  case GestureTextMenu::Input::None:
  default:
    break;
  }
}

} // namespace

namespace NetworkSetupView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font)
{
  view_font = font != nullptr ? font : LV_FONT_DEFAULT;
  panel_width = width - 28;
  panel = UiWidgets::modalPanel(parent, width, height);
  GestureTextMenu::attach(panel, &touch_state, handle_input, nullptr);
  refresh();
}

void open()
{
  screen = Screen::Main;
  selected_index = 4;
  GestureTextMenu::reset(&touch_state);
  refresh();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
}

void openPasswordEditor()
{
  screen = Screen::Password;
  selected_index = 2;
  GestureTextMenu::reset(&touch_state);
  refresh();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
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
  selected_index = GestureTextMenu::wrapIndex(selected_index, delta, row_count());
  refresh();
}

void adjustSelected(int8_t delta)
{
  if (!isOpen()) {
    return;
  }
  if (screen == Screen::Main) {
    adjust_main_action(main_action_for_index(selected_index), delta);
  } else {
    adjust_password_action(password_action_for_index(selected_index), delta);
  }
}

void activateSelected()
{
  if (!isOpen()) {
    return;
  }
  if (screen == Screen::Main) {
    activate_main_action(main_action_for_index(selected_index));
  } else {
    activate_password_action(password_action_for_index(selected_index));
  }
}

} // namespace NetworkSetupView
} // namespace DeskClock
