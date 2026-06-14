# DeskClock S3 Release Readiness

Status: **software-ready pending final hardware verification**.

The firmware has the core v1 clock/alarm behavior implemented and build-verified. Final release/use should wait for a successful normal-firmware upload and the hardware verification log in [`hardware-verification.md`](hardware-verification.md) to pass.

## Implemented v1 scope

- RTC-backed clock display with retained offline time.
- On-device setup for time/date, Wi-Fi, alarms, brightness, and display format preferences.
- Saved Wi-Fi reconnect for NTP correction.
- Local alarm storage with up to five saved alarms.
- Once, Daily, Weekdays, and Weekends recurrence.
- Alarm create/edit/delete UI with hour/minute rollers and delete confirmation.
- Themeable landscape clock face with date, mini calendar, status, settings, and next-alarm cards.
- Main clock next-alarm indicator.
- Full-screen visual alarm alert.
- Speaker alarm tone with global volume and settings test sound.
- Dismiss and fixed 10-minute Snooze touch controls.
- BOOT dismissal for active alarms.
- BOOT long-press entry to setup.
- Missed one-time alarms disable on boot instead of firing late.
- Dismissed alarms do not immediately re-trigger in the same minute.
- Snooze does not consume one of the five saved alarm slots.
- Snoozed occurrences are included in next-alarm calculations.
- Shared TCA9554 power/audio control keeps `SYS_EN` asserted after audio setup.

## Verified in software/builds

- `pio run -e waveshare_touch_lcd_3_49` passes.
- `pio run -e audio_test` passes.
- Firmware version `0.1.0-dev` is included in normal and audio-test serial boot logs.
- README documents normal/audio build commands, manual bootloader recovery, standalone battery startup, and on-device verification steps.

## Previously verified on hardware

- Display/backlight/LVGL shell works on the Waveshare board.
- PCF85063 RTC works and displays correct retained time/date.
- Audio codecs initialize in audio-test firmware.
- Speaker beep output was heard by the user.

## Remaining hardware gate

Final verification is blocked until the board is manually placed into ESP32-S3 bootloader mode and accepts the normal firmware upload. The last automatic upload attempt found `/dev/cu.usbmodem2101` but failed with:

```text
Failed to connect to ESP32-S3: No serial data received
```

See [`troubleshooting.md`](troubleshooting.md) for upload recovery notes. After upload succeeds, complete [`hardware-verification.md`](hardware-verification.md), especially:

- Standalone battery boot via PWR, including staying on after PWR release.
- BOOT short/long press logs and setup entry.
- Alarm creation and persistence after reboot.
- Visual/audio alarm firing.
- Dismiss behavior without same-minute retrigger.
- Snooze behavior and 10-minute refire.
- BOOT dismissal while an alarm is active.
- Full-slot Snooze edge case.
