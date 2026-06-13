# DeskClock S3 Troubleshooting

## ESP32-S3 upload: `No serial data received`

Symptom:

```text
Failed to connect to ESP32-S3: No serial data received
```

Observed on this project with the board visible as:

```text
/dev/cu.usbmodem2101
USB VID:PID=303A:1001
USB JTAG/serial debug unit
```

What this usually means: the USB serial/JTAG device is present, but the ESP32-S3 ROM bootloader is not responding to esptool. The board likely needs manual BOOT/reset entry into download mode.

Recovery steps:

1. Confirm the current port:

   ```sh
   pio device list
   ```

2. Start an explicit-port upload:

   ```sh
   pio run -e waveshare_touch_lcd_3_49 -t upload --upload-port /dev/cu.usbmodem2101
   ```

3. When upload reaches `Connecting...`, hold **BOOT**.
4. While holding **BOOT**, tap **RESET** if the board exposes reset, or unplug/replug USB.
5. Keep holding **BOOT** until esptool starts writing flash.
6. Release **BOOT** after writing begins.
7. After upload completes, open the serial monitor from an interactive terminal:

   ```sh
   pio device monitor -p /dev/cu.usbmodem2101 -b 115200
   ```

Expected normal-firmware boot log after a successful flash:

```text
DeskClock S3: booting RTC + LVGL shell (0.1.0-dev)
DeskClock S3: display shell started
```

If the port changes after manual bootloader entry, substitute the new port from `pio device list` in both upload and monitor commands.
