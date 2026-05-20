# Lumentree Dedicated BLE Bridge

## Decision

Use a separate ESP32-S3 firmware for Lumentree BLE work instead of extending the existing multi-feature firmware.

The dedicated firmware lives in `firmware/`. It disables Wi-Fi, defaults to BLE passive scanning, can run one manual active scan when scan response data is needed, and writes JSONL to USB serial. Host-side collection lives in `host/ble-collector/`.

## Rationale

- The current main firmware has many unrelated responsibilities: Wi-Fi, MQTT, mesh, dashboard, config, OTA, and automation.
- Lumentree BLE work needs controlled radio behavior and raw evidence, not a production bridge at first.
- USB serial is already available, so the host can collect and persist raw evidence without adding ESP32 Wi-Fi/MQTT risk.
- Parser and storage changes can happen on the host without reflashing firmware.

## Initial Flow

1. Flash `firmware` only when explicitly ready to replace firmware on the attached ESP32.
2. Run the serial collector in log-only mode.
3. Send `SCAN_ONCE` to capture raw advertisements.
4. Set a target MAC once the inverter advertisement is identified.
5. Store raw JSONL in Postgres only after log-only capture works.
6. GATT discovery is available as a manual evidence command, still without Modbus reads or writes.

## Guardrails

- GATT discovery may be run manually after the target MAC and address type are learned from scan output.
- No Modbus read in v0.
- No Modbus write ever unless explicitly approved in a later plan.
- Hard safety rule for future GATT work: write-capable characteristics may be discovered and logged, but firmware must not call any write API unless inverter setting writes are separately planned and explicitly approved.
- Active scan is allowed only as a manual scan-response probe. GATT discovery must still avoid read, notify, and write calls.
- No direct MQTT publishing from the ESP32 until BLE behavior is proven stable.
- Home Assistant should receive decoded data later; raw protocol evidence should first be retained in Postgres or JSONL logs.

## Known Lumentree BLE Evidence

Phone BLE scanner evidence from the user:

```text
Name: P240819130
Service UUID: A018739B-734D-8211-CB80-C9ACD39D13B4
```

The UUID is a 128-bit service identifier, not the BLE MAC. The dedicated firmware should mark advertisements as Lumentree candidates when the name or service UUID matches, then use the emitted `mac` field as the target for follow-up scans.

If serial output becomes noisy, use candidate-only scan mode so firmware only emits advertisements matching `P240819130` or the known Lumentree service UUID.

## Discovery Evidence

Observed target:

```text
MAC: d8:13:2a:ee:58:d6
Name from active scan response: P240819130
Passive advertisement payload: 020106051220004000
Address type: 0
```

Safe GATT discovery found:

```text
Service 00001800-0000-1000-8000-00805f9b34fb
  Characteristic 00002a00: read
  Characteristic 00002a01: read
  Characteristic 00002aa6: read
Service 00001801-0000-1000-8000-00805f9b34fb
  Characteristic 00002a05: indicate
Service 0000ffe0-0000-1000-8000-00805f9b34fb
  Characteristic 0000ffe1: read, write, write_without_response, notify
```

The likely vendor data transport is `FFE0/FFE1`. Because `FFE1` is write-capable, all future work must preserve the no-write rule unless separately approved.

Follow-up discovery run on 2026-05-16 completed with `gatt_discovery_done` and `gatt_disconnected`, then `STATUS` confirmed the ESP32 was still alive. No read, notify subscription, or write API was called.

## Next Safety Gate

Do not implement Modbus-over-BLE yet. `FFE1` supports read, write, write-without-response, and notify; a normal request/response protocol may require writing a request and subscribing to notifications. Both must stay blocked until a later plan explicitly approves the exact operation.
