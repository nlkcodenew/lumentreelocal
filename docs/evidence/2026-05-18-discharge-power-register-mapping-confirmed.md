# Discharge Time Register Mapping — Confirmed

## Evidence Summary

Two independent vendor-app changes confirmed the mapping:

| Register | Baseline | Change 1 | Change 2 | App Setting |
|----------|----------|----------|----------|-------------|
| **180** (0x00B4) | 3500 | 3450 | 2500 | First discharge time power (W) |
| **182** (0x00B6) | 3500 | 3200 | 2600 | Second discharge time power (W) |
| **183** (0x00B7) | 3500 | 3400 | 2700 | Third discharge time power (W) |
| **184** (0x00B8) | 3500 | 3300 | 2800 | Fourth discharge time power (W) |

## Characteristics

- **Unit:** direct Watt, no scaling (register value = display value in W)
- **Data type:** uint16
- **Write function:** FC16 (same as register 144 target SOC)
- **Register 181:** value = 2000, did NOT change — not a discharge power register
- **Register 95:** increments on each settings change (counter, not writable setting)
- **Register 162:** volatile telemetry counter

## Evidence Files

- Baseline (10%, all 3500W): `docs/evidence/target-soc-range-scan/2026-05-18-target-soc-10pct-ranges.decoded.json`
- Change 1 diff: `docs/evidence/2026-05-18-discharge-power-register-diff.json`
- Change 2 raw read: inline in this document (payload below)

## Change 2 Raw Payload

```
READ_RANGE 95 95
payload_hex: 0103BE01530000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D005780640000000000000000004B00320064007D0000000190006002800320000005A005A00010000000014B41388157C1388003C000000001A05120D3B35000000000001000100000000000000000000000003200578057806400014005507D009C407D00A280A8C0AF0000015680000003D004C4ECC
```

Decoded registers 180-184:
- 180: 0x09C4 = 2500
- 181: 0x07D0 = 2000 (unchanged)
- 182: 0x0A28 = 2600
- 183: 0x0A8C = 2700
- 184: 0x0AF0 = 2800

## Enable Flags — Confirmed

Tested by toggling individual slots ON/OFF via vendor app (Bluetooth connection):

| Register | App Setting | Logic |
|----------|-------------|-------|
| **135** (0x0087) | First discharge time enable | 0=OFF, 1=ON |
| **137** (0x0089) | Second discharge time enable | 0=OFF, 1=ON |
| **151** (0x0097) | Third discharge time enable | 0=OFF, 1=ON |
| **152** (0x0098) | Fourth discharge time enable | 0=OFF, 1=ON |

### Evidence

| Test | Action | Register changed |
|------|--------|-----------------|
| 1 | All OFF → First+Second ON | 135: 0→1, 137: 0→1 |
| 2 | → Third ON | 151: 0→1 |
| 3 | → Fourth ON, First OFF | 152: 0→1, 135: 1→0 |

### Characteristics

- **Data type:** uint16 (only uses values 0 and 1)
- **Write function:** FC16 (expected, same as all other settings)
- **Pattern:** registers are NOT consecutive — distributed across settings block
