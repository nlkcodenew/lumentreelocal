# 2026-05-20 Function 0x04 Firmware Integration Spec

Implementation spec for adopting Modbus function `0x04` (Read Input Registers) into the ESP32 firmware so today's vendor-reported energy totals reach Home Assistant alongside the existing function `0x03` real-time stream.

This is the follow-up to Finding 1 of `2026-05-20-syssi-cross-check-spec.md`.

## Goal

Add a low-rate (5-minute) function `0x04` read at `start_register=0, count=8` to the production upload loop, route it through the existing telemetry pipeline, and surface six new daily kWh sensors in `lumentreelocal`.

## Non-Goals

- No change to the function `0x03` polling cadence or payload format.
- No change to the function `0x10` write path or the validated discharge/mains-charge slot register addresses.
- No change to BLE service/characteristic UUIDs (`0xFFE0` / `0xFFE1`).
- No new Postgres table. Function `0x04` rows piggyback on `lumentree_telemetry` and are tagged `data_type = "statistics"` by the decoder.
- No replacement of `lumentree_energy_daily` (the sample-derived daily aggregation). Vendor-reported daily totals are exposed as parallel sensors, not a replacement.

## Why Now

User runs the inverter with WiFi disabled and no vendor cloud. Function `0x03` samples are persisted to Postgres and aggregated into `lumentree_energy_daily`, but those numbers are derived from 10-second samples and miss any window the gateway was offline. Function `0x04` returns the inverter's own daily totals and lets us cross-check (and fall back to) the vendor counter without touching any write path.

The inverter still resets these totals on power loss (so `0x04` is **not** a persistence solution). It is a cross-check signal.

## Confirmed Inputs

- Decoder `_parse_function_04_statistics` already exists at `host/local-server/lumentree_decode.py:362`.
- Mock-data test passes — see `host/local-server/test_function_04_report.md`.
- Server already routes `function_code == 0x04` through `parse_payload`, so a frame arriving on `/api/lumentree/events` with `start_register=0` and a `0104…` payload Just Works on the decode side.
- Firmware `runModbusRead` is currently hardcoded to function `0x03` in three places that need to be considered:
  - `buildReadCommand` (`firmware/src/main.cpp:221`) — frame builder.
  - `runModbusRead` (`firmware/src/main.cpp:2307`) — emits a `safety: function_03_read_only_no_setting_write` field.
  - `readRegisterFromResult` (`firmware/src/main.cpp:2448`) — rejects responses where `bytes[1] != 0x03`. Only used by the discharge-slot pre/post read path; **not** in the stats path.

## Surface Changes

### 1. Firmware: `firmware/src/main.cpp`

#### 1.1 Frame builder — add a sibling, do not generalise

Add a new builder right after `buildReadCommand` instead of changing its signature. Keeping the existing function untouched avoids touching the function `0x03` upload path.

```cpp
static String buildReadInputCommand(uint8_t slaveId, uint16_t startRegister, uint16_t registerCount) {
  uint8_t frame[8] = {
    slaveId,
    0x04,
    (uint8_t)((startRegister >> 8) & 0xFF),
    (uint8_t)(startRegister & 0xFF),
    (uint8_t)((registerCount >> 8) & 0xFF),
    (uint8_t)(registerCount & 0xFF),
    0,
    0,
  };
  uint16_t crc = crc16Modbus(frame, 6);
  frame[6] = (uint8_t)(crc & 0xFF);
  frame[7] = (uint8_t)((crc >> 8) & 0xFF);
  return bytesToHex(frame, sizeof(frame));
}
```

#### 1.2 Read driver — `runModbusReadInput`

Add a sibling to `runModbusRead`. It is a near-copy that:

- Calls `buildReadInputCommand` instead of `buildReadCommand`.
- Sets `start["safety"] = "function_04_read_only_input_registers"` instead of the function-03 string.
- Sets `start["label"]` to `today_statistics_0_7`.
- Reuses the same BLE connect / notify / disconnect logic.

