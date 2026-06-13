#include <Arduino.h>

#include "app/TimeService.h"
#include "i2c_bsp.h"
#include "lvgl_port.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"

void setup()
{
  delay(500);
  Serial.begin(115200);
  delay(500);

  Serial.println("DeskClock S3: booting RTC + LVGL shell");

  i2c_master_Init();
  DeskClock::TimeService::begin();
  lvgl_port_init();
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);

  Serial.println("DeskClock S3: display shell started");
}

void loop()
{
  DeskClock::TimeService::loop();
  delay(50);
}
