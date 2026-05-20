# Slot 1 Target SOC Entity Migration

Date: 2026-05-19

## Problem

The visible label for slot 1 target SOC had already been changed to
`Discharge slot 1 target SOC`, but the actual entity identity in Home Assistant
still remained:

- `number.lumentree_local_<device_id>_first_discharge_target_soc`

That meant Home Assistant could still sort/group slot 1 differently from the
other discharge slot entities even though the friendly name looked correct.

## Fix

- Changed the integration entity identity to:
  - `discharge_slot_1_target_soc`
- Kept the underlying settings source mapped to:
  - `first_discharge_target_soc`
- Added config-entry setup migration that updates the existing entity registry
  entry from the old unique ID/entity ID to the new slot-1 naming.

## Goal

Slot 1 target SOC should now align with the rest of the discharge slot 1
controls instead of remaining tied to the older `first_discharge_*` identity.

## Files

- `custom_components/lumentreelocal/__init__.py`
- `custom_components/lumentreelocal/number.py`
