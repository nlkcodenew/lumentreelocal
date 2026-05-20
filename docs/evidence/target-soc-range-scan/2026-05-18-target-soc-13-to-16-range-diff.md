# Target SOC 13% To 16% Manual Range Diff

Read-only comparison of manual Modbus function 03 range `95-189` after vendor app target SOC changed from `13%` to `16%`.

## Summary

- Changed registers in range `95-189`: `3`
- Register `144`: `13` -> `16` (`000d` -> `0010`)
- This confirms the prior `10 -> 13` finding for register `144`.

## Changed Registers

| Register | 13% uint | 16% uint | Delta | 13% hex | 16% hex |
|---:|---:|---:|---:|---|---|
| 95 | 331 | 332 | 1 | `014b` | `014c` |
| 144 | 13 | 16 | 3 | `000d` | `0010` |
| 162 | 14089 | 14850 | 761 | `3709` | `3a02` |

## Interpretation

Register `144` has now followed two vendor-app changes: `10 -> 13` and `13 -> 16`. It is the strongest current evidence for First discharge time target SOC. This is still read-only evidence only; any real write needs a separate command spec, explicit approval, pre-read, post-write verification, and rollback plan.
