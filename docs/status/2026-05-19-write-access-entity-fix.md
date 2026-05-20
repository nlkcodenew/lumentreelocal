# Write Access Entity Fix

Date: 2026-05-19

## Symptom

Home Assistant reported that these entities were no longer provided by
`lumentreelocal`:

- `Direct inverter write access`
- `Direct inverter write status`

At runtime they appeared as missing/stale entities after integration reload.

## Root Cause

The integration changed both entities to `EntityCategory.CONFIG`.

That is valid for write-capable `switch`, `number`, and `time` entities, but
Home Assistant rejects `sensor` and `binary_sensor` entities when they are
added with the `config` entity category.

Observed Home Assistant runtime errors:

- `Entity sensor.lumentree_local_p240819130_write_access_status cannot be added as the entity category is set to config`
- `Entity binary_sensor.lumentree_local_p240819130_write_grant_active cannot be added as the entity category is set to config`

## Fix

- Removed `EntityCategory.CONFIG` from:
  - `sensor.write_access_status`
  - `binary_sensor.write_grant_active`
- Kept the direct-inverter naming and warning copy.
- Kept `EntityCategory.CONFIG` on actual write-capable `switch`, `number`, and
  `time` entities.

## Files

- `custom_components/lumentreelocal/binary_sensor.py`
- `custom_components/lumentreelocal/sensor.py`
