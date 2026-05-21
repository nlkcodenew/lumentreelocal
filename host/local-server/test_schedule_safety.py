#!/usr/bin/env python3
"""Focused tests for schedule safety validation."""

from server import apply_schedule_change, schedule_state_from_settings, validate_schedule_conflicts, windows_overlap


def build_settings() -> dict[str, int | bool]:
  return {
    "mains_charge_slot_1_enabled": True,
    "mains_charge_slot_1_start_time": 800,
    "mains_charge_slot_1_end_time": 1000,
    "mains_charge_slot_2_enabled": False,
    "mains_charge_slot_2_start_time": 1200,
    "mains_charge_slot_2_end_time": 1400,
    "discharge_slot_1_enabled": False,
    "discharge_slot_1_start_time": 1000,
    "discharge_slot_1_end_time": 1200,
    "discharge_slot_2_enabled": False,
    "discharge_slot_2_start_time": 1300,
    "discharge_slot_2_end_time": 1500,
    "discharge_slot_3_enabled": False,
    "discharge_slot_3_start_time": 2300,
    "discharge_slot_3_end_time": 200,
    "discharge_slot_4_enabled": False,
    "discharge_slot_4_start_time": 400,
    "discharge_slot_4_end_time": 600,
  }


def main() -> None:
  assert windows_overlap(800, 1000, 1000, 1200) is False
  assert windows_overlap(100, 300, 2300, 200) is True
  assert windows_overlap(2300, 200, 100, 300) is True

  state = schedule_state_from_settings(build_settings())
  next_state = apply_schedule_change(state, "discharge", 1, "enabled", True)
  assert validate_schedule_conflicts(next_state) == []

  next_state = apply_schedule_change(next_state, "discharge", 1, "start", 900)
  conflicts = validate_schedule_conflicts(next_state)
  assert len(conflicts) == 1
  assert conflicts[0]["charge_slot"] == 1
  assert conflicts[0]["discharge_slot"] == 1

  overnight_settings = build_settings()
  overnight_settings["mains_charge_slot_1_start_time"] = 100
  overnight_settings["mains_charge_slot_1_end_time"] = 300
  overnight_settings["discharge_slot_3_enabled"] = True
  overnight_state = schedule_state_from_settings(overnight_settings)
  conflicts = validate_schedule_conflicts(overnight_state)
  assert len(conflicts) == 1
  assert conflicts[0]["charge_slot"] == 1
  assert conflicts[0]["discharge_slot"] == 3

  print("host.local-server.schedule_safety tests passed")


if __name__ == "__main__":
  main()
