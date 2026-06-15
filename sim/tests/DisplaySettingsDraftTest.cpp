#include "app/DisplaySettingsDraft.h"

#include <cstdlib>
#include <iostream>

using namespace DeskClock;

namespace {

void require(bool condition, const char *message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    std::exit(1);
  }
}

void theme_changes_are_staged_until_save_or_cancel()
{
  DisplaySettingsDraft draft;
  draft.begin(0, 3);

  draft.adjustTheme(1);
  require(draft.stagedThemeIndex() == 1, "theme adjustment updates staged theme");
  require(draft.savedThemeIndex() == 0, "saved theme remains unchanged before save");
  require(draft.hasStagedThemeChange(), "draft reports pending theme change");

  draft.cancel();
  require(draft.stagedThemeIndex() == 0, "cancel discards staged theme");
  require(draft.savedThemeIndex() == 0, "cancel leaves saved theme unchanged");

  draft.adjustTheme(-1);
  require(draft.stagedThemeIndex() == 2, "theme adjustment wraps within available themes");
  require(draft.saveTheme() == 2, "save returns staged theme to apply");
  require(draft.savedThemeIndex() == 2, "save updates saved theme");
  require(!draft.hasStagedThemeChange(), "no pending theme change remains after save");
}

} // namespace

int main()
{
  theme_changes_are_staged_until_save_or_cancel();
  std::cout << "DisplaySettingsDraft tests passed\n";
  return 0;
}
