# Mains Charge The Battery Entity Mapping

## Purpose

Dedicated evidence file for the Lumentree app topic:

```text
Work Control Settings -> Mains charge the battery
```

This file is the working area for identifying Home Assistant entities that
belong to mains/grid charging. It is intentionally read-only at this stage:
no firmware/API/HASS write command is approved until each entity has vendor-app
diff evidence and a guarded semantic write test.

## Current Boundary

- Firmware: `lumentree-ble-bridge/0.13.0`
- Production API: `https://lumentree.jonah.io.vn`
- Settings read source: Modbus FC03 `READ_RANGE 95 95`
- Current status: research preparation only
- Write status: not implemented and not approved

## Vendor App Shape

Observed in the Lumentree vendor app before the first mains-charge snapshot:

```text
Work Control Settings -> Mains charge the battery
```

| App Field | Current App Value |
|-----------|-------------------|
| First charge time enable | toggle on/off, current state to be confirmed by snapshot |
| First charge time start | `08:05` |
| First charge time end | `12:05` |
| First charge target SOC | `29%` |
| Second charge time enable | toggle on/off, current state to be confirmed by snapshot |
| Second charge time start | `14:05` |
| Second charge time end | `16:05` |
| Second charge target SOC | `69%` |

## Scheduling Constraint

Mains-charge time windows cannot be enabled if they overlap any enabled time
window under:

```text
Work Control Settings -> The battery discharges to the loads
```

The operational reason is that the inverter cannot use mains power to charge the
battery and discharge the same storage battery to the loads at the same time.

This matters for testing and Home Assistant UX:

- A mains-charge toggle may refuse to turn on even when the register mapping is
  correct if its time window overlaps an enabled discharge window.
- Toggle state must be interpreted together with the configured schedule
  windows, not as an isolated boolean.
- Write testing should avoid overlapping charge/discharge windows unless the
  specific goal is to verify the inverter's conflict rejection behavior.

## Baseline Snapshot

Latest API settings snapshot before starting mains-charge research:

- API observed at: `2026-05-18T14:34:49.004648Z`
- Firmware: `lumentree-ble-bridge/0.13.0`
- Raw settings label: `settings_registers_95_189`
- Raw payload CRC: `40dc`
- Safety: `function_03_read_only_no_setting_write`

Raw payload:

```text
0103BE014E0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D105780641000000010000000104B00321064007D1000000190006002800320000005A005A00010001000114B41388157C1388003C000000001A0512152229000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004C40DC
```

## Read-Only Mapping Snapshot

After the vendor app released the Bluetooth connection, ESP32 serial
`READ_RANGE 95 95` succeeded with Modbus FC03 read-only.

- Capture label: `manual_registers_95_189`
- CRC: OK
- Safety: `reassembled_function_03_read_response_only`
- No inverter write was performed.

Raw payload:

```text
0103BE014D0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D1057D0641000000010000000104B50321064507D10000001D0006004500320000005A005A00010001000114B41388157C1388003C000000001A0512152E24000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004C2FE1
```

The current app values map cleanly to these registers:

| App Entity | Register | Raw Value | Decoded Value | Status |
|------------|---------:|----------:|---------------|--------|
| First charge time enable | 134 | 0 | off | mapped from current off state and schedule conflict |
| First charge time start | 130 | 805 | `08:05` | exact match |
| First charge time end | 138 | 1205 | `12:05` | exact match |
| First charge target SOC | 143 | 29 | `29%` | exact match |
| Second charge time enable | 136 | 0 | off | mapped from current off state and schedule conflict |
| Second charge time start | 132 | 1405 | `14:05` | exact match |
| Second charge time end | 140 | 1605 | `16:05` | exact match |
| Second charge target SOC | 145 | 69 | `69%` | exact match |

This establishes the read mapping for the current `Mains charge the battery`
entities. Toggle write behavior still needs a separate conflict-aware test
because the current charge windows overlap enabled discharge windows.

