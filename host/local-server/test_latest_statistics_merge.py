#!/usr/bin/env python3
"""Tests for merging main telemetry with function 0x04 statistics metrics."""

from server import merge_latest_telemetry


def test_merge_latest_telemetry_combines_main_and_statistics_metrics():
    main_row = {
        "id": 10,
        "observed_at": "2026-05-20T08:09:22Z",
        "metrics": {
            "pv_power": 1847.0,
            "battery_soc": 57,
        },
        "raw": {"decode": {"data_type": "main"}},
    }
    statistics_row = {
        "id": 11,
        "observed_at": "2026-05-20T08:09:20Z",
        "metrics": {
            "today_pv_generation_kwh": 9.8,
            "today_grid_import_kwh": 3.0,
        },
        "raw": {"decode": {"data_type": "statistics"}},
    }

    merged = merge_latest_telemetry(main_row, statistics_row)

    assert merged["metrics"]["pv_power"] == 1847.0
    assert merged["metrics"]["battery_soc"] == 57
    assert merged["metrics"]["today_pv_generation_kwh"] == 9.8
    assert merged["metrics"]["today_grid_import_kwh"] == 3.0
    assert merged["raw"]["statistics"]["id"] == 11
    assert merged["raw"]["statistics"]["observed_at"] == "2026-05-20T08:09:20Z"


def test_merge_latest_telemetry_falls_back_when_main_missing():
    statistics_row = {
        "id": 11,
        "observed_at": "2026-05-20T08:09:20Z",
        "metrics": {"today_pv_generation_kwh": 9.8},
        "raw": {"decode": {"data_type": "statistics"}},
    }

    assert merge_latest_telemetry(None, statistics_row) == statistics_row
