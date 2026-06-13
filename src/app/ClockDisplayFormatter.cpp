#include "ClockDisplayFormatter.h"

#include <stdio.h>

#include "AlarmService.h"
#include "TimeSetupView.h"

namespace DeskClock {
namespace {

const char *weekday_name(uint8_t week)
{
  static constexpr const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return week < 7 ? names[week] : "---";
}

const char *month_name(uint8_t month)
{
  static constexpr const char *names[] = {
      "---", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return month <= 12 ? names[month] : "---";
}

} // namespace

namespace ClockDisplayFormatter {

uint32_t syncDotColor(SyncState state, bool blink)
{
  switch (state) {
  case SyncState::SyncedRecently:
    return blink ? 0x10B981 : 0x6EE7B7;
  case SyncState::LocalRetained:
    return blink ? 0xF59E0B : 0xFCD34D;
  case SyncState::Unreliable:
  default:
    return blink ? 0xEF4444 : 0xFCA5A5;
  }
}

const char *statusText(const TimeSnapshot &snapshot)
{
  if (!snapshot.rtc_available) {
    return "rtc unavailable";
  }
  if (!snapshot.now.valid) {
    return "time not set";
  }
  if (snapshot.bootstrapped_from_compile_time) {
    return "rtc build seed";
  }
  switch (snapshot.sync_state) {
  case SyncState::SyncedRecently:
    return "synced";
  case SyncState::LocalRetained:
    return "rtc local";
  case SyncState::Unreliable:
  default:
    return "time not set";
  }
}

void dateText(char *buffer, size_t size, const DateTime &now)
{
  snprintf(buffer, size, "%s, %s %u", weekday_name(now.week), month_name(now.month), now.day);
}

void timeText(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
  TimeSetupView::formatTime(buffer, size, hour, minute);
}

void nextAlarmText(char *buffer, size_t size, const DateTime &now)
{
  AlarmOccurrence next = AlarmService::nextAlarm(now);
  if (!next.exists) {
    snprintf(buffer, size, "Alarms: tap to add");
    return;
  }

  char alarm_time[12];
  timeText(alarm_time, sizeof(alarm_time), next.at.hour, next.at.minute);
  snprintf(buffer, size, "Alarm %s %s", alarm_time, AlarmService::recurrenceLabel(next.alarm.recurrence));
}

} // namespace ClockDisplayFormatter
} // namespace DeskClock
