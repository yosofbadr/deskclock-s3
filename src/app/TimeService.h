#pragma once

#include <stdint.h>

namespace DeskClock {

enum class SyncState : uint8_t {
  SyncedRecently,
  LocalRetained,
  Unreliable,
};

struct DateTime {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint8_t week = 0; // 0 = Sunday, 1 = Monday, ... 6 = Saturday
  bool valid = false;
};

struct TimeSnapshot {
  DateTime now;
  SyncState sync_state = SyncState::Unreliable;
  bool rtc_available = false;
  bool bootstrapped_from_compile_time = false;
};

namespace TimeService {

bool begin();
void loop();
TimeSnapshot snapshot();

} // namespace TimeService
} // namespace DeskClock
