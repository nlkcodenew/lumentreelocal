# Discharge Time Register Mapping -- Confirmed

## Evidence Summary

After the baseline capture, all four vendor-app discharge time slots were changed
by +1 minute over Bluetooth:

| Slot | App Setting | Baseline | Changed |
|------|-------------|----------|---------|
| 1 | start | 20:00 | 20:01 |
| 1 | end | 08:00 | 08:01 |
| 2 | start | 16:00 | 16:01 |
| 2 | end | 20:00 | 20:01 |
| 3 | start | 08:00 | 08:01 |
| 3 | end | 14:00 | 14:01 |
| 4 | start | 14:00 | 14:01 |
| 4 | end | 16:00 | 16:01 |

The post-change `READ_RANGE 95 95` response confirmed exactly those HHMM
setting values changed, plus known non-setting counters.

## Register Mapping

| Register | Hex | Slot | Field | Baseline | Changed | App Setting |
|----------|-----|------|-------|----------|---------|-------------|
| **131** | 0x0083 | 1 | start | 2000 | 2001 | First discharge time start |
| **139** | 0x008B | 1 | end | 800 | 801 | First discharge time end |
| **133** | 0x0085 | 2 | start | 1600 | 1601 | Second discharge time start |
| **141** | 0x008D | 2 | end | 2000 | 2001 | Second discharge time end |
| **173** | 0x00AD | 3 | start | 800 | 801 | Third discharge time start |
| **174** | 0x00AE | 3 | end | 1400 | 1401 | Third discharge time end |
| **175** | 0x00AF | 4 | start | 1400 | 1401 | Fourth discharge time start |
| **176** | 0x00B0 | 4 | end | 1600 | 1601 | Fourth discharge time end |

## Characteristics

- **Unit:** HHMM integer (`HH * 100 + MM`)
- **Valid range:** `0000` to `2359`, with minute component `00` to `59`
- **Data type:** uint16
- **Write function:** FC16, matching the guarded target SOC and discharge power
  write path
- **Register 95:** settings-change counter, not a writable setting
- **Register 162:** volatile telemetry/counter, not a writable setting

## Raw Diff

Baseline payload:

```text
0103BE014C0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D005780640000000010000000104B00320064007D0000000190006002800320000005A005A00010001000114B41388157C1388003C000000001A051213072D000000000001000100000000000000000000000003200578057806400014005507D00C8007D00C800C800C80000015680000003D004CBDC1
```

Post-change payload:

```text
0103BE01A70000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D105780641000000010000000104B00321064007D1000000190006002800320000005A005A00010001000114B41388157C1388003C000000001A0512132F0F000000000001000100000000000000000000000003210579057906410014005507D00C8007D00C800C800C80000015680000003D004C926A
```

Decoded register changes:

| Register | Baseline | Changed | Classification |
|----------|----------|---------|----------------|
| 95 | 332 | 423 | settings-change counter |
| 131 | 2000 | 2001 | slot 1 start |
| 133 | 1600 | 1601 | slot 2 start |
| 139 | 800 | 801 | slot 1 end |
| 141 | 2000 | 2001 | slot 2 end |
| 162 | 1837 | 12047 | volatile telemetry/counter |
| 173 | 800 | 801 | slot 3 start |
| 174 | 1400 | 1401 | slot 3 end |
| 175 | 1400 | 1401 | slot 4 start |
| 176 | 1600 | 1601 | slot 4 end |

## Mapping Rationale

The changed HHMM values are duplicated in a few cases (`20:00`, `08:00`,
`14:00`, and `16:00` each appear in multiple app fields), so the slot assignment
uses the surrounding known setting layout:

- Slot 1 and slot 2 values live in the earlier schedule block around the known
  enable registers `135` and `137`.
- Slot 3 and slot 4 values live in the later schedule block before the known
  discharge power registers `180`, `182`, `183`, and `184`.
- Within each block, the start/end pairing matches the vendor-app slot order and
  the +1 minute changes.
