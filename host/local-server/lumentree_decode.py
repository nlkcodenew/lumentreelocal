"""Read-only Lumentree Modbus response decoder."""

from __future__ import annotations

import math
import struct
from typing import Any


REG_ADDR = {
  "DEVICE_MODEL_START": 3,
  "BATTERY_VOLTAGE": 11,
  "BATTERY_CURRENT": 12,
  "AC_OUT_VOLTAGE": 13,
  "GRID_VOLTAGE": 15,
  "AC_OUT_FREQ": 16,
  "AC_IN_FREQ": 17,
  "AC_OUT_POWER": 18,
  "PV1_VOLTAGE": 20,
  "PV1_POWER": 22,
  "DEVICE_TEMP": 24,
  "BATTERY_TYPE": 37,
  "BATTERY_SOC": 50,
  "AC_IN_POWER": 53,
  "AC_OUT_VA": 58,
  "GRID_POWER": 59,
  "BATTERY_POWER": 61,
  "LOAD_POWER": 67,
  "UPS_MODE": 68,
  "MASTER_SLAVE_STATUS": 70,
  "PV2_VOLTAGE": 72,
  "PV2_POWER": 74,
}

MAP_BATTERY_TYPE = {
  0: "Error",
  1: "Present",
  2: "No Battery",
}
MAP_DEVICE_POWER_RATING_WATTS = {
  2: 5500,
  3: 4000,
  5: 6000,
}
MAIN_REGISTER_COUNT = 95
CELL_REGISTER_COUNT = 50

SETTINGS_REGISTERS = {
  "mains_charge_slot_1_start_time": 130,
  "mains_charge_slot_2_start_time": 132,
  "mains_charge_slot_1_enabled": 134,
  "mains_charge_slot_2_enabled": 136,
  "mains_charge_slot_1_end_time": 138,
  "mains_charge_slot_2_end_time": 140,
  "mains_charge_slot_1_target_soc": 143,
  "mains_charge_slot_2_target_soc": 145,
  "first_discharge_target_soc": 144,
  "discharge_slot_2_target_soc": 146,
  "discharge_slot_3_target_soc": 177,
  "discharge_slot_4_target_soc": 178,
  "discharge_slot_1_enabled": 135,
  "discharge_slot_2_enabled": 137,
  "discharge_slot_3_enabled": 151,
  "discharge_slot_4_enabled": 152,
  "discharge_slot_1_start_time": 131,
  "discharge_slot_1_end_time": 139,
  "discharge_slot_2_start_time": 133,
  "discharge_slot_2_end_time": 141,
  "discharge_slot_3_start_time": 173,
  "discharge_slot_3_end_time": 174,
  "discharge_slot_4_start_time": 175,
  "discharge_slot_4_end_time": 176,
  "discharge_slot_1_power": 180,
  "discharge_slot_2_power": 182,
  "discharge_slot_3_power": 183,
  "discharge_slot_4_power": 184,
}


def crc16_modbus(data: bytes) -> int:
  crc = 0xFFFF
  for byte in data:
    crc ^= byte
    for _ in range(8):
      if crc & 1:
        crc = (crc >> 1) ^ 0xA001
      else:
        crc >>= 1
  return crc & 0xFFFF


def verify_crc(frame_hex: str) -> tuple[bool, str | None]:
  if len(frame_hex) < 4:
    return False, "too short"
  try:
    body = bytes.fromhex(frame_hex[:-4])
    expected = frame_hex[-4:].lower()
  except ValueError:
    return False, "invalid hex"
  actual = crc16_modbus(body).to_bytes(2, "little").hex()
  if actual != expected:
    return False, f"mismatch {expected} vs {actual}"
  return True, None


def generate_read_command(slave_id: int, function_code: int, start: int, count: int) -> str:
  body = bytes([slave_id, function_code]) + start.to_bytes(2, "big") + count.to_bytes(2, "big")
  crc = crc16_modbus(body).to_bytes(2, "little")
  return (body + crc).hex()


