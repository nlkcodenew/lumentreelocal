# Discharge Target SOC Slot Mapping -- Confirmed

## Evidence Summary

The user changed these vendor-app settings after the baseline snapshot:

| Slot | Field | Baseline | Changed |
|------|-------|----------|---------|
| 2 | Target SOC | 50 | 49 |
| 3 | Target SOC | 20 | 19 |
| 4 | Target SOC | 85 | 81 |

The post-change `READ_RANGE 95 95` response showed exactly those setting values
changed at registers `146`, `177`, and `178`. Register `144` was already
confirmed earlier as the First discharge time Target SOC.

## Register Mapping

| Register | Hex | Slot | Baseline | Changed | App Setting |
|----------|-----|------|----------|---------|-------------|
| **144** | 0x0090 | 1 | 6 | unchanged | First discharge target SOC |
| **146** | 0x0092 | 2 | 50 | 49 | Second discharge target SOC |
| **177** | 0x00B1 | 3 | 20 | 19 | Third discharge target SOC |
| **178** | 0x00B2 | 4 | 85 | 81 | Fourth discharge target SOC |

## Characteristics

- **Unit:** percent SOC
- **Valid range:** `5..100`
- **Data type:** uint16
- **Write function:** FC16, matching the existing register `144` write path
- **Register 95:** settings-change counter, not a writable setting
- **Register 162:** volatile telemetry/counter, not a writable setting

## Raw Diff

Baseline payload:

```text
0103BE01660000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D105780641000000010000000104B00321064007D1000000190006002800320000005A005A00010001000114B41388157C1388003C000000001A0512151026000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004CD757
```

Post-change payload:

```text
0103BE01600000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D105780641000000010000000104B00321064007D1000000190006002800310000005A005A00010001000114B41388157C1388003C000000001A0512151309000000000001000100000000000000000000000003210579057906410013005107D00C8007D00C800C800C80000015680000003D004C01A7
```

Decoded register changes:

| Register | Baseline | Changed | Classification |
|----------|----------|---------|----------------|
| 95 | 358 | 352 | settings counter |
| 146 | 50 | 49 | slot 2 target SOC |
| 162 | 4134 | 4873 | volatile telemetry/counter |
| 177 | 20 | 19 | slot 3 target SOC |
| 178 | 85 | 81 | slot 4 target SOC |
