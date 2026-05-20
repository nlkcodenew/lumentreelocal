# 2026-05-17 Write Safety Plan

This document defines the safety plan for any future feature where Home
Assistant requests ESP32 to change inverter settings over BLE.

Current production firmware is read-only. It only sends Modbus function `03`
read-register requests. There is no inverter setting write path today.

## Goal

Evaluate whether controlled inverter setting writes are appropriate, then build
the smallest safe command path if the evidence supports it.

The goal is not to expose raw Modbus writes. The goal is a narrow set of
semantic, reviewed actions with strong auditability and rollback planning.

## Non-Negotiable Rules

- Do not let Home Assistant send raw register writes directly to the inverter.
- Do not expose a generic register/function/value service.
- Do not add Modbus write functions to firmware until the exact first command is
  documented, reviewed, and explicitly approved.
- Keep firmware write support disabled by default.
- Only allow whitelisted semantic commands.
- Validate every command in the API and again in firmware.
- Log every requested, approved, sent, acknowledged, failed, or rejected command
  in Postgres.
- Read the current register value before any write and read it again after the
  write for verification.
- Do not retry writes indefinitely.
- HASS UI must make dangerous actions explicit and require confirmation.
- The existing read-only telemetry path must keep working if write command
  processing is disabled or failing.

## Risk Assessment

The main risk is not BLE transport. The main risk is unknown register meaning,
unit scaling, persistence behavior, and side effects inside the inverter.

Bad writes could affect:

- battery charge/discharge limits
- grid charge behavior
- work mode
- cutoff thresholds
- backup/UPS behavior
- persistent inverter configuration

Any write feature must therefore be treated as a separate subsystem, not a small
extension of telemetry.

## Required Evidence Before Any Real Write

Before implementing a real write command, gather and record:

- The exact Modbus function, register address, scale, unit, and valid range.
- Whether the setting is volatile or persistent after reboot.
- The expected inverter response frame.
- The register(s) to read before and after write for verification.
- Whether the vendor app/cloud changes the same setting and what payload it
  sends, if payload capture is available.
- A rollback value or a documented reason why rollback is not possible.
- A safe test window and the expected physical/system effect.

## Command Model

Commands must be semantic, not raw. Example shape:

```json
{
  "command": "set_charge_current_limit",
  "device_id": "P240819130",
  "value": 20,
  "unit": "A",
  "risk_level": "medium",
  "requires_confirmation": true
}
```

Each whitelisted command definition must include:

- command name
- human label
- Modbus function
- register address or address range
- value encoding and scale
- min/max/step
- unit
- allowed inverter modes, if any
- required pre-read register(s)
- required post-write verification register(s)
- timeout
- retry policy
- risk level
- confirmation requirement
- rollback guidance

## Architecture

HASS must not talk directly to ESP32 for writes.

Recommended path:

```text
HASS action -> API command queue -> Postgres -> ESP32 poll -> BLE write -> verify read -> API ack -> HASS status
```

Write authorization is a separate gate before this command path. `Device ID`
alone is not enough for write permission. The accepted authorization design is
documented in `docs/plans/2026-05-17-write-authorization-flow.md`: ESP32 portal
generates a short-lived one-time write pairing code, HASS claims a scoped write
grant with that code, and the API stores only hashed credentials in Postgres.

### API Responsibilities

- Store commands in a durable table, for example `lumentree_commands`.
- Assign command IDs.
- Validate command name, device ID, value range, risk level, and confirmation.
- Reject unknown or out-of-range commands.
- Expose pending commands for the correct gateway/device only.
- Store state transitions:
  - `requested`
  - `approved`
  - `sent`
  - `acked`
  - `verified`
  - `failed`
  - `rejected`
  - `expired`
- Store audit fields:
  - requested by
  - requested at
  - gateway ID
  - firmware version
  - pre-read value
  - write payload hex
  - inverter response hex
  - post-read value
  - error message

### ESP32 Responsibilities

- Poll for one pending command at a time.
- Reject all writes unless write support is explicitly enabled.
- Reject commands not compiled into the whitelist.
- Validate device ID, target MAC, command name, range, and unit again.
- Execute pre-read.
- Send only the exact whitelisted write frame.
- Execute post-read verification.
- Upload command result with raw evidence.
- Continue telemetry upload even if command polling fails.

### HASS Responsibilities

- Expose only semantic controls/services.
- Use confirmation for medium/high-risk commands.
- Show last command status, last write time, and last error.
- Never expose raw register writes in the default UI.
- Explain failures in actionable language.

## Phase Plan

### Phase 0: Documentation Only

Current phase. No code write path.

- Keep read-only firmware.
- Document the safety model.
- Identify candidate settings worth researching.

### Phase 1: Protocol Research

- Build tools to capture or compare known setting changes.
- Record register map evidence.
- Do not write to the inverter.

### Phase 2: Dry-Run Command Queue

- [x] Add API command table and endpoints.
- [x] Add HASS service/control for a dry-run command.
- [x] Add ESP32 command polling in dry-run mode only.
- [x] ESP32 logs what it would do but does not call BLE write.

Exit criteria:

- [x] API can store a command.
- [x] ESP32 polls it.
- [x] ESP32 returns a dry-run result.
- [x] No BLE write occurs.
- [x] API rejects non-whitelisted commands.
- [ ] HASS service verified inside Home Assistant after HACS update/restart.

Implemented dry-run command:

- `dry_run_noop`

Production validation result:

```json
{
  "id": 1,
  "status": "dry_run_completed",
  "result": {
    "dry_run": true,
    "ble_write": false,
    "modbus_write": false,
    "write_enabled": false,
    "would_execute": false,
    "firmware": "lumentree-ble-bridge/0.5.0",
    "safety": "no_ble_write_function_called"
  }
}
```

Reject validation:

```text
POST command=set_charge_current_limit mode=dry_run -> HTTP 400
unsupported dry-run command: set_charge_current_limit
```

The HASS service `lumentreelocal.dry_run_command` now uses the scoped write
grant flow from `docs/plans/2026-05-17-write-authorization-flow.md`. Read-only
HASS setup remains Device ID only, and HASS no longer needs the production API
token in normal options.

### Phase 3: Mock Or Harmless Command

- If a truly harmless operation exists, test one tightly scoped command.
- Otherwise use a mock executor only.
- Require raw evidence in Postgres.

### Phase 4: First Real Write

Only after explicit approval for the exact command.

- Enable write support for one command only.
- Use a conservative test value.
- Pre-read and post-read must verify expected behavior.
- Stop immediately on unexpected response.

### Phase 5: Expand Whitelist Slowly

- Add one command at a time.
- Each command needs its own evidence and rollback notes.

## Candidate Commands To Research

These are candidates for research only. They are not approved write commands.

- charge current limit
- discharge current limit
- grid charge enable/disable
- work mode
- battery cutoff or reserve threshold
- time-of-use schedule
- work control setting: "the battery discharges to the loads" -> first
  discharge time target SOC. User-observed app state on 2026-05-18: first
  discharge time toggle is off, schedule is `20:00` to `08:00`, target SOC is
  `6%`, discharge power is `3500W`. Proposed research test is changing only
  target SOC from `6%` to `10%`, but this is not approved for real write until
  the exact register/function/scale, pre-read, post-read, and rollback evidence
  are recorded.

## Current Decision

Do not implement inverter writes yet.

The dry-run command queue exists and has been validated with `dry_run_noop`.
The next acceptable implementation step is protocol research or HASS-side
service validation after updating the integration. No real BLE write call is
approved.
