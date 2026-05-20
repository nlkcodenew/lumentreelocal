# Lumentree BLE ESP32 Implementation Notes

## Firmware Behavior

The first implementation should add a Lumentree-specific BLE probe path without changing the normal device role. The ESP32 remains a Wi-Fi/MQTT device first; BLE probing is a manual diagnostic feature.

Default behavior:

- Lumentree BLE probe is disabled at boot.
- Existing online behavior must continue without BLE GATT connection attempts.
- No periodic Lumentree BLE read loop is added.
- No inverter settings are written.

## Manual Commands

Add commands through the lowest-risk existing control surface, preferably serial first. MQTT or web commands can be added after serial behavior is stable.

Suggested command names:

- `LUMENTREE_BLE_STATUS`: print probe enabled state and last BLE action result.
- `LUMENTREE_BLE_SCAN_ONCE`: run one passive scan if Wi-Fi and MQTT are currently healthy.
- `LUMENTREE_BLE_SET_TARGET <mac>`: store a candidate inverter MAC for later discovery.
- `LUMENTREE_BLE_DISCOVER`: run one GATT service discovery against the whitelisted target.

All commands must print a clear refusal reason when blocked by safety guards.

## Passive Scan Design

Initial scan settings:

- Passive scan only.
- Duration: 1-2 seconds.
- Low duty cycle interval/window, for example interval around 300-500 ms and window around 30-50 ms.
- No active scan request unless later evidence proves advertisements are insufficient.

Captured fields:

- MAC address.
- Device name, if present.
- RSSI.
- Service UUID list.
- Manufacturer data as hex.
- Service data as hex.
- Wi-Fi status and RSSI before and after scan.
- MQTT connection state before and after scan.

Candidate selection:

- Do not parse Lumentree data as the existing 15-byte temperature/humidity packet.
- Treat all unknown advertisement payloads as raw evidence.
- Use exact MAC whitelist before any GATT connection.

## GATT Discovery Design

GATT discovery is a separate manual phase after stable passive scans.

Rules:

- Only run against the configured target MAC.
- Check Wi-Fi and MQTT health before connecting.
- Use short connection and discovery timeouts.
- Log service UUIDs, characteristic UUIDs, and properties: read, write, notify, indicate.
- Subscribe to notifications only after the characteristic role is understood.
- Disconnect immediately after discovery.
- If Wi-Fi or MQTT drops, disable probe and require manual intervention.

## Modbus Read Preparation

Do not add Modbus polling in the first implementation.

Only prepare a read-only request after discovery identifies a likely BLE transport characteristic. The first read should be a single small request and must be compared with the Home Assistant Lumentree raw register baseline before any periodic polling is designed.

Known comparison baseline:

- Use the Home Assistant Lumentree fork raw register diagnostics.
- Treat known mappings such as signed register 53 for AC input power as validation examples.
- If BLE frames are encrypted, authenticated, or not Modbus-like, stop at evidence collection and document the finding.

## Validation

Minimum validation before upload:

- `pio run -e esp32-s3-devkitc-1`
- Check that probe defaults to disabled.
- Check that scan command refuses when Wi-Fi/MQTT safety guard fails.
- Check that scan logs raw advertisement data without GATT connection.
- Check that no write characteristic or Modbus write operation exists in the probe path.
