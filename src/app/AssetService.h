#pragma once

#include <stddef.h>

namespace DeskClock {
namespace AssetService {

void begin();
bool available();

bool backgroundPath(const char *theme_name, char *out, size_t out_size);
bool imagePath(const char *theme_name, const char *asset_name, char *out, size_t out_size);
bool hostPathForLvglPath(const char *lvgl_path, char *out, size_t out_size);

} // namespace AssetService
} // namespace DeskClock
