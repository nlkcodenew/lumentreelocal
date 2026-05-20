# 2026-05-20 syssi/esphome-lumentree Cross-Check Spec

Read-only review spec. Compares the current Lumentree Local decoder against the
public ESPHome component `syssi/esphome-lumentree` (cloned to `/tmp/esphome-lumentree`,
commit on `main` as of 2026-05-19).

## Goal

Decide which findings from `syssi/esphome-lumentree` are worth adopting in
`host/local-server/lumentree_decode.py` and the Home Assistant integration.

## Non-Goals

- No write-side changes. All discharge/mains-charge slot register addresses
  (130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 143, 144, 145,
  146, 151, 152, 173, 174, 175, 176, 177, 178, 180, 182, 183, 184) stay as is.
  These were validated by repeated round-trip testing and are out of scope.
- No firmware register-write changes. Function `0x10` write path stays as is.
- No change to BLE service/characteristic UUIDs (`0xFFE0` / `0xFFE1`).
- No Postgres schema change.

## Decoder Surface Under Review

Only `host/local-server/lumentree_decode.py` and the Home Assistant sensor
list in `custom_components/lumentreelocal/sensor.py`.

## Reference Source

- Repo: `https://github.com/syssi/esphome-lumentree`
- Local clone: `/tmp/esphome-lumentree`
- Decoder: `components/lumentree_ble/lumentree_ble.cpp`
- Protocol doc: `docs/protocol-design.md`

## Confirmed Common Ground (no action)

These match between the two implementations and need no change:

| Item | Both agree |
|---|---|
| BLE service UUID | `0xFFE0` |
| BLE notify/write characteristic | `0xFFE1` |
| Slave address | `0x01` |
| CRC16 polynomial / init | `0xA001` / `0xFFFF` |
| Read holding registers function | `0x03` |
| Battery voltage register | 11, factor `0.01` |
| Battery current register | 12, factor `0.01` |
| Battery SOC register | 50 |
| AC out voltage | 13, factor `0.1` |
| AC in voltage (grid) | 15, factor `0.1` |
| AC out frequency | 16, factor `0.01` |
| AC in frequency | 17, factor `0.01` |
| AC out power | 18 |
| AC out VA | 58 |
| Load power | 67 |
| PV1 voltage / power | 20 / 22 |
| PV2 voltage / power | 72 / 74 |
| Battery power | 61, signed |
| Device temperature | 24, formula `(value - 1000) / 10` |
| Operation mode register | 68 |
| Serial number registers | 3..7 (5 registers, 10 ASCII bytes) |

## Findings That May Be Worth Adopting

Each finding is read-only and can be evaluated on existing captures
(`docs/evidence/`) without any write to the inverter.

### Finding 1: Function 0x04 (today statistics)

syssi reads input registers 0..7 with function `0x04` and decodes:

| Register | Field | Scaling |
|---|---|---|
| 0 | today_pv_production | `÷10` kWh |
| 1 | today_essential_load | `÷10` kWh |
| 2 | today_grid_consumption | `÷10` kWh |
| 3 | today_total_consumption | `÷10` kWh |
| 4 | today_battery_charging | `÷10` kWh |
| 5 | today_battery_discharge | `÷10` kWh |

Source: `lumentree_ble.cpp:735-775`.

Lumentree Local computes daily energy from Postgres samples instead. Adopting
this would give one extra signal that can be cross-checked against the
Postgres-derived numbers.

Risk: function `0x04` is a different read path. ble-collector currently only
issues function `0x03` reads. Sending `0x04` for the first time on a real
inverter is new traffic and needs a controlled test.

Decision needed: do we want a function `0x04` capture pass to compare numbers
against the Postgres aggregator before deciding to add this metric?

### Finding 2: Naming conflicts at register 53 and register 70

This is the most important finding because it points at possible mislabeling
that does not change numbers but affects metric semantics.

Register 53:

| Source | Name | Cast |
|---|---|---|
| Lumentree Local (`REG_ADDR["AC_IN_POWER"]`) | `ac_input_power` | signed |
| syssi (`case 53`) | `grid_power` (signed) | signed |

Register 59:

| Source | Name | Cast |
|---|---|---|
| Lumentree Local (`REG_ADDR["GRID_POWER"]`) | `grid_power` | signed |
| syssi (`case 59`) | `grid_ct_power` (signed) | signed |

Register 70:

| Source | Name | Meaning |
|---|---|---|
| Lumentree Local (`REG_ADDR["MASTER_SLAVE_STATUS"]`) | `master_slave_status` | unsigned numeric |
| syssi (`case 70`) | `grid_connection_status`; binary `grid_connected` when `value >= 7` | unsigned |

Register 54:

| Source | Name |
|---|---|
| Lumentree Local | not decoded |
| syssi (`case 54`) | `grid_export` (signed watts) |

What needs to be checked, on captured frames only:

1. For at least three captures with known grid behavior (importing, exporting,
   off-grid) compare values at register 53 vs 59 against the vendor app's
   "Grid power" reading.
