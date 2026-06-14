#include "ClockFace.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AlarmAlertView.h"
#include "AlarmManagerView.h"
#include "AssetService.h"
#include "BrightnessSettingsView.h"
#include "ClockDisplayFormatter.h"
#include "NetworkSetupView.h"
#include "SettingsService.h"
#include "SystemMenuView.h"
#include "TimeService.h"
#include "TimeSetupView.h"
#include "UiWidgets.h"
#include "lvgl.h"

extern "C" unsigned lodepng_decode32(unsigned char **out, unsigned *w, unsigned *h, const unsigned char *in, size_t insize);

LV_FONT_DECLARE(clock_font_time_88);

namespace {

struct VisualTheme {
  const char *name;
  uint32_t screen_bg;
  uint32_t stage_bg;
  uint32_t floor_bg;
  uint32_t window_bg;
  uint32_t card_bg;
  uint32_t card_border;
  uint32_t text;
  uint32_t muted;
  uint32_t faint;
  uint32_t accent;
  uint32_t accent_2;
  uint32_t mascot;
  uint32_t mascot_detail;
  uint32_t time_shadow;
  uint32_t glow;
  uint8_t card_opa;
  bool dark;
};

constexpr VisualTheme kThemes[] = {
    {"studio", 0xF3E8D2, 0xF8EEDB, 0xD7AC72, 0xFFF8E8, 0xFFFAEF, 0xE8D5BC, 0x26313D, 0x625B54, 0xA99374, 0xE8752E, 0x245C91, 0xFFFDF5, 0xF09B45, 0xC7AA82, 0xFFF4CC, LV_OPA_90, false},
    {"blush", 0xFFF0EB, 0xFFF7EE, 0xEFC19D, 0xFFFBEF, 0xFFF9F4, 0xF0C8C0, 0x51313A, 0x7D5360, 0xC99BA4, 0xE64864, 0xE93E5D, 0xFFFDF9, 0xE96B84, 0xF1B0B8, 0xFFF2D3, LV_OPA_90, false},
    {"dusk", 0x070817, 0x0D1026, 0x1B1428, 0x10173A, 0x17152A, 0x4E3B65, 0xF8F3FF, 0xC1B0DD, 0x7F659F, 0xB778FF, 0xFFFFFF, 0xF7F0FF, 0xC084FC, 0x000000, 0x2C1D48, LV_OPA_70, true},
};

constexpr uint8_t kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);
constexpr int32_t kBackgroundInset = 4;
constexpr int32_t kBackgroundRadius = 18;

lv_obj_t *root_screen = nullptr;
lv_obj_t *background_image = nullptr;
lv_obj_t *mascot_asset_image = nullptr;
lv_obj_t *weather_asset_image = nullptr;
lv_obj_t *focus_asset_image = nullptr;
lv_obj_t *message_asset_image = nullptr;
lv_obj_t *left_date_card = nullptr;
lv_obj_t *calendar_card = nullptr;
lv_obj_t *stage_card = nullptr;
lv_obj_t *right_status_card = nullptr;
lv_obj_t *right_alarm_card = nullptr;
lv_obj_t *right_message_card = nullptr;
lv_obj_t *floor_panel = nullptr;
lv_obj_t *window_panel = nullptr;
lv_obj_t *window_glow = nullptr;
lv_obj_t *window_frame_left = nullptr;
lv_obj_t *window_frame_right = nullptr;
lv_obj_t *window_sill = nullptr;
lv_obj_t *curtain_left = nullptr;
lv_obj_t *curtain_right = nullptr;
lv_obj_t *moon = nullptr;
lv_obj_t *moon_cutout = nullptr;
lv_obj_t *star_dots[10] = {};
lv_obj_t *plant_pot = nullptr;
lv_obj_t *plant_leaf_left = nullptr;
lv_obj_t *plant_leaf_right = nullptr;
lv_obj_t *mascot_ear_left = nullptr;
lv_obj_t *mascot_ear_right = nullptr;
lv_obj_t *mascot_head = nullptr;
lv_obj_t *mascot_eye_left = nullptr;
lv_obj_t *mascot_eye_right = nullptr;
lv_obj_t *mascot_mouth = nullptr;
lv_obj_t *sync_dot = nullptr;
lv_obj_t *wall_panel = nullptr;
lv_obj_t *picture_frame = nullptr;
lv_obj_t *picture_inner = nullptr;
lv_obj_t *pendant_cord = nullptr;
lv_obj_t *pendant_lamp = nullptr;
lv_obj_t *shelf = nullptr;
lv_obj_t *shelf_shadow = nullptr;
lv_obj_t *house_body = nullptr;
lv_obj_t *house_roof = nullptr;
lv_obj_t *weather_sun = nullptr;
lv_obj_t *weather_cloud_left = nullptr;
lv_obj_t *weather_cloud_right = nullptr;
lv_obj_t *weather_cloud_base = nullptr;
lv_obj_t *focus_ring = nullptr;
lv_obj_t *focus_ring_gap = nullptr;
lv_obj_t *focus_leaf = nullptr;
lv_obj_t *message_heart = nullptr;

lv_obj_t *time_shadow_label = nullptr;
lv_obj_t *ampm_shadow_label = nullptr;
lv_obj_t *seconds_shadow_label = nullptr;
lv_obj_t *time_label = nullptr;
lv_obj_t *ampm_label = nullptr;
lv_obj_t *seconds_label = nullptr;
lv_obj_t *greeting_label = nullptr;
lv_obj_t *setup_hint_label = nullptr;
lv_obj_t *weekday_label = nullptr;
lv_obj_t *month_label = nullptr;
lv_obj_t *day_label = nullptr;
lv_obj_t *calendar_month_label = nullptr;
lv_obj_t *calendar_weekday_labels[7] = {};
lv_obj_t *calendar_day_labels[42] = {};
lv_obj_t *status_label = nullptr;
lv_obj_t *weather_label = nullptr;
lv_obj_t *weather_detail_label = nullptr;
lv_obj_t *alarm_title_label = nullptr;
lv_obj_t *next_alarm_label = nullptr;
lv_obj_t *message_label = nullptr;
lv_obj_t *theme_name_label = nullptr;

struct RuntimePngImage {
  lv_image_dsc_t descriptor = {};
  uint8_t *data = nullptr;
  char path[160] = {};
};

