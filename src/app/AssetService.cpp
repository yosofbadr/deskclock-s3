#include "AssetService.h"

#include <stdio.h>
#include <string.h>

#ifdef DESKCLOCK_SIMULATOR
#include <sys/stat.h>
#else
#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#endif

namespace DeskClock {
namespace {

constexpr const char *kLvglDrive = "S:";
constexpr const char *kAssetSubdir = "/deskclock";

#ifdef DESKCLOCK_SIMULATOR
constexpr const char *kHostAssetRoot = ".pio/sdcard";
#else
constexpr const char *kHostAssetRoot = "/sdcard";
#endif

bool attempted = false;
bool mounted = false;

#ifndef DESKCLOCK_SIMULATOR
sdmmc_card_t *sd_card = nullptr;
#endif

bool file_exists(const char *path)
{
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    return false;
  }
  fclose(file);
  return true;
}

bool build_lvgl_path(const char *theme_name, const char *asset_name, const char *extension, char *out, size_t out_size)
{
  if (theme_name == nullptr || asset_name == nullptr || extension == nullptr || out == nullptr || out_size == 0) {
    return false;
  }
  const int written = snprintf(out,
                               out_size,
                               "%s%s/themes/%s/%s.%s",
                               kLvglDrive,
                               kAssetSubdir,
                               theme_name,
                               asset_name,
                               extension);
  return written > 0 && static_cast<size_t>(written) < out_size;
}

bool build_common_lvgl_path(const char *asset_name, const char *extension, char *out, size_t out_size)
{
  if (asset_name == nullptr || extension == nullptr || out == nullptr || out_size == 0) {
    return false;
  }
  const int written = snprintf(out, out_size, "%s%s/common/%s.%s", kLvglDrive, kAssetSubdir, asset_name, extension);
  return written > 0 && static_cast<size_t>(written) < out_size;
}

bool lvgl_path_exists(const char *lvgl_path)
{
  char host_path[192];
  return DeskClock::AssetService::hostPathForLvglPath(lvgl_path, host_path, sizeof(host_path)) && file_exists(host_path);
}

bool find_asset_with_extensions(const char *theme_name,
                                const char *asset_name,
                                const char *const *extensions,
                                size_t extension_count,
                                char *out,
                                size_t out_size)
{
  if (!mounted || theme_name == nullptr || asset_name == nullptr || extensions == nullptr || out == nullptr || out_size == 0) {
    return false;
  }

  char candidate[160];
  for (size_t index = 0; index < extension_count; ++index) {
    if (build_lvgl_path(theme_name, asset_name, extensions[index], candidate, sizeof(candidate)) && lvgl_path_exists(candidate)) {
      snprintf(out, out_size, "%s", candidate);
      return true;
    }
  }
  for (size_t index = 0; index < extension_count; ++index) {
    if (build_common_lvgl_path(asset_name, extensions[index], candidate, sizeof(candidate)) && lvgl_path_exists(candidate)) {
      snprintf(out, out_size, "%s", candidate);
      return true;
    }
  }
  return false;
}

} // namespace

namespace AssetService {

void begin()
{
  if (attempted) {
    return;
  }
  attempted = true;

#ifdef DESKCLOCK_SIMULATOR
  struct stat info = {};
  mounted = stat(kHostAssetRoot, &info) == 0 && S_ISDIR(info.st_mode);
  return;
#else
  // Waveshare's SD-card example enables GPIO8 before SDMMC init. The pin is
  // also used for the LCD backlight later, but setting it high here is harmless
  // and keeps the card powered/ready before LVGL takes over PWM control.
  gpio_config_t sd_enable_pin = {};
  sd_enable_pin.intr_type = GPIO_INTR_DISABLE;
  sd_enable_pin.mode = GPIO_MODE_OUTPUT;
  sd_enable_pin.pin_bit_mask = 1ULL << GPIO_NUM_8;
  sd_enable_pin.pull_down_en = GPIO_PULLDOWN_DISABLE;
  sd_enable_pin.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&sd_enable_pin);
  gpio_set_level(GPIO_NUM_8, 1);
  delay(3000);

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 8;
  mount_config.allocation_unit_size = 16 * 1024 * 3;

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;
  slot_config.clk = GPIO_NUM_41;
  slot_config.cmd = GPIO_NUM_39;
  slot_config.d0 = GPIO_NUM_40;
  slot_config.d1 = GPIO_NUM_NC;
  slot_config.d2 = GPIO_NUM_NC;
  slot_config.d3 = GPIO_NUM_NC;
  slot_config.d4 = GPIO_NUM_NC;
  slot_config.d5 = GPIO_NUM_NC;
  slot_config.d6 = GPIO_NUM_NC;
  slot_config.d7 = GPIO_NUM_NC;
  slot_config.cd = SDMMC_SLOT_NO_CD;
  slot_config.wp = SDMMC_SLOT_NO_WP;

  const esp_err_t result = esp_vfs_fat_sdmmc_mount(kHostAssetRoot, &host, &slot_config, &mount_config, &sd_card);
  mounted = result == ESP_OK && sd_card != nullptr;
  if (mounted) {
    Serial.println("AssetService: SD card mounted at /sdcard");
    sdmmc_card_print_info(stdout, sd_card);
  } else {
    Serial.printf("AssetService: SD card mount failed: %s\n", esp_err_to_name(result));
  }
#endif
}

bool available()
{
  return mounted;
}

bool backgroundPath(const char *theme_name, char *out, size_t out_size)
{
  static constexpr const char *extensions[] = {"jpg", "jpeg"};
  return find_asset_with_extensions(theme_name, "background", extensions, sizeof(extensions) / sizeof(extensions[0]), out, out_size);
}

bool imagePath(const char *theme_name, const char *asset_name, char *out, size_t out_size)
{
  static constexpr const char *extensions[] = {"png", "jpg", "jpeg"};
  return find_asset_with_extensions(theme_name, asset_name, extensions, sizeof(extensions) / sizeof(extensions[0]), out, out_size);
}

bool hostPathForLvglPath(const char *lvgl_path, char *out, size_t out_size)
{
  if (lvgl_path == nullptr || out == nullptr || out_size == 0) {
    return false;
  }
  if (strncmp(lvgl_path, kLvglDrive, strlen(kLvglDrive)) != 0) {
    return false;
  }

  const char *suffix = lvgl_path + strlen(kLvglDrive);
  const int written = snprintf(out, out_size, "%s%s", kHostAssetRoot, suffix);
  return written > 0 && static_cast<size_t>(written) < out_size;
}

} // namespace AssetService
} // namespace DeskClock
