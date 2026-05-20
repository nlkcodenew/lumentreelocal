# Lumentree BLE ESP32 Plan

## Goal

Use the ESP32-S3 firmware to investigate whether the Lumentree inverter exposes useful telemetry over BLE, without disrupting the inverter's current 2.4 GHz Wi-Fi connection and without writing any inverter settings.

The first milestone is evidence gathering, not a full telemetry client. The firmware should prove which BLE advertisements, services, characteristics, and response frames exist before any Modbus-style read loop is considered.

## Current Constraints

- The inverter is currently online through 2.4 GHz Wi-Fi.
- BLE activity can interfere with Wi-Fi/BLE coexistence, so any scan or GATT connection must be short, manual, and observable.
- Current firmware has periodic passive BLE scanning logic, but Lumentree work must not rely on always-on scan behavior.
- Lumentree protocol is not confirmed. It may expose telemetry through BLE GATT, may only expose provisioning, or may require app-specific authentication/encryption.
- Safety boundary: read-only only. Do not write BLE characteristics for inverter settings and do not write Modbus registers.

## Strategy

1. Disable automatic Lumentree BLE probing by default.
2. Add a manual, one-shot passive advertisement scan with very low duty cycle.
3. Log raw BLE advertisement evidence for candidate Lumentree devices.
4. Add Wi-Fi and MQTT safety checks before and after each BLE action.
5. Whitelist the candidate inverter by MAC, name, or service UUID before attempting GATT.
6. Run one manual GATT service discovery with short timeout and immediate disconnect.
7. Only after discovery identifies likely read/notify characteristics, prepare a single read-only Modbus-style request.
8. Compare any BLE response with the Home Assistant Lumentree raw register baseline before adding polling.

## Safety Rules

- Default state: Lumentree BLE probe disabled.
- Scan mode: passive scan first; no active scan until explicitly needed.
- Scan duration: 1-2 seconds for initial tests.
- Scan duty: low window/interval ratio, not near-continuous scan.
- GATT connect: manual command only, short timeout, disconnect immediately.
- Modbus: read-only requests only; no writes.
- Abort condition: if Wi-Fi disconnects or MQTT drops after a BLE action, disable BLE probe and do not retry automatically.

## Success Criteria

- Firmware can perform a one-shot passive BLE scan without dropping Wi-Fi or MQTT.
- Candidate Lumentree advertisement data is captured as raw hex with enough metadata to identify the device.
- A later GATT discovery can list service and characteristic UUIDs without persistent connection.
- No inverter setting is written at any stage.
- The implementation leaves the existing online ESP32 recoverable through serial and normal firmware upload.