## Vendor-App Toggle Conflict Test

The user disabled overlapping discharge schedules in the vendor app, then
enabled First charge time:

- `The battery discharges to the loads` slot 1 remained enabled.
- Discharge slot 1 window is `20:01` to `08:01`, which does not overlap First
  charge time `08:05` to `12:05`.
- Discharge slot 2, slot 3, and slot 4 were toggled off to avoid conflicts.
- `Mains charge the battery` First charge time was then toggled on.

ESP32 serial `READ_RANGE 95 95` then succeeded with Modbus FC03 read-only.

- Capture label: `manual_registers_95_189`
- CRC: OK
- Safety: `reassembled_function_03_read_response_only`
- No inverter write was performed by ESP32/API/HASS.

Raw payload:

```text
0103BE014F0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D1057D0641000100010000000004B50321064507D10000001D0006004500320000005A005A00010000000014B41388157C1388003C000000001A0512153402000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004CFF24
```

Confirmed toggle states after the vendor-app operation:

| Entity | Register | Value | Interpretation |
|--------|---------:|------:|----------------|
| First charge time enable | 134 | 1 | on |
| Second charge time enable | 136 | 0 | off |
| Discharge slot 1 enable | 135 | 1 | on |
| Discharge slot 2 enable | 137 | 0 | off |
| Discharge slot 3 enable | 151 | 0 | off |
| Discharge slot 4 enable | 152 | 0 | off |

Confirmed mains-charge values remained stable:

| Entity | Register | Value | Decoded |
|--------|---------:|------:|---------|
| First charge time start | 130 | 805 | `08:05` |
| First charge time end | 138 | 1205 | `12:05` |
| First charge target SOC | 143 | 29 | `29%` |
| Second charge time start | 132 | 1405 | `14:05` |
| Second charge time end | 140 | 1605 | `16:05` |
| Second charge target SOC | 145 | 69 | `69%` |

This confirms that register `134` is the First charge time enable flag and that
the inverter accepts it when conflicting discharge windows are disabled.

## Vendor-App Second Toggle Test

The user then toggled `Mains charge the battery` First charge time off and
Second charge time on in the vendor app.

ESP32 serial `READ_RANGE 95 95` succeeded with Modbus FC03 read-only.

- Capture label: `manual_registers_95_189`
- CRC: OK
- Safety: `reassembled_function_03_read_response_only`
- No inverter write was performed by ESP32/API/HASS.

Raw payload:

```text
0103BE014E0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D1057D0641000000010001000004B50321064507D10000001D0006004500320000005A005A00010000000014B41388157C1388003C000000001A0512153814000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004C1080
```

Confirmed toggle states after the vendor-app operation:

| Entity | Register | Value | Interpretation |
|--------|---------:|------:|----------------|
| First charge time enable | 134 | 0 | off |
| Second charge time enable | 136 | 1 | on |
| Discharge slot 1 enable | 135 | 1 | on |
| Discharge slot 2 enable | 137 | 0 | off |
| Discharge slot 3 enable | 151 | 0 | off |
| Discharge slot 4 enable | 152 | 0 | off |

Confirmed mains-charge values remained stable:

| Entity | Register | Value | Decoded |
|--------|---------:|------:|---------|
| First charge time start | 130 | 805 | `08:05` |
| First charge time end | 138 | 1205 | `12:05` |
| First charge target SOC | 143 | 29 | `29%` |
| Second charge time start | 132 | 1405 | `14:05` |
| Second charge time end | 140 | 1605 | `16:05` |
| Second charge target SOC | 145 | 69 | `69%` |

This confirms that register `136` is the Second charge time enable flag.

## Vendor-App First Charge Time And Target SOC Test

The user then toggled Second charge time off and changed First charge time in
the vendor app:

