#pragma once

#include "AlarmService.h"
#include "TimeService.h"

namespace DeskClock {
namespace AlarmToneService {

void begin();
void loop(const ActiveAlarmAlert &alert);
void testTone();
bool available();

} // namespace AlarmToneService
} // namespace DeskClock
