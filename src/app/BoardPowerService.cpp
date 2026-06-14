#include "BoardPowerService.h"

#include <Arduino.h>

#include "esp_err.h"
#include "i2c_bsp.h"
#include "tca9554/esp_io_expander_tca9554.h"

namespace DeskClock {
namespace {

constexpr uint32_t kTca9554SysEnablePin = IO_EXPANDER_PIN_NUM_6;

esp_io_expander_handle_t power_expander = nullptr;
bool power_hold_enabled = false;
bool begin_attempted = false;

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

  err = esp_io_expander_set_dir(power_expander, kTca9554SysEnablePin, IO_EXPANDER_OUTPUT);
  if (err != ESP_OK) {
    Serial.printf("BoardPowerService: failed to set SYS_EN direction, err=%d\n", err);
    return false;
  }

  err = esp_io_expander_set_level(power_expander, kTca9554SysEnablePin, 1);
  if (err != ESP_OK) {
    Serial.printf("BoardPowerService: failed to enable SYS_EN, err=%d\n", err);
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

} // namespace BoardPowerService
} // namespace DeskClock