- First charge time: `08:05 -> 12:05` changed to `08:00 -> 12:00`.
- First charge target SOC: `29%` changed to `19%`.
- Second charge time was toggled off.

ESP32 serial `READ_RANGE 95 95` succeeded with Modbus FC03 read-only.

- Capture label: `manual_registers_95_189`
- CRC: OK
- Safety: `reassembled_function_03_read_response_only`
- No inverter write was performed by ESP32/API/HASS.

Raw payload:

```text
0103BE014E0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032007D1057D0641000000010000000004B00321064507D1000000130006004500320000005A005A00010000000014B41388157C1388003C000000001A0512153B2B000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004CBA6D
```

Confirmed changed values:

| Entity | Register | Previous | Current | Interpretation |
|--------|---------:|---------:|--------:|----------------|
| First charge time start | 130 | 805 | 800 | `08:00` |
| First charge time end | 138 | 1205 | 1200 | `12:00` |
| First charge target SOC | 143 | 29 | 19 | `19%` |
| Second charge time enable | 136 | 1 | 0 | off |

Unchanged context values:

| Entity | Register | Value | Interpretation |
|--------|---------:|------:|----------------|
| First charge time enable | 134 | 0 | off |
| Second charge time start | 132 | 1405 | `14:05` |
| Second charge time end | 140 | 1605 | `16:05` |
| Second charge target SOC | 145 | 69 | `69%` |
| Discharge slot 1 enable | 135 | 1 | on |
| Discharge slot 2 enable | 137 | 0 | off |
| Discharge slot 3 enable | 151 | 0 | off |
| Discharge slot 4 enable | 152 | 0 | off |

This confirms the first charge start, end, and target SOC register mapping with
a real vendor-app value change.

## Production Semantic Write Test

Firmware `lumentree-ble-bridge/0.14.0` added guarded semantic write commands for
the confirmed `Mains charge the battery` registers. The production API accepts
only these semantic commands; there is still no generic register writer.

Requested first charge settings:

- First charge time: `08:01 -> 12:01`
- First charge target SOC: `40%`
- First charge time enable: `on`

Before enabling First charge time, active discharge schedules were checked:

| Discharge Slot | Enabled | Window | Overlap With `08:01 -> 12:01` |
|----------------|---------|--------|--------------------------------|
| 1 | true | `20:01 -> 08:01` | false |
| 2 | false | `16:01 -> 20:01` | not active |
| 3 | false | `08:01 -> 14:01` | not active |
| 4 | false | `14:01 -> 16:01` | not active |

The slot 1 discharge window ends exactly at `08:01`; the overlap check treats
schedule windows as half-open intervals `[start, end)`, so `08:01 -> 12:01`
does not overlap `20:01 -> 08:01`.

Command results:

| Command ID | Command | Register | Before | Requested | After | Verified | Completed At |
|-----------:|---------|---------:|-------:|----------:|------:|----------|--------------|
| 15 | `set_mains_charge_time_start` | 130 | 800 | 801 | 801 | true | `2026-05-18T15:06:55.621814Z` |
| 16 | `set_mains_charge_time_end` | 138 | 1200 | 1201 | 1201 | true | `2026-05-18T15:07:24.340269Z` |
| 17 | `set_mains_charge_target_soc` | 143 | 19 | 40 | 40 | true | `2026-05-18T15:07:53.226648Z` |
| 18 | `set_mains_charge_time_enable` | 134 | 0 | 1 | 1 | true | `2026-05-18T15:08:24.191900Z` |

Final API settings snapshot after firmware upload and decoder refresh:

- API observed at: `2026-05-18T15:09:42.524006Z`
- Firmware: `lumentree-ble-bridge/0.14.0`
- Raw label: `settings_registers_95_189`

