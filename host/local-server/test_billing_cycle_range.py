#!/usr/bin/env python3
"""Sanity checks for billing-cycle date windows."""

from __future__ import annotations

from datetime import datetime

from server import LOCAL_TIMEZONE, local_billing_cycle_range


def main() -> None:
  dt = datetime(2026, 5, 23, 9, 0, tzinfo=LOCAL_TIMEZONE)
  start, end = local_billing_cycle_range(dt)
  assert start.isoformat() == "2026-05-22"
  assert end.isoformat() == "2026-06-22"

  dt = datetime(2026, 5, 10, 9, 0, tzinfo=LOCAL_TIMEZONE)
  start, end = local_billing_cycle_range(dt)
  assert start.isoformat() == "2026-04-22"
  assert end.isoformat() == "2026-05-22"

  dt = datetime(2026, 1, 5, 9, 0, tzinfo=LOCAL_TIMEZONE)
  start, end = local_billing_cycle_range(dt)
  assert start.isoformat() == "2025-12-22"
  assert end.isoformat() == "2026-01-22"

  print("ok")


if __name__ == "__main__":
  main()
