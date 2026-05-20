# Second Discharge Target SOC Baseline Before Vendor-App Change

## Purpose

Baseline snapshot before the user changes the Second discharge time Target SOC
from the vendor app. This is intended to recover the missing slot 2/3/4 target
SOC mapping.

## Capture

- Local time: `2026-05-18T21:16:32+07:00`
- Device ID: `P240819130`
- Firmware: `lumentree-ble-bridge/0.12.0`
- Read command: `READ_RANGE 95 95`
- BLE label: `manual_registers_95_189`
- Safety: read-only Modbus FC03

## Current Decoded Settings

| Setting | Value |
|---------|------:|
| First target SOC | 6 |
| Slot 1 enabled | true |
| Slot 2 enabled | true |
| Slot 3 enabled | true |
| Slot 4 enabled | true |
| Slot 1 time | 20:01 -> 08:01 |
| Slot 2 time | 16:01 -> 20:01 |
| Slot 3 time | 08:01 -> 14:01 |
| Slot 4 time | 14:01 -> 16:01 |
| Slot 1 power | 3200 W |
| Slot 2 power | 3200 W |
| Slot 3 power | 3200 W |
| Slot 4 power | 3200 W |

## SOC Candidate Cluster

| Register | Baseline Value |
|----------|---------------:|
| 143 | 25 |
| 144 | 6 |
| 145 | 40 |
| 146 | 50 |
| 147 | 0 |
| 148 | 90 |
| 149 | 90 |

Register `144` is already confirmed as First discharge time Target SOC. The
next diff should identify which register changes when the user edits Second
discharge time Target SOC.

## Raw Payload

```text
0103BE01660000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D105780641000000010000000104B00321064007D1000000190006002800320000005A005A00010001000114B41388157C1388003C000000001A0512151026000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004CD757
```
