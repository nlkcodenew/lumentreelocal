# Target SOC 10% To 13% Manual Range Diff

Read-only comparison of manual Modbus function 03 ranges after vendor app target SOC changed from `10%` to `13%`.

## Summary

- Manual baseline ranges at 10%: `6`
- Manual after ranges at 13%: `6`
- Changed registers across captured manual ranges: `9`
- Clean `10 -> 13` register candidates: `1`
  - register `144`: `10` -> `13` (`000a` -> `000d`)

## Changed Registers

| Register | 10% uint | 13% uint | Delta | 10% hex | 13% hex |
|---:|---:|---:|---:|---|---|
| 95 | 325 | 331 | 6 | `0145` | `014b` |
| 144 | 10 | 13 | 3 | `000a` | `000d` |
| 162 | 11293 | 13112 | 1819 | `2c1d` | `3338` |
| 190 | 37 | 38 | 1 | `0025` | `0026` |
| 191 | 5284 | 5286 | 2 | `14a4` | `14a6` |
| 192 | 65524 | 65525 | 1 | `fff4` | `fff5` |
| 258 | 5157 | 5158 | 1 | `1425` | `1426` |
| 259 | 65444 | 65446 | 2 | `ffa4` | `ffa6` |
| 260 | 500 | 501 | 1 | `01f4` | `01f5` |

## Interpretation

Register(s) `144` exactly followed the app change from `10%` to `13%`. These are strong target SOC candidates, but this is still read-only evidence only. Before any real write, confirm with one reverse or third-value vendor-app change, identify whether duplicated registers represent multiple schedule slots, and document exact pre-read/post-read verification.