Keep `readRegisterFromResult` unchanged — it is only used by the discharge-slot test path which stays on `0x03`.

#### 1.3 Production upload — extend `runUploadOnce`

In `runUploadOnce` (`firmware/src/main.cpp:2711`), add a third block after the settings upload:

```cpp
unsigned long now = millis();
if (lastStatsUploadMs == 0 || now - lastStatsUploadMs >= STATS_UPLOAD_INTERVAL_MS) {
  ModbusReadResult stats = runModbusReadInput(0, 8, "today_statistics_0_7");
  if (stats.ok) {
    postTelemetry(stats.payloadHex, stats.notifyCount, stats.length, 0, 8, "today_statistics_0_7");
    lastStatsUploadMs = millis();
  } else {
    emitError("stats_read_empty", "no statistics Modbus response payload to upload");
  }
}
```

Add the cadence constant alongside `SETTINGS_UPLOAD_INTERVAL_MS`:

```cpp
static const unsigned long STATS_UPLOAD_INTERVAL_MS = 5UL * 60UL * 1000UL;  // 5 minutes
static unsigned long lastStatsUploadMs = 0;
```

5 minutes is a deliberate choice: vendor-reported daily totals only update every few minutes inside the inverter, and we already pay ~10 s of BLE reconnect overhead per read. Going faster yields no new information and burns battery on the gateway side.

#### 1.4 Manual command — `READ_STATS_ONCE`

Add to `handleCommand` next to `READ_MAIN_ONCE`:

```cpp
} else if (line == "READ_STATS_ONCE") {
  runModbusReadInput(0, 8, "today_statistics_0_7");
```

And to `printHelp`:

```cpp
Serial0.println("# READ_STATS_ONCE");
```

This gives the same operator-friendly serial control as the existing `READ_MAIN_ONCE` / `READ_CELLS_ONCE` and matches how the user already verifies behavior via serial (see auto-memory: `BLE verification method`).

#### 1.5 What deliberately stays the same

- `postTelemetry` body fields are unchanged. `start_register=0, register_count=8` and the new label flow as-is. The hardcoded `raw["safety"] = "function_03_read_only_no_setting_write"` literal is **kept as-is** because the local server does not gate on it. Touching it risks unrelated regressions on settings/main uploads. If we want a function-aware safety tag later, that is a separate change.
- `MIN_UPLOAD_INTERVAL_SECONDS` / `uploadIntervalSeconds` (the function 0x03 cadence) are not touched.

### 2. Local Server — `host/local-server/server.py`

No code change required. Verification only:

- `decode_payload` (`server.py:377`) reads `start_register` from `raw` (firmware sends `0`) and calls `parse_payload(payload_hex, start_register=0)`. The decoder routes on `function_code` and returns `{"data_type": "statistics", "metrics": {...}, "diagnostics": {...}}`.
- `store_event` writes `metrics` (six `today_*_kwh` floats) and a `raw.decode.data_type = "statistics"` marker into `lumentree_telemetry`.
- `update_energy_aggregate` will early-return on these rows because none of `pv_power` / `load_power` / `grid_power` / `battery_power` / `ac_input_power` / `ac_output_power` exist in a statistics frame. That is the desired behaviour — we do **not** want statistics rows polluting `lumentree_energy_daily`.

### 3. Home Assistant — `custom_components/lumentreelocal/sensor.py`

Add six new `LumentreeLocalSensorDescription` entries with `device_class=ENERGY`, `state_class=TOTAL_INCREASING`, `native_unit_of_measurement="kWh"`:

| `metric_key` | `translation_key` |
|---|---|
| `today_pv_generation_kwh` | `today_pv_generation_kwh` |
| `today_essential_load_kwh` | `today_essential_load_kwh` |
| `today_total_load_kwh` | `today_total_load_kwh` |
| `today_grid_import_kwh` | `today_grid_import_kwh` |
| `today_battery_charge_kwh` | `today_battery_charge_kwh` |
| `today_battery_discharge_kwh` | `today_battery_discharge_kwh` |

