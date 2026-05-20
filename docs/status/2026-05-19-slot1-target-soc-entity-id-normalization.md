# Slot 1 target SOC entity ID normalization

Date: 2026-05-19

## Problem

- The slot 1 discharge target SOC migration created a temporary entity ID with the wrong prefix:
  - `number.lumentreelocal_<device_id>_discharge_slot_1_target_soc`
- The rest of the integration uses the normalized Home Assistant entity ID pattern:
  - `number.lumentree_local_<device_id>_*`

## Fix

- Updated the migration target in `__init__.py` to normalize slot 1 target SOC to:
  - `number.lumentree_local_<device_id>_discharge_slot_1_target_soc`

## Expected result

- Dashboard cards and entity lists can use a consistent `lumentree_local` prefix for all writable entities.
