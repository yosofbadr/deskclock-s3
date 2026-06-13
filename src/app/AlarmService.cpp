#include "AlarmService.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace DeskClock {
namespace {

portMUX_TYPE alarm_mux = portMUX_INITIALIZER_UNLOCKED;
Alarm alarm_list[kMaxAlarms];
size_t alarm_count = 0;
uint8_t next_alarm_id = 1;
int64_t last_seen_now = -1;
ActiveAlarmAlert active_alert;

constexpr uint32_t kAlarmStoreMagic = 0xD35C10C1UL;
constexpr uint16_t kAlarmStoreVersion = 1;
constexpr const char *kAlarmStoreNamespace = "deskclock";
constexpr const char *kAlarmStoreKey = "alarms";

struct PersistedAlarmStore {
  uint32_t magic = kAlarmStoreMagic;
  uint16_t version = kAlarmStoreVersion;
  uint8_t count = 0;
  uint8_t next_id = 1;
  Alarm alarms[kMaxAlarms];
};

bool is_leap_year(uint16_t year)
{
  return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

uint8_t days_in_month(uint16_t year, uint8_t month)
{
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 0;
  }
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return days[month - 1];
}

bool is_valid_date(uint16_t year, uint8_t month, uint8_t day)
{
  return year >= 2024 && month >= 1 && month <= 12 && day >= 1 && day <= days_in_month(year, month);
}

uint8_t day_of_week(uint16_t year, uint8_t month, uint8_t day)
{
  uint32_t y = year;
  uint32_t m = month;
  if (m < 3) {
    m += 12;
    y--;
  }

  uint32_t val = (day + ((m + 1U) * 26U) / 10U + y + y / 4U + 6U * (y / 100U) + y / 400U) % 7U;
  if (val == 0) {
    val = 7;
  }
  return static_cast<uint8_t>(val - 1U); // 0 = Sunday
}

// Howard Hinnant's civil calendar algorithms. Returns days since 1970-01-01.
int64_t days_from_civil(int64_t year, unsigned month, unsigned day)
{
  year -= month <= 2;
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void civil_from_days(int64_t z, uint16_t &year, uint8_t &month, uint8_t &day)
{
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = static_cast<int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);

  year = static_cast<uint16_t>(y);
  month = static_cast<uint8_t>(m);
  day = static_cast<uint8_t>(d);
}

int64_t to_epoch_seconds(const DateTime &dt)
{
  if (!dt.valid || !is_valid_date(dt.year, dt.month, dt.day)) {
    return -1;
  }
  return (days_from_civil(dt.year, dt.month, dt.day) * 86400LL) +
         (static_cast<int64_t>(dt.hour) * 3600LL) +
         (static_cast<int64_t>(dt.minute) * 60LL) +
         static_cast<int64_t>(dt.second);
}

DateTime from_epoch_seconds(int64_t seconds)
{
  DateTime dt;
  if (seconds < 0) {
    return dt;
  }

  const int64_t days = seconds / 86400LL;
  const int64_t seconds_of_day = seconds % 86400LL;
  civil_from_days(days, dt.year, dt.month, dt.day);
  dt.hour = static_cast<uint8_t>(seconds_of_day / 3600LL);
  dt.minute = static_cast<uint8_t>((seconds_of_day % 3600LL) / 60LL);
  dt.second = static_cast<uint8_t>(seconds_of_day % 60LL);
  dt.week = day_of_week(dt.year, dt.month, dt.day);
  dt.valid = true;
  return dt;
}

bool is_weekday(uint8_t week)
{
  return week >= 1 && week <= 5;
}

bool is_weekend(uint8_t week)
{
  return week == 0 || week == 6;
}

bool recurrence_allows_week(AlarmRecurrence recurrence, uint8_t week)
{
  switch (recurrence) {
  case AlarmRecurrence::Daily:
    return true;
  case AlarmRecurrence::Weekdays:
    return is_weekday(week);
  case AlarmRecurrence::Weekends:
    return is_weekend(week);
  case AlarmRecurrence::Once:
  default:
    return false;
  }
}

DateTime alarm_time_on_date(const DateTime &date, const Alarm &alarm)
{
  DateTime candidate = date;
  candidate.hour = alarm.hour;
  candidate.minute = alarm.minute;
  candidate.second = 0;
  candidate.valid = date.valid;
  return candidate;
}

bool occurrence_for_alarm(const Alarm &alarm, const DateTime &now, AlarmOccurrence &occurrence)
{
  if (!alarm.enabled || !now.valid) {
    return false;
  }

  const int64_t now_seconds = to_epoch_seconds(now);
  if (now_seconds < 0) {
    return false;
  }

  int64_t candidate_seconds = -1;
  DateTime candidate;

  if (alarm.recurrence == AlarmRecurrence::Once) {
    candidate.year = alarm.year;
    candidate.month = alarm.month;
    candidate.day = alarm.day;
    candidate.hour = alarm.hour;
    candidate.minute = alarm.minute;
    candidate.second = 0;
    candidate.valid = is_valid_date(candidate.year, candidate.month, candidate.day);
    if (candidate.valid) {
      candidate.week = day_of_week(candidate.year, candidate.month, candidate.day);
    }
    candidate_seconds = to_epoch_seconds(candidate);
    if (candidate_seconds <= now_seconds) {
      return false;
    }
  } else {
    for (uint8_t day_offset = 0; day_offset < 8; ++day_offset) {
      DateTime day = from_epoch_seconds((now_seconds / 86400LL + day_offset) * 86400LL);
      if (!recurrence_allows_week(alarm.recurrence, day.week)) {
        continue;
      }

      candidate = alarm_time_on_date(day, alarm);
      candidate_seconds = to_epoch_seconds(candidate);
      if (candidate_seconds > now_seconds) {
        break;
      }
      candidate_seconds = -1;
    }

    if (candidate_seconds <= now_seconds) {
      return false;
    }
  }

  occurrence.exists = true;
  occurrence.alarm = alarm;
  occurrence.at = candidate;
  occurrence.seconds_until = static_cast<uint32_t>(candidate_seconds - now_seconds);
  return true;
}

bool is_valid_alarm(const Alarm &alarm)
{
  if (alarm.hour > 23 || alarm.minute > 59) {
    return false;
  }
  if (alarm.recurrence == AlarmRecurrence::Once) {
    return is_valid_date(alarm.year, alarm.month, alarm.day);
  }
  return true;
}

uint8_t allocate_alarm_id()
{
  if (next_alarm_id == 0) {
    next_alarm_id = 1;
  }
  return next_alarm_id++;
}

bool append_alarm_unlocked(const Alarm &alarm)
{
  if (alarm_count >= kMaxAlarms || !is_valid_alarm(alarm)) {
    return false;
  }
  alarm_list[alarm_count++] = alarm;
  return true;
}

bool same_alarm_minute(const Alarm &alarm, const DateTime &now)
{
  if (alarm.recurrence == AlarmRecurrence::Once) {
    return alarm.year == now.year && alarm.month == now.month && alarm.day == now.day &&
           alarm.hour == now.hour && alarm.minute == now.minute;
  }
  return recurrence_allows_week(alarm.recurrence, now.week) && alarm.hour == now.hour && alarm.minute == now.minute;
}

bool disable_one_time_alarm_unlocked(uint8_t id)
{
  for (size_t index = 0; index < alarm_count; ++index) {
    if (alarm_list[index].id == id && alarm_list[index].recurrence == AlarmRecurrence::Once) {
      alarm_list[index].enabled = false;
      return true;
    }
  }
  return false;
}

bool persist_alarm_snapshot(const Alarm *alarms, size_t count, uint8_t next_id)
{
  PersistedAlarmStore store;
  store.count = static_cast<uint8_t>(count > kMaxAlarms ? kMaxAlarms : count);
  store.next_id = next_id == 0 ? 1 : next_id;
  for (size_t index = 0; index < store.count; ++index) {
    store.alarms[index] = alarms[index];
    store.alarms[index].development_seed = false;
  }

  Preferences preferences;
  if (!preferences.begin(kAlarmStoreNamespace, false)) {
    Serial.println("AlarmService: failed to open alarm storage for write");
    return false;
  }
  const size_t written = preferences.putBytes(kAlarmStoreKey, &store, sizeof(store));
  preferences.end();
  if (written != sizeof(store)) {
    Serial.println("AlarmService: failed to persist alarms");
    return false;
  }
  return true;
}

bool save_alarms()
{
  Alarm snapshot[kMaxAlarms];
  size_t snapshot_count = 0;
  uint8_t snapshot_next_id = 1;

  portENTER_CRITICAL(&alarm_mux);
  for (size_t index = 0; index < alarm_count && snapshot_count < kMaxAlarms; ++index) {
    if (alarm_list[index].development_seed) {
      continue;
    }
    snapshot[snapshot_count++] = alarm_list[index];
  }
  snapshot_next_id = next_alarm_id;
  portEXIT_CRITICAL(&alarm_mux);

  return persist_alarm_snapshot(snapshot, snapshot_count, snapshot_next_id);
}

bool load_alarms()
{
  Preferences preferences;
  if (!preferences.begin(kAlarmStoreNamespace, true)) {
    Serial.println("AlarmService: no readable alarm storage yet");
    return false;
  }

  PersistedAlarmStore store;
  const size_t bytes = preferences.getBytes(kAlarmStoreKey, &store, sizeof(store));
  preferences.end();

  if (bytes != sizeof(store) || store.magic != kAlarmStoreMagic || store.version != kAlarmStoreVersion || store.count > kMaxAlarms) {
    return false;
  }

  size_t loaded = 0;
  portENTER_CRITICAL(&alarm_mux);
  alarm_count = 0;
  next_alarm_id = store.next_id == 0 ? 1 : store.next_id;
  for (size_t index = 0; index < store.count; ++index) {
    Alarm alarm = store.alarms[index];
    alarm.development_seed = false;
    if (alarm.id == 0 || !append_alarm_unlocked(alarm)) {
      continue;
    }
    loaded++;
    if (alarm.id >= next_alarm_id) {
      next_alarm_id = alarm.id + 1;
      if (next_alarm_id == 0) {
        next_alarm_id = 1;
      }
    }
  }
  portEXIT_CRITICAL(&alarm_mux);

  Serial.printf("AlarmService: loaded %u persisted alarms\n", static_cast<unsigned>(loaded));
  return true;
}

size_t copy_alarm_list(Alarm *destination, size_t capacity)
{
  if (destination == nullptr || capacity == 0) {
    return 0;
  }

  portENTER_CRITICAL(&alarm_mux);
  const size_t copied = alarm_count < capacity ? alarm_count : capacity;
  memcpy(destination, alarm_list, copied * sizeof(Alarm));
  portEXIT_CRITICAL(&alarm_mux);
  return copied;
}

void seed_development_alarm(const DateTime &now)
{
  if (!now.valid) {
    return;
  }

  const int64_t now_seconds = to_epoch_seconds(now);
  if (now_seconds < 0) {
    return;
  }

  const DateTime fire_at = from_epoch_seconds(((now_seconds + 2 * 60 + 59) / 60) * 60);
  Alarm alarm;
  alarm.enabled = true;
  alarm.recurrence = AlarmRecurrence::Once;
  alarm.year = fire_at.year;
  alarm.month = fire_at.month;
  alarm.day = fire_at.day;
  alarm.hour = fire_at.hour;
  alarm.minute = fire_at.minute;
  alarm.development_seed = true;

  bool added = false;
  portENTER_CRITICAL(&alarm_mux);
  if (alarm_count == 0) {
    alarm.id = allocate_alarm_id();
    added = append_alarm_unlocked(alarm);
  }
  portEXIT_CRITICAL(&alarm_mux);

  if (added) {
    Serial.printf(
        "AlarmService: seeded temporary development alarm for %04u-%02u-%02u %02u:%02u\n",
        alarm.year,
        alarm.month,
        alarm.day,
        alarm.hour,
        alarm.minute);
  }
}

} // namespace