| Register | Value | Interpretation |
|---------:|------:|----------------|
| 130 | 801 | First charge start `08:01` |
| 138 | 1201 | First charge end `12:01` |
| 143 | 40 | First charge target SOC `40%` |
| 134 | 1 | First charge enabled |
| 132 | 1405 | Second charge start `14:05` |
| 140 | 1605 | Second charge end `16:05` |
| 145 | 69 | Second charge target SOC `69%` |
| 136 | 0 | Second charge disabled |
| 135 | 1 | Discharge slot 1 enabled |
| 137 | 0 | Discharge slot 2 disabled |
| 151 | 0 | Discharge slot 3 disabled |
| 152 | 0 | Discharge slot 4 disabled |

This confirms the ESP32/API write path for First charge time start, First charge
time end, First charge target SOC, and First charge enable.

## Production Second Charge Setup With Overlap Rejection

On `2026-05-19` local time, the user requested Second charge time setup:

- Second charge time: `14:00 -> 16:00`
- Second charge target SOC: `60%`
- Enable Second charge time only after checking overlap against
  `The battery discharges to the loads`.

The guarded ESP32/API writes updated the second charge parameters:

| Command ID | Command | Register | Before | Requested | After | Verified | Completed At |
|-----------:|---------|---------:|-------:|----------:|------:|----------|--------------|
| 31 | `set_mains_charge_time_start` | 132 | 1405 | 1400 | 1400 | true | `2026-05-18T23:36:20.508415Z` |
| 32 | `set_mains_charge_time_end` | 140 | 1605 | 1600 | 1600 | true | `2026-05-18T23:36:50.724215Z` |
| 33 | `set_mains_charge_target_soc` | 145 | 69 | 60 | 60 | true | `2026-05-18T23:37:20.456172Z` |

Before enabling Second charge time, active discharge schedules were checked:

| Discharge Slot | Enabled | Window | Overlap With `14:00 -> 16:00` |
|----------------|---------|--------|--------------------------------|
| 1 | true | `20:01 -> 08:01` | false |
| 2 | true | `16:01 -> 20:01` | false |
| 3 | true | `08:01 -> 14:01` | true |
| 4 | true | `14:01 -> 16:01` | true |

Because active discharge slots 3 and 4 overlap Second charge time, the enable
command for register `136` was not sent. This is intentional and required:
mains charge must not be enabled while any active discharge window overlaps.

Manual read-only verification after the writes:

- Capture label: `manual_registers_95_189`
- CRC: OK
- Safety: `reassembled_function_03_read_response_only`
- No enable write was sent after overlap was detected.

Raw payload:

```text
0103BE01440000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032107D105780641000000010000000104B10321064007D1000000280006003C00320000005A005A00010001000114B41388157C1388003C000000001A0513062526000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004C3AB4
```

Decoded final values from the manual read:

| Register | Value | Interpretation |
|---------:|------:|----------------|
| 132 | 1400 | Second charge start `14:00` |
| 140 | 1600 | Second charge end `16:00` |
| 145 | 60 | Second charge target SOC `60%` |
| 136 | 0 | Second charge remains disabled because overlap exists |
| 135 | 1 | Discharge slot 1 enabled |
| 137 | 1 | Discharge slot 2 enabled |
| 151 | 1 | Discharge slot 3 enabled |
| 152 | 1 | Discharge slot 4 enabled |

## Home Assistant Write Requirement

Home Assistant must enforce the same overlap check before it allows either
`Mains charge the battery` enable switch to turn on.

Required behavior:

- Before turning on First charge time or Second charge time, read the current
  mains-charge time window for that slot.
- Compare it with every enabled `The battery discharges to the loads` window.
- Treat schedule windows as half-open intervals `[start, end)`, including
  overnight windows split across midnight.
- If any enabled discharge window overlaps, block the Home Assistant switch
  action and surface an explicit error instead of sending the write command.
- Do not rely only on the inverter to reject conflicts; the integration must
  prevent confusing ON/OFF state in the UI.

## Known Non-Mains-Charge Registers In This Range

