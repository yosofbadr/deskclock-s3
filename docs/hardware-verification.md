# DeskClock S3 Hardware Verification Log

Use this checklist after flashing the normal `waveshare_touch_lcd_3_49` firmware to the Waveshare ESP32-S3-Touch-LCD-3.49 board.

## Test session

- Date:
- Tester:
- Firmware version from serial boot log:
- Board USB port:
- Upload method:
  - [ ] Automatic upload
  - [ ] Manual BOOT/reset bootloader recovery
- Notes:

## Preflight

- [ ] `pio run -e waveshare_touch_lcd_3_49` passes.
- [ ] Serial monitor opens at 115200 baud.
- [ ] Serial boot log includes `DeskClock S3: booting RTC + LVGL shell (0.1.0-dev)`.
- [ ] Display backlight turns on.
- [ ] Clock face is visible in landscape orientation.

## BOOT button behavior

- [ ] Short-press BOOT logs `BOOT: pressed`.
- [ ] Short-press BOOT logs `BOOT: released after ... ms`.
- [ ] Long-press BOOT for at least 1.2 seconds logs `BOOT: long press opening setup`.
- [ ] Long-press BOOT opens setup on-device.
- [ ] BOOT does not perform PWR/screen behavior in v1.

Observed logs:

```text

```

## Clock and setup behavior

- [ ] Clock face shows retained RTC time and date.
- [ ] Manual time setup can adjust hour/minute.
- [ ] Manual time setup can adjust day/month/year.
- [ ] Invalid dates are clamped when changing month/year.
- [ ] Saved Wi-Fi credentials reconnect automatically when available.
- [ ] Brightness setting is adjustable and persists.
- [ ] 12/24-hour format preference is adjustable and persists.

Notes:

```text

```

## Alarm end-to-end behavior

- [ ] Alarm list opens from the next-alarm area.
- [ ] Add an alarm a few minutes ahead using hour/minute rollers.
- [ ] Select recurrence with explicit recurrence buttons.
- [ ] Main clock face shows the next-alarm indicator.
- [ ] Reboot and confirm the alarm persists.
- [ ] Alarm fires at the expected time.
- [ ] Full-screen visual alert appears.
- [ ] Alarm audio plays through the speaker.
- [ ] Tap Dismiss and confirm the alert closes.
- [ ] Dismissed alarm does not immediately re-open during the same minute.
- [ ] Create/fire another alarm, tap Snooze, and confirm the next-alarm indicator shows the snoozed occurrence.
- [ ] Snoozed alarm fires after 10 minutes.
- [ ] Snoozed alarm gets a fresh audio window.
- [ ] BOOT dismisses an active alarm and logs `BOOT: dismissing active alarm`.
- [ ] One-time alarms disable after firing.
- [ ] Missed one-time alarms do not fire unexpectedly after reboot.

Observed logs:

```text

```

## Full-slot snooze edge check

- [ ] Fill all five saved alarm slots.
- [ ] Fire one alarm.
- [ ] Tap Snooze.
- [ ] Snooze succeeds without requiring a free saved-alarm slot.
- [ ] Snoozed occurrence appears in the next-alarm indicator.
- [ ] Snoozed occurrence fires.

Notes:

```text

```

## Result

- [ ] PASS: firmware is ready for local release/use.
- [ ] FAIL: issues found; list below.

Issues:

```text

```
