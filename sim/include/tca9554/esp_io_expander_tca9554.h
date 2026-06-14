#ifndef DESKCLOCK_SIM_TCA9554_H
#define DESKCLOCK_SIM_TCA9554_H

#include "esp_err.h"
#include "i2c_bsp.h"

using esp_io_expander_handle_t = void *;

#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000 0x20
#define IO_EXPANDER_PIN_NUM_6 (1U << 6)
#define IO_EXPANDER_PIN_NUM_7 (1U << 7)
#define IO_EXPANDER_OUTPUT 1
#define ESP_IO_EXPANDER_OUTPUT 1

inline esp_err_t esp_io_expander_new_i2c_tca9554(i2c_master_bus_handle_t, int, esp_io_expander_handle_t *handle)
{
  static int expander;
  if (handle != nullptr) {
    *handle = &expander;
  }
  return ESP_OK;
}
inline esp_err_t esp_io_expander_set_dir(esp_io_expander_handle_t, int, int) { return ESP_OK; }
inline esp_err_t esp_io_expander_set_level(esp_io_expander_handle_t, int, int) { return ESP_OK; }

#endif /* DESKCLOCK_SIM_TCA9554_H */
