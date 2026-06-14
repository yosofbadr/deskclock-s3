#ifndef DESKCLOCK_SIM_I2C_BSP_H
#define DESKCLOCK_SIM_I2C_BSP_H

#include <cstdint>

using i2c_master_bus_handle_t = void *;
using i2c_master_dev_handle_t = void *;

extern i2c_master_dev_handle_t disp_touch_dev_handle;
extern i2c_master_dev_handle_t rtc_dev_handle;
extern i2c_master_dev_handle_t imu_dev_handle;

void i2c_master_Init(void);
uint8_t i2c_write_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len);
uint8_t i2c_master_write_read_dev(i2c_master_dev_handle_t dev_handle, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen);
uint8_t i2c_read_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len);
uint8_t i2c_master_touch_write_read(i2c_master_dev_handle_t dev_handle, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen);
int i2c_master_get_bus_handle(int port, i2c_master_bus_handle_t *bus);

#endif /* DESKCLOCK_SIM_I2C_BSP_H */
