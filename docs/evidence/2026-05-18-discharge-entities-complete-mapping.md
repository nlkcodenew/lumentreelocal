# The Battery Discharges To The Loads Entity Mapping

## Purpose

Dedicated evidence file for the Lumentree app topic:

```text
Work Control Settings -> The battery discharges to the loads
```

All writable entities documented in this file belong to this topic. This file
is the mapping source for Home Assistant enable, start/end time, target SOC, and
discharge power entities exposed by `lumentreelocal`.

## Current Implementation

- Firmware: `lumentree-ble-bridge/0.13.0`
- Integration: `lumentreelocal` manifest `0.13.0`
- Production API: `https://lumentree.jonah.io.vn`
- Settings read source: Modbus FC03 `READ_RANGE 95 95`
- Write style: semantic commands only, Modbus FC16, pre-read + write + post-read
  verification. No generic register writer.

## Entity Mapping

| HA Entity Key | Register | Slot | Field | Type | Current Baseline |
|---------------|---------:|-----:|-------|------|-----------------:|
| `first_discharge_target_soc` | 144 | 1 | target SOC | number, % | 6 |
| `discharge_slot_2_target_soc` | 146 | 2 | target SOC | number, % | 50 |
| `discharge_slot_3_target_soc` | 177 | 3 | target SOC | number, % | 20 |
| `discharge_slot_4_target_soc` | 178 | 4 | target SOC | number, % | 85 |
| `discharge_slot_1_enabled` | 135 | 1 | enable | switch | true |
| `discharge_slot_2_enabled` | 137 | 2 | enable | switch | true |
| `discharge_slot_3_enabled` | 151 | 3 | enable | switch | true |
| `discharge_slot_4_enabled` | 152 | 4 | enable | switch | true |
| `discharge_slot_1_start_time` | 131 | 1 | start time | time, HHMM | 2001 |
| `discharge_slot_1_end_time` | 139 | 1 | end time | time, HHMM | 801 |
| `discharge_slot_2_start_time` | 133 | 2 | start time | time, HHMM | 1601 |
| `discharge_slot_2_end_time` | 141 | 2 | end time | time, HHMM | 2001 |
| `discharge_slot_3_start_time` | 173 | 3 | start time | time, HHMM | 801 |
| `discharge_slot_3_end_time` | 174 | 3 | end time | time, HHMM | 1401 |
| `discharge_slot_4_start_time` | 175 | 4 | start time | time, HHMM | 1401 |
| `discharge_slot_4_end_time` | 176 | 4 | end time | time, HHMM | 1601 |
| `discharge_slot_1_power` | 180 | 1 | discharge power | number, W | 3200 |
| `discharge_slot_2_power` | 182 | 2 | discharge power | number, W | 3200 |
| `discharge_slot_3_power` | 183 | 3 | discharge power | number, W | 3200 |
| `discharge_slot_4_power` | 184 | 4 | discharge power | number, W | 3200 |

## Baseline Before Full Target SOC Recheck

- Local time: `2026-05-18T21:24:35+07:00`
- Firmware: `lumentree-ble-bridge/0.13.0`
- API telemetry row: `5502`
- API observed at: `2026-05-18T14:24:30.467162Z`
- Production upload was then disabled with `SET_PRODUCTION 0` so the vendor app
  can change all four target SOC values without ESP32 BLE contention.

### Baseline Settings

