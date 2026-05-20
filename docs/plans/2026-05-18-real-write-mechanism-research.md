# 2026-05-18 Real Write Mechanism Research

This note records the current write investigation boundary for Lumentree Local.

## Current Evidence

- The target SOC setting in the vendor app is strongly mapped to register `144`.
- Register `144` followed two vendor-app changes:
  - `10%` -> `13%`
  - `13%` -> `16%`
- A guarded ESP32 test attempted a single Modbus function `06` write:

```text
01060090000609E5
```

- The inverter did not echo the request.
- Post-read verification kept register `144` at `16`.
- Conclusion: do not retry function `06` blindly.

## Vendor MQTT Findings

The old Home Assistant integration defines:

- broker: `lesvr.suntcn.com:1886`
- subscribe topic: `reportApp/{device_sn}`
- publish topic: `listenApp/{device_sn}`

Source review found that the old integration only publishes read commands
through `listenApp/{device_sn}`:

- main read: function `03`, registers `0-94`
- cell read: function `03`, registers `250-299`

It contains no real setting-write implementation.

Read-only baseline capture on `2026-05-18T11:24:50+07:00` confirmed live vendor
MQTT traffic:

```text
listenApp/P240819130 01030000005f05f2
listenApp/P240819130 010300fa0032e42e
reportApp/P240819130 <main-register response>
```

The two `listenApp` frames are read-only Modbus requests:

- `01030000005f05f2`: read registers `0-94`
- `010300fa0032e42e`: read registers `250-299`

Evidence file:

```text
docs/evidence/vendor-mqtt-captures/2026-05-18-passive-baseline-P240819130.log
```

## Working Hypotheses

The real vendor write mechanism is still unknown. Plausible mechanisms:

- The mobile app or cloud publishes a Modbus write frame to
  `listenApp/{device_id}`.
- It uses function `16` (`0x10`) write-multiple-registers rather than function
  `06`.
- It writes a settings block around register `144`, not only register `144`.
- It requires an unlock, auth, or commit frame before the setting write.
- BLE write may need write-without-response, but MQTT is currently the safer
  capture surface because it can be observed passively.

## Safety Boundary

Until a real vendor write payload is captured and decoded:

- Do not send another Modbus write frame from ESP32.
- Do not add a generic register write command.
- Do not enable Home Assistant real-write services.
- Keep HASS command support limited to dry-run unless a new plan explicitly
  approves one semantic command.

## Next Capture Procedure

Use the read-only helper:

```text
tools/capture_vendor_mqtt.sh DEVICE_ID
```

Then change the setting in the vendor app while the helper is running. Capture
both:

- `listenApp/{device_id}` command payloads
- `reportApp/{device_id}` responses

For target SOC, the preferred next observation is:

1. Read current register `144`.
2. Start `tools/capture_vendor_mqtt.sh DEVICE_ID`.
3. In the vendor app, change First discharge time target SOC to a distinct
   value.
4. Stop the capture.
5. Decode any non-read `listenApp` frame and compare it with the before/after
   value of register `144`.

Only after this should a new write spec be drafted.

## Live Capture: Target SOC 16% To 6%

The user changed the vendor app setting from `16%` to `6%` while this read-only
MQTT capture was running:

```text
docs/evidence/vendor-mqtt-captures/2026-05-18-target-soc-16-to-6-live-capture.log
```

The captured write frame was repeated several times:

```text
0110009000010200063b02
```

Decoded:

- slave id: `01`
- function: `10` hex, Modbus write multiple registers
- start register: `0090` hex = `144`
- register count: `0001`
- byte count: `02`
- register value: `0006` = `6`
- CRC: `3b02`, valid Modbus CRC little-endian

The vendor response/verification sequence at `2026-05-18T11:28:37+07:00` was:

```text
reportApp/P240819130 0110009000010200063b022b2b2b2b01100090000101e4
listenApp/P240819130 0103009000018427
reportApp/P240819130 01030090000184272b2b2b2b01030200063846
```

Decoded:

- `01100090000101e4`: function `10` ACK for start register `144`, count `1`;
  CRC valid.
- `0103009000018427`: read register `144`, count `1`; CRC valid.
- `01030200063846`: read response value `0006`; CRC valid.

Interpretation:

- The vendor app writes this setting with Modbus function `16` (`0x10`), not
  function `06`.
- The semantic write for this exact setting is:

```text
write register 144 as one-register function-16 payload
```

- The app repeats the write command until it receives or observes success.
- The immediate post-write verification reads register `144` and confirms the
  requested value.

Updated safety conclusion:

- We now have the real vendor payload for `Target SOC 16% -> 6%`.
- The next implementation should still not expose a generic register writer.
- A future ESP32 command should be one semantic command only, for example
  `SET_FIRST_DISCHARGE_TARGET_SOC`, guarded by:
  - scoped write grant,
  - allowed range,
  - pre-read of register `144`,
  - function `16` one-register write,
  - ACK validation,
  - post-read verification,
  - audit log.
