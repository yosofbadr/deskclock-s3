#pragma once

namespace DeskClock {
namespace BoardPowerService {

// Hold the board's internal battery power rail on after the user releases PWR.
// Returns false when the TCA9554 expander is unavailable, which is expected on
// host simulators and some bring-up states but means standalone battery runtime
// will not be latched by firmware.
bool begin();
bool batteryPowerHoldEnabled();

// Enable the board audio amplifier rail through the same TCA9554 instance used
// for SYS_EN. Keeping all expander writes here prevents later audio setup from
// resetting the power-hold pin back to input mode.
bool enableAudioPower();

// Release the SYS_EN hold. On battery power this lets the board shut down after
// the user releases PWR; on USB power the board may stay powered by USB.
bool releaseBatteryPowerHold();

} // namespace BoardPowerService
} // namespace DeskClock
