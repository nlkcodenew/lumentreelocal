# 2026-05-18 Target SOC Write Command Rollout

## Scope

Implemented one semantic write command:

```text
set_first_discharge_target_soc
```

This is not a generic register writer.

## Server

The API now accepts:

```json
{
  "device_id": "P240819130",
  "command": "set_first_discharge_target_soc",
  "mode": "write",
  "payload": {"target_soc": 17}
}
```

Safety constraints:

- command must be exactly `set_first_discharge_target_soc`
- mode must be `write`
- `payload.target_soc` must be integer `5..100`
- command creation still requires server auth or a scoped write grant

## Firmware

Firmware version:

```text
lumentree-ble-bridge/0.9.0
```

Execution path:

1. Pre-read register `144`.
2. If current value equals requested value, skip BLE write and report verified.
3. Otherwise write one register with Modbus function `16` (`0x10`):

```text
slave=1, function=16, register=144, count=1, value=target_soc
```

4. Validate function `16` ACK.
5. Post-read register `144`.
6. Report completed only if post-read matches the requested value.

## Home Assistant

Added service:

```text
lumentreelocal.set_first_discharge_target_soc
```

Fields:

- `device_id` optional
- `target_soc` required, `5..100`

The service requires the configured write grant token, same authorization
surface as the dry-run command service.

## Live Test

Target test:

```text
First discharge time Target SOC: 6% -> 17%
```

Command:

```json
{
  "id": 4,
  "command": "set_first_discharge_target_soc",
  "mode": "write",
  "requested_by": "codex_live_test",
  "payload": {"target_soc": 17}
}
```

Result:

```json
{
  "status": "completed",
  "firmware": "lumentree-ble-bridge/0.9.0",
  "ble_write": true,
  "modbus_write": true,
  "write_enabled": true,
  "register": 144,
  "requested_value": 17,
  "before_value": 6,
  "after_value": 17,
  "write_ack": true,
  "verified": true,
  "safety": "semantic_target_soc_pre_read_function_16_post_read"
}
```

Conclusion:

- The semantic write command succeeded.
- Register `144` changed from `6` to `17`.
- Post-read verification confirmed `17`.
- ESP32 remained online on firmware `0.9.0` after flashing and command
  execution.
