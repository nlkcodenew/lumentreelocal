# Schedule Safety Implementation Status

Date: 2026-05-21

## Scope Completed

The schedule safety invariant was implemented across all three enforcement
layers:

1. Home Assistant integration
2. local server
3. ESP32 firmware

The invariant is:

- an enabled mains charge window must never overlap an enabled discharge window
- touching boundaries are allowed
- overnight windows are treated correctly
- start/end time edits require the target slot to be OFF first

## What Changed

### Home Assistant

- Added a shared schedule safety helper module:
  - `custom_components/lumentreelocal/schedule_safety.py`
- Switch enable writes now validate both directions:
  - enabling mains charge checks enabled discharge windows
  - enabling discharge checks enabled mains charge windows
- Time writes now:
  - require the slot to be OFF first
  - reject changes that would make the resulting next-state overlap the
    opposite enabled schedule

### Server

- Added the same canonical overlap validator in `host/local-server/server.py`
- Schedule write commands are now rejected before queueing if:
  - the latest settings snapshot is missing
  - the latest settings snapshot is older than 5 minutes
  - the requested next-state would create a charge/discharge overlap
  - the user tries to edit start/end while the target slot is still enabled

### Firmware

- Added final schedule validation directly on the ESP32 before BLE write
- The firmware now:
  - reads the current schedule snapshot from the inverter immediately before
    deciding whether to write
  - rejects unsafe enable/time commands before Modbus/BLE write
  - reports those results back as `rejected`
- Added retry persistence for failed command-result uploads so a locally
  rejected command does not get lost if the network is down at the moment the
  rejection is reported

## Validation Completed

The following checks passed in this session:

- `python3 custom_components/lumentreelocal/test_schedule_safety.py`
- `python3 host/local-server/test_schedule_safety.py`
- `python3 -m py_compile custom_components/lumentreelocal/*.py`
- `python3 -m py_compile host/local-server/server.py`
- `python3 host/local-server/test_read_grant_endpoints.py`
- `python3 host/local-server/test_server_function_04.py`
- `python3 host/local-server/test_latest_statistics_merge.py`
- `pio run -e esp32-s3-8mb-nopsram-release`
- `pio run -e esp32-s3-8mb-nopsram-release -t upload --upload-port /dev/ttyACM0`

## Rollout Caveat

The ESP32 firmware was flashed successfully in this session.

The local server code was updated in the working tree and validated by tests,
but the system service was not restarted from this session because:

- `systemctl restart lumentree-local-server.service` required interactive
  authentication on this machine

That means:

- the repo contains the finished server-side schedule gate
- the current systemd-managed live server process may still be running the
  previous server code until it is restarted manually

## Immediate Next Step

Restart the local server service manually, then verify at runtime that a known
overlapping schedule command is rejected before being queued or executed.

## 2026-05-21 Follow-up: Write-Lane Latency Tuning

After the initial safety rollout, write UX was tightened further without
weakening the schedule-safety invariant.

Changes:

- firmware now polls pending commands before periodic telemetry upload in the
  main loop
- firmware forces an immediate settings snapshot upload after a successful write
  command
- firmware resets command polling immediately after write completion so the next
  queued command does not wait for the normal full poll interval
- Home Assistant temporarily switches to a faster poll interval while the last
  command status is still `requested` or `sent`

Goal:

- keep guarded writes precise
- make command status converge faster
- make the UI reflect inverter state sooner
- preserve all three schedule-safety gates unchanged
