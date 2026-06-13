# DeskClock S3

DeskClock S3 is open firmware for ESP32-S3 touch-display boards. It should behave first as a reliable desk clock and alarm: the time is readable at a glance, alarms are dependable, and setup can happen on-device. The project also leaves room for optional experiments—weather, voice input/output, LLM-backed messages and greetings—without making those experiments required for the core clock/alarm experience.

Visual styling is user-controlled. This repository should ship a neutral default theme and avoid bundled branded, copyrighted, or character-specific assets.

## Language

**Clock/Alarm Core**:
The dependable baseline experience: showing the current local time, showing the date, and firing configured alarms. The core must remain useful without network access, weather data, or AI services.
_Avoid_: novelty display, cloud-only clock, AI-dependent alarm

**Reliable Time**:
The clock's displayed time should remain plausible and useful even when internet access is unavailable. Network access may correct the time, but the clock should not depend on it for everyday use.
_Avoid_: Wi-Fi-only time, online-only clock

**Alarm**:
A user-configured local event that produces an audible and/or visible alert at the selected time. Alarms are part of the core experience and should be stored locally.
_Avoid_: cloud reminder, notification-only event, productivity timer

**Alarm Reliability**:
An alarm should fire from local device state even when optional services are unavailable. Network sync may improve time accuracy, but alarms must not depend on weather, generated messages, or LLM responses.
_Avoid_: AI-gated alarm, internet-required alarm

**Alarm Alert**:
The visible and optionally audible signal produced when an alarm fires. It must always include a visual alert, while sound may be added when speaker output is available and configured.
_Avoid_: audio-only alarm, speaker-required alarm

**Alarm Dismissal**:
The user action that stops an active alarm alert. It should be available from both the touchscreen and the BOOT button so the alarm can be stopped reliably in normal use.
_Avoid_: touch-only dismissal, app-only dismissal, power-button-required dismissal

**Snooze**:
A temporary postponement of an active alarm alert. In v1, snooze should use a fixed 10-minute delay rather than a user-configurable duration.
_Avoid_: custom snooze rule, theme-owned snooze behavior

**Sync Status**:
A small visible cue that tells whether the displayed time is recently externally confirmed, locally retained while offline, or not yet trustworthy. It should build trust without becoming the main focus of the clock face.
_Avoid_: Wi-Fi status, connection badge

**On-device Setup**:
A setup experience that can be completed using the clock itself, without editing code, reflashing firmware, or relying on a computer after installation. It covers first-time configuration, later changes to user preferences, and an offline path when network access is unavailable.
_Avoid_: hard-coded setup, computer-only setup

**First-run Setup**:
The setup experience shown when the clock has not yet been configured. It should guide the user directly into required choices while allowing Wi-Fi to be skipped in favor of manual time setup.
_Avoid_: out-of-box demo mode, computer-required first setup

**Local Timezone**:
The user's chosen place-based time rule for converting universal time into the wall-clock time shown by the clock. It should be selected during setup rather than guessed from network access.
_Avoid_: guessed timezone, raw UTC offset

**Manual Time Setup**:
The on-device setup path where the user enters the current date and time without relying on network access. Manually entered time should make the clock usable offline but should be distinguished from externally synced time.
_Avoid_: computer-set time, fake synced time

**Calm Time Display**:
A clock face where hours and minutes are the primary readable information. Seconds may appear as a small supporting detail or gentle animation, but should not make the clock feel busy.
_Avoid_: stopwatch display, seconds-first clock

**Time Format**:
The user's preference for displaying wall-clock time as either 12-hour or 24-hour time. It should be selected during setup and changeable later.
_Avoid_: hard-coded hour format

**Date Display**:
A secondary line of calendar information shown on the main clock face. It should support usefulness without competing with the primary time display.
_Avoid_: date-first display, theme-owned date behavior

**Desk Clock**:
A personal ambient display focused on showing the current time, date, and alarm state. Optional data may appear when enabled, but the device should still read primarily as a desk clock.
_Avoid_: general smart display, phone-shaped dashboard

**Brightness Schedule**:
A user preference that changes screen brightness between daytime and nighttime levels. It should make the clock comfortable as an ambient desk display without requiring extra light-sensing hardware.
_Avoid_: auto brightness, fixed brightness only

**Always-glanceable Display**:
The clock face should remain visible by default so the device behaves like a desktop clock rather than a wake-on-touch gadget. Night behavior should prefer dimming over turning the screen fully off.
_Avoid_: sleep-first display, touch-to-wake clock

**Network Selection**:
The on-device setup step where the user chooses which nearby network the clock should join. The user should select from discovered network names rather than manually entering the network name.
_Avoid_: manual SSID entry, computer-assisted Wi-Fi setup

**Credential Entry**:
The on-device setup step where the user enters a selected network's password. It should be possible to complete without a computer or reflashing the clock.
_Avoid_: USB password entry, hard-coded credentials

**Landscape Clock Face**:
The main clock view is meant to sit horizontally on a desk, prioritizing large readable time across the wide dimension of the screen. It should feel like a small desktop clock rather than a phone-shaped widget.
_Avoid_: portrait clock face, vertical widget

**Visual Theme**:
A selectable visual presentation for the clock. A visual theme may change colors, fonts, spacing, backgrounds, decorative assets, and character identity, but must not change the clock's product identity, timekeeping, alarm behavior, setup, or reliability behavior.
_Avoid_: behavioral theme, clock type, mode, product identity

**Theme Asset**:
A user-supplied image, decorative graphic, font, or other visual material used by a visual theme. Theme assets are replaceable presentation content rather than core clock behavior.
_Avoid_: built-in clock logic, bundled character asset, system behavior

**Neutral Default Theme**:
The built-in visual theme that is always available so the clock can present a complete face even when no user-added themes are present. It should be generic and suitable for publication in an open repository.
_Avoid_: branded default, character-specific default, copyrighted default art

**Custom Theme**:
A user-added visual theme beyond the neutral default theme. It should extend the available appearances without changing what the clock is.
_Avoid_: plug-in behavior that changes core logic, required custom assets

**Theme Selection**:
The on-device preference flow where the user previews and chooses the active visual theme. Theme selection changes appearance only and should not be triggered accidentally from the main clock face.
_Avoid_: quick theme switching, accidental appearance change

**Optional Capability**:
A non-core feature used to explore what the board can do, such as weather, voice input/output, generated messages, or LLM integration. Optional capabilities must be opt-in and should degrade cleanly when unavailable.
_Avoid_: core dependency, always-on cloud feature

**Weather Summary**:
A small, secondary piece of forecast or current-condition data shown only when configured. Weather should support glanceability without competing with the clock/alarm core.
_Avoid_: weather-first dashboard, network-required clock face

**Generated Message**:
An optional greeting, note, or short message produced by an AI service or local generation path. Generated messages are presentation content and must not determine alarm reliability or timekeeping behavior.
_Avoid_: AI-owned clock behavior, required greeting service

**Voice Interface**:
An optional microphone/speaker interaction path for experiments such as spoken setup, queries, or LLM-backed interaction. It must be explicit, privacy-conscious, and separable from the core clock/alarm functions.
_Avoid_: always-listening requirement, voice-only setup, smart-speaker-first framing

**Settings Entry**:
The deliberate action that opens the clock's on-device setup and preferences. It should be unlikely to happen accidentally during normal clock viewing. The preferred entry is a long press on the board's BOOT button during normal operation.
_Avoid_: always-visible settings control, hidden accidental gesture
