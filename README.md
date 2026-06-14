# DeskClock S3

Open, themeable clock and alarm firmware for ESP32-S3 touch-display boards.

The first supported target is the Waveshare ESP32-S3-Touch-LCD-3.49. The project is framed as a dependable desk clock/alarm first, with optional experiments for learning what the board can support: weather, voice input/output, LLM-backed greetings, and generated messages.

## Project goals

- Reliable time display that remains useful offline.
- Dependable local alarms that do not require network, weather, or AI services.
- On-device setup for time, timezone, Wi-Fi, alarms, brightness, and display preferences.
- A neutral built-in theme, with custom theming left to users.
- An app structure that can host optional experiments without making them part of the core clock/alarm path.

## Publishing and theming stance

This repository should not bundle branded, copyrighted, or character-specific artwork. Themes are visual only: colors, fonts, layouts, and user-supplied assets may change the appearance, but not the core timekeeping, alarm, or setup behavior.

Optional connected features should be framed as opt-in extensions:

- weather summaries
- generated greetings/messages
- microphone/speaker interaction
- LLM integration

## Current firmware behavior

The firmware currently brings up a landscape LVGL desk clock with:

- neutral theme/settings placeholder on the left
- large RTC-backed time display on the right
- date line and sync status dot
- next-alarm indicator on the clock face
- on-device time setup, Wi-Fi setup, brightness settings, and alarm management
- persisted settings and alarms using ESP32 local storage
- alarm audio through the board speaker when enabled

Alarm behavior implemented so far:

- up to five saved alarms
- recurrence choices: once, daily, weekdays, weekends
- hour/minute rollers for alarm time editing
- explicit recurrence buttons in the alarm editor
- enable/disable per alarm
- delete confirmation
- full-screen alarm alert with Dismiss and Snooze
- fixed 10-minute snooze
- one-time alarms disable after firing
- missed alarms do not catch up on boot
- BOOT button dismisses an active alarm
- BOOT long-press opens setup

## Standalone/battery power

The Waveshare board uses an internal battery power-hold circuit controlled through the TCA9554 I/O expander. DeskClock S3 now enables the board `SYS_EN` hold pin early during boot, matching the behavior used by the RSVP Nano firmware, so the device should remain powered after USB is removed or after the user releases the board power button.

If the board is fully off and not connected to USB, hold/press the board **PWR** button to start it. The firmware must boot far enough to enable `SYS_EN`; if it does not stay on, reconnect USB, flash the latest firmware, then test again with a charged battery.

## Build and flash

Normal firmware:

```sh
pio run -e waveshare_touch_lcd_3_49
pio run -e waveshare_touch_lcd_3_49 -t upload
```

Serial monitor:

```sh
pio device monitor -p /dev/cu.usbmodem2101 -b 115200
```

Run the serial monitor in an interactive terminal. Non-interactive shells may fail to initialize PlatformIO's monitor console because it expects a real TTY.

If automatic upload fails with `Failed to connect to ESP32-S3: No serial data received`, put the board in bootloader mode manually and retry upload. See [`docs/troubleshooting.md`](docs/troubleshooting.md) for the current upload-failure notes.

Manual ESP32-S3 bootloader recovery:

1. Start the upload command and wait for `Connecting...`, or prepare to run it in the next step.
2. Hold **BOOT**.
3. While still holding **BOOT**, tap **RESET** if available, or unplug/replug USB.
4. Keep holding **BOOT** until the terminal moves past `Connecting...` and begins writing flash.
5. Release **BOOT** after upload starts.
6. If the port changes, list ports with `pio device list` and retry with `--upload-port <port>`.

Example explicit-port retry:

```sh
pio run -e waveshare_touch_lcd_3_49 -t upload --upload-port /dev/cu.usbmodem2101
```

Audio hardware test firmware:

```sh
pio run -e audio_test
pio run -e audio_test -t upload
```

Current release-readiness build checks:

```sh
pio run -e waveshare_touch_lcd_3_49
pio run -e audio_test
```

## Desktop simulator

A macOS LVGL/SDL simulator is available for UI work without the physical board. It renders the app at the board's landscape screen size, 640×172, with mocked RTC, preferences, Wi-Fi, backlight, and audio services.

Build and run the interactive simulator:

```sh
cmake -S sim -B .pio/sim-build
cmake --build .pio/sim-build
.pio/sim-build/deskclock_sim
```

Capture a headless screenshot for review/regression checks:

```sh
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-sim.ppm
sips -s format png .pio/deskclock-sim.ppm --out .pio/deskclock-sim.png
```

Open a specific view before capturing:

```sh
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-time.ppm --open time
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-alarms.ppm --open alarms
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-brightness.ppm --open brightness
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-network.ppm --open network
```

Dump visible LVGL object coordinates when diagnosing layout issues:

```sh
.pio/sim-build/deskclock_sim --open time --dump-layout
```

The simulator is not a full ESP32-S3 hardware emulator: it does not verify real LCD/touch timing, speaker output, Wi-Fi, power, or RTC electrical behavior. Use it for layout, theme, navigation, and clock/alarm state-flow work; keep the hardware checklist for final device validation.

## On-device verification checklist

A release-readiness summary is available at [`docs/release-readiness.md`](docs/release-readiness.md).
A printable/fillable test log is available at [`docs/hardware-verification.md`](docs/hardware-verification.md).

After flashing normal firmware, verify:

1. Serial boot log includes `DeskClock S3: booting RTC + LVGL shell (0.1.0-dev)` so the flashed image is identifiable.
2. Clock face shows retained RTC time and date.
3. Short-press BOOT and confirm serial logs `BOOT: pressed` and `BOOT: released after ... ms`.
4. Long-press BOOT for at least 1.2 seconds and confirm serial log `BOOT: long press opening setup` plus setup UI opens.
5. Alarm list opens from the next-alarm area.
6. Add an alarm a few minutes ahead using the rollers and recurrence buttons.
7. Reboot and confirm the alarm persists.
8. Let the alarm fire and confirm visual alert plus audio.
9. Tap Dismiss and confirm the alert does not immediately re-open during the same minute.
10. Create/fire another alarm, tap Snooze, and confirm the next-alarm indicator shows the snoozed occurrence.
11. Let the snoozed alarm fire and confirm the sound window restarts.
12. Test BOOT while an alert is active and confirm it logs `BOOT: dismissing active alarm` and dismisses the alarm.
13. Optional edge check: fill all five saved alarm slots, fire one, tap Snooze, and confirm Snooze still works without needing a free saved-alarm slot.
