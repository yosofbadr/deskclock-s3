#pragma once

#include <stdint.h>

namespace DeskClock {

class DisplaySettingsDraft {
public:
  void begin(uint8_t saved_theme_index, uint8_t theme_count);
  void adjustTheme(int8_t delta);
  uint8_t saveTheme();
  void cancel();

  uint8_t savedThemeIndex() const;
  uint8_t stagedThemeIndex() const;
  bool hasStagedThemeChange() const;

private:
  uint8_t saved_theme_index_ = 0;
  uint8_t staged_theme_index_ = 0;
  uint8_t theme_count_ = 1;

  uint8_t normalize(uint8_t index) const;
};

} // namespace DeskClock
