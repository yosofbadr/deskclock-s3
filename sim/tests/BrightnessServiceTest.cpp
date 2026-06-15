#include "app/BrightnessService.h"
#include "Preferences.h"

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

DateTime invalid_time()
{
  DateTime now = {};
  now.valid = false;
  return now;
}

void power_button_brightness_cycle_defers_flash_persistence()
{
  Preferences::clear();
  BrightnessService::begin();
  const size_t writes_after_begin = Preferences::writeCount();

  const uint8_t next = BrightnessService::cyclePreset(1);
  require(next == BrightnessService::currentBrightness(), "brightness changes immediately on cycle");
  require(Preferences::writeCount() == writes_after_begin, "brightness cycle does not synchronously write preferences");

  require(BrightnessService::flushPendingSave(), "pending brightness save can be flushed explicitly");
  require(Preferences::writeCount() > writes_after_begin, "flushed brightness save writes preferences");
  require(!BrightnessService::flushPendingSave(), "second flush is a no-op when nothing is pending");
}

void deferred_save_flushes_from_service_loop()
{
  Preferences::clear();
  BrightnessService::begin();
  const size_t writes_after_begin = Preferences::writeCount();

  BrightnessService::cyclePreset(1);
  require(Preferences::writeCount() == writes_after_begin, "cycle remains non-blocking before loop flush");
  delay(1600);
  BrightnessService::loop(invalid_time());
  require(Preferences::writeCount() > writes_after_begin, "service loop eventually persists deferred brightness change");
}

} // namespace

int main()
{
  power_button_brightness_cycle_defers_flash_persistence();
  deferred_save_flushes_from_service_loop();
  std::cout << "BrightnessService tests passed\n";
  return 0;
}
