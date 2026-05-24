# ESP32-C3 Persistent BLE Refactor Rollout and Rollback

Date: 2026-05-24

## Scope

This note records the full `ESP32-C3` firmware refactor path away from the old
`connect -> read -> disconnect` BLE telemetry pattern, the hardware-tested
result, and the exact rollback path back to the pre-refactor baseline.

This note is the source of truth for:

- what changed
- which commits matter
- which firmware should be used now
- how to roll back safely if needed

## Board and Runtime Identity

Validated board:

- `ESP32-C3 Super Mini`
- USB port used during testing: `/dev/ttyACM0`

Validated runtime identity during the test cycle:

- gateway id: `esp32-lumentree-01cc9c`
- device id: `P240819130`
- target MAC: `d8:13:2a:ee:58:d6`
- mDNS hostname: `lumentree-cc9c.local`
- last confirmed LAN IP during this refactor session: `192.168.1.245`

## Problem Statement

The original production telemetry path was not wrong because it polled every
`10s`. The deeper problem was that too many blocking lanes overlapped:

- BLE read path could block for a long time
- command polling ran synchronously in `loop()`
- HTTP upload retries could pile onto the same cycle
- older logic reconnected BLE repeatedly instead of keeping a stable session

On `ESP32-C3`, this led to LAN API instability and poor runtime responsiveness
under real hardware load.

## Correct Direction Chosen

The accepted design for `ESP32-C3` is:

- keep the BLE session connected for normal runtime work
- let `ESP32-C3` actively send read requests to the inverter
- avoid `connect -> read -> disconnect` on every telemetry cycle
- separate blocking lanes so local API responsiveness does not depend on BLE or
  HTTP timing
- preserve server compatibility by continuing to upload a compatible full
  `main_registers_0_94` payload

Important protocol conclusion:

- the inverter does not behave like a passive realtime publisher here
- `ESP32-C3` still has to issue read requests
- BLE is only the transport layer; the effective data flow is still
  request/response

## Commit Timeline

Pre-refactor checkpoint:

- `9a5f06e` `chore: checkpoint before persistent BLE session refactor`

Persistent-session foundation:

- `93d5b2c` `feat: keep BLE session alive across telemetry reads`

Early runtime isolation and command-poll fixes:

- `448b36f` `debug: isolate c3 BLE scheduler blocking`
- `fab74fb` `debug: move c3 command polling off main loop`
- `16aa1f8` `debug: add c3 no-upload command-poll task target`
- `3edbdf3` `debug: add c3 command-poll task production target`
- `3c8a2d0` `debug: instrument c3 upload status and cmdtask target`
- `5125d25` `debug: back off dirty telemetry upload retries on c3`

Final validated improvement for this refactor:

- `356568f` `feat: add c3 fast bulk telemetry cache`
- `9f52f5d` `debug: add c3 fastbulk telemetry task target`

## Final Recommended Firmware

Recommended commit:

- `9f52f5d`

Recommended build target:

- `esp32-c3-4mb-debug-cmdtask-fastbulk-task`

Reported firmware version:

- `0.15.1-exp-c3-debug-cmdtask-fastbulk-task`

This is the recommended runtime for `ESP32-C3` after the refactor.

## Why the Final Version Won

The final hardware-tested version kept the new persistent BLE direction but
added the missing pieces needed for `ESP32-C3` stability:

- `command_poll` moved off the main loop into its own task
- telemetry collection moved into its own task
- a fast/bulk cache reduced BLE pressure without breaking server decoding
- HTTP operations were guarded so command polling and telemetry upload no
  longer collide as easily
- upload retry behavior was backed off instead of retrying too aggressively

This kept the local API responsive while still letting the server receive
compatible full telemetry frames.

## Main Firmware Behavior After Refactor

The final `ESP32-C3` path now works like this:

1. BLE stays connected across normal reads instead of reconnecting every cycle.
2. `ESP32-C3` sends read requests on purpose; the inverter replies to those
   requests.
3. Fast ranges are read more often and merged into a cached full main-register
   image.
4. Full main refresh still happens periodically so the cache cannot drift
   forever.
