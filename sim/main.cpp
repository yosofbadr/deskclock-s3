#include "Arduino.h"

#include "app/AlarmManagerView.h"
#include "app/AlarmService.h"
#include "app/AlarmToneService.h"
#include "app/BrightnessService.h"
#include "app/BrightnessSettingsView.h"
#include "app/NetworkService.h"
#include "app/NetworkSetupView.h"
#include "app/SettingsService.h"
#include "app/TimeService.h"
#include "app/TimeSetupView.h"
#include "i2c_bsp.h"
#include "Preferences.h"
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/others/snapshot/lv_snapshot.h"
#include "src/widgets/button/lv_button.h"
#include "src/widgets/label/lv_label.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

extern "C" void clock_face_create(void);
extern "C" void deskclock_sim_set_epoch(time_t epoch);

namespace {
constexpr int kDisplayWidth = 640;
constexpr int kDisplayHeight = 172;

struct Options {
  std::string screenshot_path;
  std::string open_view;
  std::string fixed_time;
  bool dump_layout = false;
  bool reference_scene = false;
  uint32_t run_ms = 0;
};

Options parse_options(int argc, char **argv)
{
  Options options;
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--screenshot") == 0 && index + 1 < argc) {
      options.screenshot_path = argv[++index];
    } else if (std::strcmp(argv[index], "--open") == 0 && index + 1 < argc) {
      options.open_view = argv[++index];
    } else if (std::strcmp(argv[index], "--dump-layout") == 0) {
      options.dump_layout = true;
    } else if (std::strcmp(argv[index], "--fixed-time") == 0 && index + 1 < argc) {
      options.fixed_time = argv[++index];
    } else if (std::strcmp(argv[index], "--reference-scene") == 0) {
      options.reference_scene = true;
      if (options.fixed_time.empty()) {
        options.fixed_time = "2024-05-22T10:24:36";
      }
    } else if (std::strcmp(argv[index], "--run-ms") == 0 && index + 1 < argc) {
      options.run_ms = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
    } else if (std::strcmp(argv[index], "--help") == 0) {
      std::cout << "Usage: deskclock_sim [--screenshot out.ppm] [--open time|alarms|brightness|network] [--dump-layout] [--fixed-time YYYY-MM-DDTHH:MM:SS] [--reference-scene] [--run-ms milliseconds]\n";
      std::exit(0);
    }
  }
  return options;
}

