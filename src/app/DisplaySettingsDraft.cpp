#include "DisplaySettingsDraft.h"

namespace DeskClock {

void DisplaySettingsDraft::begin(uint8_t saved_theme_index, uint8_t theme_count)
{
  theme_count_ = theme_count == 0 ? 1 : theme_count;
  saved_theme_index_ = normalize(saved_theme_index);
  staged_theme_index_ = saved_theme_index_;
}

void DisplaySettingsDraft::adjustTheme(int8_t delta)
{
  const int16_t count = static_cast<int16_t>(theme_count_);
  int16_t next = static_cast<int16_t>(staged_theme_index_) + static_cast<int16_t>(delta);
  while (next < 0) {
    next += count;
  }
  while (next >= count) {
    next -= count;
  }
  staged_theme_index_ = static_cast<uint8_t>(next);
}

uint8_t DisplaySettingsDraft::saveTheme()
{
  saved_theme_index_ = staged_theme_index_;
  return saved_theme_index_;
}

void DisplaySettingsDraft::cancel()
{
  staged_theme_index_ = saved_theme_index_;
}

uint8_t DisplaySettingsDraft::savedThemeIndex() const
{
  return saved_theme_index_;
}

uint8_t DisplaySettingsDraft::stagedThemeIndex() const
{
  return staged_theme_index_;
}

bool DisplaySettingsDraft::hasStagedThemeChange() const
{
  return staged_theme_index_ != saved_theme_index_;
}

uint8_t DisplaySettingsDraft::normalize(uint8_t index) const
{
  return static_cast<uint8_t>(index % theme_count_);
}

} // namespace DeskClock
