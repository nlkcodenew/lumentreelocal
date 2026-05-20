# 2026-05-18 Target SOC Write Test Spec

This is the first real inverter write test for Lumentree Local. It is scoped to
one setting and one value.

## Setting

- App path: Work Control Settings -> the battery discharges to the loads ->
  First discharge time -> Target SOC
- Current app value before test: `16%`
- Requested test value: `6%`
- First discharge time toggle: off
- Time window observed in app: `20:00` to `08:00`
- Discharge power observed in app: `3500W`

## Read-Only Evidence

Register `144` followed two vendor-app changes:

- `10%` -> `13%`: register `144` changed `10` -> `13`
- `13%` -> `16%`: register `144` changed `13` -> `16`

Evidence files live under:

```text
docs/evidence/target-soc-range-scan/
```

## Test Command

Firmware command:

```text
WRITE_TARGET_SOC_144_TO_6 CONFIRM
```

This is not a generic register write command.

## Guards

The firmware must:

1. Pre-read range `95-189` with Modbus function `03`.
2. Extract register `144`.
3. Require register `144 == 16`.
4. If the precondition fails, skip the write.
5. Send exactly one Modbus function `06` single-register write:
   - slave id: `1`
   - register: `144`
   - value: `6`
6. Require the function `06` response to echo the request.
7. Post-read range `95-189` with Modbus function `03`.
8. Verify register `144 == 6`.

## Expected Frame

Function `06`, register `144` (`0x0090`), value `6` (`0x0006`):

```text
01 06 00 90 00 06 <crc_lo> <crc_hi>
```

The firmware computes CRC internally and logs the exact `command_hex`.

## Stop Conditions

Stop and do not continue if:

- BLE target MAC is missing.
- BLE address type is unknown.
- FFE0/FFE1 cannot be discovered.
- Pre-read fails.
- Register `144` is not `16` before the write.
- The function `06` response does not echo the request.
- Post-read fails.
- Register `144` does not become `6`.

## Rollback

Expected final value is `6`, which was the original user-observed value before
this research. If the value does not verify, use the vendor app to inspect and
restore the target SOC manually before any further write testing.

## Test Result

Executed once on firmware `lumentree-ble-bridge/0.8.0`.

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

Observed write frame:

```text
01060090000609E5
```

Conclusion:

- The guarded test did not change register `144`.
- The inverter did not echo the Modbus function `06` request.
- Post-read verification kept register `144` at `16`.
- Do not retry function `06` blindly.
- Next research should determine the vendor app's actual write mechanism before
  any further real write attempt.