2. Decide whether `ac_input_power` and `grid_power` are the same physical
   signal or two distinct signals (grid CT vs AC input). If two distinct
   signals, both should remain.
3. Check whether register 70 actually represents grid connection status. The
   vendor app reading expected: device transitions to a value `>= 7` when
   tied to grid.

If syssi is right on register 70, the current sensor `master_slave_status`
should be renamed and the binary sensor `grid_connected` (`value >= 7`) added.
Renaming is a HACS-visible change, so the rename plan needs a migration step
similar to `2026-05-19-slot1-target-soc-entity-migration.md`.

### Finding 3: Five undecoded read-only registers

syssi decodes the following that Lumentree Local currently does not:

| Reg | syssi field | Type | Notes |
|---|---|---|---|
| 0 | `device_type` | uint16 | Identifies inverter family |
| 1 | `pv2_support` | binary | True when value `== 0x0102` |
| 8 | `device_power_rating_code` + watts table | uint16 | Code 2=5500 W, 3=4000 W, 5=6000 W, else 3600 W |
| 37 | `battery_status` | uint16 | `0` error, `1` connected, `2` no battery; binary `battery_connected` when `value != 2` |
| 94 | `device_type_image` | uint16 | Vendor app uses for icon mapping |

Lumentree Local already decodes register 37 as `battery_type` with a partial
map (`{2: "No Battery"}`); aligning with syssi's three-state mapping would be
a small refinement.

Decision needed: are any of these worth adding as Home Assistant diagnostic
sensors? They are static or rarely change.

### Finding 4: Device model derivation

syssi maps `(device_type, power_rating_code, light_engine)` to a model name.
`light_engine` is derived from the first character of the serial number being
`'H'`.

Source: `lumentree_ble.cpp:23-28, 449`.

Lumentree Local already exposes the serial number string via
`mqtt_device_sn`. Adding a derived model name (e.g., `SUNT-6.0KW-H`) is
purely a string transformation on existing decoded fields and does not touch
any new register.

Decision needed: is a friendly model string worth a new sensor, or should
the serial-only sensor stay?

### Finding 5: Operation mode mapping

syssi exposes register 68 as text:

| Value | Mode |
|---|---|
| 0 | Battery Mode |
| 1 | Hybrid Mode |
| 2 | Grid-Tie Mode |

Lumentree Local exposes register 68 as `is_ups_mode = (value == 0)` only.

Decision needed: keep the current binary semantic, add a string sensor
alongside, or replace it. Replacement is a HACS-visible rename.

### Finding 6: Frame reassembly approach

syssi uses a 5-second buffer timeout, detects MODBUS frame start by
`[0x01, 0x03]` or `[0x01, 0x04]`, and verifies CRC before decoding any
frame.

Source: `lumentree_ble.cpp:125-190`.

The local decoder already verifies CRC. The firmware-side reassembly belongs
to ble-collector / firmware and is out of scope for this spec.

## Findings Explicitly Out of Scope

Listed only so they do not get reopened later:

- **Battery configuration registers 101-157.** syssi reads these but only
  logs them; no metric is published. Lumentree Local already covers this
  range via `SETTINGS_REGISTERS`. No action.
- **System control registers 160-180.** syssi reads but mostly logs. The
  parts that matter (slot enables, power, target SOC) are already covered
  by Lumentree Local. No action.
- **Lithium BMS registers 185-197.** Not validated on this inverter. Defer
  unless we have a captured frame that includes them.
- **5-minute and yearly chart registers.** Documented by syssi but not
  implemented even there. Storage cost dwarfs value. No action.
- **Battery cell registers 250..299.** Lumentree Local already decodes
  these. syssi has no equivalent. No change.

## Decision Inputs Needed

For each finding the user wants to evaluate, the following inputs help close
the question without sending any new traffic to the inverter:

1. One captured frame for register range 0-94 with the grid in `Importing`
   state (`docs/evidence/...`).
2. One captured frame with the grid in `Exporting` state.
3. One captured frame with the grid disconnected.
4. One captured frame from register range 95-189 (already routine).
5. The vendor app's "Grid Power" reading at the same wall-clock time as
   each of (1)-(3), to compare with values at register 53 and 59.

If the decision involves Finding 1 (function `0x04`), one new capture is
required. This is the only finding that needs a real inverter touch.

## Suggested Order of Evaluation

1. Finding 2 (register 53 / 59 / 70 naming) — read-only, blocks naming
   decisions later. Highest signal-to-cost.
2. Finding 5 (operation mode mapping) — read-only.
3. Finding 4 (model name derivation) — read-only, pure string work.
4. Finding 3 (extra diagnostic registers) — read-only, low value but cheap.
5. Finding 1 (function `0x04`) — requires real inverter capture, do last.

## Stop Conditions

Stop and revisit this spec if any of the following appear:

- Any captured value at register 53 or 59 disagrees with vendor-app
  "Grid Power" by more than 5 W under stable conditions.
- Any value at register 70 stays above 7 while the vendor app reports the
  grid as disconnected.
- A function `0x04` capture returns a frame that does not pass CRC, or
  returns fewer than 16 data bytes.
