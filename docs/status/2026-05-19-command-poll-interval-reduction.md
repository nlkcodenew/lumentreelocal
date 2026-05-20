# Command Poll Interval Reduction

Date: 2026-05-19

## Problem

Writable Home Assistant switches could feel inconsistent when users toggled
multiple slots quickly.

The real bottleneck was not telemetry upload. The firmware was polling the
production command queue only every `30s`:

- `COMMAND_POLL_INTERVAL_MS = 30000`

That meant Home Assistant could enqueue multiple write commands quickly while
the ESP32 still had not claimed the earlier command from the server.

## Change

Reduced the firmware command poll interval only:

- `COMMAND_POLL_INTERVAL_MS: 30000 -> 5000`

Telemetry upload remained unchanged:

- production upload interval still `10s`

The command poll path is now named `pollPendingCommand()` to match its actual
role instead of the older dry-run naming.

## Safety

This change does not widen write scope and does not add new write commands.

It only reduces how long the ESP32 waits before asking the server whether a
queued command exists.

Write safety boundaries remain:

- write pairing code required
- scoped write grant required
- allow-listed semantic commands only
- inverter-side verification still runs per command

## Real-device validation

Tested on the physical ESP32-S3 device after flashing firmware `0.15.1`.

Safe no-op write test:

- command: `set_discharge_time_enable`
- requested value: slot 1 -> `OFF` while slot 1 was already `OFF`
- `requested_at`: `2026-05-19T04:55:16.908756Z`
- `claimed_at`: `2026-05-19T04:55:17.927879Z`
- `completed_at`: `2026-05-19T04:55:22.207991Z`
- safety result: `discharge_time_enable_already_at_requested_value_no_write_sent`

This confirms the command was claimed about one second after request instead of
waiting for the old 30-second polling cadence.

## Files

- `firmware/src/main.cpp`
- `firmware/platformio.ini`
