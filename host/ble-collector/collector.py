#!/usr/bin/env python3
"""Read Lumentree BLE bridge JSONL from serial and optionally store it."""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from contextlib import nullcontext
from pathlib import Path
from datetime import datetime, timezone
from typing import Any, TextIO

import serial


CREATE_TABLE_SQL = """
CREATE TABLE IF NOT EXISTS lumentree_ble_events (
  id BIGSERIAL PRIMARY KEY,
  observed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  event_type TEXT NOT NULL,
  mac TEXT,
  payload JSONB NOT NULL
);
"""

INSERT_SQL = """
INSERT INTO lumentree_ble_events (observed_at, event_type, mac, payload)
VALUES (%s, %s, %s, %s)
"""


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--port", default="/dev/ttyACM0", help="serial port")
  parser.add_argument("--baud", type=int, default=115200, help="serial baud rate")
  parser.add_argument("--postgres-dsn", default=os.getenv("LUMENTREE_BLE_POSTGRES_DSN", ""))
  parser.add_argument("--init-db", action="store_true", help="create Postgres table before reading")
  parser.add_argument("--jsonl-out", default="", help="append raw serial lines to this JSONL/log file")
  parser.add_argument("--duration", type=float, default=0, help="stop after this many seconds; 0 means run forever")
  parser.add_argument("--send", action="append", default=[], help="command to send after opening serial")
  parser.add_argument("--scan-once", action="store_true", help="send SCAN_ONCE after opening serial")
  parser.add_argument("--read-main-once", action="store_true", help="send READ_MAIN_ONCE after opening serial")
  parser.add_argument("--read-cells-once", action="store_true", help="send READ_CELLS_ONCE after opening serial")
  parser.add_argument("--read-main-interval", type=float, default=0, help="send READ_MAIN_ONCE every N seconds")
  parser.add_argument("--read-cells-interval", type=float, default=0, help="send READ_CELLS_ONCE every N seconds")
  parser.add_argument("--api-url", default=os.getenv("LUMENTREE_API_URL", ""), help="local server base URL")
  parser.add_argument("--api-token", default=os.getenv("LUMENTREE_API_TOKEN", ""), help="local server bearer token")
  parser.add_argument("--device-id", default=os.getenv("LUMENTREE_DEVICE_ID", ""), help="stable inverter/device id")
  parser.add_argument("--gateway-id", default=os.getenv("LUMENTREE_GATEWAY_ID", "usb-collector"), help="collector/gateway id")
  return parser.parse_args()


def connect_postgres(dsn: str, init_db: bool):
  if not dsn:
    return None
  try:
    import psycopg
    from psycopg.types.json import Jsonb
  except ImportError as exc:
    raise SystemExit("Install Postgres support with: pip install -r requirements.txt") from exc

  conn = psycopg.connect(dsn)
  if init_db:
    with conn.cursor() as cur:
      cur.execute(CREATE_TABLE_SQL)
    conn.commit()
  return conn, Jsonb


def store_event(pg_bundle: Any, event: dict[str, Any]) -> None:
  if pg_bundle is None:
    return
  conn, jsonb_cls = pg_bundle
  observed_at = datetime.now(timezone.utc)
  event_type = str(event.get("type") or "unknown")
  mac = event.get("mac")
  with conn.cursor() as cur:
    cur.execute(INSERT_SQL, (observed_at, event_type, mac, jsonb_cls(event)))
  conn.commit()


def post_event(api_url: str, api_token: str, device_id: str, gateway_id: str, event: dict[str, Any]) -> None:
  if not api_url:
    return
  endpoint = api_url.rstrip("/") + "/api/lumentree/events"
  payload = {
    "device_id": device_id or event.get("name") or event.get("mac") or event.get("target_mac"),
    "mac": event.get("mac") or event.get("target_mac"),
    "gateway_id": gateway_id,
    "firmware": event.get("fw"),
    "uptime_ms": event.get("uptime_ms"),
    "raw": {
      "transport": "ble-serial",
      "payload_hex": event.get("response_hex") or event.get("payload_hex"),
      "event": event,
    },
    "metrics": {},
  }
  data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
  headers = {
    "Content-Type": "application/json",
    "User-Agent": "LumentreeBLECollector/0.1",
  }
  if api_token:
    headers["Authorization"] = "Bearer " + api_token
  request = urllib.request.Request(endpoint, data=data, headers=headers, method="POST")
  try:
    with urllib.request.urlopen(request, timeout=5) as response:
      response.read()
  except urllib.error.URLError as exc:
    print(f"api_post_failed: {exc}", file=sys.stderr)


def open_jsonl(path: str) -> TextIO | None:
  if not path:
    return None
  output_path = Path(path)
  output_path.parent.mkdir(parents=True, exist_ok=True)
  return output_path.open("a", encoding="utf-8")


def main() -> int:
  args = parse_args()
  pg_bundle = connect_postgres(args.postgres_dsn, args.init_db)
  deadline = time.monotonic() + args.duration if args.duration > 0 else None

  jsonl_context = open_jsonl(args.jsonl_out) if args.jsonl_out else nullcontext(None)

  with serial.Serial(args.port, args.baud, timeout=1) as ser, jsonl_context as jsonl_file:
    time.sleep(2)
    for command in args.send:
      ser.write((command.strip() + "\n").encode("utf-8"))
    if args.scan_once:
      ser.write(b"SCAN_ONCE\n")
    if args.read_main_once:
      ser.write(b"READ_MAIN_ONCE\n")
    if args.read_cells_once:
      ser.write(b"READ_CELLS_ONCE\n")

    next_main_read = (
      time.monotonic() + args.read_main_interval
      if args.read_main_interval > 0
      else None
    )
    next_cells_read = (
      time.monotonic() + args.read_cells_interval
      if args.read_cells_interval > 0
      else None
    )

    while deadline is None or time.monotonic() < deadline:
      now = time.monotonic()
      if next_main_read is not None and now >= next_main_read:
        ser.write(b"READ_MAIN_ONCE\n")
        next_main_read = now + args.read_main_interval
      if next_cells_read is not None and now >= next_cells_read:
        ser.write(b"READ_CELLS_ONCE\n")
        next_cells_read = now + args.read_cells_interval

      raw = ser.readline()
      if not raw:
        continue
      line = raw.decode("utf-8", errors="replace").strip()
      if not line:
        continue
      print(line, flush=True)
      if jsonl_file is not None:
        jsonl_file.write(line + "\n")
        jsonl_file.flush()
      if not line.startswith("{"):
        continue
      try:
        event = json.loads(line)
      except json.JSONDecodeError:
        continue
      store_event(pg_bundle, event)
      post_event(args.api_url, args.api_token, args.device_id, args.gateway_id, event)


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except KeyboardInterrupt:
    print("\ncollector stopped", file=sys.stderr)
    raise SystemExit(130)
