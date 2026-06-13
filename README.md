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

If automatic upload fails with `Failed to connect to ESP32-S3: No serial data received`, put the board in bootloader mode manually and retry upload. On this board that usually means holding **BOOT**, tapping reset/reconnect, then releasing **BOOT** once upload starts.

Audio hardware test firmware:

```sh
pio run -e audio_test -t upload
```

## On-device verification checklist

After flashing normal firmware, verify:

1. Clock face shows retained RTC time and date.
2. Long-press BOOT opens setup.
3. Alarm list opens from the next-alarm area.
4. Add an alarm a few minutes ahead using the rollers and recurrence buttons.
5. Reboot and confirm the alarm persists.
6. Let the alarm fire and confirm visual alert plus audio.
7. Test Snooze, then Dismiss.
8. Test BOOT while an alert is active and confirm it dismisses the alarm.
