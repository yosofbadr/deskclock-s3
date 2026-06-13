#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TimeService.h"

namespace DeskClock {
namespace ClockDisplayFormatter {

uint32_t syncDotColor(SyncState state, bool blink);
const char *statusText(const TimeSnapshot &snapshot);
void dateText(char *buffer, size_t size, const DateTime &now);
void timeText(char *buffer, size_t size, uint8_t hour, uint8_t minute);
void nextAlarmText(char *buffer, size_t size, const DateTime &now);

} // namespace ClockDisplayFormatter
} // namespace DeskClock
