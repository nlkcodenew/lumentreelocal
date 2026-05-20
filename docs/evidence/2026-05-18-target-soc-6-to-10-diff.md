# Target SOC 6% To 10% Register Diff

Read-only comparison of BLE Modbus function 03 main registers 0-94.

## Windows

- Before assumed target SOC: `6%`
- Before ids: `3635` to `3644`
- Before observed_at: `2026-05-18T03:30:30.102505+00:00` to `2026-05-18T03:33:30.322671+00:00`
- After assumed target SOC: `10%`
- After ids: `3653` to `3662`
- After observed_at: `2026-05-18T03:36:30.921153+00:00` to `2026-05-18T03:39:30.604428+00:00`
- Register count: `95`

## Summary

- Changed or volatile registers in this comparison: `22`
- No register showed a clean stable `6 -> 10` transition in the 0-94 main-register window.
- Register `59` had values near `10` after the app change, but before-window samples already included `10` plus outliers, so it is not reliable evidence for target SOC.
- Most changed registers align with realtime telemetry changes: battery current/power, PV voltage/power, grid/load values, frequency, SOC, and temperature.

## Changed Registers

| Register | Before mode | After mode | Delta | Before values | After values |
|---:|---:|---:|---:|---|---|
| 11 | 5350 | 5340 | -10 | 5340x1, 5350x9 | 5340x10 |
| 12 | 64536 | 64896 | 360 | 64146x1, 64196x1, 64206x1, 64246x1, 64266x1, 64336x1, 64346x1, 64476x1, 64526x1, 64536x1 | 64606x1, 64626x1, 64636x1, 64646x1, 64686x1, 64726x1, 64766x1, 64886x1, 64896x1, 64906x1 |
| 13 | 2310 | 2310 | 0 | 2280x1, 2290x1, 2300x1, 2310x5, 2320x2 | 2280x1, 2300x3, 2310x6 |
| 15 | 2330 | 2320 | -10 | 2310x2, 2330x4, 2340x4 | 2300x1, 2310x1, 2320x5, 2330x3 |
| 16 | 4960 | 4980 | 20 | 4950x2, 4960x5, 4970x1, 4980x2 | 4970x1, 4980x9 |
| 17 | 4960 | 4980 | 20 | 4950x2, 4960x5, 4970x1, 4980x2 | 4970x1, 4980x9 |
| 20 | 405 | 394 | -11 | 399x1, 402x3, 405x5, 408x1 | 393x1, 394x4, 397x1, 400x2, 402x1, 405x1 |
| 21 | 280 | 230 | -50 | 240x1, 260x2, 270x2, 280x3, 290x2 | 190x1, 200x1, 210x2, 220x2, 230x4 |
| 22 | 1045 | 756 | -289 | 987x1, 1030x1, 1045x1, 1083x1, 1086x1, 1121x1, 1135x1, 1145x1, 1163x1, 1172x1 | 756x1, 782x1, 817x1, 847x1, 872x1, 892x1, 919x1, 921x1, 923x1, 926x1 |
| 24 | 1378 | 1385 | 7 | 1371x2, 1372x1, 1373x2, 1375x2, 1376x1, 1378x2 | 1383x1, 1384x2, 1385x4, 1386x3 |
| 25 | 1440 | 1450 | 10 | 1440x6, 1450x4 | 1450x10 |
| 33 | 22 | 24 | 2 | 22x6, 24x4 | 24x10 |
| 34 | 65532 | 65531 | -1 | 65531x4, 65532x6 | 65531x10 |
| 41 | 18 | 19 | 1 | 18x6, 19x4 | 19x10 |
| 50 | 36 | 37 | 1 | 36x7, 37x3 | 37x10 |
| 53 | 65080 | 65082 | 2 | 65014x1, 65063x1, 65067x1, 65076x1, 65078x1, 65079x1, 65080x2, 65093x1, 65094x1 | 65039x1, 65078x3, 65081x1, 65082x3, 65083x1, 65103x1 |
| 54 | 65306 | 65306 | 0 | 65146x1, 65306x6, 65316x2, 65326x1 | 65206x1, 65306x7, 65316x2 |
| 59 | 10 | 9 | -1 | 7x1, 9x2, 10x4, 13x1, 180x1, 286x1 | 7x1, 9x5, 10x3, 275x1 |
| 61 | 64998 | 65191 | 193 | 64792x1, 64814x1, 64824x1, 64842x1, 64855x1, 64894x1, 64895x1, 64967x1, 64993x1, 64998x1 | 65038x1, 65047x1, 65053x1, 65056x1, 65080x1, 65100x1, 65124x1, 65184x1, 65191x1, 65199x1 |
| 63 | 477 | 475 | -2 | 463x2, 477x2, 478x1, 479x1, 480x1, 489x1, 495x1, 542x1 | 455x1, 475x3, 476x2, 478x2, 479x1, 517x1 |
| 64 | 27 | 28 | 1 | 27x6, 28x4 | 28x10 |
| 67 | 466 | 463 | -3 | 450x1, 455x1, 465x1, 466x2, 468x1, 470x1, 479x1, 653x1, 808x1 | 440x1, 463x3, 464x2, 467x2, 468x1, 772x1 |

## Current Interpretation

The current production BLE read covers only main registers `0-94`. The target SOC setting may be stored in another settings register range, may not be reflected in this realtime register response, or may require a different read window. This evidence is insufficient to approve a real write command.

Recommended next research step: add a read-only, manually triggered settings-register scan for candidate ranges, then repeat the vendor-app 10% -> 6% or 10% -> 12% change while comparing those ranges. Keep all operations Modbus function 03 read-only.