namespace AlarmService {

void begin(const DateTime &now)
{
  portENTER_CRITICAL(&alarm_mux);
  alarm_count = 0;
  next_alarm_id = 1;
  active_alert = ActiveAlarmAlert();
  portEXIT_CRITICAL(&alarm_mux);

  const bool loaded = load_alarms();
  last_seen_now = to_epoch_seconds(now);
  if (!loaded || count() == 0) {
    seed_development_alarm(now);
  }
}

void loop(const DateTime &now)
{
  if (!now.valid) {
    return;
  }

  const int64_t now_seconds = to_epoch_seconds(now);
  if (now_seconds < 0 || now_seconds == last_seen_now) {
    return;
  }
  last_seen_now = now_seconds;

  portENTER_CRITICAL(&alarm_mux);
  if (!active_alert.active) {
    for (size_t index = 0; index < alarm_count; ++index) {
      Alarm &alarm = alarm_list[index];
      if (!alarm.enabled || !same_alarm_minute(alarm, now)) {
        continue;
      }

      active_alert.active = true;
      active_alert.alarm = alarm;
      active_alert.started_at = now;
      active_alert.sound_allowed = true;
      const bool disabled_one_time = disable_one_time_alarm_unlocked(alarm.id);
      Serial.printf("AlarmService: alarm %u active at %02u:%02u\n", alarm.id, now.hour, now.minute);
      if (disabled_one_time) {
        portEXIT_CRITICAL(&alarm_mux);
        save_alarms();
        return;
      }
      break;
    }
  }
  portEXIT_CRITICAL(&alarm_mux);
}

bool dismissActiveAlert()
{
  bool dismissed = false;
  portENTER_CRITICAL(&alarm_mux);
  if (active_alert.active) {
    active_alert = ActiveAlarmAlert();
    dismissed = true;
  }
  portEXIT_CRITICAL(&alarm_mux);
  if (dismissed) {
    Serial.println("AlarmService: active alarm dismissed");
  }
  return dismissed;
}

bool snoozeActiveAlert(const DateTime &now)
{
  ActiveAlarmAlert alert;
  portENTER_CRITICAL(&alarm_mux);
  alert = active_alert;
  if (active_alert.active) {
    active_alert = ActiveAlarmAlert();
  }
  portEXIT_CRITICAL(&alarm_mux);

  if (!alert.active || !now.valid) {
    return false;
  }

  const int64_t now_seconds = to_epoch_seconds(now);
  if (now_seconds < 0) {
    return false;
  }

  const DateTime snoozed_at = from_epoch_seconds(now_seconds + static_cast<int64_t>(kSnoozeMinutes) * 60LL);
  Alarm snoozed = alert.alarm;
  snoozed.id = 0;
  snoozed.enabled = true;
  snoozed.recurrence = AlarmRecurrence::Once;
  snoozed.year = snoozed_at.year;
  snoozed.month = snoozed_at.month;
  snoozed.day = snoozed_at.day;
  snoozed.hour = snoozed_at.hour;
  snoozed.minute = snoozed_at.minute;
  snoozed.development_seed = false;

  uint8_t id = 0;
  const bool added = addAlarm(snoozed, &id);
  if (added) {
    Serial.printf("AlarmService: snoozed alarm %u as one-time alarm %u for %02u:%02u\n", alert.alarm.id, id, snoozed.hour, snoozed.minute);
  }
  return added;
}

ActiveAlarmAlert activeAlert()
{
  ActiveAlarmAlert copy;
  portENTER_CRITICAL(&alarm_mux);
  copy = active_alert;
  portEXIT_CRITICAL(&alarm_mux);
  return copy;
}

bool addAlarm(const Alarm &alarm, uint8_t *created_id)
{
  if (!is_valid_alarm(alarm)) {
    return false;
  }

  Alarm copy = alarm;
  bool added = false;
  portENTER_CRITICAL(&alarm_mux);
  if (alarm_count < kMaxAlarms) {
    copy.id = allocate_alarm_id();
    added = append_alarm_unlocked(copy);
  }
  portEXIT_CRITICAL(&alarm_mux);

  if (added && created_id != nullptr) {
    *created_id = copy.id;
  }
  if (added && !copy.development_seed) {
    save_alarms();
  }
  return added;
}

bool updateAlarm(uint8_t id, const Alarm &alarm)
{
  if (id == 0 || !is_valid_alarm(alarm)) {
    return false;
  }

  bool updated = false;
  portENTER_CRITICAL(&alarm_mux);
  for (size_t index = 0; index < alarm_count; ++index) {
    if (alarm_list[index].id != id) {
      continue;
    }
    Alarm copy = alarm;
    copy.id = id;
    alarm_list[index] = copy;
    updated = true;
    break;
  }
  portEXIT_CRITICAL(&alarm_mux);
  if (updated) {
    save_alarms();
  }
  return updated;
}

bool removeAlarm(uint8_t id)
{
  bool removed = false;
  portENTER_CRITICAL(&alarm_mux);
  for (size_t index = 0; index < alarm_count; ++index) {
    if (alarm_list[index].id != id) {
      continue;
    }
    for (size_t move = index; move + 1 < alarm_count; ++move) {
      alarm_list[move] = alarm_list[move + 1];
    }
    alarm_count--;
    removed = true;
    break;
  }
  portEXIT_CRITICAL(&alarm_mux);
  if (removed) {
    save_alarms();
  }
  return removed;
}

bool setEnabled(uint8_t id, bool enabled)
{
  bool changed = false;
  portENTER_CRITICAL(&alarm_mux);
  for (size_t index = 0; index < alarm_count; ++index) {
    if (alarm_list[index].id == id) {
      alarm_list[index].enabled = enabled;
      changed = true;
      break;
    }
  }
  portEXIT_CRITICAL(&alarm_mux);
  if (changed) {
    save_alarms();
  }
  return changed;
}

size_t count()
{
  portENTER_CRITICAL(&alarm_mux);
  const size_t value = alarm_count;
  portEXIT_CRITICAL(&alarm_mux);
  return value;
}

size_t copyAlarms(Alarm *destination, size_t capacity)
{
  return copy_alarm_list(destination, capacity);
}

AlarmOccurrence nextAlarm(const DateTime &now)
{
  Alarm snapshot[kMaxAlarms];
  const size_t snapshot_count = copy_alarm_list(snapshot, kMaxAlarms);

  AlarmOccurrence best;
  for (size_t index = 0; index < snapshot_count; ++index) {
    AlarmOccurrence candidate;
    if (!occurrence_for_alarm(snapshot[index], now, candidate)) {
      continue;
    }
    if (!best.exists || candidate.seconds_until < best.seconds_until) {
      best = candidate;
    }
  }
  return best;
}

const char *recurrenceLabel(AlarmRecurrence recurrence)
{
  switch (recurrence) {
  case AlarmRecurrence::Once:
    return "once";
  case AlarmRecurrence::Daily:
    return "daily";
  case AlarmRecurrence::Weekdays:
    return "weekdays";
  case AlarmRecurrence::Weekends:
    return "weekends";
  default:
    return "alarm";
  }
}

} // namespace AlarmService
} // namespace DeskClock
