# 2026-05-24 Persistent BLE Session Refactor Plan

## Problem

Current firmware treats BLE as a short-lived transport for each read cycle:

- connect
- subscribe for notify
- send one Modbus request
- wait for response
- disconnect

This happens repeatedly during normal telemetry upload. That design creates
avoidable overhead, reconnect churn, and extra latency even though the gateway,
inverter, and server all have stable power.

## Goal

Refactor the firmware to keep one long-lived BLE session to the paired inverter
and move polling policy to an internal scheduler with register tiers.

Primary targets:

- lower end-to-end telemetry latency
- reduce BLE reconnect churn
- preserve guarded write behavior
- keep Home Assistant reading server cache instead of driving inverter reads

## Scope

In scope:

- firmware BLE session lifecycle
- firmware telemetry scheduler
- firmware read cache / latest snapshot handling
- reconnect and backoff policy
- documentation updates needed for rollout and handoff

Out of scope for this phase:

- changing the public API contract
- changing Home Assistant entity model
- adding arbitrary Modbus write support
- multi-gateway coordination changes
- flash-site UX changes unless firmware selection text must be updated

## Intended Design Direction

1. Introduce a persistent BLE session manager.
2. Keep notify registration active across multiple requests.
3. Split reads into tiers:
   - fast realtime metrics
   - slower settings/config snapshots
   - slower statistics snapshots
4. Upload from cached snapshots rather than starting a fresh BLE connection per
   upload loop.
5. Reconnect only on real BLE failure, with bounded backoff.
6. Keep command polling from the server separate from BLE telemetry cadence.

## Acceptance Criteria

- no normal telemetry path reconnect for every read cycle
- known target MAC reconnects directly without discovery scan on every loop
- fast telemetry path can run at a tighter cadence than settings/statistics
- write verification still works and does not bypass schedule safety
- firmware still builds for:
  - `esp32-s3-8mb-nopsram-release`
  - `esp32-c3-4mb-experimental`

## Risk Notes

- some vendor BLE devices behave poorly with very aggressive read cadence even
  when the transport stays connected
- `ESP32-C3` RAM pressure may be tighter than `ESP32-S3` once session state and
  caches are added
- write and read serialization must stay explicit; concurrent BLE requests are
  not acceptable for this inverter
