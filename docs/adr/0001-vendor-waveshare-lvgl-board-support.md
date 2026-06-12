# Vendor Waveshare LVGL v9 board support code

We will copy Waveshare's LVGL v9 example board-support code into this repository as the initial hardware layer for the ESP32-S3-Touch-LCD-3.49. This trades the maintenance cost of vendored vendor code for a much faster and safer bring-up path, because the display, touch, I2C, and backlight initialization details are board-specific and already proven in Waveshare's example.
