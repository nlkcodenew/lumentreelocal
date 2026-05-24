# 2026-05-24 Persistent BLE Session Refactor Implementation

## Summary

Firmware was refactored away from the normal per-read BLE reconnect pattern
toward a persistent session model with a tiered telemetry scheduler.

This is an implementation checkpoint, not yet a flashed runtime rollout note.

## What Changed

- added an in-firmware shared BLE session path for normal Modbus reads/writes
- stopped the normal periodic telemetry loop from reconnecting for each read
- added reconnect backoff for real BLE failures
- changed telemetry collection to maintain cached snapshots for:
  - main registers
  - settings registers
  - statistics registers
- changed the production scheduler to:
  - refresh cache by tier
  - upload cached snapshots
  - retry cached uploads without forcing a fresh BLE read first
- lowered firmware-configured minimum `SET_UPLOAD_INTERVAL` from `5` seconds to
  `1` second so fast cadence can be tested after flash

## Current Validation

Compile validation succeeded for:

- `esp32-s3-8mb-nopsram-release`
  - flash used: `1641865 / 3342336`
  - RAM used: `70124 / 327680`
- `esp32-c3-4mb-experimental`
  - flash used: `1767212 / 3014656`
  - RAM used: `64436 / 327680`

## Important Caveat

This note does not claim field validation yet.

Still required on real hardware:

- verify repeated telemetry cycles do not reconnect each time
- verify inverter stability at `SET_UPLOAD_INTERVAL 2`
- optionally try `SET_UPLOAD_INTERVAL 1` only after `2` is stable
- verify write path still completes cleanly during the persistent session
- verify reconnect after inverter/link interruption

## Follow-Up Recommendation

Do not tighten Home Assistant default polling until one flashed board confirms
that the new firmware session remains stable under faster telemetry cadence.
