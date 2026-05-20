# Slot 1 Target SOC Label Fix

Date: 2026-05-19

## Problem

The slot 1 discharge target SOC entity still appeared as:

- `First discharge target SOC`

That made it sort away from the other discharge slot target SOC entities and
look like a separate concept instead of `Discharge slot 1`.

## Fix

- Kept the underlying settings key `first_discharge_target_soc` for compatibility
- Changed the displayed translation key/name to:
  - `Discharge slot 1 target SOC`

This keeps the existing entity identity stable while making the UI group slot 1
with slots 2-4 more naturally.

## Files

- `custom_components/lumentreelocal/number.py`
- `custom_components/lumentreelocal/strings.json`
- `custom_components/lumentreelocal/translations/en.json`