bool parse_fixed_time(const std::string &value, time_t &epoch)
{
  if (value.empty()) {
    return false;
  }

  std::tm tm = {};
  char separator = 'T';
  if (std::sscanf(value.c_str(), "%d-%d-%d%c%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &separator, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 7) {
    return false;
  }
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
#if defined(__APPLE__) || defined(__linux__)
  epoch = timegm(&tm);
#else
  epoch = std::mktime(&tm);
#endif
  return epoch != static_cast<time_t>(-1);
}

void seed_reference_preferences()
{
  Preferences preferences;
  if (preferences.begin("deskclock", false)) {
    preferences.putBool("configured", true);
    preferences.putBool("time24", false);
    preferences.putUChar("theme", 0);
    preferences.end();
  }
}

void initialize_app(const Options &options)
{
  Serial.begin(115200);
  Serial.println("DeskClock S3 simulator starting");

  i2c_master_Init();
  if (options.reference_scene) {
    seed_reference_preferences();
  }
  if (!options.fixed_time.empty()) {
    time_t fixed_epoch = 0;
    if (parse_fixed_time(options.fixed_time, fixed_epoch)) {
      deskclock_sim_set_epoch(fixed_epoch);
    } else {
      std::cerr << "Invalid --fixed-time value: " << options.fixed_time << "\n";
    }
  }
  DeskClock::SettingsService::begin();
  DeskClock::NetworkService::begin();
  DeskClock::TimeService::begin();
  DeskClock::AlarmService::begin(DeskClock::TimeService::snapshot().now);
  DeskClock::AlarmToneService::begin();
  DeskClock::BrightnessService::begin();
}

void run_app_services()
{
  DeskClock::TimeService::loop();
  const DeskClock::TimeSnapshot snapshot = DeskClock::TimeService::snapshot();
  DeskClock::AlarmService::loop(snapshot.now);
  DeskClock::AlarmToneService::loop(DeskClock::AlarmService::activeAlert());
  DeskClock::BrightnessService::loop(snapshot.now);
  DeskClock::NetworkService::loop();
}

void pump_for(uint32_t milliseconds)
{
  const uint32_t started_at = millis();
  while (millis() - started_at < milliseconds) {
    run_app_services();
    const uint32_t wait = lv_timer_handler();
    delay(wait == 0 ? 5 : std::min<uint32_t>(wait, 20));
  }
}

std::string object_label(lv_obj_t *obj)
{
  if (lv_obj_check_type(obj, &lv_label_class)) {
    return lv_label_get_text(obj);
  }
  const uint32_t children = lv_obj_get_child_count(obj);
  for (uint32_t index = 0; index < children; ++index) {
    lv_obj_t *child = lv_obj_get_child(obj, index);
    if (lv_obj_check_type(child, &lv_label_class)) {
      return lv_label_get_text(child);
    }
  }
  return "";
}

void dump_layout(lv_obj_t *obj, int depth = 0)
{
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  const bool clickable = lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  const std::string label = object_label(obj);
  for (int index = 0; index < depth; ++index) {
    std::cout << "  ";
  }
  std::cout << (clickable ? "* " : "- ") << coords.x1 << "," << coords.y1 << " "
            << (coords.x2 - coords.x1 + 1) << "x" << (coords.y2 - coords.y1 + 1);
  if (!label.empty()) {
    std::cout << "  \"" << label << "\"";
  }
  std::cout << "\n";

  const uint32_t children = lv_obj_get_child_count(obj);
  for (uint32_t index = 0; index < children; ++index) {
    dump_layout(lv_obj_get_child(obj, index), depth + 1);
  }
}

bool save_snapshot_ppm(const std::string &path)
{
#if LV_USE_SNAPSHOT
  lv_obj_update_layout(lv_screen_active());
  lv_refr_now(nullptr);
  lv_draw_buf_t *snapshot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
  if (snapshot == nullptr || snapshot->data == nullptr) {
    std::cerr << "Failed to create LVGL snapshot\n";
    return false;
  }

  std::ofstream output(path, std::ios::binary);
  if (!output) {
    std::cerr << "Failed to open screenshot path: " << path << "\n";
    lv_draw_buf_destroy(snapshot);
    return false;
  }

  const uint32_t width = snapshot->header.w;
  const uint32_t height = snapshot->header.h;
  const uint32_t stride = snapshot->header.stride;
  output << "P6\n" << width << " " << height << "\n255\n";
  for (uint32_t y = 0; y < height; ++y) {
    const uint8_t *row = snapshot->data + (y * stride);
    for (uint32_t x = 0; x < width; ++x) {
      const uint8_t *pixel = row + (x * 4); // LV_COLOR_FORMAT_ARGB8888 is BGRA in memory.
      const char rgb[3] = {static_cast<char>(pixel[2]), static_cast<char>(pixel[1]), static_cast<char>(pixel[0])};
      output.write(rgb, sizeof(rgb));
    }
  }
  lv_draw_buf_destroy(snapshot);
  return true;
#else
  (void)path;
  std::cerr << "LV_USE_SNAPSHOT is disabled\n";
  return false;
#endif
}
} // namespace

int main(int argc, char **argv)
{
  const Options options = parse_options(argc, argv);
  if (!options.screenshot_path.empty()) {
    setenv("SDL_VIDEODRIVER", "dummy", 0);
  }

  initialize_app(options);

  lv_init();
  lv_display_t *display = lv_sdl_window_create(kDisplayWidth, kDisplayHeight);
  lv_sdl_window_set_title(display, "DeskClock S3 simulator");
  lv_sdl_window_set_resizeable(display, false);
  lv_sdl_mouse_create();
  lv_sdl_keyboard_create();

  clock_face_create();
  if (options.open_view == "time") {
    DeskClock::TimeSetupView::open();
  } else if (options.open_view == "alarms") {
    DeskClock::AlarmManagerView::open();
  } else if (options.open_view == "brightness") {
    DeskClock::BrightnessSettingsView::open();
  } else if (options.open_view == "network") {
    DeskClock::NetworkSetupView::open();
  } else if (!options.open_view.empty()) {
    std::cerr << "Unknown --open view: " << options.open_view << "\n";
    return 2;
  }
  pump_for(options.run_ms == 0 ? 500 : options.run_ms);

  if (options.dump_layout) {
    dump_layout(lv_screen_active());
    if (options.screenshot_path.empty()) {
      lv_sdl_quit();
      return 0;
    }
  }

  if (!options.screenshot_path.empty()) {
    const bool ok = save_snapshot_ppm(options.screenshot_path);
    lv_sdl_quit();
    return ok ? 0 : 1;
  }

  while (true) {
    pump_for(50);
  }
}
