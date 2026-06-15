# Setup Session owns network setup lifecycle

## Status

Accepted

## Context

DeskClock S3 must preserve Setup Isolation: On-device Setup, Network Selection, Credential Entry, phone setup, visual theme changes, and optional configuration failures must not freeze or disable the Clock/Alarm Core.

The current implementation spread setup lifecycle knowledge across menu views, network code, web handlers, storage calls, and LVGL refresh paths. That made simple setup actions sensitive to ordering mistakes, task ownership mistakes, and Wi-Fi mode transitions.

## Decision

DeskClock S3 will route setup work through a Setup Session state machine. UI and web actions create Setup Intents; the Setup Session owns phone setup, portal startup, Credential Entry persistence, connection attempts, and user-visible setup status.

Credential Entry is considered saved only after credentials are durably written to local device state. A successful Wi-Fi connection is a later outcome, not a precondition for saved credentials.

## Consequences

- On-device Setup actions must not perform blocking Wi-Fi work inline.
- Network Selection, phone setup, and connection attempts become state transitions owned by the Setup Session.
- The Clock/Alarm Core must continue ticking in every setup state.
- Tests can exercise setup through the Setup Session interface rather than through scattered menu and Wi-Fi calls.
- The implementation will need adapters for hardware Wi-Fi, portal handling, local device state, and UI status updates.
