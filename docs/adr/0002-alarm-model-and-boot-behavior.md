# Alarm model and boot behavior

DeskClock S3 treats alarms as a small local clock/alarm core feature: up to five saved alarms, persisted locally, with one-time, daily, weekdays, and weekends recurrence. One-time alarms represent an exact date and time and disable after firing or being missed; the device must not surprise-fire missed alarms on boot, because reliability should not feel like unexpected catch-up behavior.

The main clock face should show a small next-alarm indicator with the alarm time and short recurrence label. Snooze is fixed at 10 minutes for v1, and each snoozed occurrence gets its own 5-minute sound window.
