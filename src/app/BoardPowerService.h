#pragma once

namespace DeskClock {
namespace BoardPowerService {

// Hold the board's internal battery power rail on after the user releases PWR.
// Returns false when the TCA9554 expander is unavailable, which is expected on
// host simulators and some bring-up states but means standalone battery runtime
// will not be latched by firmware.
bool begin();
bool batteryPowerHoldEnabled();

} // namespace BoardPowerService
} // namespace DeskClock
