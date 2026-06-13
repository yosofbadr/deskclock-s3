#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TimeService.h"

namespace DeskClock {

constexpr size_t kMaxAlarms = 5;
constexpr uint16_t kSnoozeMinutes = 10;
constexpr uint16_t kAlarmSoundLimitSeconds = 5 * 60;

enum class AlarmRecurrence : uint8_t {
  Once,
  Daily,
  Weekdays,
  Weekends,
};

struct Alarm {
  uint8_t id = 0;
  bool enabled = false;
  AlarmRecurrence recurrence = AlarmRecurrence::Once;
  uint16_t year = 0;  // Used for one-time alarms.
  uint8_t month = 0;  // Used for one-time alarms.
  uint8_t day = 0;    // Used for one-time alarms.
  uint8_t hour = 0;
  uint8_t minute = 0;
  bool transient = false;
};

struct AlarmOccurrence {
  bool exists = false;
  Alarm alarm;
  DateTime at;
  uint32_t seconds_until = 0;
};

struct ActiveAlarmAlert {
  bool active = false;
  Alarm alarm;
  DateTime started_at;
  bool sound_allowed = false;
};

namespace AlarmService {

void begin(const DateTime &now);
void loop(const DateTime &now);
bool dismissActiveAlert();
bool snoozeActiveAlert(const DateTime &now);
ActiveAlarmAlert activeAlert();
bool addAlarm(const Alarm &alarm, uint8_t *created_id = nullptr);
bool updateAlarm(uint8_t id, const Alarm &alarm);
bool removeAlarm(uint8_t id);
bool setEnabled(uint8_t id, bool enabled);
size_t count();
size_t copyAlarms(Alarm *destination, size_t capacity);
AlarmOccurrence nextAlarm(const DateTime &now);
const char *recurrenceLabel(AlarmRecurrence recurrence);

} // namespace AlarmService
} // namespace DeskClock