def _extract_response_hex(payload_hex: str) -> str | None:
  ph = (payload_hex or "").strip().lower()
  if not ph:
    return None
  sep = "2b2b2b2b"
  if sep in ph:
    parts = ph.split(sep, 1)
    if len(parts) == 2 and (parts[1].startswith("0103") or parts[1].startswith("0104")):
      return parts[1]
  if ph.startswith("0103") or ph.startswith("0104"):
    return ph
  return None


def _read_register(
  data: bytes,
  register: int,
  signed: bool,
  factor: float = 1.0,
  byte_count: int = 2,
) -> float | None:
  offset = register * 2
  if offset + byte_count > len(data):
    return None
  raw = data[offset : offset + byte_count]
  if byte_count == 2:
    value = struct.unpack(">h" if signed else ">H", raw)[0]
  elif byte_count == 4:
    value = struct.unpack(">i" if signed else ">I", raw)[0]
  else:
    return None
  result = round(value * factor, 3)
  return result if math.isfinite(result) else None


def _read_string(data: bytes, start_register: int, register_count: int) -> str | None:
  offset = start_register * 2
  length = register_count * 2
  if offset + length > len(data):
    return None
  text = data[offset : offset + length].decode("ascii", "ignore").replace("\x00", "").strip()
  return text or None


def _reader(data: bytes):
  def read(name: str, signed: bool, factor: float = 1.0, byte_count: int = 2):
    register = REG_ADDR.get(name)
    if register is None:
      return None
    return _read_register(data, register, signed, factor, byte_count)

  return read


