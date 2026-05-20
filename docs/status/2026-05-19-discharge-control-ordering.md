# Discharge control ordering

Date: 2026-05-19

## What changed

- Reordered the discharge `number` entity descriptions so each `Discharge slot N power` entity is created before `Discharge slot N target SOC`.

## Why

- Home Assistant currently presents these writable controls in platform-driven order.
- For discharge controls that means the practical user-facing order should be:
  1. enable switch
  2. start time
  3. end time
  4. discharge power
  5. target SOC

## Scope

- This change only affects writable discharge number entity ordering.
- No write semantics, register mapping, or inverter command paths changed.