5. Upload sends a server-compatible `main_registers_0_94` snapshot built from
   the cache.
6. `command_poll`, telemetry work, and upload are no longer all stacked inside
   the same hot path in `loop()`.

## Hardware-Tested Result

The best hardware-tested result on the real `ESP32-C3` board was:

- local `/api/status`: stable across repeated checks
- telemetry upload: successful with HTTP `201`
- public `/latest`: updated with the new firmware name and decoded metrics
- main telemetry cache: valid with full `95/95` registers populated
- loop responsiveness: much better than the earlier blocking versions

The final judgement from the real board test was:

- use the new refactored firmware
- do not roll back to the old reconnect-per-read design for normal operation

## Rollback Strategy

There are two rollback levels.

### Level 1: Roll Back to the Last Good Refactor Stage

Use this if the newest fast/bulk task build regresses but you still want the
persistent-session refactor family.

Rollback commit:

- `5125d25` `debug: back off dirty telemetry upload retries on c3`

What this means:

- keeps the command-poll task split
- keeps the broader refactor family
- drops the final fast/bulk task stage

### Level 2: Roll Back to the Exact Pre-Refactor Baseline

Use this if the persistent BLE refactor itself must be abandoned.

Rollback commit:

- `9a5f06e` `chore: checkpoint before persistent BLE session refactor`

What this means:

- returns to the exact firmware checkpoint before the persistent-session
  refactor started
- this is the clean answer to the question "how do I get back to the known
  baseline before this refactor?"

## Exact Rollback Procedure

These steps assume:

- repo path: `/home/mrlinh/esp32-lumentree`
- board connected on `/dev/ttyACM0`
- you want rollback without rewriting history

### Option A: Roll Back to `5125d25`

Create a temporary worktree at the known-good commit:

```bash
git worktree add /tmp/esp32-lumentree-rollback-5125d25 5125d25
```

Build and flash from that worktree:

```bash
cd /tmp/esp32-lumentree-rollback-5125d25/firmware
pio run -e esp32-c3-4mb-debug-cmdtask-chunk24 -t upload --upload-port /dev/ttyACM0
```

Then remove the temporary worktree when done:

```bash
cd /home/mrlinh/esp32-lumentree
git worktree remove /tmp/esp32-lumentree-rollback-5125d25
```

### Option B: Roll Back to the Exact Pre-Refactor Baseline `9a5f06e`

Create a temporary worktree at the pre-refactor checkpoint:

```bash
git worktree add /tmp/esp32-lumentree-rollback-9a5f06e 9a5f06e
```

Build and flash from that worktree:

```bash
cd /tmp/esp32-lumentree-rollback-9a5f06e/firmware
pio run -e esp32-c3-4mb-experimental -t upload --upload-port /dev/ttyACM0
```

Then remove the temporary worktree when done:

```bash
cd /home/mrlinh/esp32-lumentree
git worktree remove /tmp/esp32-lumentree-rollback-9a5f06e
```

## Rollback Notes

- normal firmware upload should preserve NVS, so Wi-Fi credentials and pairing
  state usually remain unless flash erase is performed separately
- do not use `erase_flash` if the goal is only firmware rollback
- rollback should be done by flashing a specific known commit, not by guessing
  build flags from memory
- if the board comes back but local API does not, verify the IP or use
  `lumentree-cc9c.local`

## Operational Recommendation

For normal `ESP32-C3` use after this session:

- stay on `9f52f5d`
- use `esp32-c3-4mb-debug-cmdtask-fastbulk-task`
- treat `5125d25` as the first rollback stop
- treat `9a5f06e` as the full pre-refactor escape hatch

## Related Files

- `docs/plans/2026-05-24-persistent-ble-session-refactor-plan.md`
- `docs/specs/2026-05-24-persistent-ble-session-and-adaptive-polling-spec.md`
- `docs/status/2026-05-24-persistent-ble-session-refactor-implementation.md`
- `docs/status/2026-05-24-session-wrapup-c3-runtime-and-s3-erase.md`
- `SESSION_HANDOFF.md`
