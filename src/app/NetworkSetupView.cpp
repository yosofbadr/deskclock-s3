#include "NetworkSetupView.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "NetworkService.h"
#include "UiWidgets.h"

namespace DeskClock {
namespace {

lv_obj_t *panel = nullptr;
lv_obj_t *value_label = nullptr;

void refresh()
{
  if (value_label == nullptr) {
    return;
  }
  NetworkSnapshot network = NetworkService::snapshot();
  char scan_lines[96] = "";
  const int scanned = NetworkService::scannedNetworkCount();
  for (int index = 0; index < scanned && index < 3; ++index) {
    char line[32];
    snprintf(line, sizeof(line), "  %d:%.18s", index + 1, NetworkService::scannedSsid(index));
    strlcat(scan_lines, line, sizeof(scan_lines));
  }
  char buffer[192];
  snprintf(buffer,
           sizeof(buffer),
           "%s\nNet %.24s  Pass %.18s%s",
           network.status,
           network.ssid,
           network.password_preview,
           scan_lines);
  lv_label_set_text(value_label, buffer);
}

void close_event(lv_event_t *)
{
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void skip_event(lv_event_t *)
{
  NetworkService::setEnabled(false);
  refresh();
}

void scan_event(lv_event_t *)
{
  NetworkService::scanNetworks();
  refresh();
}

void select_scanned_event(lv_event_t *event)
{
  const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  NetworkService::selectScannedNetwork(index);
  refresh();
}

void append_password_event(lv_event_t *event)
{
  const char value = static_cast<char>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  NetworkService::appendPasswordChar(value);
  refresh();
}

void backspace_password_event(lv_event_t *)
{
  NetworkService::backspacePassword();
  refresh();
}

void connect_event(lv_event_t *)
{
  NetworkService::connectSelected();
  refresh();
}

void demo_event(lv_event_t *)
{
  NetworkService::selectDemoNetwork();
  refresh();
}

void create_password_key(lv_obj_t *parent, char label, int32_t x, int32_t y)
{
  char text[2] = {label, '\0'};
  lv_obj_t *button = UiWidgets::button(parent, text, 28, 20);
  lv_obj_align(button, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_add_event_cb(button, append_password_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(label)));
}

} // namespace

namespace NetworkSetupView {

void create(lv_obj_t *parent, int32_t width, int32_t height, const lv_font_t *font)
{
  panel = UiWidgets::modalPanel(parent, width, height);

  lv_obj_t *title = lv_label_create(panel);
  lv_obj_set_style_text_font(title, font, 0);
  UiWidgets::setTextColor(title, 0x1F2933);
  lv_label_set_text(title, "Network setup");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 8);

  value_label = lv_label_create(panel);
  lv_obj_set_style_text_font(value_label, font, 0);
  lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(value_label, width - 260);
  UiWidgets::setTextColor(value_label, 0x1F2933);
  lv_label_set_text(value_label, "offline");
  lv_obj_align(value_label, LV_ALIGN_TOP_MID, 18, 8);

  lv_obj_t *scan_wifi = UiWidgets::button(panel, "Scan", 70, 28);
  lv_obj_align(scan_wifi, LV_ALIGN_TOP_RIGHT, -14, 8);
  lv_obj_add_event_cb(scan_wifi, scan_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *select_one = UiWidgets::button(panel, "1", 42, 26);
  lv_obj_align(select_one, LV_ALIGN_TOP_RIGHT, -138, 42);
  lv_obj_add_event_cb(select_one, select_scanned_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(0)));
  lv_obj_t *select_two = UiWidgets::button(panel, "2", 42, 26);
  lv_obj_align(select_two, LV_ALIGN_TOP_RIGHT, -90, 42);
  lv_obj_add_event_cb(select_two, select_scanned_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  lv_obj_t *select_three = UiWidgets::button(panel, "3", 42, 26);
  lv_obj_align(select_three, LV_ALIGN_TOP_RIGHT, -42, 42);
  lv_obj_add_event_cb(select_three, select_scanned_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(2)));

  const char row_one[] = {'a', 'b', 'c', 'd', 'e', 'f'};
  const char row_two[] = {'g', 'h', 'i', 'j', 'k', 'l'};
  const char row_three[] = {'1', '2', '3', '4', '5', '6'};
  for (uint8_t index = 0; index < 6; ++index) {
    create_password_key(panel, row_one[index], 14 + (index * 32), 38);
    create_password_key(panel, row_two[index], 14 + (index * 32), 62);
    create_password_key(panel, row_three[index], 14 + (index * 32), 86);
  }
  create_password_key(panel, '-', 210, 38);
  create_password_key(panel, '_', 210, 62);
  create_password_key(panel, '!', 210, 86);

  lv_obj_t *pass_back = UiWidgets::button(panel, "Bk", 44, 26);
  lv_obj_align(pass_back, LV_ALIGN_TOP_RIGHT, -138, 76);
  lv_obj_add_event_cb(pass_back, backspace_password_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *connect = UiWidgets::button(panel, "Conn", 58, 26);
  lv_obj_align(connect, LV_ALIGN_TOP_RIGHT, -80, 76);
  lv_obj_add_event_cb(connect, connect_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *skip_wifi = UiWidgets::button(panel, "Skip", 68, 28);
  lv_obj_align(skip_wifi, LV_ALIGN_BOTTOM_LEFT, 14, -2);
  lv_obj_add_event_cb(skip_wifi, skip_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *demo_wifi = UiWidgets::button(panel, "Demo", 68, 28);
  lv_obj_align(demo_wifi, LV_ALIGN_BOTTOM_LEFT, 90, -2);
  lv_obj_add_event_cb(demo_wifi, demo_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *close = UiWidgets::button(panel, "Close", 74, 28);
  lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, -14, -2);
  lv_obj_add_event_cb(close, close_event, LV_EVENT_CLICKED, nullptr);
}

void open()
{
  refresh();
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(panel);
}

} // namespace NetworkSetupView
} // namespace DeskClock
