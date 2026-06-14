#include "TimeService.h"

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "i2c_bsp.h"

namespace DeskClock {
namespace {

constexpr uint8_t kPcf85063Ctrl1Reg = 0x00;
constexpr uint8_t kPcf85063SecondReg = 0x04;
constexpr uint8_t kPcf85063Ctrl1StopBit = 0x20;
constexpr uint8_t kPcf85063Ctrl1Mode12HourBit = 0x02;
constexpr uint16_t kMinimumPlausibleYear = 2024;
constexpr uint32_t kRtcPollIntervalMs = 1000;
constexpr uint32_t kRecentNetworkSyncMs = 6UL * 60UL * 60UL * 1000UL;

portMUX_TYPE snapshot_mux = portMUX_INITIALIZER_UNLOCKED;
TimeSnapshot current_snapshot;
uint32_t last_rtc_poll_ms = 0;
uint32_t last_network_sync_ms = 0;

uint8_t dec_to_bcd(uint8_t value)
{
  return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

uint8_t bcd_to_dec(uint8_t value)
{
  return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

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

uint8_t day_of_week(uint16_t year, uint8_t month, uint8_t day)
{
  // Zeller's congruence, same convention as Waveshare SensorLib: 0 = Sunday.
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
  return static_cast<uint8_t>(val - 1U);
}

bool is_plausible(const DateTime &dt)
{
  if (!dt.valid) {
    return false;
  }
  if (dt.year < kMinimumPlausibleYear || dt.month < 1 || dt.month > 12) {
    return false;
  }
  if (dt.day < 1 || dt.day > days_in_month(dt.year, dt.month)) {
    return false;
  }
  if (dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
    return false;
  }
  return true;
}

void store_snapshot(const TimeSnapshot &snapshot)
{
  portENTER_CRITICAL(&snapshot_mux);
  current_snapshot = snapshot;
  portEXIT_CRITICAL(&snapshot_mux);
}

DateTime compile_time_datetime()
{
  DateTime dt;

  const char *date = __DATE__; // Example: "Jun 13 2026"
  const char *time = __TIME__; // Example: "09:05:07"

  static constexpr const char *months[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  for (uint8_t index = 0; index < 12; ++index) {
    if (strncmp(date, months[index], 3) == 0) {
      dt.month = index + 1;
      break;
    }
  }

  dt.day = static_cast<uint8_t>(atoi(date + 4));
  dt.year = static_cast<uint16_t>(atoi(date + 7));
  dt.hour = static_cast<uint8_t>(atoi(time));
  dt.minute = static_cast<uint8_t>(atoi(time + 3));
  dt.second = static_cast<uint8_t>(atoi(time + 6));
  dt.week = day_of_week(dt.year, dt.month, dt.day);
  dt.valid = true;
  return dt;
}

bool read_rtc(DateTime &dt, bool &clock_integrity_guaranteed)
{
  uint8_t buffer[7] = {0};
  if (i2c_read_buff(rtc_dev_handle, kPcf85063SecondReg, buffer, sizeof(buffer)) != 0) {
    return false;
  }

  clock_integrity_guaranteed = (buffer[0] & 0x80U) == 0;

  dt.second = bcd_to_dec(buffer[0] & 0x7FU);
  dt.minute = bcd_to_dec(buffer[1] & 0x7FU);
  dt.hour = bcd_to_dec(buffer[2] & 0x3FU);
  dt.day = bcd_to_dec(buffer[3] & 0x3FU);
  dt.week = bcd_to_dec(buffer[4] & 0x07U);
  dt.month = bcd_to_dec(buffer[5] & 0x1FU);
  dt.year = static_cast<uint16_t>(2000U + bcd_to_dec(buffer[6]));
  dt.valid = clock_integrity_guaranteed;
  return true;
}

bool write_rtc(const DateTime &dt)
{
  uint8_t buffer[7] = {
      static_cast<uint8_t>(dec_to_bcd(dt.second) & 0x7FU),
      dec_to_bcd(dt.minute),
      dec_to_bcd(dt.hour),
      dec_to_bcd(dt.day),
      day_of_week(dt.year, dt.month, dt.day),
      dec_to_bcd(dt.month),
      dec_to_bcd(static_cast<uint8_t>(dt.year % 100U)),
  };

  return i2c_write_buff(rtc_dev_handle, kPcf85063SecondReg, buffer, sizeof(buffer)) == 0;
}

void configure_rtc_control()
{
  uint8_t ctrl1 = 0;
  if (i2c_read_buff(rtc_dev_handle, kPcf85063Ctrl1Reg, &ctrl1, 1) != 0) {
    return;
  }
  // Keep the PCF85063 ticking in 24-hour mode. If STOP was set by a demo,
  // battery event, or previous firmware, the RTC returns a plausible but frozen time.
  ctrl1 &= static_cast<uint8_t>(~(kPcf85063Ctrl1StopBit | kPcf85063Ctrl1Mode12HourBit));
  (void)i2c_write_buff(rtc_dev_handle, kPcf85063Ctrl1Reg, &ctrl1, 1);
}

TimeSnapshot read_snapshot_from_rtc(bool bootstrapped)
{
  DateTime rtc_time;
  bool integrity = false;
  TimeSnapshot snapshot;
  snapshot.bootstrapped_from_compile_time = bootstrapped;

  if (!read_rtc(rtc_time, integrity)) {
    snapshot.rtc_available = false;
    snapshot.sync_state = SyncState::Unreliable;
    snapshot.now.valid = false;
    return snapshot;
  }

  snapshot.rtc_available = true;
  snapshot.now = rtc_time;
  snapshot.now.valid = integrity && is_plausible(rtc_time);
  if (snapshot.now.valid && last_network_sync_ms != 0 && millis() - last_network_sync_ms < kRecentNetworkSyncMs) {
    snapshot.sync_state = SyncState::SyncedRecently;
  } else {
    snapshot.sync_state = snapshot.now.valid ? SyncState::LocalRetained : SyncState::Unreliable;
  }
  return snapshot;
}

} // namespace

namespace TimeService {

bool begin()
{
  configure_rtc_control();

  TimeSnapshot initial = read_snapshot_from_rtc(false);
  if (initial.rtc_available && initial.now.valid) {
    store_snapshot(initial);
    Serial.printf(
        "RTC: using retained time %04u-%02u-%02u %02u:%02u:%02u\n",
        initial.now.year,
        initial.now.month,
        initial.now.day,
        initial.now.hour,
        initial.now.minute,
        initial.now.second);
    return true;
  }

  DateTime seed = compile_time_datetime();
  bool seeded = write_rtc(seed);
  if (seeded) {
    TimeSnapshot seeded_snapshot = read_snapshot_from_rtc(true);
    seeded_snapshot.bootstrapped_from_compile_time = true;
    store_snapshot(seeded_snapshot);
    Serial.printf(
        "RTC: invalid/unset, seeded from firmware build time %04u-%02u-%02u %02u:%02u:%02u\n",
        seed.year,
        seed.month,
        seed.day,
        seed.hour,
        seed.minute,
        seed.second);
    return seeded_snapshot.now.valid;
  }

  TimeSnapshot failed;
  failed.rtc_available = false;
  failed.sync_state = SyncState::Unreliable;
  failed.now.valid = false;
  store_snapshot(failed);
  Serial.println("RTC: unavailable or failed to seed");
  return false;
}

void loop()
{
  const uint32_t now_ms = millis();
  if (now_ms - last_rtc_poll_ms < kRtcPollIntervalMs) {
    return;
  }
  last_rtc_poll_ms = now_ms;

  TimeSnapshot previous = snapshot();
  store_snapshot(read_snapshot_from_rtc(previous.bootstrapped_from_compile_time));
}

bool setManualTime(const DateTime &date_time)
{
  DateTime adjusted = date_time;
  adjusted.week = day_of_week(adjusted.year, adjusted.month, adjusted.day);
  adjusted.valid = true;
  if (!is_plausible(adjusted) || !write_rtc(adjusted)) {
    return false;
  }

  last_network_sync_ms = 0;
  TimeSnapshot manual;
  manual.now = adjusted;
  manual.sync_state = SyncState::LocalRetained;
  manual.rtc_available = true;
  manual.bootstrapped_from_compile_time = false;
  store_snapshot(manual);
  Serial.printf(
      "RTC: manually set to %04u-%02u-%02u %02u:%02u:%02u\n",
      adjusted.year,
      adjusted.month,
      adjusted.day,
      adjusted.hour,
      adjusted.minute,
      adjusted.second);
  return true;
}

bool setNetworkTime(const DateTime &date_time)
{
  DateTime adjusted = date_time;
  adjusted.week = day_of_week(adjusted.year, adjusted.month, adjusted.day);
  adjusted.valid = true;
  if (!is_plausible(adjusted) || !write_rtc(adjusted)) {
    return false;
  }

  last_network_sync_ms = millis();
  TimeSnapshot synced;
  synced.now = adjusted;
  synced.sync_state = SyncState::SyncedRecently;
  synced.rtc_available = true;
  synced.bootstrapped_from_compile_time = false;
  store_snapshot(synced);
  Serial.printf(
      "RTC: network synced to %04u-%02u-%02u %02u:%02u:%02u\n",
      adjusted.year,
      adjusted.month,
      adjusted.day,
      adjusted.hour,
      adjusted.minute,
      adjusted.second);
  return true;
}

TimeSnapshot snapshot()
{
  TimeSnapshot copy;
  portENTER_CRITICAL(&snapshot_mux);
  copy = current_snapshot;
  portEXIT_CRITICAL(&snapshot_mux);
  return copy;
}

} // namespace TimeService
} // namespace DeskClock
