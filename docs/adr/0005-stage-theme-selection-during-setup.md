# Stage Theme Selection during setup

## Status

Accepted

## Context

Visual Theme changes can require LVGL object updates and Theme Asset reloads. Applying those changes on every left/right adjustment in On-device Setup increases the risk that setup work destabilizes the Clock/Alarm Core.

DeskClock S3 prioritizes Setup Isolation over instant theme preview. Brightness is a special case because immediate changes help recover display visibility.

## Decision

Theme Selection changes will be staged during On-device Setup and applied only when the user saves display settings. Brightness changes may remain immediate when used for visibility or recovery.

## Consequences

- Theme adjustment in settings should update an editing value rather than immediately refreshing the clock face.
- Saving display settings applies the selected Visual Theme through a UI-owned path.
- Back/cancel can discard staged theme changes.
- A richer theme preview can be considered later, but it must preserve Setup Isolation.
