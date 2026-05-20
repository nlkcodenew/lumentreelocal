# 2026-05-18 Target SOC Register Research

This note tracks the read-only evidence for researching the Lumentree app
setting:

```text
Work Control Settings ->
The battery discharges to the loads ->
First discharge time ->
Target SOC
```

## Before Snapshot

Captured before the user changes the vendor app value from `6%` to `10%`.

Assumed app state from user observation:

- First discharge time toggle: off
- Time window: `20:00` to `08:00`
- Target SOC: `6%`
- Discharge power: `3500W`

Evidence file:

```text
docs/evidence/2026-05-18-target-soc-before-6pct.json
```

Snapshot details:

- Captured at: `2026-05-18T03:33:46.718353+00:00`
- Latest included telemetry id: `3644`
- Latest included observed_at: `2026-05-18T03:33:30.322671+00:00`
- Sample count: `10`
- Register count per sample: `95`
- Source: local Postgres table `lumentree_telemetry`
- Transport safety: existing ESP32 read-only Modbus function `03`

## Next Step

After the user changes target SOC from `6%` to `10%` in the vendor app, capture
another 10-sample snapshot and compare register-level diffs against this file.

No inverter write has been performed by ESP32 or HASS for this research step.

## After Snapshot

Captured after the user changed the vendor app target SOC from `6%` to `10%`.

Evidence files:

```text
docs/evidence/2026-05-18-target-soc-after-10pct.json
docs/evidence/2026-05-18-target-soc-6-to-10-diff.md
```

Snapshot details:

- Captured at: `2026-05-18T03:39:45.838+00:00`
- Included telemetry ids: `3653` to `3662`
- Included observed_at: `2026-05-18T03:36:30.921153+00:00` to
  `2026-05-18T03:39:30.604428+00:00`
- Sample count: `10`
- Register count per sample: `95`
- Source: local Postgres table `lumentree_telemetry`
- Transport safety: existing ESP32 read-only Modbus function `03`

Diff result:

- `22` registers changed or were volatile between the before and after windows.
- No register showed a clean stable `6 -> 10` transition in the current
  main-register window `0-94`.
- Register `59` is not reliable evidence for the target SOC setting because
  the before-window already included value `10` plus large outliers.
- The changed registers mostly match realtime telemetry changes, not persistent
  configuration: battery voltage/current/power, PV voltage/power, grid/load
  values, frequency, SOC, and temperature.

Current interpretation:

- The target SOC setting is probably not exposed in the current realtime
  register window `0-94`, or the window is too noisy to identify it from this
  app change alone.
- This evidence is not sufficient to approve any real ESP32/HASS write command.
- Recommended next research step is a read-only settings-register scan for
  candidate ranges, then repeat a vendor-app change and diff those ranges.

## Manual Range Read Plan

Firmware `0.7.0` adds a serial-only research command:

```text
READ_RANGE start_register register_count
```

Safety boundary:

- It uses the existing `runModbusRead()` path.
- It only builds Modbus function `03` read-register requests.
- `register_count` is limited to `1..95`.
- It does not add any Modbus write function.
- It does not add any BLE write path for inverter settings.

Initial target-SOC research will capture candidate read windows while the app
target SOC is `10%`, then repeat after the user changes the app target SOC to
`13%`.

## Manual Range Baseline At 10%

Firmware `0.7.0` was built and flashed successfully. Public health reported:

- firmware: `lumentree-ble-bridge/0.7.0`
- gateway: `esp32-lumentree`
- pairing status: `paired`
- production upload: resumed after flashing

Captured manual read-only ranges before the user changes target SOC from `10%`
to `13%`:

```text
READ_RANGE 95 95
READ_RANGE 190 95
READ_RANGE 285 95
READ_RANGE 380 95
READ_RANGE 475 95
READ_RANGE 570 95
```

Evidence files:

```text
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-10pct-ranges.jsonl
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-10pct-ranges.decoded.json
```

Initial observations:

- All manual reads used Modbus function `03` through the existing BLE read path.
- No inverter write was performed.
- Six manual range responses were captured.
- Range `95-189` is the strongest current candidate because it contains:
  - register `112` = `10`
  - register `144` = `10`
  - several values matching or resembling configured power limits, including
    `3500` at registers `180`, `182`, and `183`
- Range `190-284` also contains register `223` = `13`, but this was present
  before the planned app change to `13%`, so it is not evidence yet.

Next step:

- User changes app target SOC from `10%` to `13%`.
- Repeat the same read-only ranges.
- Diff absolute register values, with special attention to registers `112`,
  `144`, and nearby power/time registers in `95-189`.

