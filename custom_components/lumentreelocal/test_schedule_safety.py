"""Focused tests for shared schedule safety helpers."""

from schedule_safety import apply_schedule_change, schedule_state_from_settings, validate_schedule_conflicts, windows_overlap


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


def test_touching_boundaries_are_allowed() -> None:
    assert windows_overlap(800, 1000, 1000, 1200) is False


def test_overnight_overlap_is_detected() -> None:
    assert windows_overlap(100, 300, 2300, 200) is True
    assert windows_overlap(2300, 200, 100, 300) is True


def test_conflict_is_detected_after_time_change() -> None:
    state = schedule_state_from_settings(build_settings())
    next_state = apply_schedule_change(state, "discharge", 1, "enabled", True)
    conflicts = validate_schedule_conflicts(next_state)
    assert len(conflicts) == 0

    next_state = apply_schedule_change(next_state, "discharge", 1, "start", 900)
    conflicts = validate_schedule_conflicts(next_state)
    assert len(conflicts) == 1
    assert conflicts[0]["charge_slot"] == 1
    assert conflicts[0]["discharge_slot"] == 1


def test_overnight_conflict_is_detected_from_settings_snapshot() -> None:
    settings = build_settings()
    settings["mains_charge_slot_1_start_time"] = 100
    settings["mains_charge_slot_1_end_time"] = 300
    settings["discharge_slot_3_enabled"] = True
    state = schedule_state_from_settings(settings)
    conflicts = validate_schedule_conflicts(state)
    assert len(conflicts) == 1
    assert conflicts[0]["charge_slot"] == 1
    assert conflicts[0]["discharge_slot"] == 3


if __name__ == "__main__":
    test_touching_boundaries_are_allowed()
    test_overnight_overlap_is_detected()
    test_conflict_is_detected_after_time_change()
    test_overnight_conflict_is_detected_from_settings_snapshot()
    print("custom_components.lumentreelocal.schedule_safety tests passed")
