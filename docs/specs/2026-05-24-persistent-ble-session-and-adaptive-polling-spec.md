# 2026-05-24 Persistent BLE Session And Adaptive Polling Spec

## Objective

Replace per-read BLE reconnects with a persistent inverter session and an
adaptive polling scheduler that serves near-realtime telemetry while preserving
write safety.

## Existing Runtime Shape

Current firmware behavior:

- server command polling runs on its own timer
- production upload loop calls direct BLE reads
- each BLE read opens and closes a GATT session
- main registers are read every upload interval
- settings and statistics are read on slower side intervals

This separation between server polling and telemetry cadence is good and should
be preserved. The BLE session lifecycle is the part that should change.

## New Runtime Model

### 1. BLE Session State

Firmware should maintain explicit states:

- `unpaired`
- `discovering`
- `connecting`
- `ready`
- `degraded`
- `backoff_wait`

`ready` means:

- BLE link connected
- FFE0 service resolved
- FFE1 characteristic resolved
- notify registration completed if supported

### 2. Request Serialization

Only one inverter transaction may be in flight at a time.

Rules:

- no overlapping read/read
- no overlapping read/write
- no overlapping write/write
- writes have priority over periodic reads

### 3. Poll Tiers

#### Fast tier

Purpose:

- near-realtime user-facing power and state values

Target cadence:

- 1 to 2 seconds if stable
- allow fallback to 3 to 5 seconds if field validation shows inverter stress

Preferred source:

- the smallest proven register range that carries:
  - SOC
  - PV power
  - grid power
  - load power
  - battery power
  - core status/alarm fields

#### Settings tier

Purpose:

- schedule/config snapshots

Target cadence:

- 60 to 300 seconds
- immediate refresh after successful write completion

#### Statistics tier

Purpose:

- slower-changing energy counters from function `0x04`

Target cadence:

- 60 to 300 seconds

### 4. Upload Behavior

ESP32 should upload from the latest cached snapshots:

- fast telemetry cache
- settings cache
- statistics cache

The upload loop should not imply a fresh BLE connect.

### 5. Reconnect Policy

When a healthy paired session drops:

- reconnect directly to known `target_mac`
- do not rescan unless direct reconnect repeatedly fails
- use bounded backoff such as `1s, 2s, 5s, 10s, 30s`

Auto-discovery should remain only for:

- initial pairing
- explicit repair / portal-driven rebind

### 6. Home Assistant / Server Contract

No architecture change is needed here:

- ESP32 talks to inverter over BLE
- ESP32 uploads to server
- Home Assistant reads server cache

This means:

- Home Assistant polling may become faster without increasing inverter BLE load
- server `/latest` remains the stable consumption surface

### 7. Write Safety Requirements

The new session manager must not weaken the existing guarded write path.

Mandatory invariants:

- write commands remain semantic only
- schedule safety checks still run before BLE write
- post-write verification still runs before reporting success
- periodic reads must not interrupt a write verification chain

## Implementation Notes

- start with one shared BLE client/session for both reads and writes
- separate transport/session code from register scheduling logic
- store last-good timestamps for each cache tier
- expose enough serial logs to distinguish:
  - connected but idle
  - reconnecting
  - backoff
  - in-flight read
  - in-flight write

## Validation Standard

The refactor is only acceptable if all of these are true:

- repeated fast telemetry updates happen without reconnect-per-read churn
- settings snapshot still uploads after write completion
- function `0x04` statistics still decode and upload correctly
- direct reconnect after a forced disconnect works on both current firmware
  targets