def _register_diagnostics(payload_hex: str, response_hex: str, data: bytes, start_register: int = 0) -> dict[str, Any]:
  register_hex = []
  register_uint = []
  register_int = []
  for idx in range(len(data) // 2):
    raw = data[idx * 2 : idx * 2 + 2]
    register_hex.append(raw.hex())
    register_uint.append(int.from_bytes(raw, "big", signed=False))
    register_int.append(int.from_bytes(raw, "big", signed=True))

  named_registers = {}
  for name, idx in REG_ADDR.items():
    if idx < len(register_uint):
      named_registers[name] = {
        "register": idx,
        "hex": register_hex[idx],
        "uint": register_uint[idx],
        "int": register_int[idx],
      }

  crc_ok, crc_error = verify_crc(response_hex)
  return {
    "register_count": len(register_uint),
    "start_register": start_register,
    "end_register": start_register + len(register_uint) - 1,
    "raw_hex": payload_hex,
    "response_hex": response_hex,
    "response_hex_length": len(response_hex),
    "slave_id": int(response_hex[0:2], 16),
    "function_code": int(response_hex[2:4], 16),
    "byte_count": int(response_hex[4:6], 16),
    "crc_hex": response_hex[-4:],
    "crc_ok": crc_ok,
    "crc_error": crc_error,
    "register_hex": register_hex,
    "register_uint": register_uint,
    "register_int": register_int,
    "named_registers": named_registers,
    "device_sn": _read_string(data, REG_ADDR["DEVICE_MODEL_START"], 5),
  }


def _parse_settings_data(data: bytes, start_register: int) -> dict[str, Any]:
  settings = {}
  registers = {}
  for key, register in SETTINGS_REGISTERS.items():
    offset = (register - start_register) * 2
    if offset < 0 or offset + 2 > len(data):
      continue
    value = int.from_bytes(data[offset : offset + 2], "big", signed=False)
    registers[key] = {"register": register, "value": value}
    if key.endswith("_enabled"):
      settings[key] = bool(value)
    else:
      settings[key] = value
  return {"settings": settings, "setting_registers": registers}


def _parse_cell_data(data: bytes) -> dict[str, Any] | None:
  cells = []
  for idx in range(CELL_REGISTER_COUNT):
    raw_mv = _read_register(data, idx, False)
    if raw_mv is None:
      continue
    voltage = round(raw_mv / 1000.0, 3)
    if 1.5 <= voltage <= 5.0:
      cells.append({"cell": idx + 1, "voltage": voltage})
  if not cells:
    return None
  voltages = [cell["voltage"] for cell in cells]
  return {
    "cell_count": len(cells),
    "min_cell_voltage": min(voltages),
    "max_cell_voltage": max(voltages),
    "delta_cell_voltage": round(max(voltages) - min(voltages), 3),
    "cells": cells,
  }


def parse_payload(payload_hex: str, start_register: int = 0) -> dict[str, Any] | None:
  response_hex = _extract_response_hex(payload_hex)
  if not response_hex or len(response_hex) < 10:
    return None
  try:
    function_code = int(response_hex[2:4], 16)
    byte_count = int(response_hex[4:6], 16)
    data = bytes.fromhex(response_hex[6:-4])
  except ValueError:
    return None
  if byte_count != len(data) or len(data) == 0:
    return None

  if function_code == 0x04:
    return _parse_function_04_statistics(payload_hex, response_hex, data, start_register)

  expected_main_bytes = MAIN_REGISTER_COUNT * 2
  expected_cell_bytes = CELL_REGISTER_COUNT * 2
  data_type = "main"
  if start_register == 95 and len(data) >= expected_main_bytes:
    data_type = "settings"
    data = data[:expected_main_bytes]
  elif len(data) == expected_main_bytes + 12:
    data = data[:expected_main_bytes]
  elif len(data) == 198:
    data = data[:expected_main_bytes]
  elif len(data) == expected_cell_bytes:
    data_type = "cells"
  elif abs(len(data) - expected_main_bytes) <= 20 and len(data) >= expected_main_bytes - 10:
    data = data[:expected_main_bytes].ljust(expected_main_bytes, b"\x00")
  else:
    return None

  diagnostics = _register_diagnostics(payload_hex, response_hex, data, start_register)
  if data_type == "settings":
    diagnostics.update(_parse_settings_data(data, start_register))
    return {"data_type": "settings", "metrics": {}, "diagnostics": diagnostics}
  if data_type == "cells":
    cell_info = _parse_cell_data(data)
    if not cell_info:
      return None
    return {"data_type": "cells", "metrics": {"battery_cell_info": cell_info}, "diagnostics": diagnostics}

  rr = _reader(data)
  metrics: dict[str, Any] = {}

  device_type_code = _read_register(data, 0, False)
  if device_type_code is not None:
    metrics["device_type_code"] = int(device_type_code)

  device_power_rating_code = _read_register(data, 8, False)
  if device_power_rating_code is not None:
    device_power_rating_code = int(device_power_rating_code)
    metrics["device_power_rating_code"] = device_power_rating_code
    device_power_rating_w = MAP_DEVICE_POWER_RATING_WATTS.get(device_power_rating_code)
    if device_power_rating_w is not None:
      metrics["device_power_rating_w"] = device_power_rating_w

  bat_volt = rr("BATTERY_VOLTAGE", False, 0.01)
  if bat_volt is not None:
    metrics["battery_voltage"] = bat_volt
  bat_curr = rr("BATTERY_CURRENT", True, 0.01)
  if bat_curr is not None:
    metrics["battery_current"] = -bat_curr
  ac_out_v = rr("AC_OUT_VOLTAGE", False, 0.1)
  if ac_out_v is not None:
    metrics["ac_output_voltage"] = ac_out_v
  grid_v = rr("GRID_VOLTAGE", False, 0.1)
  if grid_v is not None:
    metrics["grid_voltage"] = grid_v
    metrics["ac_input_voltage"] = grid_v
  ac_out_f = rr("AC_OUT_FREQ", False, 0.01)
  if ac_out_f is not None:
    metrics["ac_output_frequency"] = ac_out_f
  ac_in_f = rr("AC_IN_FREQ", False, 0.01)
  if ac_in_f is not None:
    metrics["ac_input_frequency"] = ac_in_f
  temp_raw = rr("DEVICE_TEMP", True)
  if temp_raw is not None:
    temp_c = round((temp_raw - 1000) / 10, 1)
    if -40 < temp_c < 150:
      metrics["device_temperature"] = temp_c

  for source, key, signed in [
    ("PV1_VOLTAGE", "pv1_voltage", False),
    ("PV2_VOLTAGE", "pv2_voltage", False),
    ("GRID_POWER", "grid_power", True),
    ("AC_IN_POWER", "ac_input_power", True),
    ("LOAD_POWER", "load_power", False),
    ("AC_OUT_POWER", "ac_output_power", False),
    ("AC_OUT_VA", "ac_output_va", False),
  ]:
    value = rr(source, signed)
    if value is not None:
      metrics[key] = value

  battery_power_raw = rr("BATTERY_POWER", True)
  if battery_power_raw is not None:
    metrics["battery_power"] = -battery_power_raw
    metrics["battery_status"] = "Charging" if metrics["battery_power"] > 0 else "Discharging"
  else:
    metrics["battery_status"] = "Unknown"

  grid_power = metrics.get("grid_power")
  metrics["grid_status"] = (
    "Importing" if grid_power is not None and grid_power > 0
    else "Exporting" if grid_power is not None
    else "Unknown"
  )

  pv1 = rr("PV1_POWER", False)
  pv2 = rr("PV2_POWER", False)
  if pv1 is not None:
    metrics["pv1_power"] = pv1
  if pv2 is not None:
    metrics["pv2_power"] = pv2
  if pv1 is not None or pv2 is not None:
    metrics["pv_power"] = (pv1 or 0) + (pv2 or 0)

  soc = rr("BATTERY_SOC", False)
  if soc is not None:
    metrics["battery_soc"] = max(0, min(100, int(soc)))
  ups = rr("UPS_MODE", False)
  if ups is not None:
    metrics["is_ups_mode"] = ups == 0
  battery_type = rr("BATTERY_TYPE", False)
  if battery_type is not None:
    battery_type = int(battery_type)
    metrics["battery_type"] = MAP_BATTERY_TYPE.get(battery_type, "Unknown")
    metrics["battery_connected"] = battery_type != 2
  master_slave = rr("MASTER_SLAVE_STATUS", False)
  if master_slave is not None:
    metrics["master_slave_status"] = master_slave
  device_sn = _read_string(data, REG_ADDR["DEVICE_MODEL_START"], 5)
  if device_sn is not None:
    metrics["mqtt_device_sn"] = device_sn

  return {"data_type": "main", "metrics": metrics, "diagnostics": diagnostics}


def _parse_function_04_statistics(payload_hex: str, response_hex: str, data: bytes, start_register: int) -> dict[str, Any]:
  """Parse function 0x04 (Read Input Registers) - today's energy statistics."""
  if start_register != 0 or len(data) != 16:
    return None

  diagnostics = _register_diagnostics(payload_hex, response_hex, data, start_register)
  metrics = {}

  # All values are in 0.1 kWh units, divide by 10 to get kWh
  pv_gen_raw = _read_register(data, 0, False)
  if pv_gen_raw is not None:
    metrics["today_pv_generation_kwh"] = round(pv_gen_raw / 10.0, 1)

  essential_load_raw = _read_register(data, 1, False)
  if essential_load_raw is not None:
    metrics["today_essential_load_kwh"] = round(essential_load_raw / 10.0, 1)

  grid_import_raw = _read_register(data, 2, False)
  if grid_import_raw is not None:
    metrics["today_grid_import_kwh"] = round(grid_import_raw / 10.0, 1)

  total_load_raw = _read_register(data, 3, False)
  if total_load_raw is not None:
    metrics["today_total_load_kwh"] = round(total_load_raw / 10.0, 1)

  battery_charge_raw = _read_register(data, 4, False)
  if battery_charge_raw is not None:
    metrics["today_battery_charge_kwh"] = round(battery_charge_raw / 10.0, 1)

  battery_discharge_raw = _read_register(data, 5, False)
  if battery_discharge_raw is not None:
    metrics["today_battery_discharge_kwh"] = round(battery_discharge_raw / 10.0, 1)

  return {"data_type": "statistics", "metrics": metrics, "diagnostics": diagnostics}