RuntimePngImage mascot_runtime_image;
RuntimePngImage weather_runtime_image;
RuntimePngImage focus_runtime_image;
RuntimePngImage message_runtime_image;

bool background_asset_active = false;
bool mascot_asset_active = false;
bool weather_asset_active = false;
bool focus_asset_active = false;
bool message_asset_active = false;

const lv_font_t *time_font()
{
  return &clock_font_time_88;
}

const lv_font_t *large_font()
{
#if LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
#elif LV_FONT_MONTSERRAT_28
  return &lv_font_montserrat_28;
#else
  return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *seconds_font()
{
#if LV_FONT_MONTSERRAT_28
  return &lv_font_montserrat_28;
#else
  return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *body_font()
{
#if LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#else
  return LV_FONT_DEFAULT;
#endif
}

const lv_font_t *small_font()
{
#if LV_FONT_MONTSERRAT_10
  return &lv_font_montserrat_10;
#elif LV_FONT_MONTSERRAT_12
  return &lv_font_montserrat_12;
#else
  return LV_FONT_DEFAULT;
#endif
}

const VisualTheme &theme()
{
  const DeskClock::SettingsSnapshot settings = DeskClock::SettingsService::snapshot();
  return kThemes[settings.theme_index % kThemeCount];
}

void set_text_color(lv_obj_t *obj, uint32_t color)
{
  if (obj != nullptr) {
    DeskClock::UiWidgets::setTextColor(obj, color);
  }
}

void set_hidden(lv_obj_t *obj, bool hidden)
{
  if (obj == nullptr) {
    return;
  }
  if (hidden) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

void release_runtime_png(RuntimePngImage &image)
{
  if (image.data != nullptr) {
    free(image.data);
    image.data = nullptr;
  }
  image.descriptor = {};
  image.path[0] = '\0';
}

bool png_size_within_limit(const uint8_t *bytes, size_t size, uint32_t max_pixels)
{
  static constexpr uint8_t kPngMagic[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (bytes == nullptr || size < 24 || memcmp(bytes, kPngMagic, sizeof(kPngMagic)) != 0) {
    return false;
  }
  const uint32_t width = (static_cast<uint32_t>(bytes[16]) << 24) | (static_cast<uint32_t>(bytes[17]) << 16) |
                         (static_cast<uint32_t>(bytes[18]) << 8) | static_cast<uint32_t>(bytes[19]);
  const uint32_t height = (static_cast<uint32_t>(bytes[20]) << 24) | (static_cast<uint32_t>(bytes[21]) << 16) |
                          (static_cast<uint32_t>(bytes[22]) << 8) | static_cast<uint32_t>(bytes[23]);
  return width > 0 && height > 0 && width <= 512 && height <= 512 && (width * height) <= max_pixels;
}

bool load_png_runtime_image(RuntimePngImage &image, const char *lvgl_path, uint32_t max_pixels)
{
  if (lvgl_path == nullptr || lvgl_path[0] == '\0') {
    release_runtime_png(image);
    return false;
  }
  if (image.data != nullptr && strcmp(image.path, lvgl_path) == 0) {
    return true;
  }

  char host_path[192];
  if (!DeskClock::AssetService::hostPathForLvglPath(lvgl_path, host_path, sizeof(host_path))) {
    return false;
  }

  FILE *file = fopen(host_path, "rb");
  if (file == nullptr) {
    return false;
  }
  fseek(file, 0, SEEK_END);
  const long file_size_long = ftell(file);
  fseek(file, 0, SEEK_SET);
  if (file_size_long <= 0 || file_size_long > (2L * 1024L * 1024L)) {
    fclose(file);
    return false;
  }

  const size_t file_size = static_cast<size_t>(file_size_long);
  uint8_t *encoded = static_cast<uint8_t *>(malloc(file_size));
  if (encoded == nullptr) {
    fclose(file);
    return false;
  }
  const size_t read_count = fread(encoded, 1, file_size, file);
  fclose(file);
  if (read_count != file_size || !png_size_within_limit(encoded, file_size, max_pixels)) {
    free(encoded);
    return false;
  }

  unsigned width = 0;
  unsigned height = 0;
  unsigned char *rgba = nullptr;
  const unsigned decode_error = lodepng_decode32(&rgba, &width, &height, encoded, file_size);
  free(encoded);
  if (decode_error != 0 || rgba == nullptr || width == 0 || height == 0 || (width * height) > max_pixels) {
    if (rgba != nullptr) {
      free(rgba);
    }
    return false;
  }

  const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
  uint8_t *pixels = static_cast<uint8_t *>(malloc(pixel_count * 4U));
  if (pixels == nullptr) {
    free(rgba);
    return false;
  }
  for (size_t index = 0; index < pixel_count; ++index) {
    pixels[(index * 4U) + 0U] = rgba[(index * 4U) + 2U];
    pixels[(index * 4U) + 1U] = rgba[(index * 4U) + 1U];
    pixels[(index * 4U) + 2U] = rgba[(index * 4U) + 0U];
    pixels[(index * 4U) + 3U] = rgba[(index * 4U) + 3U];
  }
  free(rgba);

  release_runtime_png(image);
  image.data = pixels;
  image.descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
  image.descriptor.header.cf = LV_COLOR_FORMAT_ARGB8888;
  image.descriptor.header.flags = 0;
  image.descriptor.header.w = static_cast<uint16_t>(width);
  image.descriptor.header.h = static_cast<uint16_t>(height);
  image.descriptor.header.stride = static_cast<uint16_t>(width * 4U);
  image.descriptor.data_size = static_cast<uint32_t>(pixel_count * 4U);
  image.descriptor.data = pixels;
  snprintf(image.path, sizeof(image.path), "%s", lvgl_path);
  return true;
}

bool set_image_asset(lv_obj_t *obj,
                     RuntimePngImage &runtime_image,
                     const char *lvgl_path,
                     lv_image_align_t align,
                     uint32_t max_pixels)
{
  if (obj == nullptr || lvgl_path == nullptr || lvgl_path[0] == '\0') {
    if (obj != nullptr) {
      lv_image_set_src(obj, nullptr);
    }
    set_hidden(obj, true);
    release_runtime_png(runtime_image);
    return false;
  }

  const char *extension = strrchr(lvgl_path, '.');
  const bool is_png = extension != nullptr && (strcmp(extension, ".png") == 0 || strcmp(extension, ".PNG") == 0);
  if (is_png) {
    if (!load_png_runtime_image(runtime_image, lvgl_path, max_pixels)) {
      lv_image_set_src(obj, nullptr);
      set_hidden(obj, true);
      return false;
    }
    lv_image_set_src(obj, &runtime_image.descriptor);
  } else {
    release_runtime_png(runtime_image);
    lv_image_set_src(obj, lvgl_path);
  }
  lv_image_set_inner_align(obj, align);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  return true;
}

uint8_t days_in_month(uint16_t year, uint8_t month)
{
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  if (month == 2 && (((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U))) {
    return 29;
  }
  return days[month - 1];
}

const char *weekday_name(uint8_t week)
{
  static constexpr const char *names[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  return week < 7 ? names[week] : "DAY";
}

const char *month_name(uint8_t month)
{
  static constexpr const char *names[] = {"---", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  return month <= 12 ? names[month] : "---";
}

const char *greeting_for_hour(uint8_t hour)
{
  if (hour < 5) {
    return "Rest well";
  }
  if (hour < 12) {
    return "Good morning";
  }
  if (hour < 18) {
    return "Good afternoon";
  }
  return "Good evening";
}

lv_obj_t *plain_obj(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, int32_t height)
{
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, width, height);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

lv_obj_t *card(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, int32_t height)
{
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, width, height);
  lv_obj_set_style_radius(obj, 12, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

void style_card(lv_obj_t *obj, uint32_t bg, uint32_t border, uint8_t opa)
{
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(obj, opa, 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(obj, 10, 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_20, 0);
  lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_ofs_y(obj, 3, 0);
}

void style_soft_shape(lv_obj_t *obj, uint32_t bg, int32_t radius)
{
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
}

void style_vertical_gradient(lv_obj_t *obj, uint32_t top, uint32_t bottom, int32_t radius)
{
  if (obj == nullptr) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(top), 0);
  lv_obj_set_style_bg_grad_color(obj, lv_color_hex(bottom), 0);
  lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
}

void set_room_decor_hidden(bool hidden)
{
  set_hidden(wall_panel, hidden);
  set_hidden(floor_panel, hidden);
  set_hidden(window_panel, hidden);
  set_hidden(window_glow, hidden);
  set_hidden(window_frame_left, hidden);
  set_hidden(window_frame_right, hidden);
  set_hidden(window_sill, hidden);
  set_hidden(curtain_left, hidden);
  set_hidden(curtain_right, hidden);
  set_hidden(moon, hidden);
  set_hidden(moon_cutout, hidden);
  set_hidden(picture_frame, hidden);
  set_hidden(pendant_cord, hidden);
  set_hidden(pendant_lamp, hidden);
  set_hidden(shelf_shadow, hidden);
  set_hidden(shelf, hidden);
  set_hidden(house_body, hidden);
  set_hidden(house_roof, hidden);
  set_hidden(plant_pot, hidden);
  set_hidden(plant_leaf_left, hidden);
  set_hidden(plant_leaf_right, hidden);
  for (lv_obj_t *dot : star_dots) {
    set_hidden(dot, hidden);
  }
}

void set_drawn_mascot_hidden(bool hidden)
{
  set_hidden(mascot_ear_left, hidden);
  set_hidden(mascot_ear_right, hidden);
  set_hidden(mascot_head, hidden);
}

void set_drawn_weather_hidden(bool hidden)
{
  set_hidden(weather_sun, hidden);
  set_hidden(weather_cloud_left, hidden);
  set_hidden(weather_cloud_right, hidden);
  set_hidden(weather_cloud_base, hidden);
}

void format_primary_time(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
  if (DeskClock::TimeSetupView::use24HourFormat()) {
    snprintf(buffer, size, "%02u:%02u", hour, minute);
    return;
  }

  uint8_t display_hour = hour % 12U;
  if (display_hour == 0) {
    display_hour = 12;
  }
  snprintf(buffer, size, "%u:%02u", display_hour, minute);
}

void realign_time_details()
{
  if (time_label == nullptr || ampm_label == nullptr || seconds_label == nullptr) {
    return;
  }

  constexpr int32_t time_x = 206;
  constexpr int32_t time_y = 42;
  constexpr int32_t details_x = 449;
  constexpr int32_t ampm_y = 55;
  constexpr int32_t seconds_y = 77;

  if (time_shadow_label != nullptr) {
    lv_obj_set_align(time_shadow_label, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(time_shadow_label, time_x + 2, time_y + 3);
  }
  lv_obj_set_align(time_label, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(time_label, time_x, time_y);

  if (ampm_shadow_label != nullptr) {
    lv_obj_set_align(ampm_shadow_label, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(ampm_shadow_label, details_x + 1, ampm_y + 2);
  }
  lv_obj_set_align(ampm_label, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(ampm_label, details_x, ampm_y);

  if (seconds_shadow_label != nullptr) {
    lv_obj_set_align(seconds_shadow_label, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(seconds_shadow_label, details_x + 1, seconds_y + 2);
  }
  lv_obj_set_align(seconds_label, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(seconds_label, details_x, seconds_y);
}

void update_calendar_cards(const DeskClock::DateTime &now)
{
  const VisualTheme &t = theme();
  if (!now.valid) {
    lv_label_set_text(weekday_label, "SET TIME");
    lv_label_set_text(month_label, "CLOCK");
    lv_label_set_text(day_label, "--");
    lv_label_set_text(calendar_month_label, "calendar");
    for (lv_obj_t *cell : calendar_day_labels) {
      lv_label_set_text(cell, "");
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    }
    return;
  }

  lv_label_set_text(weekday_label, weekday_name(now.week));
  lv_label_set_text(month_label, month_name(now.month));
  char day_buffer[4];
  snprintf(day_buffer, sizeof(day_buffer), "%u", now.day);
  lv_label_set_text(day_label, day_buffer);

  char month_buffer[16];
  snprintf(month_buffer, sizeof(month_buffer), "%s %04u", month_name(now.month), now.year);
  lv_label_set_text(calendar_month_label, month_buffer);

  const uint8_t month_days = days_in_month(now.year, now.month);
  const uint8_t first_weekday = static_cast<uint8_t>((now.week + 7U - ((now.day - 1U) % 7U)) % 7U);
  for (uint8_t index = 0; index < 42; ++index) {
    const int16_t day_number = static_cast<int16_t>(index) - static_cast<int16_t>(first_weekday) + 1;
    lv_obj_t *cell = calendar_day_labels[index];
    if (day_number < 1 || day_number > month_days) {
      lv_label_set_text(cell, "");
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
      continue;
    }

    char buffer[4];
    snprintf(buffer, sizeof(buffer), "%d", day_number);
    lv_label_set_text(cell, buffer);
    const bool selected = day_number == now.day;
    lv_obj_set_style_radius(cell, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cell, lv_color_hex(t.accent), 0);
    lv_obj_set_style_bg_opa(cell, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    set_text_color(cell, selected ? t.card_bg : t.text);
  }
}

void open_time_setup_event(lv_event_t *)
{
  DeskClock::TimeSetupView::open();
}

void open_system_menu_event(lv_event_t *)
{
  DeskClock::SystemMenuView::open();
}

void open_brightness_event(lv_event_t *)
{
  DeskClock::BrightnessSettingsView::open();
}

void open_alarms_event(lv_event_t *)
{
  DeskClock::AlarmManagerView::open();
}

void apply_theme_to_static_objects()
{
  const VisualTheme &t = theme();
  if (root_screen == nullptr) {
    return;
  }

  lv_obj_set_style_bg_color(root_screen, lv_color_hex(t.screen_bg), 0);
  lv_obj_set_style_bg_opa(root_screen, LV_OPA_COVER, 0);

  char asset_path[160];
  background_asset_active = DeskClock::AssetService::backgroundPath(t.name, asset_path, sizeof(asset_path));
  if (background_asset_active && background_image != nullptr) {
    const int32_t display_width = lv_display_get_horizontal_resolution(nullptr);
    const int32_t display_height = lv_display_get_vertical_resolution(nullptr);
    lv_image_set_src(background_image, asset_path);
    lv_image_set_inner_align(background_image, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_pos(background_image, kBackgroundInset, kBackgroundInset);
    lv_obj_set_size(background_image, display_width - (kBackgroundInset * 2), display_height - (kBackgroundInset * 2));
    lv_obj_set_style_radius(background_image, kBackgroundRadius, 0);
    lv_obj_set_style_clip_corner(background_image, true, 0);
    lv_obj_set_style_bg_color(background_image, lv_color_hex(t.screen_bg), 0);
    lv_obj_set_style_bg_opa(background_image, LV_OPA_COVER, 0);
    lv_obj_clear_flag(background_image, LV_OBJ_FLAG_HIDDEN);
  } else {
    set_hidden(background_image, true);
  }

  if (DeskClock::AssetService::imagePath(t.name, "mascot", asset_path, sizeof(asset_path))) {
    mascot_asset_active = set_image_asset(mascot_asset_image, mascot_runtime_image, asset_path, LV_IMAGE_ALIGN_CONTAIN, 180U * 180U);
  } else {
    mascot_asset_active = set_image_asset(mascot_asset_image, mascot_runtime_image, nullptr, LV_IMAGE_ALIGN_CONTAIN, 0);
  }
  if (DeskClock::AssetService::imagePath(t.name, "weather", asset_path, sizeof(asset_path))) {
    weather_asset_active = set_image_asset(weather_asset_image, weather_runtime_image, asset_path, LV_IMAGE_ALIGN_CONTAIN, 96U * 96U);
  } else {
    weather_asset_active = set_image_asset(weather_asset_image, weather_runtime_image, nullptr, LV_IMAGE_ALIGN_CONTAIN, 0);
  }
  if (DeskClock::AssetService::imagePath(t.name, "focus", asset_path, sizeof(asset_path))) {
    focus_asset_active = set_image_asset(focus_asset_image, focus_runtime_image, asset_path, LV_IMAGE_ALIGN_CONTAIN, 64U * 64U);
  } else {
    focus_asset_active = set_image_asset(focus_asset_image, focus_runtime_image, nullptr, LV_IMAGE_ALIGN_CONTAIN, 0);
  }
  if (DeskClock::AssetService::imagePath(t.name, "message", asset_path, sizeof(asset_path))) {
    message_asset_active = set_image_asset(message_asset_image, message_runtime_image, asset_path, LV_IMAGE_ALIGN_CONTAIN, 64U * 64U);
  } else {
    message_asset_active = set_image_asset(message_asset_image, message_runtime_image, nullptr, LV_IMAGE_ALIGN_CONTAIN, 0);
  }

  set_room_decor_hidden(background_asset_active);
  set_drawn_mascot_hidden(mascot_asset_active || background_asset_active);
  set_drawn_weather_hidden(weather_asset_active);
  set_hidden(focus_leaf, focus_asset_active);
  set_hidden(message_heart, message_asset_active);

  style_vertical_gradient(stage_card, t.stage_bg, t.screen_bg, 0);
  style_vertical_gradient(wall_panel, t.stage_bg, t.screen_bg, 0);
  style_vertical_gradient(floor_panel, t.floor_bg, t.dark ? 0x120D20 : 0xE7C18A, 0);
  if (background_asset_active) {
    lv_obj_set_style_bg_opa(stage_card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(wall_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(floor_panel, LV_OPA_TRANSP, 0);
  }

  style_card(left_date_card, t.card_bg, t.card_border, t.card_opa);
  style_card(calendar_card, t.card_bg, t.card_border, t.card_opa);
  style_card(right_status_card, t.card_bg, t.card_border, t.card_opa);
  style_card(right_alarm_card, t.card_bg, t.card_border, t.card_opa);
  style_card(right_message_card, t.card_bg, t.card_border, t.card_opa);

  style_soft_shape(window_panel, t.window_bg, 12);
  lv_obj_set_style_bg_opa(window_panel, t.dark ? LV_OPA_70 : LV_OPA_50, 0);
  lv_obj_set_style_border_width(window_panel, 1, 0);
  lv_obj_set_style_border_color(window_panel, lv_color_hex(t.card_border), 0);
  lv_obj_set_style_border_opa(window_panel, t.dark ? LV_OPA_60 : LV_OPA_40, 0);
  style_soft_shape(window_glow, t.glow, 14);
  lv_obj_set_style_bg_opa(window_glow, t.dark ? LV_OPA_50 : LV_OPA_60, 0);
  style_soft_shape(window_frame_left, t.card_border, 1);
  style_soft_shape(window_frame_right, t.card_border, 1);
  style_soft_shape(window_sill, t.floor_bg, 3);
  lv_obj_set_style_bg_opa(window_frame_left, t.dark ? LV_OPA_40 : LV_OPA_60, 0);
  lv_obj_set_style_bg_opa(window_frame_right, t.dark ? LV_OPA_40 : LV_OPA_60, 0);
  lv_obj_set_style_bg_opa(window_sill, t.dark ? LV_OPA_50 : LV_OPA_70, 0);
  style_soft_shape(curtain_left, t.dark ? 0x27163F : 0xF3C5B7, 18);
  style_soft_shape(curtain_right, t.dark ? 0x27163F : 0xF3C5B7, 18);
  lv_obj_set_style_bg_opa(curtain_left, t.dark ? LV_OPA_50 : LV_OPA_40, 0);
  lv_obj_set_style_bg_opa(curtain_right, t.dark ? LV_OPA_50 : LV_OPA_40, 0);
  style_soft_shape(moon, t.dark ? 0xD9B6FF : 0xF6C15D, LV_RADIUS_CIRCLE);
  if (moon != nullptr) {
    lv_obj_set_style_bg_opa(moon, t.dark ? LV_OPA_COVER : LV_OPA_40, 0);
  }
  style_soft_shape(moon_cutout, t.window_bg, LV_RADIUS_CIRCLE);
  if (moon_cutout != nullptr && !background_asset_active) {
    if (t.dark) {
      lv_obj_clear_flag(moon_cutout, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(moon_cutout, LV_OBJ_FLAG_HIDDEN);
    }
  }
  for (lv_obj_t *dot : star_dots) {
    style_soft_shape(dot, t.dark ? 0xF7E9FF : 0xF8D39C, LV_RADIUS_CIRCLE);
    if (dot != nullptr) {
      lv_obj_set_style_bg_opa(dot, t.dark ? LV_OPA_70 : LV_OPA_30, 0);
    }
  }

  style_soft_shape(picture_frame, t.floor_bg, 6);
  style_soft_shape(picture_inner, t.card_bg, 4);
  style_soft_shape(pendant_cord, t.accent, 0);
  style_soft_shape(pendant_lamp, t.glow, LV_RADIUS_CIRCLE);
  if (pendant_lamp != nullptr) {
    lv_obj_set_style_shadow_width(pendant_lamp, 12, 0);
    lv_obj_set_style_shadow_opa(pendant_lamp, t.dark ? LV_OPA_40 : LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(pendant_lamp, lv_color_hex(t.glow), 0);
  }
  style_soft_shape(shelf_shadow, t.dark ? 0x0B0712 : 0xB98A52, 2);
  lv_obj_set_style_bg_opa(shelf_shadow, t.dark ? LV_OPA_50 : LV_OPA_40, 0);
  style_soft_shape(shelf, t.floor_bg, 3);
  style_soft_shape(house_body, t.card_bg, 3);
  style_soft_shape(house_roof, t.accent, 2);
  style_soft_shape(plant_pot, t.card_bg, 6);
  style_soft_shape(plant_leaf_left, t.dark ? 0x587346 : 0x78945B, LV_RADIUS_CIRCLE);
  style_soft_shape(plant_leaf_right, t.dark ? 0x4F6B3F : 0x6F8C51, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_ear_left, t.mascot, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_ear_right, t.mascot, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_head, t.mascot, 24);
  style_soft_shape(mascot_eye_left, t.text, LV_RADIUS_CIRCLE);
  style_soft_shape(mascot_eye_right, t.text, LV_RADIUS_CIRCLE);
  style_soft_shape(weather_sun, t.accent, LV_RADIUS_CIRCLE);
  style_soft_shape(weather_cloud_left, t.dark ? 0xF4F1FF : 0xFFFFFF, LV_RADIUS_CIRCLE);
  style_soft_shape(weather_cloud_right, t.dark ? 0xF4F1FF : 0xFFFFFF, LV_RADIUS_CIRCLE);
  style_soft_shape(weather_cloud_base, t.dark ? 0xF4F1FF : 0xFFFFFF, 8);
  style_soft_shape(focus_ring, t.accent, LV_RADIUS_CIRCLE);
  style_soft_shape(focus_ring_gap, t.card_bg, LV_RADIUS_CIRCLE);
  style_soft_shape(focus_leaf, 0x6B8F3F, LV_RADIUS_CIRCLE);
  style_soft_shape(message_heart, t.accent_2, LV_RADIUS_CIRCLE);

  set_text_color(time_shadow_label, t.time_shadow);
  set_text_color(ampm_shadow_label, t.time_shadow);
  set_text_color(seconds_shadow_label, t.time_shadow);
  if (time_shadow_label != nullptr) {
    lv_obj_set_style_text_opa(time_shadow_label, t.dark ? LV_OPA_70 : LV_OPA_40, 0);
  }
  if (ampm_shadow_label != nullptr) {
    lv_obj_set_style_text_opa(ampm_shadow_label, t.dark ? LV_OPA_70 : LV_OPA_30, 0);
  }
  if (seconds_shadow_label != nullptr) {
    lv_obj_set_style_text_opa(seconds_shadow_label, t.dark ? LV_OPA_70 : LV_OPA_30, 0);
  }
  set_text_color(time_label, t.accent_2);
  set_text_color(ampm_label, t.accent);
  set_text_color(seconds_label, t.accent_2);
  set_text_color(greeting_label, t.text);
  set_text_color(setup_hint_label, t.accent);
  set_text_color(weekday_label, t.accent);
  set_text_color(month_label, t.text);
  set_text_color(day_label, t.text);
  set_text_color(calendar_month_label, t.text);
  set_text_color(status_label, t.muted);
  set_text_color(weather_label, t.text);
  set_text_color(weather_detail_label, t.text);
  set_text_color(alarm_title_label, t.accent);
  set_text_color(next_alarm_label, t.text);
  set_text_color(message_label, t.text);
  set_text_color(theme_name_label, t.faint);
  set_text_color(mascot_mouth, t.text);

  for (lv_obj_t *label : calendar_weekday_labels) {
    set_text_color(label, t.faint);
  }
  for (lv_obj_t *label : calendar_day_labels) {
    set_text_color(label, t.text);
  }

  if (setup_hint_label != nullptr) {
    lv_obj_set_style_bg_color(setup_hint_label, lv_color_hex(t.card_bg), 0);
    lv_obj_set_style_bg_grad_dir(setup_hint_label, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(setup_hint_label, LV_OPA_80, 0);
    lv_obj_set_style_radius(setup_hint_label, 10, 0);
    lv_obj_set_style_pad_hor(setup_hint_label, 8, 0);
    lv_obj_set_style_pad_ver(setup_hint_label, 3, 0);
  }
}

void update_clock_from_time_service(lv_timer_t *)
{
  const DeskClock::TimeSnapshot snapshot = DeskClock::TimeService::snapshot();
  const bool blink = snapshot.now.valid ? ((snapshot.now.second % 2U) == 0U) : true;
  const VisualTheme &t = theme();

  if (theme_name_label != nullptr) {
    char theme_buffer[24];
    snprintf(theme_buffer, sizeof(theme_buffer), "%s theme", t.name);
    lv_label_set_text(theme_name_label, theme_buffer);
  }
  if (setup_hint_label != nullptr) {
    if (DeskClock::SettingsService::snapshot().configured) {
      lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(setup_hint_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (!snapshot.now.valid) {
    lv_label_set_text(time_shadow_label, "--:--");
    lv_label_set_text(time_label, "--:--");
    lv_label_set_text(ampm_shadow_label, "");
    lv_label_set_text(ampm_label, "");
    lv_label_set_text(seconds_shadow_label, "set");
    lv_label_set_text(seconds_label, "set");
    lv_label_set_text(greeting_label, "Set the time to start");
    lv_label_set_text(status_label, DeskClock::ClockDisplayFormatter::statusText(snapshot));
    lv_label_set_text(weather_label, "--°C");
    lv_label_set_text(weather_detail_label, "Weather\nopt-in");
    lv_label_set_text(next_alarm_label, "Deep Work\nSet time first");
    lv_label_set_text(message_label, "Hold BOOT\nfor setup");
    lv_obj_set_style_bg_color(sync_dot, lv_color_hex(DeskClock::ClockDisplayFormatter::syncDotColor(DeskClock::SyncState::Unreliable, blink)), 0);
    update_calendar_cards(snapshot.now);
    DeskClock::AlarmAlertView::update(snapshot.now);
    realign_time_details();
    return;
  }

  char time_buffer[12];
  format_primary_time(time_buffer, sizeof(time_buffer), snapshot.now.hour, snapshot.now.minute);
  lv_label_set_text(time_shadow_label, time_buffer);
  lv_label_set_text(time_label, time_buffer);

  const char *period = "";
  if (!DeskClock::TimeSetupView::use24HourFormat()) {
    period = snapshot.now.hour >= 12 ? "PM" : "AM";
  }
  lv_label_set_text(ampm_shadow_label, period);
  lv_label_set_text(ampm_label, period);

  char seconds_buffer[8];
  snprintf(seconds_buffer, sizeof(seconds_buffer), "%02u", snapshot.now.second);
  lv_label_set_text(seconds_shadow_label, seconds_buffer);
  lv_label_set_text(seconds_label, seconds_buffer);

  char greeting_buffer[48];
  snprintf(greeting_buffer, sizeof(greeting_buffer), "%s, Alex", greeting_for_hour(snapshot.now.hour));
  lv_label_set_text(greeting_label, greeting_buffer);

  lv_label_set_text(status_label, DeskClock::ClockDisplayFormatter::statusText(snapshot));
  lv_label_set_text(weather_label, "24°C");
  lv_label_set_text(weather_detail_label, "Cloudy\n26 / 18");
  lv_label_set_text(message_label, "You got this!");
  lv_label_set_text(next_alarm_label, "Deep Work\nEnds 12:00");
  update_calendar_cards(snapshot.now);
  lv_obj_set_style_bg_color(sync_dot, lv_color_hex(DeskClock::ClockDisplayFormatter::syncDotColor(snapshot.sync_state, blink)), 0);

  DeskClock::AlarmAlertView::update(snapshot.now);
  realign_time_details();
}

} // namespace

extern "C" void clock_face_refresh_theme(void)
{
  apply_theme_to_static_objects();
  update_clock_from_time_service(nullptr);
}

extern "C" void clock_face_create(void)
{
  DeskClock::TimeSetupView::loadPreferences();
  root_screen = lv_screen_active();
  lv_obj_clean(root_screen);

  const int32_t width = lv_display_get_horizontal_resolution(nullptr);
  const int32_t height = lv_display_get_vertical_resolution(nullptr);
  const int32_t margin = 8;
  const int32_t gap = 8;
  const int32_t left_width = 108;
  const int32_t right_width = 124;
  const int32_t right_x = width - margin - right_width;
  const int32_t center_x = margin + left_width + gap;
  const int32_t center_width = right_x - gap - center_x;
  const int32_t content_height = height - (margin * 2);

  background_image = lv_image_create(root_screen);
  lv_obj_remove_style_all(background_image);
  lv_obj_set_pos(background_image, kBackgroundInset, kBackgroundInset);
  lv_obj_set_size(background_image, width - (kBackgroundInset * 2), height - (kBackgroundInset * 2));
  lv_image_set_inner_align(background_image, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_set_style_radius(background_image, kBackgroundRadius, 0);
  lv_obj_set_style_clip_corner(background_image, true, 0);
  lv_obj_add_flag(background_image, LV_OBJ_FLAG_HIDDEN);

  stage_card = plain_obj(root_screen, 0, 0, width, height);
  left_date_card = card(root_screen, margin, margin, left_width, 64);
  calendar_card = card(root_screen, margin, margin + 72, left_width, content_height - 72);
  right_status_card = card(root_screen, right_x, margin + 4, right_width, 48);
  right_alarm_card = card(root_screen, right_x, margin + 60, right_width, 50);
  right_message_card = card(root_screen, right_x + 44, margin + 112, right_width - 44, content_height - 112);

  lv_obj_add_flag(right_alarm_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(right_alarm_card, open_alarms_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(right_message_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(right_message_card, open_brightness_event, LV_EVENT_CLICKED, nullptr);

  const int32_t window_x = center_x + 52;
  const int32_t window_w = center_width - 88;
  wall_panel = plain_obj(stage_card, 0, 0, width, height);
  window_panel = plain_obj(stage_card, window_x, 0, window_w, 116);
  window_glow = plain_obj(stage_card, window_x + 26, 18, window_w - 52, 78);
  curtain_left = plain_obj(stage_card, window_x - 26, 0, 42, 118);
  curtain_right = plain_obj(stage_card, window_x + window_w - 16, 0, 44, 118);
  window_frame_left = plain_obj(stage_card, window_x, 0, 3, 118);
  window_frame_right = plain_obj(stage_card, window_x + window_w - 3, 0, 3, 118);
  window_sill = plain_obj(stage_card, window_x - 7, 108, window_w + 14, 7);
  floor_panel = plain_obj(stage_card, 0, height - 48, width, 48);
  moon = plain_obj(stage_card, window_x + window_w - 100, 27, 18, 18);
  moon_cutout = plain_obj(stage_card, window_x + window_w - 94, 22, 18, 18);

  struct DotSpec {
    int16_t x;
    int16_t y;
    int16_t size;
  };
  const DotSpec dots[] = {
      {static_cast<int16_t>(window_x + 38), 24, 2},  {static_cast<int16_t>(window_x + 86), 14, 2},
      {static_cast<int16_t>(window_x + 132), 31, 3}, {static_cast<int16_t>(window_x + 190), 21, 2},
      {static_cast<int16_t>(window_x + 225), 42, 2}, {static_cast<int16_t>(window_x + 74), 56, 2},
      {static_cast<int16_t>(window_x + 155), 64, 2}, {static_cast<int16_t>(window_x + 252), 62, 3},
      {static_cast<int16_t>(window_x + 48), 82, 2},  {static_cast<int16_t>(window_x + 210), 84, 2},
  };
  for (uint8_t index = 0; index < 10; ++index) {
    star_dots[index] = plain_obj(stage_card, dots[index].x, dots[index].y, dots[index].size, dots[index].size);
  }

  // Lightweight decorative room/plant/mascot shapes. They keep the bundled
  // firmware themeable without shipping branded character artwork.
  picture_frame = plain_obj(stage_card, center_x + 18, 36, 38, 48);
  picture_inner = plain_obj(picture_frame, 5, 5, 28, 38);
  pendant_cord = plain_obj(stage_card, center_x + 84, 0, 2, 38);
  pendant_lamp = plain_obj(stage_card, center_x + 68, 33, 34, 22);
  shelf_shadow = plain_obj(stage_card, center_x - 4, height - 18, 132, 6);
  shelf = plain_obj(stage_card, center_x, height - 25, 126, 8);
  house_body = plain_obj(stage_card, center_x + 96, height - 55, 30, 28);
  house_roof = plain_obj(stage_card, center_x + 101, height - 66, 20, 18);
  plant_pot = plain_obj(stage_card, center_x + 32, height - 67, 28, 42);
  plant_leaf_left = plain_obj(stage_card, center_x + 19, height - 93, 26, 16);
  plant_leaf_right = plain_obj(stage_card, center_x + 47, height - 96, 28, 16);
  mascot_ear_left = plain_obj(stage_card, right_x - 62, height - 66, 12, 38);
  mascot_ear_right = plain_obj(stage_card, right_x - 40, height - 66, 12, 38);
  mascot_head = plain_obj(stage_card, right_x - 78, height - 42, 62, 37);
  mascot_eye_left = plain_obj(mascot_head, 17, 15, 6, 6);
  mascot_eye_right = plain_obj(mascot_head, 36, 15, 6, 6);
  mascot_mouth = lv_label_create(mascot_head);
  lv_obj_set_style_text_font(mascot_mouth, small_font(), 0);
  lv_label_set_text(mascot_mouth, "x");
  lv_obj_align(mascot_mouth, LV_ALIGN_CENTER, 0, 8);

  mascot_asset_image = lv_image_create(stage_card);
  lv_obj_remove_style_all(mascot_asset_image);
  lv_obj_set_pos(mascot_asset_image, right_x - 96, height - 84);
  lv_obj_set_size(mascot_asset_image, 104, 82);
  lv_image_set_inner_align(mascot_asset_image, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_add_flag(mascot_asset_image, LV_OBJ_FLAG_HIDDEN);

  weekday_label = lv_label_create(left_date_card);
  lv_obj_set_style_text_font(weekday_label, small_font(), 0);
  lv_label_set_text(weekday_label, "WEDNESDAY");
  lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 0, 8);

  month_label = lv_label_create(left_date_card);
  lv_obj_set_style_text_font(month_label, small_font(), 0);
  lv_label_set_text(month_label, "MAY");
  lv_obj_align(month_label, LV_ALIGN_TOP_MID, 0, 20);

  day_label = lv_label_create(left_date_card);
  lv_obj_set_style_text_font(day_label, seconds_font(), 0);
  lv_label_set_text(day_label, "22");
  lv_obj_align(day_label, LV_ALIGN_BOTTOM_MID, 0, -1);

  calendar_month_label = lv_label_create(calendar_card);
  lv_obj_set_style_text_font(calendar_month_label, small_font(), 0);
  lv_label_set_text(calendar_month_label, "MAY 2024");
  lv_obj_align(calendar_month_label, LV_ALIGN_TOP_MID, 0, 5);

  static constexpr const char *weekdays[] = {"S", "M", "T", "W", "T", "F", "S"};
  for (uint8_t index = 0; index < 7; ++index) {
    calendar_weekday_labels[index] = lv_label_create(calendar_card);
    lv_obj_set_style_text_font(calendar_weekday_labels[index], small_font(), 0);
    lv_label_set_text(calendar_weekday_labels[index], weekdays[index]);
    lv_obj_set_size(calendar_weekday_labels[index], 13, 12);
    lv_obj_set_style_text_align(calendar_weekday_labels[index], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(calendar_weekday_labels[index], 8 + (index * 13), 20);
  }

  for (uint8_t row = 0; row < 6; ++row) {
    for (uint8_t column = 0; column < 7; ++column) {
      const uint8_t index = static_cast<uint8_t>((row * 7U) + column);
      calendar_day_labels[index] = lv_label_create(calendar_card);
      lv_obj_set_style_text_font(calendar_day_labels[index], small_font(), 0);
      lv_obj_set_size(calendar_day_labels[index], 13, 12);
      lv_obj_set_style_text_align(calendar_day_labels[index], LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(calendar_day_labels[index], 8 + (column * 13), 31 + (row * 9));
      lv_label_set_text(calendar_day_labels[index], "");
    }
  }

  time_shadow_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(time_shadow_label, time_font(), 0);
  lv_label_set_text(time_shadow_label, "--:--");

  time_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(time_label, time_font(), 0);
  lv_label_set_text(time_label, "--:--");

  ampm_shadow_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(ampm_shadow_label, body_font(), 0);
  lv_label_set_text(ampm_shadow_label, "");

  ampm_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(ampm_label, body_font(), 0);
  lv_label_set_text(ampm_label, "");

  seconds_shadow_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(seconds_shadow_label, seconds_font(), 0);
  lv_label_set_text(seconds_shadow_label, "set");

  seconds_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(seconds_label, seconds_font(), 0);
  lv_label_set_text(seconds_label, "set");

  greeting_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(greeting_label, body_font(), 0);
  lv_label_set_text(greeting_label, "Good morning, Alex");
  lv_obj_align(greeting_label, LV_ALIGN_BOTTOM_MID, 0, -36);

  setup_hint_label = lv_label_create(stage_card);
  lv_obj_set_style_text_font(setup_hint_label, small_font(), 0);
  lv_label_set_text(setup_hint_label, "setup time / Wi-Fi");
  lv_obj_align(setup_hint_label, LV_ALIGN_TOP_MID, 0, 7);
  lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(setup_hint_label, open_system_menu_event, LV_EVENT_CLICKED, nullptr);
  if (DeskClock::SettingsService::snapshot().configured) {
    lv_obj_add_flag(setup_hint_label, LV_OBJ_FLAG_HIDDEN);
  }

  weather_sun = plain_obj(right_status_card, 32, 11, 18, 18);
  weather_cloud_left = plain_obj(right_status_card, 18, 22, 24, 19);
  weather_cloud_right = plain_obj(right_status_card, 35, 20, 24, 20);
  weather_cloud_base = plain_obj(right_status_card, 18, 29, 42, 14);
  weather_asset_image = lv_image_create(right_status_card);
  lv_obj_remove_style_all(weather_asset_image);
  lv_obj_set_pos(weather_asset_image, 14, 7);
  lv_obj_set_size(weather_asset_image, 50, 35);
  lv_image_set_inner_align(weather_asset_image, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_add_flag(weather_asset_image, LV_OBJ_FLAG_HIDDEN);

  sync_dot = plain_obj(right_status_card, right_width - 15, 8, 7, 7);
  lv_obj_set_style_radius(sync_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(sync_dot, LV_OPA_COVER, 0);

  weather_label = lv_label_create(right_status_card);
  lv_obj_set_style_text_font(weather_label, body_font(), 0);
  lv_label_set_text(weather_label, "24°C");
  lv_obj_set_pos(weather_label, 72, 6);

  weather_detail_label = lv_label_create(right_status_card);
  lv_obj_set_style_text_font(weather_detail_label, small_font(), 0);
  lv_label_set_text(weather_detail_label, "Cloudy\n26 / 18");
  lv_obj_set_pos(weather_detail_label, 72, 25);

  status_label = lv_label_create(right_status_card);
  lv_obj_set_style_text_font(status_label, small_font(), 0);
  lv_label_set_text(status_label, "time setup");
  lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -12, -5);
  lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(status_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status_label, open_system_menu_event, LV_EVENT_CLICKED, nullptr);

  focus_leaf = plain_obj(right_alarm_card, 14, 16, 13, 10);
  focus_asset_image = lv_image_create(right_alarm_card);
  lv_obj_remove_style_all(focus_asset_image);
  lv_obj_set_pos(focus_asset_image, 9, 12);
  lv_obj_set_size(focus_asset_image, 24, 24);
  lv_image_set_inner_align(focus_asset_image, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_add_flag(focus_asset_image, LV_OBJ_FLAG_HIDDEN);

  alarm_title_label = lv_label_create(right_alarm_card);
  lv_obj_set_style_text_font(alarm_title_label, small_font(), 0);
  lv_label_set_text(alarm_title_label, "FOCUS MODE");
  lv_obj_align(alarm_title_label, LV_ALIGN_TOP_LEFT, 34, 8);

  next_alarm_label = lv_label_create(right_alarm_card);
  lv_obj_set_style_text_font(next_alarm_label, small_font(), 0);
  lv_obj_set_width(next_alarm_label, right_width - 70);
  lv_label_set_long_mode(next_alarm_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(next_alarm_label, "Deep Work\nEnds 12:00");
  lv_obj_align(next_alarm_label, LV_ALIGN_BOTTOM_LEFT, 34, -7);
  lv_obj_add_flag(next_alarm_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(next_alarm_label, open_alarms_event, LV_EVENT_CLICKED, nullptr);

  focus_ring = plain_obj(right_alarm_card, right_width - 32, 15, 27, 27);
  focus_ring_gap = plain_obj(right_alarm_card, right_width - 26, 21, 15, 15);

  message_label = lv_label_create(right_message_card);
  lv_obj_set_style_text_font(message_label, small_font(), 0);
  lv_label_set_text(message_label, "You got this!");
  lv_obj_align(message_label, LV_ALIGN_TOP_LEFT, 10, 10);

  message_heart = plain_obj(right_message_card, 12, 30, 9, 9);
  message_asset_image = lv_image_create(right_message_card);
  lv_obj_remove_style_all(message_asset_image);
  lv_obj_set_pos(message_asset_image, 8, 27);
  lv_obj_set_size(message_asset_image, 18, 18);
  lv_image_set_inner_align(message_asset_image, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_add_flag(message_asset_image, LV_OBJ_FLAG_HIDDEN);

  theme_name_label = lv_label_create(right_message_card);
  lv_obj_set_style_text_font(theme_name_label, small_font(), 0);
  lv_label_set_text(theme_name_label, "theme");
  lv_obj_align(theme_name_label, LV_ALIGN_BOTTOM_RIGHT, -8, -5);
  lv_obj_add_flag(theme_name_label, LV_OBJ_FLAG_HIDDEN);

  apply_theme_to_static_objects();
  realign_time_details();

  DeskClock::BrightnessSettingsView::create(root_screen, width, height, body_font());
  DeskClock::TimeSetupView::create(root_screen, width, height, body_font());
  DeskClock::NetworkSetupView::create(root_screen, width, height, body_font());
  DeskClock::AlarmManagerView::create(root_screen, width, height, body_font());
  DeskClock::SystemMenuView::create(root_screen, width, height, body_font());
  DeskClock::AlarmAlertView::create(root_screen, width, height, time_font());

  lv_timer_create(update_clock_from_time_service, 250, nullptr);
  update_clock_from_time_service(nullptr);
}
