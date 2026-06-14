#include "Arduino.h"
#include "esp_err.h"
#include "i2c_bsp.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"

#include <ctime>
#include <cstring>

SerialStub Serial;

i2c_master_dev_handle_t disp_touch_dev_handle = reinterpret_cast<i2c_master_dev_handle_t>(0x1);
i2c_master_dev_handle_t rtc_dev_handle = reinterpret_cast<i2c_master_dev_handle_t>(0x2);
i2c_master_dev_handle_t imu_dev_handle = reinterpret_cast<i2c_master_dev_handle_t>(0x3);

namespace {
constexpr uint8_t kPcf85063Ctrl1Reg = 0x00;
constexpr uint8_t kPcf85063SecondReg = 0x04;

time_t rtc_epoch_base = 0;
uint32_t rtc_millis_base = 0;
uint8_t ctrl1_register = 0;
uint16_t current_backlight = 0;

uint8_t dec_to_bcd(uint8_t value)
{
  return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

uint8_t bcd_to_dec(uint8_t value)
{
  return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

time_t simulated_epoch()
{
  if (rtc_epoch_base == 0) {
    rtc_epoch_base = std::time(nullptr);
    rtc_millis_base = millis();
  }
  return rtc_epoch_base + static_cast<time_t>((millis() - rtc_millis_base) / 1000U);
}

void set_simulated_epoch(time_t epoch)
{
  rtc_epoch_base = epoch;
  rtc_millis_base = millis();
}
} // namespace

void i2c_master_Init(void)
{
  if (rtc_epoch_base == 0) {
    set_simulated_epoch(std::time(nullptr));
  }
}

uint8_t i2c_write_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len)
{
  if (dev_handle == rtc_dev_handle && reg == kPcf85063Ctrl1Reg && len >= 1 && buf != nullptr) {
    ctrl1_register = buf[0];
    return 0;
  }

  if (dev_handle == rtc_dev_handle && reg == kPcf85063SecondReg && len >= 7 && buf != nullptr) {
    std::tm tm = {};
    tm.tm_sec = bcd_to_dec(buf[0] & 0x7FU);
    tm.tm_min = bcd_to_dec(buf[1] & 0x7FU);
    tm.tm_hour = bcd_to_dec(buf[2] & 0x3FU);
    tm.tm_mday = bcd_to_dec(buf[3] & 0x3FU);
    tm.tm_mon = static_cast<int>(bcd_to_dec(buf[5] & 0x1FU)) - 1;
    tm.tm_year = static_cast<int>(2000U + bcd_to_dec(buf[6])) - 1900;
#if defined(__APPLE__) || defined(__linux__)
    set_simulated_epoch(timegm(&tm));
#else
    set_simulated_epoch(std::mktime(&tm));
#endif
    return 0;
  }

  return 0;
}

uint8_t i2c_read_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len)
{
  if (buf == nullptr) {
    return 1;
  }
  std::memset(buf, 0, len);

  if (dev_handle == rtc_dev_handle && reg == kPcf85063Ctrl1Reg && len >= 1) {
    buf[0] = ctrl1_register;
    return 0;
  }

  if (dev_handle == rtc_dev_handle && reg == kPcf85063SecondReg && len >= 7) {
    const time_t epoch = simulated_epoch();
    std::tm tm = {};
#if defined(_WIN32)
    gmtime_s(&tm, &epoch);
#else
    gmtime_r(&epoch, &tm);
#endif
    buf[0] = dec_to_bcd(static_cast<uint8_t>(tm.tm_sec)) & 0x7FU; // integrity bit clear = guaranteed
    buf[1] = dec_to_bcd(static_cast<uint8_t>(tm.tm_min));
    buf[2] = dec_to_bcd(static_cast<uint8_t>(tm.tm_hour));
    buf[3] = dec_to_bcd(static_cast<uint8_t>(tm.tm_mday));
    buf[4] = dec_to_bcd(static_cast<uint8_t>(tm.tm_wday));
    buf[5] = dec_to_bcd(static_cast<uint8_t>(tm.tm_mon + 1));
    buf[6] = dec_to_bcd(static_cast<uint8_t>((tm.tm_year + 1900) % 100));
    return 0;
  }

  return 0;
}

uint8_t i2c_master_write_read_dev(i2c_master_dev_handle_t, uint8_t *, uint8_t, uint8_t *readBuf, uint8_t readLen)
{
  if (readBuf != nullptr) {
    std::memset(readBuf, 0, readLen);
  }
  return 0;
}

uint8_t i2c_master_touch_write_read(i2c_master_dev_handle_t dev_handle, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen)
{
  return i2c_master_write_read_dev(dev_handle, writeBuf, writeLen, readBuf, readLen);
}

int i2c_master_get_bus_handle(int, i2c_master_bus_handle_t *bus)
{
  static int fake_bus;
  if (bus != nullptr) {
    *bus = &fake_bus;
  }
  return ESP_OK;
}

extern "C" void lcd_bl_pwm_bsp_init(uint16_t duty)
{
  current_backlight = duty;
}

extern "C" void setUpduty(uint16_t duty)
{
  current_backlight = duty;
  (void)current_backlight;
}
