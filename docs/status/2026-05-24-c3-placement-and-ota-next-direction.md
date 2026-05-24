# ESP32-C3 Placement and OTA Next Direction

Date: 2026-05-24

## Decision Recorded

The next practical direction for the `ESP32-C3` line is:

1. move the board physically close to the inverter
2. connect it to the `2.4 GHz` Wi-Fi on the same floor as the inverter
3. treat OTA as the next operational priority after RF placement is improved

This direction is based on the current real-world topology:

- current gateway position forces BLE through one concrete floor
- current gateway position also forces Wi-Fi through one concrete floor
- both links use `2.4 GHz`
- this makes RF path quality a bigger bottleneck than `ESP32-C3` vs
  `ESP32-S3`

## Why Placement Comes First

The current firmware is already past the main architectural blocker:

- BLE session is persistent
- command polling is off the main loop
- telemetry work is off the main loop
- fast/bulk cache is working

The remaining high-latency symptom is still mostly the real BLE/Wi-Fi path:

- `ble_request` stays around roughly `0.8s` to `1.0s` on healthy reads
- longer spikes still happen

Because the board will likely be moved near the inverter, future work should
optimize for a gateway that is operationally inconvenient to reach physically.
That makes OTA more valuable than more local USB-only workflows.

## OTA Reference Read From `esp32-s3-openclaw`

Useful OTA building blocks already exist in:

- `/home/mrlinh/esp32-s3-openclaw/platformio/src/ota_handler.cpp`
- `/home/mrlinh/esp32-s3-openclaw/platformio/src/web_server.cpp`
- `/home/mrlinh/esp32-s3-openclaw/platformio/include/config.h`

The reusable ideas are:

- manifest-based update check
- firmware download by URL
- SHA256 verification before accepting the image
- retry with backoff
- skip OTA while in AP mode
- manual trigger endpoint such as `POST /ota/check`
- web upload fallback such as `POST /ota`

These are the correct design ideas to reuse for Lumentree.

## Important Constraint For `ESP32-C3 4MB`

The current `ESP32-C3` experimental target does **not** have a normal OTA
layout.

Current partition table:

- `firmware/single_app_4MB_experimental.csv`
- one large `factory` app partition
- no `ota_0` / `ota_1` dual-app slots

Current firmware size is also too large for a normal `4MB` dual-OTA layout.

Current repo guidance already states:

- firmware image is around `1.64 MB+`
- normal `4MB` dual-OTA slot is about `1.25 MB`

The current tested `ESP32-C3` build is even larger than that older estimate:

- recent tested flash usage is about `1.77 MB`

So the direct conclusion is:

- OTA is the right operational priority
- but full safe dual-slot OTA is not currently available on `ESP32-C3 4MB`
  with the present firmware size

## What This Means Practically

If the board is moved near the inverter and kept there long-term, remote update
must be planned deliberately.

The safe order of work should be:

1. move the board near the inverter and join floor-3 Wi-Fi
2. verify that BLE request latency and upload stability improve materially
3. decide the OTA strategy only after measuring the post-move runtime

## Recommended OTA Strategy Order

### Option 1: Add OTA only after shrinking the firmware enough for dual-slot 4MB

This is the cleanest architecture for remote updates, but it requires real size
reduction first.

Needed outcome:

- app image must fit a realistic `4MB` dual-OTA slot
- partition layout must switch from single factory app to dual OTA app slots

This is the safest long-term solution, but not the fastest.

### Option 2: Keep `ESP32-C3 4MB` runtime as-is and add a controlled web-update path later

This may be useful operationally, but it is not equivalent to full robust
dual-slot OTA unless partitioning is redesigned correctly.

This should not be rushed or described as a safe production OTA path until the
partition/update behavior is verified on real hardware.

### Option 3: If remote update becomes mandatory, reconsider hardware only for OTA headroom

This does **not** mean changing board to fix BLE latency first.

It means:

- keep `ESP32-C3` near the inverter if RF results are good enough
- but if reliable remote firmware replacement is mandatory, larger-flash
  hardware may still be the simpler OTA platform

This is an operational decision, not a BLE-performance decision.

## Recommended Next Direction

For the next Lumentree session, prioritize this sequence:

1. relocate `ESP32-C3` near the inverter
2. connect it to floor-3 Wi-Fi
3. re-measure:
   - `ble_request.last_duration_ms`
   - `telemetry_upload.failures`
   - `command_poll.failures`
   - HA telemetry age
4. if RF improvement is confirmed, start an OTA design note for `ESP32-C3`
   using the `openclaw` OTA reference as source material
5. do **not** promise safe dual-slot OTA on `4MB C3` until firmware size and
   partition strategy are solved explicitly

## Short Conclusion

The user's placement plan is the right next move.

OTA should become the next engineering priority **because** the board will be
moved to a harder-to-reach location.

But on `ESP32-C3 4MB`, OTA is currently a design task with a flash-layout
constraint, not just a quick feature toggle.