These registers are already assigned to the completed topic
`The battery discharges to the loads` and should not be reused for mains charge:

| Register | Existing Entity |
|---------:|-----------------|
| 131 | `discharge_slot_1_start_time` |
| 133 | `discharge_slot_2_start_time` |
| 135 | `discharge_slot_1_enabled` |
| 137 | `discharge_slot_2_enabled` |
| 139 | `discharge_slot_1_end_time` |
| 141 | `discharge_slot_2_end_time` |
| 144 | `first_discharge_target_soc` |
| 146 | `discharge_slot_2_target_soc` |
| 151 | `discharge_slot_3_enabled` |
| 152 | `discharge_slot_4_enabled` |
| 173 | `discharge_slot_3_start_time` |
| 174 | `discharge_slot_3_end_time` |
| 175 | `discharge_slot_4_start_time` |
| 176 | `discharge_slot_4_end_time` |
| 177 | `discharge_slot_3_target_soc` |
| 178 | `discharge_slot_4_target_soc` |
| 180 | `discharge_slot_1_power` |
| 182 | `discharge_slot_2_power` |
| 183 | `discharge_slot_3_power` |
| 184 | `discharge_slot_4_power` |

## Remaining Mains-Charge Candidate Registers

These are only remaining candidates after the time/toggle/target-SOC read
mapping above. They need a vendor-app before/after diff under
`Work Control Settings -> Mains charge the battery` before use.

| Register | Current Value | Candidate Meaning | Confidence | Needed Evidence |
|---------:|--------------:|-------------------|------------|-----------------|
| 100 | 1 | possible enable flag | low | toggle mains charge enable in vendor app |
| 101 | 5600 | possible charge voltage limit | low | change matching mains charge voltage/threshold |
| 102 | 5700 | possible charge voltage cutoff | low | change matching mains charge voltage/threshold |
| 106 | 58 | possible charge current limit | low | change mains charge current |
| 107 | 70 | possible mains/grid charge current | low | change mains charge current |
| 112 | 10 | possible SOC threshold | low | change mains charge target/stop SOC |
| 117 | 5000 | possible battery charge voltage | low | change charge voltage setting |
| 118 | 5000 | possible float charge voltage | low | change float voltage setting |
| 148 | 90 | possible max charge SOC | low | change charge cutoff/max SOC |
| 149 | 90 | possible backup/reserve SOC | low | change related SOC setting |
| 179 | 2000 | possible max charge power | low | change mains charge power |
| 181 | 2000 | possible second charge power slot | low | change mains charge power/time slot |

## Confirmed Read Mapping

| Register | Entity | Unit/Encoding |
|---------:|--------|---------------|
| 130 | First charge time start | HHMM; vendor-app and ESP32/API write verified |
| 132 | Second charge time start | HHMM; ESP32/API write verified |
| 134 | First charge time enable | boolean, `0=off`, `1=on`; vendor-app and ESP32/API write verified |
| 136 | Second charge time enable | boolean, `0=off`, `1=on`; vendor-app toggle verified; enable blocked when overlap exists |
| 138 | First charge time end | HHMM; vendor-app and ESP32/API write verified |
| 140 | Second charge time end | HHMM; ESP32/API write verified |
| 143 | First charge target SOC | percent; vendor-app and ESP32/API write verified |
| 145 | Second charge target SOC | percent; ESP32/API write verified |

## Next Research Step

1. Change exactly one visible field under
   `Work Control Settings -> Mains charge the battery` in the vendor app.
2. Capture the after snapshot for the same range.
3. Diff only stable config registers, ignoring known discharge-topic registers
   and volatile counters such as register `95` and `162`.
4. Promote write support only after one reverse or third-value confirmation.

No mains-charge entity should be exposed as writable until this file contains
the confirmed register, valid range, unit/scale, pre-read value, post-read
verification plan, and rollback value.