Notes on naming alignment:

- These exactly match the keys produced by `_parse_function_04_statistics` in the decoder.
- They use suffix `_kwh` to keep them visibly distinct from the existing `lumentree_energy_daily`-derived sensors.
- `TOTAL_INCREASING` is correct because the inverter resets these counters at midnight, which Home Assistant's energy dashboard handles natively.

Add corresponding strings under `translations/en.json` and (if used) `translations/vi.json`.

No `binary_sensor`, `number`, `switch`, `select`, or `time` change is needed.

### 4. Decoder — `host/local-server/lumentree_decode.py`

No code change. The current implementation:

- Routes on `function_code == 0x04` in `parse_payload`.
- Validates `start_register == 0` and `len(data) == 16` in `_parse_function_04_statistics`. Returns `None` otherwise.
- Maps registers `0..5` to the six `today_*_kwh` keys at scale `÷10`.
- Ignores registers `6..7` (reserved per syssi protocol notes).

### 5. CI / Tests

- `host/local-server/test_function_04.py` already covers realistic, zero, and 0x03-regression cases. Keep it. Run as part of `pytest` for the local-server package.
- Add one integration test against the live `/api/lumentree/events` endpoint that posts a synthetic statistics payload and asserts the resulting telemetry row has `metrics.today_pv_generation_kwh == 12.5`. This is the cheapest way to catch a server-side decode regression.

## Rollout Order

1. Decoder + tests (already merged; just confirm green).
2. Server: no change; manually `curl` a synthetic frame to `/api/lumentree/events` and inspect the row.
3. Firmware: build, flash, run `READ_STATS_ONCE` over serial. Inspect the JSON line:
   - `payload_hex` starts with `0104`.
   - CRC verifies.
   - Decoder returns six metric keys.
4. Enable the periodic upload. Watch one upload cycle: function 0x03 main → settings (if due) → stats. Confirm Postgres row count grows and `metrics.today_pv_generation_kwh` is set.
5. Add the six HA sensors, reload the integration, confirm they populate within one 5-minute cycle.

Each step is reversible without touching the inverter.

## Risks and Stop Conditions

- **Firmware regression in 0x03 path.** Mitigated by adding sibling functions (`buildReadInputCommand`, `runModbusReadInput`) instead of modifying `buildReadCommand` / `runModbusRead`. Stop and revert if any function 0x03 frame fails CRC after the firmware change.
- **Inverter rejects `0x04`.** If `_parse_function_04_statistics` returns `None` for every real-inverter frame for >30 minutes, disable `STATS_UPLOAD_INTERVAL_MS` (set to 0 / large) and re-evaluate. The inverter could be on a firmware variant that does not implement input registers.
- **Cross-check disagreement.** If the function 0x04 daily total disagrees with the matching `lumentree_energy_daily` row by more than ~10% under sustained generation, treat it as an investigation finding, not a deployment blocker. Both numbers are valid signals; the gap is the value of having both.
- **BLE airtime contention.** Function 0x04 adds one extra connect/read/disconnect cycle every 5 min. If we observe BLE queue starvation on the function 0x03 path, double the stats interval rather than removing the feature.

## Out of Scope (revisit later)

- Hourly / monthly / yearly chart registers documented by syssi but not implemented.
- A function-aware `safety` tag on `postTelemetry` payloads.
- Adopting any of the other five Findings from `2026-05-20-syssi-cross-check-spec.md` (register 53/59/70 naming, operation-mode mapping, model-name derivation, extra diagnostic registers, frame reassembly). Each is its own decision.

## References

- `host/local-server/lumentree_decode.py` — `_parse_function_04_statistics`
- `host/local-server/test_function_04.py` — mock validation
- `host/local-server/test_function_04_report.md` — test results
- `firmware/src/main.cpp` — `buildReadCommand`, `runModbusRead`, `runUploadOnce`, `postTelemetry`
- `docs/specs/2026-05-20-syssi-cross-check-spec.md` — Finding 1 (origin of this work)
