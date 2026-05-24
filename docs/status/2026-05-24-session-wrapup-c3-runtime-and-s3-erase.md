# Session Wrap-Up: C3 Runtime, Flash-Site Publish, and S3 Erase

Date: 2026-05-24

## Scope Completed

This session covered three concrete outcomes:

1. `ESP32-C3 Super Mini` was validated end-to-end against the real inverter,
   local server, and Home Assistant.
2. The public flash site was updated so members can explicitly choose between:
   - `ESP32-S3` stable
   - `ESP32-C3 Super Mini` experimental
3. The older `ESP32-S3` test board was erased on request without reflashing.

## ESP32-C3 Runtime Result

The new `ESP32-C3 Super Mini` board is no longer just a build-only target.
It passed real-world runtime checks:

- flashed successfully
- booted successfully
- AP provisioning worked
- Wi-Fi join worked
- LAN portal worked
- BLE candidate flow worked
- inverter pairing worked
- telemetry upload worked
- Home Assistant updated from live data on the expected polling cycle

Observed runtime identity:

- gateway id: `esp32-lumentree-01cc9c`
- firmware: `lumentree-ble-bridge/0.15.1-exp-c3-4mb`
- device id: `P240819130`
- target MAC: `d8:13:2a:ee:58:d6`
- mDNS hostname: `lumentree-cc9c.local`
- last confirmed LAN IP during this session: `192.168.1.245`

Current support position:

- keep `ESP32-C3 Super Mini` labeled `experimental`
- do not reclassify it as stable until it survives longer soak testing

## Flash-Site Publish Result

The flash site now exposes two board choices.

Stable:

- `ESP32-S3`

Experimental:

- `ESP32-C3 Super Mini`

Published manifests now include:

- `/firmware/flash-manifest.json`
- `/firmware/esp32-s3/flash-manifest.json`
- `/firmware/esp32-c3/flash-manifest.json`

The website text and operator docs were updated so users understand the board
choice and the support level of each line.

See:

- `docs/status/2026-05-24-esp32-c3-flash-site-publish.md`

## ESP32-S3 Erase Result

The older `ESP32-S3` board was reconnected on USB and verified before erase:

- port: `/dev/ttyACM0`
- chip: `ESP32-S3`
- MAC: `3c:dc:75:63:47:5c`

Action completed:

- `erase_flash` only
- no firmware reflashed

Result:

- flash erase succeeded
- board was left blank intentionally

## Repo State

Relevant publish commit from this session:

- `fb9c9e1` `feat: publish esp32-c3 firmware on flash site`

This commit was pushed to:

- `private/main`

## Next Session Guidance

If the next session continues C3 work:

1. read `docs/status/2026-05-24-esp32-c3-experimental-first-boot.md`
2. read `docs/status/2026-05-24-esp32-c3-flash-site-publish.md`
3. treat the current board as a real experimental runtime target, not just a
   hypothetical build target
4. if new instability appears during the one-week soak test, compare:
   - gateway status freshness
   - telemetry age
   - LAN portal reachability
   - BLE pairing state
   - Home Assistant entity update cadence
