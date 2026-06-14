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

- themeable landscape clock face with date, mini calendar, status, next-alarm, and settings cards
- large RTC-backed time display in the center
- date line and sync status dot
- next-alarm indicator on the clock face
- on-device text-menu settings for time, Wi-Fi, brightness/theme/audio, and alarm management
- persisted settings and alarms using ESP32 local storage
- alarm audio through the board speaker when enabled
- optional personal theme assets loaded directly from a FAT32 microSD card; see [`docs/sd-assets.md`](docs/sd-assets.md)

Alarm behavior implemented so far:

- up to five saved alarms
- recurrence choices: once, daily, weekdays, weekends
- compact text-row alarm list and editor
- selected-row gesture controls: swipe up/down to choose a row, swipe left/right to adjust the selected value, tap to enter/save/back
- enable/disable per alarm from the editor or by left/right swiping an alarm row in the list
- delete confirmation
- full-screen alarm alert with Dismiss and Snooze
- fixed 10-minute snooze
- one-time alarms disable after firing
- missed alarms do not catch up on boot
- BOOT button dismisses an active alarm
- BOOT short-press opens settings from the clock face, or activates/adjusts the selected row in any open text menu
- BOOT long-press also opens the settings menu
- PWR short-press is ignored to avoid accidental navigation
- PWR long-press releases the battery power hold for shutdown on battery power; holding PWR starts the board when it is off
- RESET is a hardware reset line; on ESP32 external-reset boots, firmware advances the saved brightness through five visible levels

## Standalone/battery power

The Waveshare board uses an internal battery power-hold circuit controlled through the TCA9554 I/O expander. DeskClock S3 enables the board `SYS_EN` hold pin early during boot, matching the behavior used by the RSVP Nano firmware, so the device should remain powered after USB is removed or after the user releases the board power button. Audio enable also goes through the same `BoardPowerService` expander path so speaker setup cannot reset the shared power-hold pin.

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
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-settings.ppm --open settings
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-time.ppm --open time
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-alarms.ppm --open alarms
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-brightness.ppm --open brightness
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-network.ppm --open network
.pio/sim-build/deskclock_sim --screenshot .pio/deskclock-network-password.ppm --open network-password
```

Drive the selected-row interaction before capturing with comma-separated `--nav` actions (`up`, `down`, `left`, `right`, `tap`). This mirrors the text-menu control model: vertical movement selects rows, horizontal movement adjusts the selected value, and tap enters/saves/backs out.

```sh
.pio/sim-build/deskclock_sim --open time --nav down,right,down,right --screenshot .pio/deskclock-time-nav.ppm
.pio/sim-build/deskclock_sim --open alarms --nav tap,right,down,right --screenshot .pio/deskclock-alarm-edit-nav.ppm
.pio/sim-build/deskclock_sim --open network-password --nav right,tap,down,tap --screenshot .pio/deskclock-network-password-nav.ppm
```

Capture the deterministic visual-reference scene used for clock-face layout comparison:

```sh
.pio/sim-build/deskclock_sim --reference-scene --screenshot .pio/deskclock-reference-scene.ppm
sips -s format png .pio/deskclock-reference-scene.ppm --out .pio/deskclock-reference-scene.png
```

Preview a specific built-in theme by index:

```sh
.pio/sim-build/deskclock_sim --reference-scene --theme 2 --screenshot .pio/deskclock-theme-2.ppm
```

Preview SD-card theme assets in the simulator by preparing `.pio/sdcard` first:

```sh
python3 scripts/prepare_sd_assets.py
.pio/sim-build/deskclock_sim --reference-scene --theme 2 --screenshot .pio/deskclock-theme-2.ppm
```

Copy regenerated assets to a mounted FAT32 microSD card named `DESKCLOCK`:

```sh
python3 scripts/copy_sd_assets.py
```

Or pass an explicit mount path:

```sh
python3 scripts/copy_sd_assets.py /Volumes/DESKCLOCK
```

Or pin any RTC time in the simulator:

```sh
.pio/sim-build/deskclock_sim --fixed-time 2024-05-22T10:24:36 --screenshot .pio/deskclock-fixed.ppm
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
3. Short-press BOOT and confirm serial logs `BOOT: pressed`, `BOOT: released after ... ms`, and `BOOT: opening settings menu` plus setup UI opens.
4. In settings, verify swipe up/down changes the selected text row and swipe left/right adjusts timezone, brightness, or theme rows.
5. Press BOOT while a row is selected and confirm it activates/adjusts that row; navigate to `Back` and press BOOT to close the menu.
6. Alarm list opens from the next-alarm area.
7. Add an alarm a few minutes ahead using the selected-row editor: up/down selects hour/minute/repeat/enabled rows; left/right adjusts values; tap Save stores it.
8. Reboot and confirm the alarm persists.
9. Let the alarm fire and confirm visual alert plus audio.
10. Tap Dismiss and confirm the alert does not immediately re-open during the same minute.
11. Create/fire another alarm, tap Snooze, and confirm the next-alarm indicator shows the snoozed occurrence.
12. Let the snoozed alarm fire and confirm the sound window restarts.
13. Test BOOT while an alert is active and confirm it logs `BOOT: dismissing active alarm` and dismisses the alarm.
14. Optional edge check: fill all five saved alarm slots, fire one, tap Snooze, and confirm Snooze still works without needing a free saved-alarm slot.
