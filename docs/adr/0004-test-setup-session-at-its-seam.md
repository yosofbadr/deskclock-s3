# Test Setup Session at its seam

## Status

Accepted

## Context

Setup reliability failures are difficult to diagnose by manual hardware testing alone. The failures usually involve ordering between UI actions, Wi-Fi work, portal handling, local device state, and Clock/Alarm Core ticking.

DeskClock S3 needs a repeatable reliability loop without requiring a giant full-device simulator.

## Decision

DeskClock S3 will test setup reliability at the Setup Session seam before and during major setup refactors. The harness should use fake adapters for Wi-Fi/AP operations, portal handling, and local device state persistence.

The first harness should verify the core setup flow: start phone setup, reach portal-active state, submit credentials, durably save credentials, begin connection, preserve credentials on failure, and keep setup transitions non-blocking relative to the Clock/Alarm Core.

## Consequences

- The Setup Session interface becomes the main test surface for setup reliability.
- Manual hardware tests remain necessary but become smaller confirmation tests.
- Network Selection and Credential Entry bugs should be reproducible without joining the real setup access point.
- The project avoids over-investing in a full simulator before the main setup seam is clear.
