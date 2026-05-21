"""Shared schedule safety helpers for Lumentree Local."""

from __future__ import annotations

from copy import deepcopy
from typing import Any, Mapping


MAINS_CHARGE_SLOTS = (1, 2)
DISCHARGE_SLOTS = (1, 2, 3, 4)


def hhmm_to_minutes(value: int) -> int:
    """Convert an HHMM integer to minutes since midnight."""
    return (value // 100) * 60 + (value % 100)


def expand_window(start_hhmm: int, end_hhmm: int) -> tuple[int, int]:
    """Expand an HHMM window, normalizing overnight windows."""
    start = hhmm_to_minutes(start_hhmm)
    end = hhmm_to_minutes(end_hhmm)
    if end <= start:
        end += 1440
    return (start, end)


def windows_overlap(a_start_hhmm: int, a_end_hhmm: int, b_start_hhmm: int, b_end_hhmm: int) -> bool:
    """Return whether two HHMM windows overlap with nonzero duration."""
    a_start, a_end = expand_window(a_start_hhmm, a_end_hhmm)
    b_start, b_end = expand_window(b_start_hhmm, b_end_hhmm)
    candidates = (
        (b_start, b_end),
        (b_start + 1440, b_end + 1440),
        (b_start - 1440, b_end - 1440),
    )
    return any(candidate_start < a_end and a_start < candidate_end for candidate_start, candidate_end in candidates)


def format_hhmm(value: int) -> str:
    """Return an HHMM integer as HH:MM."""
    return f"{value // 100:02d}:{value % 100:02d}"


def schedule_state_from_settings(settings: Mapping[str, Any]) -> dict[str, dict[int, dict[str, Any]]]:
    """Build normalized schedule state from a settings snapshot."""
    state: dict[str, dict[int, dict[str, Any]]] = {
        "mains_charge": {},
        "discharge": {},
    }
    for slot in MAINS_CHARGE_SLOTS:
        state["mains_charge"][slot] = {
            "enabled": settings.get(f"mains_charge_slot_{slot}_enabled"),
            "start": settings.get(f"mains_charge_slot_{slot}_start_time"),
            "end": settings.get(f"mains_charge_slot_{slot}_end_time"),
        }
    for slot in DISCHARGE_SLOTS:
        state["discharge"][slot] = {
            "enabled": settings.get(f"discharge_slot_{slot}_enabled"),
            "start": settings.get(f"discharge_slot_{slot}_start_time"),
            "end": settings.get(f"discharge_slot_{slot}_end_time"),
        }
    return state


def slot_enabled(state: Mapping[str, dict[int, dict[str, Any]]], group: str, slot: int) -> bool:
    """Return whether one slot is enabled."""
    return state.get(group, {}).get(slot, {}).get("enabled") is True


def apply_schedule_change(
    state: Mapping[str, dict[int, dict[str, Any]]],
    group: str,
    slot: int,
    field: str,
    value: Any,
) -> dict[str, dict[int, dict[str, Any]]]:
    """Return a copy of schedule state with one field changed."""
    next_state = deepcopy(state)
    next_state[group][slot][field] = value
    return next_state


def validate_schedule_conflicts(state: Mapping[str, dict[int, dict[str, Any]]]) -> list[dict[str, Any]]:
    """Return all overlapping enabled charge/discharge slot conflicts."""
    conflicts: list[dict[str, Any]] = []
    for charge_slot in MAINS_CHARGE_SLOTS:
        charge = state["mains_charge"][charge_slot]
        if charge.get("enabled") is not True:
            continue
        charge_start = charge.get("start")
        charge_end = charge.get("end")
        if not isinstance(charge_start, int) or not isinstance(charge_end, int):
            continue
        for discharge_slot in DISCHARGE_SLOTS:
            discharge = state["discharge"][discharge_slot]
            if discharge.get("enabled") is not True:
                continue
            discharge_start = discharge.get("start")
            discharge_end = discharge.get("end")
            if not isinstance(discharge_start, int) or not isinstance(discharge_end, int):
                continue
            if windows_overlap(charge_start, charge_end, discharge_start, discharge_end):
                conflicts.append(
                    {
                        "charge_slot": charge_slot,
                        "charge_start": charge_start,
                        "charge_end": charge_end,
                        "discharge_slot": discharge_slot,
                        "discharge_start": discharge_start,
                        "discharge_end": discharge_end,
                    }
                )
    return conflicts


def describe_conflict(conflict: Mapping[str, Any], perspective: str) -> str:
    """Render one conflict for a user-facing error message."""
    charge_window = f"{format_hhmm(int(conflict['charge_start']))}-{format_hhmm(int(conflict['charge_end']))}"
    discharge_window = f"{format_hhmm(int(conflict['discharge_start']))}-{format_hhmm(int(conflict['discharge_end']))}"
    if perspective == "mains_charge":
        return f"discharge slot {conflict['discharge_slot']} ({discharge_window})"
    if perspective == "discharge":
        return f"mains charge slot {conflict['charge_slot']} ({charge_window})"
    return (
        f"mains charge slot {conflict['charge_slot']} ({charge_window}) "
        f"and discharge slot {conflict['discharge_slot']} ({discharge_window})"
    )