## Manual Range Diff 10% To 13%

After the user changed the vendor app target SOC from `10%` to `13%`, the same
manual read-only ranges were captured again.

Evidence files:

```text
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-13pct-ranges.jsonl
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-13pct-ranges.decoded.json
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-10-to-13-range-diff.md
```

Result:

- Six manual range responses were captured at `13%`.
- Changed registers across captured manual ranges: `9`.
- Clean target-SOC candidate:
  - register `144`: `10` -> `13` (`000a` -> `000d`)
- Register `112` stayed at `10`, so it is not the changed first discharge
  target SOC in this test.
- Range `95-189` remains the strongest settings candidate range.

Current interpretation:

- Register `144` is a strong candidate for the target SOC value that the app
  changed in Work Control Settings -> "the battery discharges to the loads" ->
  First discharge time.
- This is still read-only evidence only. It is not yet approval for a real
  write.
- Before any real write implementation, confirm with one more vendor-app change
  in the reverse direction or to a third value, then document:
  - exact register: likely `144`
  - unit/scale: likely direct percent integer
  - pre-read: register `144`
  - post-read verification: register `144`
  - relationship to nearby schedule/power registers
  - rollback value

## Manual Range Confirmation 13% To 16%

Before changing the vendor app from `13%` to `16%`, a confirmation baseline was
captured for range `95-189`:

```text
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-13pct-confirm-before-16pct.jsonl
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-13pct-confirm-before-16pct.decoded.json
```

Baseline result:

- register `144`: `13` (`000d`)

After the user changed the vendor app target SOC from `13%` to `16%`, range
`95-189` was captured again:

```text
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-16pct-confirm.jsonl
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-16pct-confirm.decoded.json
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-13-to-16-range-diff.md
```

Diff result:

- Changed registers in range `95-189`: `3`
- register `144`: `13` -> `16` (`000d` -> `0010`)
- Other changed registers:
  - register `95`: `331` -> `332`
  - register `162`: `14089` -> `14850`

Updated interpretation:

- Register `144` has now followed two vendor-app changes:
  - `10%` -> `13%`
  - `13%` -> `16%`
- This is strong read-only evidence that register `144` is the First discharge
  time target SOC value.
- This still does not approve a real write. The next engineering step is a
  written command spec for a single semantic command, including authorization,
  pre-read, exact Modbus write frame, post-read verification, and rollback.

## Guarded Write Test 16% To 6%

The user requested a real write test to change the target SOC from `16%` back
to `6%`.

Implementation/spec commit:

```text
450d5fc feat: add guarded target soc write test
```

Firmware:

- `lumentree-ble-bridge/0.8.0`
- Serial command: `WRITE_TARGET_SOC_144_TO_6 CONFIRM`
- Scope: one-off whitelisted write test only
- Precondition: register `144` must be `16`
- Write attempt: Modbus function `06`, register `144`, value `6`
- Postcondition: register `144` must verify as `6`

Evidence:

```text
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-write-16-to-6.jsonl
docs/evidence/target-soc-range-scan/2026-05-18-target-soc-write-16-to-6.summary.json
```

Result:

```json
{
  "register": 144,
  "before_value": 16,
  "requested_value": 6,
  "after_value": 16,
  "write_ack": false,
  "verified": false
}
```

Conclusion:

- The test was safe in the sense that pre-read and post-read worked and the
  value did not change.
- The write attempt did not succeed.
- The inverter did not echo the function `06` request.
- Register `144` remained `16`.
- Do not retry function `06` blindly.
- Further write research must identify the vendor app's actual write mechanism,
  function code, framing, authorization, or commit behavior before another real
  write attempt.

## Vendor MQTT Write Mechanism Capture

After the failed function `06` ESP32 test, a read-only vendor MQTT capture was
run while the user changed the vendor app target SOC from `16%` to `6%`.

Evidence:

```text
docs/evidence/vendor-mqtt-captures/2026-05-18-target-soc-16-to-6-live-capture.log
docs/plans/2026-05-18-real-write-mechanism-research.md
```

Captured write frame:

```text
0110009000010200063b02
```

Decoded result:

- function code: `0x10` / Modbus function `16`, write multiple registers
- start register: `144`
- register count: `1`
- value: `6`
- CRC valid

Captured verification:

```text
0103009000018427
01030200063846
```

The app read register `144` and the inverter response contained value `0006`.

Conclusion:

- The target SOC write mechanism is function `16`, not function `06`.
- The next implementation should be a semantic guarded command only, not a
  generic register writer.
