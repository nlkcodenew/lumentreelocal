# 2026-05-25 Dual-Line Firmware Strategy: C3 LAN Update, S3 OTA Stable

## Objective

Define a maintainable two-line firmware strategy for this project as the number
of deployed boards grows and USB/COM-based servicing becomes operationally
insufficient.

The strategy must:

- prioritize `ESP32-C3` first
- preserve the proven persistent-BLE runtime work already landed on `C3`
- give the user a practical network-based update path for `C3` deployments near
  the inverter
- then establish `ESP32-S3` as the more stable OTA-capable release line

## Why This Strategy Is Needed

The project is moving past the stage where boards can reliably stay within easy
USB reach.

The user's intended deployment direction is now:

- move `ESP32-C3` close to the inverter
- let it use the Wi-Fi on the same floor as the inverter
- keep it in place for long-term operation

That changes the firmware priorities:

- physical USB flashing becomes less practical
- network update paths become operationally necessary
- update strategy must be board-specific rather than assumed identical across
  `C3` and `S3`

## Current Reality

### ESP32-C3

Current `C3` line status:

- real board tested
- persistent BLE session logic already landed
- task-separated command poll and telemetry logic landed
- fast/bulk telemetry cache landed
- LAN-only recovery was proven:
  - local portal remained reachable
  - `POST /api/save` triggered a remote reboot
  - telemetry recovered after reboot

Current `C3` constraints:

- `4MB` flash only
- current target uses `single_app_4MB_experimental.csv`
- no standard `ota_0` / `ota_1` dual-slot OTA layout
- current app size is too large for a normal `4MB` dual-OTA slot

### ESP32-S3

Current `S3` line status:

- this remains the stable release family in repo policy
- larger flash headroom already exists on the supported release line
- but the current `S3` release firmware has not yet been brought forward to the
  same persistent-BLE runtime model proven on the current `C3` experimental
  line

Direct conclusion:

- do not add formal OTA to the older `S3` runtime first
- first align `S3` with the newer BLE/session/runtime architecture
- then build OTA on top of that newer stable runtime

## Strategic Split

### Line A: ESP32-C3

Role:

- deployment-near-inverter line
- RF-first line
- practical network-recoverable line

Primary goal:

- deliver good runtime results after the board is moved near the inverter

Primary update model:

- LAN-based web update first
- not full rollback-capable OTA yet

Why:

- this is the line the user wants to deploy physically near the inverter first
- this line already has the latest BLE/session logic
- the immediate operational need is "update/recover without USB"

### Line B: ESP32-S3

Role:

- stable release line
- future canonical production firmware line
- OTA-first line

Primary goal:

- become the safest long-term maintainable release target

Primary update model:

- manifest-driven OTA
- proper integrity verification
- release-grade rollout flow

Why:

- `S3` has the better path to robust OTA due to board-class support and flash
  headroom
- this is the right place to formalize release-quality remote updates

## Priority Order

The implementation order must be:

1. `ESP32-C3` post-relocation validation
2. `ESP32-C3` LAN update path
3. `ESP32-C3` stalled-session recovery improvements if needed
4. `ESP32-S3` runtime uplift to the new persistent-BLE model
5. `ESP32-S3` release-grade OTA

This order is mandatory because the user explicitly wants `ESP32-C3` to show
real results after the placement move before attention shifts to `S3`.

## C3 First-Phase Scope

### Goal

Make `ESP32-C3` operationally manageable after it is moved near the inverter.

### Required Outcomes

After the board move, the `C3` line should be evaluated on:

- BLE request latency
- telemetry freshness in Home Assistant
- upload failure rate
- command poll failure rate
- frequency of telemetry stalls
- whether remote recovery is still needed or becomes rare

### Minimum Network Update Capability

For `C3`, the first acceptable network update target is:

- authenticated local web/LAN firmware update
- upload `.bin` over LAN to the board
- flash in place
- reboot automatically

This should be described as:

- `LAN web update`
- or `network firmware upload`

It should **not** be described yet as:

- robust OTA
- rollback-capable OTA
- dual-slot safe OTA

### C3 Safety Requirements For LAN Update

The `C3` LAN update path should require:

- local network reachability
- explicit authentication or a deliberate physical/local trust boundary
- file type and size validation
- firmware integrity validation if practical
- reboot only after full write success

Nice to have:

- version display
- upload progress
- build identifier shown in portal

Out of scope for the first `C3` network-update phase:

- true `ota_0` / `ota_1` slot switching
- automatic rollback to previous firmware
- remote fleet rollout

## C3 Second-Phase Scope

If post-move runtime still shows periodic telemetry stalls, the next firmware
improvement on `C3` should be:

- explicit stalled-session recovery

Required behavior:

- detect repeated lack of successful BLE reads or uploads
- invalidate the BLE session
- reconnect with bounded backoff
- only escalate to `ESP.restart()` after repeated recovery failure

This should be prioritized above any cosmetic update UX work.

## S3 Runtime Uplift Scope

Before OTA becomes official for `S3`, the `S3` line should be brought to the
same runtime model already proven on the `C3` experimental line.

Required uplift areas:

- persistent BLE session lifecycle
- command poll task separation
- telemetry task separation where appropriate
- fast/bulk telemetry cache model or equivalent
- probe/reporting behavior aligned enough to debug field issues consistently

The purpose is to avoid creating:

- "old runtime + new OTA plumbing"

That would make remote updates easier while leaving the runtime architecture
behind.

## S3 OTA Scope

Once the newer runtime is running on `S3`, the OTA line should implement:

- manifest-based update discovery
- URL-based firmware fetch
- SHA256 verification
- retry with backoff
- skip OTA in AP/provisioning mode
- manual trigger route such as `POST /ota/check`
- optional local web upload fallback

The existing OTA work in `/home/mrlinh/esp32-s3-openclaw` is the reference
source for ideas, not a direct copy target.

## Acceptance Criteria

### C3 Acceptance

The `C3` priority phase is acceptable when:

- the board is relocated and re-tested
- telemetry freshness materially improves under the better RF path
- the board can be recovered or updated over LAN without USB
- write safety behavior remains unchanged
- no claim of full safe OTA is made until partition/size constraints are solved

### S3 Acceptance

The `S3` OTA phase is acceptable only when:

- `S3` is running the newer BLE/session runtime model
- OTA is verified on a real `S3` board
- firmware integrity checks are enforced
- release docs and flash/update procedure are explicit

## Non-Negotiable Safety Rule

Neither the `C3` LAN update path nor the later `S3` OTA path may weaken:

- guarded semantic write rules
- write-grant/token requirements
- schedule overlap protection
- firmware-side last-line schedule validation

Update transport work and inverter write safety are separate concerns and must
stay separate.

## Recommended Next Session Order

1. move `ESP32-C3` near the inverter and connect it to floor-3 Wi-Fi
2. collect real post-move runtime measurements
3. if the RF path improves as expected, implement `C3` LAN web update first
4. only then begin the `S3` runtime uplift branch
5. build formal OTA for `S3` only after the uplift is validated

## Short Conclusion

The correct strategy is not "one update mechanism for every board immediately".

The correct strategy is:

- `ESP32-C3` first, because it is the line the user wants deployed near the
  inverter right now
- give `C3` a practical LAN update path first
- then make `ESP32-S3` the formal OTA-stable release line after its runtime is
  aligned with the newer BLE architecture
