# Switch Pending Confirmation

Date: 2026-05-19

## Problem

When a user toggled a writable switch such as `Discharge slot 1 enabled`,
Home Assistant could briefly snap back to the old state before the refreshed
settings snapshot arrived from the ESP32/API round-trip.

That created a confusing UX:

1. user toggles `ON -> OFF`
2. UI jumps back to `ON`
3. user may toggle again because it looks like the first action failed
4. a later poll finally updates the switch to `OFF`

## Fix

Writable switch entities now use a short optimistic pending-confirmation state.

- After a successful command enqueue, the switch keeps the requested state
  locally instead of immediately reverting to the old snapshot value.
- The switch remains in `pending_confirmation` until either:
  - the real settings snapshot matches the requested value, or
  - the timeout expires
- Repeated toggle attempts are blocked while a previous change is still waiting
  for inverter confirmation.

## Scope

This change applies to writable switch entities only:

- discharge slot enable switches
- mains charge slot enable switches

## Files

- `custom_components/lumentreelocal/switch.py`
- `custom_components/lumentreelocal/const.py`
