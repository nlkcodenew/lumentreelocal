# Write UX Guardrails

Date: 2026-05-19

## Decision

For newly flashed devices without write authorization, the integration should
not hide write-capable entities and should not disable them in the entity
registry.

Instead:

- Read telemetry remains available normally.
- Write-capable `switch`, `number`, and `time` entities remain visible so users
  can see the inverter control surface and the current loaded values.
- Actual write actions stay blocked until the user claims write access with a
  one-time `Write pairing code` generated from the ESP32 portal at
  `http://192.168.4.1`.

## UX changes recorded

### 1. Write-capable entities grouped as config

All write-capable entities are now marked with Home Assistant
`EntityCategory.CONFIG` so they appear as configuration controls rather than
normal telemetry.

### 2. Write access made more visible

The following entities are now part of the configuration area instead of being
buried in diagnostics:

- `Direct inverter write access`
- `Direct inverter write status`

### 3. Explicit user warning

The integration options flow now warns that write-capable entities directly
change inverter settings.

The options form also makes the write setup path more explicit:

- open ESP32 portal
- go to `Write Access`
- generate one-time `Write pairing code`
- enter that code in Lumentree Local options

### 4. Entity-level warning attributes

Write-capable entities now expose warning attributes that state they write
directly to inverter schedules/settings.

## Files

- `custom_components/lumentreelocal/switch.py`
- `custom_components/lumentreelocal/number.py`
- `custom_components/lumentreelocal/time.py`
- `custom_components/lumentreelocal/binary_sensor.py`
- `custom_components/lumentreelocal/sensor.py`
- `custom_components/lumentreelocal/strings.json`
- `custom_components/lumentreelocal/translations/en.json`