| Entity Key | Baseline |
|------------|---------:|
| `first_discharge_target_soc` | 6 |
| `discharge_slot_2_target_soc` | 49 |
| `discharge_slot_3_target_soc` | 19 |
| `discharge_slot_4_target_soc` | 81 |
| `discharge_slot_1_enabled` | true |
| `discharge_slot_2_enabled` | true |
| `discharge_slot_3_enabled` | true |
| `discharge_slot_4_enabled` | true |
| `discharge_slot_1_start_time` | 2001 |
| `discharge_slot_1_end_time` | 801 |
| `discharge_slot_2_start_time` | 1601 |
| `discharge_slot_2_end_time` | 2001 |
| `discharge_slot_3_start_time` | 801 |
| `discharge_slot_3_end_time` | 1401 |
| `discharge_slot_4_start_time` | 1401 |
| `discharge_slot_4_end_time` | 1601 |
| `discharge_slot_1_power` | 3200 |
| `discharge_slot_2_power` | 3200 |
| `discharge_slot_3_power` | 3200 |
| `discharge_slot_4_power` | 3200 |

### Baseline Raw Payload

Serial `READ_RANGE 95 95`, label `manual_registers_95_189`:

```text
0103BE01550000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D105780641000000010000000104B00321064007D1000000190006002800310000005A005A00010001000114B41388157C1388003C000000001A0512151820000000000001000100000000000000000000000003210579057906410013005107D00C8007D00C800C800C80000015680000003D004C3B57
```

## Full Target SOC Recheck

Completed after the user changed all four target SOC values in the vendor app:

| Slot | Baseline | Vendor-App Change | Register | Confirmed |
|------|---------:|------------------:|---------:|----------:|
| 1 | 6 | 11 | 144 | 11 |
| 2 | 49 | 22 | 146 | 22 |
| 3 | 19 | 33 | 177 | 33 |
| 4 | 81 | 44 | 178 | 44 |

### Recheck Raw Payload

Serial `READ_RANGE 95 95`, label `manual_registers_95_189`:

```text
0103BE01540000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D105780641000000010000000104B00321064007D100000019000B002800160000005A005A00010001000114B41388157C1388003C000000001A0512151A33000000000001000100000000000000000000000003210579057906410021002C07D00C8007D00C800C800C80000015680000003D004C29A9
```

### Recheck Diff

| Register | Baseline | Recheck | Classification |
|----------|---------:|--------:|----------------|
| 95 | 341 | 340 | settings counter |
| 144 | 6 | 11 | slot 1 target SOC |
| 146 | 49 | 22 | slot 2 target SOC |
| 162 | 6176 | 6707 | volatile telemetry/counter |
| 177 | 19 | 33 | slot 3 target SOC |
| 178 | 81 | 44 | slot 4 target SOC |

The second full-target-SOC recheck confirms the production mapping:

- slot 1 target SOC -> register `144`
- slot 2 target SOC -> register `146`
- slot 3 target SOC -> register `177`
- slot 4 target SOC -> register `178`

## Production Write Test

Completed with the production API and firmware `lumentree-ble-bridge/0.13.0`
after the user changed all four target SOC values to `11/22/33/44`.

Commands were submitted one at a time through the semantic API command
`set_discharge_target_soc`; each command completed only after firmware pre-read,
Modbus FC16 write, post-read, and `verified=true`.

| Command ID | Slot | Register | Before | Requested | After | Verified | Completed At |
|-----------:|-----:|---------:|-------:|----------:|------:|----------|--------------|
| 11 | 1 | 144 | 11 | 6 | 6 | true | `2026-05-18T14:32:49.548948Z` |
| 12 | 2 | 146 | 22 | 50 | 50 | true | `2026-05-18T14:33:19.791985Z` |
| 13 | 3 | 177 | 33 | 20 | 20 | true | `2026-05-18T14:33:49.455155Z` |
| 14 | 4 | 178 | 44 | 85 | 85 | true | `2026-05-18T14:34:19.875127Z` |

Final API settings snapshot after firmware upload:

- API observed at: `2026-05-18T14:34:49.004648Z`
- Firmware: `lumentree-ble-bridge/0.13.0`
- `first_discharge_target_soc`: `6`
- `discharge_slot_2_target_soc`: `50`
- `discharge_slot_3_target_soc`: `20`
- `discharge_slot_4_target_soc`: `85`

This confirms the Home Assistant/API write path for all four target SOC slots.
