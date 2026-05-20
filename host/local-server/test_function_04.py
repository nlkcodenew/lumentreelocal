#!/usr/bin/env python3
"""Test function 0x04 (Read Input Registers) - today's energy statistics.

This test validates the decoder logic with mock data before touching the real inverter.
Function 0x04 is read-only and safe, but we test with mock data first.
"""

import sys
import struct
from lumentree_decode import parse_payload


def calc_crc(data_hex: str) -> str:
    data = bytes.fromhex(data_hex)
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return struct.pack('<H', crc).hex()


def build_frame(data_hex: str) -> str:
    """Build a function 0x04 response frame with correct CRC.
    data_hex: exactly 32 hex chars (16 bytes = 8 registers)
    """
    assert len(data_hex) == 32, f"data_hex must be 32 chars, got {len(data_hex)}"
    byte_count = "10"  # 16 bytes
    header = "0104" + byte_count + data_hex
    crc = calc_crc(header)
    return header + crc


def test_realistic_values():
    """Test with realistic daily energy values."""
    print("\n=== Test 1: Realistic daily values ===")

    # 8 registers, each 2 bytes:
    # Reg 0: PV generation 12.5 kWh -> 125 = 0x007D
    # Reg 1: Essential load 8.9 kWh -> 89 = 0x0059
    # Reg 2: Grid import 3.2 kWh -> 32 = 0x0020
    # Reg 3: Total load 11.5 kWh -> 115 = 0x0073
    # Reg 4: Battery charge 7.8 kWh -> 78 = 0x004E
    # Reg 5: Battery discharge 6.4 kWh -> 64 = 0x0040
    # Reg 6-7: Reserved = 0
    data_hex = "007d005900200073004e004000000000"
    payload_hex = build_frame(data_hex)
    print(f"Frame: {payload_hex}")

    result = parse_payload(payload_hex, 0)
    print(f"Result: {result}")

    assert result is not None, "Parser returned None"
    assert result["data_type"] == "statistics", f"Wrong data_type: {result['data_type']}"

    metrics = result["metrics"]
    assert metrics["today_pv_generation_kwh"] == 12.5, f"PV gen: {metrics['today_pv_generation_kwh']}"
    assert metrics["today_essential_load_kwh"] == 8.9, f"Essential load: {metrics['today_essential_load_kwh']}"
    assert metrics["today_grid_import_kwh"] == 3.2, f"Grid import: {metrics['today_grid_import_kwh']}"
    assert metrics["today_total_load_kwh"] == 11.5, f"Total load: {metrics['today_total_load_kwh']}"
    assert metrics["today_battery_charge_kwh"] == 7.8, f"Battery charge: {metrics['today_battery_charge_kwh']}"
    assert metrics["today_battery_discharge_kwh"] == 6.4, f"Battery discharge: {metrics['today_battery_discharge_kwh']}"

    print("✓ All metrics match expected values")


def test_zero_values():
    """Test with zero values (night time scenario)."""
    print("\n=== Test 2: Zero values (night time) ===")

    data_hex = "00000000000000000000000000000000"
    payload_hex = build_frame(data_hex)
    print(f"Frame: {payload_hex}")

    result = parse_payload(payload_hex, 0)
    print(f"Result: {result}")

    assert result is not None, "Parser returned None"
    assert result["data_type"] == "statistics"

    metrics = result["metrics"]
    assert metrics["today_pv_generation_kwh"] == 0.0
    assert metrics["today_essential_load_kwh"] == 0.0
    assert metrics["today_grid_import_kwh"] == 0.0
    assert metrics["today_total_load_kwh"] == 0.0
    assert metrics["today_battery_charge_kwh"] == 0.0
    assert metrics["today_battery_discharge_kwh"] == 0.0

    print("✓ All zero values handled correctly")


def test_function_03_regression():
    """Ensure function 0x03 still works (regression test)."""
    print("\n=== Test 3: Function 0x03 regression ===")

    # 95 registers = 190 bytes = 0xBE byte count
    response_no_crc = "0103be" + "0000" * 95
    crc = calc_crc(response_no_crc)
    payload_hex = response_no_crc + crc

    result = parse_payload(payload_hex, 0)
    print(f"Result type: {result['data_type'] if result else 'None'}")

    assert result is not None, "Function 0x03 parser broken"
    assert result["data_type"] != "statistics", "Function 0x03 incorrectly parsed as statistics"

    print("✓ Function 0x03 still works correctly")


if __name__ == "__main__":
    print("Testing function 0x04 decoder with mock data...")
    print("This is safe - no communication with real inverter.")

    try:
        test_realistic_values()
        test_zero_values()
        test_function_03_regression()

        print("\n" + "="*50)
        print("✓ ALL TESTS PASSED")
        print("Decoder logic is working correctly with mock data.")
        print("="*50)
        sys.exit(0)

    except AssertionError as e:
        print(f"\n✗ TEST FAILED: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\n✗ UNEXPECTED ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
