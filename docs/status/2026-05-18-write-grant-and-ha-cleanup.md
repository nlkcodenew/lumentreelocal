# 2026-05-18 Write Grant And HA Cleanup

This status note records the completed scoped write grant rollout and the Home
Assistant warning cleanup that followed.

## Scoped Write Grant Rollout

Commit pushed:

```text
fd67f1c feat: add scoped lumentree write grant flow
```

Production state after rollout:

- ESP32 firmware flashed and validated: `lumentree-ble-bridge/0.6.0`.
- Home Assistant integration source version: `0.9.0`.
- Production API server restarted and active as `lumentree-local-server`.
- Public API remains `https://lumentree.jonah.io.vn`.
- ESP32 remained online after flashing; health reported firmware `0.6.0`.

Implemented components:

- API/Postgres:
  - `lumentree_write_pairing_codes`
  - `lumentree_write_grants`
  - `lumentree_write_audit`
  - gateway endpoint for registering one-time write pairing codes
  - HASS endpoint for claiming scoped write grants
  - grant status endpoint
  - grant revoke endpoint
- ESP32 firmware:
  - portal `Write Access` section
  - serial command `WRITE_STATUS`
  - serial command `GENERATE_WRITE_CODE`
  - code registration with API using the ESP32 production token
- HASS integration:
  - normal setup remains Device ID only
  - options flow accepts one-time write pairing code
  - production API token is no longer part of normal HASS write UX
  - scoped write grant token is stored internally after claim
  - diagnostic entities added for write access status and active grant

Safety validation:

- No BLE write command was added.
- No Modbus write function was added.
- Only command accepted by API remains:
  - `mode=dry_run`
  - `command=dry_run_noop`
- Dry-run through a scoped grant completed with:
  - `dry_run=true`
  - `ble_write=false`
  - `modbus_write=false`
  - `write_enabled=false`
- Unauthenticated command creation returned:
  - HTTP `401`
  - `write grant required`
- Temporary validation write grants were revoked.
- Temporary active write pairing codes were consumed/cleared.

Current write grant status after cleanup:

```json
{
  "write_available": true,
  "write_enabled": false,
  "grant_active": false,
  "active_grant_count": 0,
  "command_mode": "dry_run_only",
  "safety": "no_real_inverter_write_enabled"
}
```

## Home Assistant Warning Cleanup

User-reported warning:

```text
Forced update failed. Entity update.lumentree_inverter_update_2 not found.
Forced update failed. Entity update.tb_energy_flow_card_update not found.
```

Findings:

- Both entities were HACS `update` entities in restored/unavailable state.
- They were stale entity registry entries, not live update entities.
- Valid related update entities remain:
  - `update.lumentree_inverter_hass_update`
  - `update.lumentree_local_update`
  - `update.hacs_update`

Cleanup performed through Home Assistant WebSocket API:

```text
config/entity_registry/remove update.lumentree_inverter_update_2
config/entity_registry/remove update.tb_energy_flow_card_update
```

Post-cleanup validation:

- `/api/states` no longer contains:
  - `update.lumentree_inverter_update_2`
  - `update.tb_energy_flow_card_update`
- `config/entity_registry/list` no longer contains either entity.
- Valid HACS update entities still exist.

Note:

- The HA log keeps historical warning lines until the log rotates or Home
  Assistant is restarted.
- A later warning at `2026-05-18 01:53:38` was produced by a deliberate
  validation call that targeted the removed entities to confirm the behavior.
  It is not evidence of a new recurring source after registry cleanup.

## Next Checks

- After the next Home Assistant restart, confirm the two stale update entities
  do not return.
- Update/restart the HASS integration to load `lumentreelocal` version `0.9.0`.
- Test user-facing write grant UX:
  1. Generate code from ESP32 portal or `GENERATE_WRITE_CODE`.
  2. Enter code in Lumentree Local options.
  3. Confirm write grant diagnostic entity becomes active.
  4. Run `lumentreelocal.dry_run_command`.

## HASS Write Grant UX Validation

Validated on 2026-05-18 after Home Assistant loaded `lumentreelocal` version
`0.9.0`.

Confirmed HASS entities:

- `sensor.lumentree_local_p240819130_write_access_status`
- `binary_sensor.lumentree_local_p240819130_write_grant_active`

Flow used:

1. ESP32 serial command `GENERATE_WRITE_CODE` registered a one-time code with
   the API.
2. Home Assistant options flow accepted the write pairing code.
3. `binary_sensor.lumentree_local_p240819130_write_grant_active` changed to
   `on`.
4. `sensor.lumentree_local_p240819130_write_access_status` changed to
   `enabled`.
5. HASS service call succeeded:

```text
lumentreelocal.dry_run_command
device_id=P240819130
command=dry_run_noop
```

Production command result:

```json
{
  "id": 3,
  "requested_by": "home_assistant",
  "status": "dry_run_completed",
  "result": {
    "dry_run": true,
    "firmware": "lumentree-ble-bridge/0.6.0",
    "ble_write": false,
    "modbus_write": false,
    "write_enabled": false,
    "would_execute": false,
    "safety": "no_ble_write_function_called"
  }
}
```

This validates the intended full loop:

```text
ESP32 one-time code -> HASS options claim -> scoped grant -> HASS service ->
API command queue -> ESP32 poll -> dry-run result
```

No real inverter write path exists after this validation.

## Command Status Diagnostics

Task 1 implementation after the dry-run validation:

- Added public redacted endpoint:
  `/api/lumentree/devices/{device_id}/commands/status`
- Full command history remains protected by the production server token at:
  `/api/lumentree/devices/{device_id}/commands`
- Added HASS diagnostic sensors:
  - `last_command_status`
  - `last_command_name`
  - `last_command_time`
  - `last_command_safety`
  - `last_command_error`
- HASS integration source version bumped to `0.9.1`.

The redacted endpoint exposes command status and safety flags only. It does not
expose command payloads, raw tokens, or unrestricted command history.

Recorded future write research candidate:

- Lumentree app -> Work Control Settings -> "the battery discharges to the
  loads" -> First discharge time.
- User-observed state:
  - toggle: off
  - time: `20:00` to `08:00`
  - target SOC: `6%`
  - discharge power: `3500W`
- Candidate research test: target SOC `6%` to `10%`.
- This is not approved as a real write command yet. It still needs exact
  protocol evidence, pre-read/post-read verification, and explicit approval.
