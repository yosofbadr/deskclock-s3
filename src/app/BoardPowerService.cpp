#include "BoardPowerService.h"

#include <Arduino.h>

#include "esp_err.h"
#include "i2c_bsp.h"
#include "tca9554/esp_io_expander_tca9554.h"

namespace DeskClock {
namespace {

constexpr uint32_t kTca9554SysEnablePin = IO_EXPANDER_PIN_NUM_6;
constexpr uint32_t kTca9554AudioEnablePin = IO_EXPANDER_PIN_NUM_7;

esp_io_expander_handle_t power_expander = nullptr;
bool power_hold_enabled = false;
bool audio_power_enabled = false;
bool begin_attempted = false;

bool set_expander_output(uint32_t pin_mask, uint8_t level, const char *name)
{
  if (power_expander == nullptr) {
    Serial.printf("BoardPowerService: %s unavailable; expander not initialized\n", name);
    return false;
  }

  esp_err_t err = esp_io_expander_set_dir(power_expander, pin_mask, IO_EXPANDER_OUTPUT);
  if (err != ESP_OK) {
    Serial.printf("BoardPowerService: failed to set %s direction, err=%d\n", name, err);
    return false;
  }

  err = esp_io_expander_set_level(power_expander, pin_mask, level);
  if (err != ESP_OK) {
    Serial.printf("BoardPowerService: failed to set %s level, err=%d\n", name, err);
    return false;
  }

  return true;
}

} // namespace

namespace BoardPowerService {

bool begin()
{
  if (power_hold_enabled) {
    return true;
  }
  if (begin_attempted) {
    return false;
  }
  begin_attempted = true;

  i2c_master_bus_handle_t bus = nullptr;
  esp_err_t err = i2c_master_get_bus_handle(0, &bus);
  if (err != ESP_OK || bus == nullptr) {
    Serial.printf("BoardPowerService: failed to get I2C bus 0, err=%d\n", err);
    return false;
  }

  err = esp_io_expander_new_i2c_tca9554(bus, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &power_expander);
  if (err != ESP_OK || power_expander == nullptr) {
    Serial.printf("BoardPowerService: failed to create TCA9554 expander, err=%d\n", err);
    return false;
  }

  if (!set_expander_output(kTca9554SysEnablePin, 1, "SYS_EN")) {
    return false;
  }

  power_hold_enabled = true;
  Serial.println("BoardPowerService: battery power hold enabled");
  return true;
}

bool batteryPowerHoldEnabled()
{
  return power_hold_enabled;
}

bool enableAudioPower()
{
  if (audio_power_enabled) {
    return true;
  }

  if (!power_hold_enabled && !begin()) {
    return false;
  }

  if (!set_expander_output(kTca9554AudioEnablePin, 1, "AUDIO_EN")) {
    return false;
  }

  // The audio and power rails share the same TCA9554. Reassert SYS_EN after
  // touching audio so a future expander-driver change cannot strand the power
  // hold pin as an input.
  if (!set_expander_output(kTca9554SysEnablePin, 1, "SYS_EN")) {
    return false;
  }

  audio_power_enabled = true;
  Serial.println("BoardPowerService: audio power enabled");
  return true;
}

bool releaseBatteryPowerHold()
{
  if (!power_hold_enabled && !begin()) {
    return false;
  }
  if (!set_expander_output(kTca9554SysEnablePin, 0, "SYS_EN")) {
    return false;
  }
  power_hold_enabled = false;
  Serial.println("BoardPowerService: battery power hold released");
  return true;
}

} // namespace BoardPowerService
} // namespace DeskClock
