#!/usr/bin/env python3
"""Local Lumentree vendor-server replacement."""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import secrets
import sys
from datetime import date, datetime, timedelta, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import parse_qs, unquote, urlparse
from zoneinfo import ZoneInfo

from lumentree_decode import parse_payload

try:
  import psycopg
  from psycopg.rows import dict_row
  from psycopg.types.json import Jsonb
except ModuleNotFoundError:
  psycopg = None
  dict_row = None
  Jsonb = None


SCHEMA_SQL = """
CREATE TABLE IF NOT EXISTS lumentree_devices (
  device_id TEXT PRIMARY KEY,
  mac TEXT,
  gateway_id TEXT,
  first_seen_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  last_seen_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  metadata JSONB NOT NULL DEFAULT '{}'::jsonb
);

CREATE TABLE IF NOT EXISTS lumentree_telemetry (
  id BIGSERIAL PRIMARY KEY,
  device_id TEXT NOT NULL REFERENCES lumentree_devices(device_id) ON DELETE CASCADE,
  mac TEXT,
  gateway_id TEXT,
  observed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  firmware TEXT,
  uptime_ms BIGINT,
  metrics JSONB NOT NULL DEFAULT '{}'::jsonb,
  raw JSONB NOT NULL DEFAULT '{}'::jsonb,
  payload JSONB NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_lumentree_telemetry_device_observed
  ON lumentree_telemetry(device_id, observed_at DESC);

CREATE TABLE IF NOT EXISTS lumentree_energy_daily (
  device_id TEXT NOT NULL REFERENCES lumentree_devices(device_id) ON DELETE CASCADE,
  day DATE NOT NULL,
  pv_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  load_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  grid_in_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  grid_out_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  battery_charge_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  battery_discharge_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  ac_input_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  ac_output_kwh DOUBLE PRECISION NOT NULL DEFAULT 0,
  sample_count INTEGER NOT NULL DEFAULT 0,
  covered_seconds DOUBLE PRECISION NOT NULL DEFAULT 0,
  first_observed_at TIMESTAMPTZ,
  last_observed_at TIMESTAMPTZ,
  updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  PRIMARY KEY (device_id, day)
);

CREATE TABLE IF NOT EXISTS lumentree_gateway_status (
  gateway_id TEXT PRIMARY KEY,
  device_id TEXT,
  firmware TEXT,
  target_mac TEXT,
  pairing_status TEXT,
  wifi_connected BOOLEAN,
  wifi_rssi INTEGER,
  ip TEXT,
  uptime_ms BIGINT,
  candidates JSONB NOT NULL DEFAULT '[]'::jsonb,
  payload JSONB NOT NULL DEFAULT '{}'::jsonb,
  updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_lumentree_gateway_status_device
  ON lumentree_gateway_status(device_id);

CREATE TABLE IF NOT EXISTS lumentree_gateway_candidates (
  id BIGSERIAL PRIMARY KEY,
  gateway_id TEXT NOT NULL,
  device_id TEXT NOT NULL,
  mac TEXT NOT NULL,
  address_type INTEGER,
  name TEXT,
  service_uuid TEXT,
  characteristic_uuid TEXT,
  rssi INTEGER,
  payload JSONB NOT NULL DEFAULT '{}'::jsonb,
  first_seen_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  last_seen_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  active BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE INDEX IF NOT EXISTS idx_lumentree_gateway_candidates_lookup
  ON lumentree_gateway_candidates(gateway_id, device_id, mac, active, last_seen_at DESC);

CREATE TABLE IF NOT EXISTS lumentree_commands (
  id BIGSERIAL PRIMARY KEY,
  device_id TEXT NOT NULL,
  gateway_id TEXT,
  command TEXT NOT NULL,
  mode TEXT NOT NULL DEFAULT 'dry_run',
  status TEXT NOT NULL DEFAULT 'requested',
  requested_by TEXT,
  payload JSONB NOT NULL DEFAULT '{}'::jsonb,
  result JSONB NOT NULL DEFAULT '{}'::jsonb,
  error TEXT,
  requested_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  claimed_at TIMESTAMPTZ,
  completed_at TIMESTAMPTZ,
  updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_lumentree_commands_pending
  ON lumentree_commands(device_id, status, requested_at);

CREATE INDEX IF NOT EXISTS idx_lumentree_commands_gateway
  ON lumentree_commands(gateway_id, status, requested_at);

CREATE TABLE IF NOT EXISTS lumentree_write_pairing_codes (
  id BIGSERIAL PRIMARY KEY,
  gateway_id TEXT NOT NULL,
  device_id TEXT NOT NULL,
  code_hash TEXT NOT NULL,
  expires_at TIMESTAMPTZ NOT NULL,
  consumed_at TIMESTAMPTZ,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  firmware TEXT,
  payload JSONB NOT NULL DEFAULT '{}'::jsonb
);

CREATE INDEX IF NOT EXISTS idx_lumentree_write_pairing_codes_active
  ON lumentree_write_pairing_codes(device_id, expires_at DESC)
  WHERE consumed_at IS NULL;

CREATE TABLE IF NOT EXISTS lumentree_write_grants (
  id BIGSERIAL PRIMARY KEY,
  gateway_id TEXT NOT NULL,
  device_id TEXT NOT NULL,
  grant_token_hash TEXT NOT NULL UNIQUE,
  scope TEXT NOT NULL DEFAULT 'dry_run',
  enabled BOOLEAN NOT NULL DEFAULT TRUE,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  revoked_at TIMESTAMPTZ,
  last_used_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_lumentree_write_grants_device
  ON lumentree_write_grants(device_id, enabled, revoked_at);

CREATE TABLE IF NOT EXISTS lumentree_write_audit (
  id BIGSERIAL PRIMARY KEY,
  device_id TEXT,
  gateway_id TEXT,
  event TEXT NOT NULL,
  detail JSONB NOT NULL DEFAULT '{}'::jsonb,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS lumentree_auth_audit (
  id BIGSERIAL PRIMARY KEY,
  device_id TEXT,
  gateway_id TEXT,
  mac TEXT,
  event TEXT NOT NULL,
  detail JSONB NOT NULL DEFAULT '{}'::jsonb,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS lumentree_read_pairing_tokens (
  id BIGSERIAL PRIMARY KEY,
  gateway_id TEXT NOT NULL,
  device_id TEXT NOT NULL,
  mac TEXT NOT NULL,
  token_hash TEXT NOT NULL,
  expires_at TIMESTAMPTZ NOT NULL,
  consumed_at TIMESTAMPTZ,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  firmware TEXT,
  payload JSONB NOT NULL DEFAULT '{}'::jsonb
);

CREATE INDEX IF NOT EXISTS idx_lumentree_read_pairing_tokens_active
  ON lumentree_read_pairing_tokens(device_id, mac, expires_at DESC)
  WHERE consumed_at IS NULL;

CREATE TABLE IF NOT EXISTS lumentree_read_grants (
  id BIGSERIAL PRIMARY KEY,
  gateway_id TEXT NOT NULL,
  device_id TEXT NOT NULL,
  mac TEXT NOT NULL,
  grant_token_hash TEXT NOT NULL UNIQUE,
  enabled BOOLEAN NOT NULL DEFAULT TRUE,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  revoked_at TIMESTAMPTZ,
  last_used_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_lumentree_read_grants_device
  ON lumentree_read_grants(device_id, mac, enabled, revoked_at);
"""


UPSERT_DEVICE_SQL = """
INSERT INTO lumentree_devices (device_id, mac, gateway_id, first_seen_at, last_seen_at, metadata)
VALUES (%s, %s, %s, %s, %s, %s)
ON CONFLICT (device_id) DO UPDATE SET
  mac = COALESCE(EXCLUDED.mac, lumentree_devices.mac),
  gateway_id = COALESCE(EXCLUDED.gateway_id, lumentree_devices.gateway_id),
  last_seen_at = GREATEST(lumentree_devices.last_seen_at, EXCLUDED.last_seen_at),
  metadata = lumentree_devices.metadata || EXCLUDED.metadata
"""


INSERT_TELEMETRY_SQL = """
INSERT INTO lumentree_telemetry
  (device_id, mac, gateway_id, observed_at, firmware, uptime_ms, metrics, raw, payload)
VALUES
  (%s, %s, %s, %s, %s, %s, %s, %s, %s)
RETURNING id
"""


UPSERT_ENERGY_DAILY_SQL = """
INSERT INTO lumentree_energy_daily
  (
    device_id, day, pv_kwh, load_kwh, grid_in_kwh, grid_out_kwh,
    battery_charge_kwh, battery_discharge_kwh, ac_input_kwh, ac_output_kwh,
    sample_count, covered_seconds, first_observed_at, last_observed_at,
    updated_at
  )
VALUES
  (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, 1, %s, %s, %s, NOW())
ON CONFLICT (device_id, day) DO UPDATE SET
  pv_kwh = lumentree_energy_daily.pv_kwh + EXCLUDED.pv_kwh,
  load_kwh = lumentree_energy_daily.load_kwh + EXCLUDED.load_kwh,
  grid_in_kwh = lumentree_energy_daily.grid_in_kwh + EXCLUDED.grid_in_kwh,
  grid_out_kwh = lumentree_energy_daily.grid_out_kwh + EXCLUDED.grid_out_kwh,
  battery_charge_kwh = lumentree_energy_daily.battery_charge_kwh + EXCLUDED.battery_charge_kwh,
  battery_discharge_kwh = lumentree_energy_daily.battery_discharge_kwh + EXCLUDED.battery_discharge_kwh,
  ac_input_kwh = lumentree_energy_daily.ac_input_kwh + EXCLUDED.ac_input_kwh,
  ac_output_kwh = lumentree_energy_daily.ac_output_kwh + EXCLUDED.ac_output_kwh,
  sample_count = lumentree_energy_daily.sample_count + 1,
  covered_seconds = lumentree_energy_daily.covered_seconds + EXCLUDED.covered_seconds,
  first_observed_at = LEAST(lumentree_energy_daily.first_observed_at, EXCLUDED.first_observed_at),
  last_observed_at = GREATEST(lumentree_energy_daily.last_observed_at, EXCLUDED.last_observed_at),
  updated_at = NOW()
"""


UPSERT_GATEWAY_STATUS_SQL = """
INSERT INTO lumentree_gateway_status
  (
    gateway_id, device_id, firmware, target_mac, pairing_status,
    wifi_connected, wifi_rssi, ip, uptime_ms, candidates, payload, updated_at
  )
VALUES
  (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, NOW())
ON CONFLICT (gateway_id) DO UPDATE SET
  device_id = COALESCE(EXCLUDED.device_id, lumentree_gateway_status.device_id),
  firmware = COALESCE(EXCLUDED.firmware, lumentree_gateway_status.firmware),
  target_mac = EXCLUDED.target_mac,
  pairing_status = COALESCE(EXCLUDED.pairing_status, lumentree_gateway_status.pairing_status),
  wifi_connected = EXCLUDED.wifi_connected,
  wifi_rssi = EXCLUDED.wifi_rssi,
  ip = EXCLUDED.ip,
  uptime_ms = EXCLUDED.uptime_ms,
  candidates = EXCLUDED.candidates,
  payload = EXCLUDED.payload,
  updated_at = NOW()
"""


MAX_AGGREGATION_INTERVAL_SECONDS = 300.0
LOCAL_TIMEZONE_NAME = "Asia/Ho_Chi_Minh"
LOCAL_TIMEZONE = ZoneInfo(LOCAL_TIMEZONE_NAME)
ENERGY_KEYS = (
  "pv_kwh",
  "load_kwh",
  "grid_in_kwh",
  "grid_out_kwh",
  "battery_charge_kwh",
  "battery_discharge_kwh",
  "ac_input_kwh",
  "ac_output_kwh",
)

ALLOWED_DRY_RUN_COMMANDS = {
  "dry_run_noop",
}
ALLOWED_WRITE_COMMANDS = {
  "set_first_discharge_target_soc",
  "set_discharge_target_soc",
  "set_discharge_power",
  "set_discharge_time_enable",
  "set_discharge_time_start",
  "set_discharge_time_end",
  "set_mains_charge_target_soc",
  "set_mains_charge_time_enable",
  "set_mains_charge_time_start",
  "set_mains_charge_time_end",
}
TARGET_SOC_MIN = 5
TARGET_SOC_MAX = 100
DISCHARGE_POWER_MIN = 500
DISCHARGE_POWER_MAX = 5000
DISCHARGE_POWER_SLOTS = {1, 2, 3, 4}
DISCHARGE_TIME_SLOTS = {1, 2, 3, 4}
MAINS_CHARGE_TIME_SLOTS = {1, 2}
WRITE_PAIRING_CODE_TTL_SECONDS = 600
READ_PAIRING_TOKEN_TTL_SECONDS = 600
WRITE_GRANT_SCOPE = "write"
READ_GRANT_SCOPE = "read"


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--host", default=os.getenv("LUMENTREE_SERVER_HOST", "127.0.0.1"))
  parser.add_argument("--port", type=int, default=int(os.getenv("LUMENTREE_SERVER_PORT", "8787")))
  parser.add_argument("--postgres-dsn", default=os.getenv("LUMENTREE_POSTGRES_DSN", ""))
  parser.add_argument("--token", default=os.getenv("LUMENTREE_API_TOKEN", ""))
  parser.add_argument("--init-db", action="store_true")
  return parser.parse_args()


def json_default(value: Any) -> str:
  if isinstance(value, datetime):
    return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")
  raise TypeError(f"{type(value).__name__} is not JSON serializable")


def parse_observed_at(value: Any) -> datetime:
  if not value:
    return datetime.now(timezone.utc)
  if not isinstance(value, str):
    raise ValueError("observed_at must be an ISO-8601 string")
  normalized = value.replace("Z", "+00:00")
  observed_at = datetime.fromisoformat(normalized)
  if observed_at.tzinfo is None:
    observed_at = observed_at.replace(tzinfo=timezone.utc)
  return observed_at.astimezone(timezone.utc)


def local_day(value: datetime) -> date:
  return value.astimezone(LOCAL_TIMEZONE).date()


def local_month_range(value: datetime) -> tuple[date, date]:
  local_now = value.astimezone(LOCAL_TIMEZONE)
  start = date(local_now.year, local_now.month, 1)
  if local_now.month == 12:
    end = date(local_now.year + 1, 1, 1)
  else:
    end = date(local_now.year, local_now.month + 1, 1)
  return start, end


def local_year_range(value: datetime) -> tuple[date, date]:
  local_now = value.astimezone(LOCAL_TIMEZONE)
  return date(local_now.year, 1, 1), date(local_now.year + 1, 1, 1)


def local_day_segments(start_at: datetime, end_at: datetime) -> list[tuple[date, float, datetime, datetime]]:
  segments = []
  cursor = start_at.astimezone(LOCAL_TIMEZONE)
  end = end_at.astimezone(LOCAL_TIMEZONE)
  while cursor < end:
    next_midnight = (cursor.replace(hour=0, minute=0, second=0, microsecond=0) + timedelta(days=1))
    segment_end = min(end, next_midnight)
    seconds = (segment_end - cursor).total_seconds()
    if seconds > 0:
      segments.append(
        (
          cursor.date(),
          seconds,
          cursor.astimezone(timezone.utc),
          segment_end.astimezone(timezone.utc),
        )
      )
    cursor = segment_end
  return segments


def utc_now() -> datetime:
  return datetime.now(timezone.utc)


def sha256_hex(value: str) -> str:
  return hashlib.sha256(value.encode("utf-8")).hexdigest()


def normalize_pairing_secret(value: str) -> str:
  return value.strip().upper()


def scoped_code_hash(gateway_id: str, device_id: str, code: str) -> str:
  return sha256_hex(f"write-code:{gateway_id}:{device_id}:{normalize_pairing_secret(code)}")


def scoped_read_token_hash(gateway_id: str, device_id: str, mac: str, token: str) -> str:
  return sha256_hex(f"read-token:{gateway_id}:{device_id}:{mac.lower()}:{normalize_pairing_secret(token)}")


def token_hash(token: str) -> str:
  return sha256_hex(f"write-grant:{token.strip()}")


def read_grant_token_hash(token: str) -> str:
  return sha256_hex(f"read-grant:{token.strip()}")


def constant_time_equal(left: str, right: str) -> bool:
  return hmac.compare_digest(left.encode("utf-8"), right.encode("utf-8"))


def is_valid_hhmm(value: int) -> bool:
  hours = value // 100
  minutes = value % 100
  return 0 <= hours <= 23 and 0 <= minutes <= 59


def init_db(dsn: str) -> None:
  require_psycopg()
  with psycopg.connect(dsn) as conn:
    with conn.cursor() as cur:
      cur.execute(SCHEMA_SQL)
    conn.commit()


def require_psycopg() -> None:
  if psycopg is None:
    raise SystemExit("Install dependencies first: pip install -r requirements.txt")


def decode_payload(raw: dict[str, Any], payload: dict[str, Any]) -> dict[str, Any] | None:
  payload_hex = None
  for candidate in (
    raw.get("payload_hex"),
    raw.get("response_hex"),
    raw.get("modbus_hex"),
    payload.get("payload_hex"),
    payload.get("response_hex"),
  ):
    if isinstance(candidate, str) and candidate.strip():
      payload_hex = candidate
      break

  event = raw.get("event")
  if payload_hex is None and isinstance(event, dict):
    for candidate in (event.get("response_hex"), event.get("payload_hex"), event.get("modbus_hex")):
      if isinstance(candidate, str) and candidate.strip():
        payload_hex = candidate
        break

  if payload_hex is None:
    return None
  start_register = raw.get("start_register") or payload.get("start_register")
  try:
    start_register = int(start_register)
  except (TypeError, ValueError):
    start_register = 0
  return parse_payload(payload_hex, start_register=start_register)


def metric_float(metrics: dict[str, Any], key: str) -> float | None:
  value = metrics.get(key)
  if isinstance(value, bool) or value is None:
    return None
  try:
    return float(value)
  except (TypeError, ValueError):
    return None


def avg_power(prev_metrics: dict[str, Any], metrics: dict[str, Any], key: str) -> float | None:
  prev = metric_float(prev_metrics, key)
  current = metric_float(metrics, key)
  if prev is None or current is None:
    return None
  return (prev + current) / 2.0


def positive_kwh(avg_watts: float | None, seconds: float) -> float:
  if avg_watts is None:
    return 0.0
  return max(avg_watts, 0.0) * seconds / 3_600_000.0


def negative_kwh(avg_watts: float | None, seconds: float) -> float:
  if avg_watts is None:
    return 0.0
  return max(-avg_watts, 0.0) * seconds / 3_600_000.0


def rounded_energy(row: dict[str, Any] | None) -> dict[str, Any]:
  if row is None:
    return {key: 0.0 for key in ENERGY_KEYS} | {
      "sample_count": 0,
      "covered_seconds": 0.0,
      "first_observed_at": None,
      "last_observed_at": None,
    }
  result = {key: round(float(row.get(key) or 0.0), 4) for key in ENERGY_KEYS}
  result.update(
    {
      "sample_count": int(row.get("sample_count") or 0),
      "covered_seconds": round(float(row.get("covered_seconds") or 0.0), 1),
      "first_observed_at": row.get("first_observed_at"),
      "last_observed_at": row.get("last_observed_at"),
    }
  )
  return result


def merge_latest_telemetry(
  main_row: dict[str, Any] | None,
  statistics_row: dict[str, Any] | None,
) -> dict[str, Any] | None:
  """Merge latest statistics metrics into the latest main telemetry row."""
  if main_row is None:
    return statistics_row
  if statistics_row is None:
    return main_row

  merged = dict(main_row)
  main_metrics = main_row.get("metrics") or {}
  stats_metrics = statistics_row.get("metrics") or {}
  if not isinstance(main_metrics, dict):
    main_metrics = {}
  if not isinstance(stats_metrics, dict):
    stats_metrics = {}
  merged["metrics"] = {**main_metrics, **stats_metrics}

  raw = main_row.get("raw") or {}
  if isinstance(raw, dict):
    merged["raw"] = {
      **raw,
      "statistics": {
        "id": statistics_row.get("id"),
        "observed_at": statistics_row.get("observed_at"),
      },
    }
  return merged


def hydrate_telemetry_metrics(row: dict[str, Any] | None) -> dict[str, Any] | None:
  """Decode metrics on read when an older stored row is missing decoded metrics."""
  if row is None:
    return None
  metrics = row.get("metrics") or {}
  if isinstance(metrics, dict) and metrics:
    return row

  raw = row.get("raw")
  payload = row.get("payload")
  if not isinstance(raw, dict) or not isinstance(payload, dict):
    return row

  decoded = decode_payload(raw, payload)
  if decoded is None:
    return row

  hydrated = dict(row)
  hydrated["metrics"] = decoded["metrics"]
  hydrated["raw"] = {
    **raw,
    "decode": {
      "data_type": decoded["data_type"],
      "diagnostics": decoded["diagnostics"],
    },
  }
  return hydrated


def sanitize_latest_response(row: dict[str, Any] | None) -> dict[str, Any] | None:
  """Drop raw transport details from public latest telemetry responses."""
  if row is None:
    return None
  return {
    "id": row.get("id"),
    "device_id": row.get("device_id"),
    "gateway_id": row.get("gateway_id"),
    "observed_at": row.get("observed_at"),
    "firmware": row.get("firmware"),
    "uptime_ms": row.get("uptime_ms"),
    "metrics": row.get("metrics") if isinstance(row.get("metrics"), dict) else {},
  }


def sanitize_settings_response(row: dict[str, Any] | None) -> dict[str, Any] | None:
  """Drop raw transport details from public settings responses."""
  if row is None:
    return None
  return {
    "id": row.get("id"),
    "device_id": row.get("device_id"),
    "gateway_id": row.get("gateway_id"),
    "observed_at": row.get("observed_at"),
    "firmware": row.get("firmware"),
    "uptime_ms": row.get("uptime_ms"),
    "settings": row.get("settings") if isinstance(row.get("settings"), dict) else {},
    "setting_registers": row.get("setting_registers") if isinstance(row.get("setting_registers"), dict) else {},
  }


class LumentreeServer:
  def __init__(self, dsn: str, token: str):
    if not dsn:
      raise SystemExit("LUMENTREE_POSTGRES_DSN or --postgres-dsn is required")
    self.dsn = dsn
    self.token = token

  def connect(self):
    require_psycopg()
    return psycopg.connect(self.dsn, row_factory=dict_row)

  def check_database(self) -> str:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute("SELECT 1")
        cur.fetchone()
    return "ok"

  def store_event(self, payload: dict[str, Any]) -> dict[str, Any]:
    device_id = payload.get("device_id") or payload.get("mac")
    if not isinstance(device_id, str) or not device_id.strip():
      raise ValueError("device_id or mac is required")
    device_id = device_id.strip()

    mac = payload.get("mac")
    gateway_id = payload.get("gateway_id")
    firmware = payload.get("firmware")
    uptime_ms = payload.get("uptime_ms")
    metrics = payload.get("metrics") or {}
    raw = payload.get("raw") or {}
    metadata = payload.get("metadata") or {}
    observed_at = parse_observed_at(payload.get("observed_at"))

    if not isinstance(metrics, dict):
      raise ValueError("metrics must be an object")
    if not isinstance(raw, dict):
      raise ValueError("raw must be an object")
    if not isinstance(metadata, dict):
      raise ValueError("metadata must be an object")

    decoded = decode_payload(raw, payload)
    if decoded is not None:
      if not metrics:
        metrics = decoded["metrics"]
      raw = {
        **raw,
        "decode": {
          "data_type": decoded["data_type"],
          "diagnostics": decoded["diagnostics"],
        },
      }

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          UPSERT_DEVICE_SQL,
          (device_id, mac, gateway_id, observed_at, observed_at, Jsonb(metadata)),
        )
        cur.execute(
          INSERT_TELEMETRY_SQL,
          (
            device_id,
            mac,
            gateway_id,
            observed_at,
            firmware,
            uptime_ms,
            Jsonb(metrics),
            Jsonb(raw),
            Jsonb(payload),
          ),
        )
        telemetry_id = cur.fetchone()["id"]
        self.update_energy_aggregate(cur, device_id, observed_at, metrics)
      conn.commit()
    return {"ok": True, "id": telemetry_id, "device_id": device_id}

  def store_gateway_status(self, payload: dict[str, Any]) -> dict[str, Any]:
    gateway_id = payload.get("gateway_id")
    if not isinstance(gateway_id, str) or not gateway_id.strip():
      raise ValueError("gateway_id is required")
    gateway_id = gateway_id.strip()

    device_id = payload.get("device_id")
    if isinstance(device_id, str):
      device_id = device_id.strip() or None
    else:
      device_id = None

    candidates = payload.get("candidates") or []
    if not isinstance(candidates, list):
      raise ValueError("candidates must be an array")

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          UPSERT_GATEWAY_STATUS_SQL,
          (
            gateway_id,
            device_id,
            payload.get("firmware"),
            payload.get("target_mac"),
            payload.get("pairing_status"),
            payload.get("wifi_connected"),
            payload.get("wifi_rssi"),
            payload.get("ip"),
            payload.get("uptime_ms"),
            Jsonb(candidates),
            Jsonb(payload),
          ),
        )
      conn.commit()
    return {"ok": True, "gateway_id": gateway_id, "device_id": device_id}

  def gateway_status_for_device(self, device_id: str) -> dict[str, Any] | None:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT gateway_id, device_id, firmware, target_mac, pairing_status,
                 wifi_connected, wifi_rssi, ip, uptime_ms, candidates, updated_at
          FROM lumentree_gateway_status
          WHERE device_id = %s
          ORDER BY updated_at DESC
          LIMIT 1
          """,
          (device_id,),
        )
        return cur.fetchone()

  def gateway_status(self, gateway_id: str) -> dict[str, Any] | None:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT gateway_id, device_id, firmware, target_mac, pairing_status,
                 wifi_connected, wifi_rssi, ip, uptime_ms, candidates, updated_at
          FROM lumentree_gateway_status
          WHERE gateway_id = %s
          """,
          (gateway_id,),
        )
        return cur.fetchone()

  def store_gateway_candidates(self, gateway_id: str, payload: dict[str, Any]) -> dict[str, Any]:
    gateway_id = gateway_id.strip()
    if not gateway_id:
      raise ValueError("gateway_id is required")
    candidates = payload.get("candidates") or []
    if not isinstance(candidates, list):
      raise ValueError("candidates must be an array")

    normalized: list[dict[str, Any]] = []
    seen_keys: set[tuple[str, str, str]] = set()
    for item in candidates:
      if not isinstance(item, dict):
        continue
      device_id = item.get("device_id") or item.get("name")
      mac = item.get("mac")
      if not isinstance(device_id, str) or not device_id.strip():
        continue
      if not isinstance(mac, str) or not mac.strip():
        continue
      device_id = device_id.strip()
      mac = mac.strip().lower()
      key = (gateway_id, device_id, mac)
      if key in seen_keys:
        continue
      seen_keys.add(key)
      normalized.append(
        {
          "device_id": device_id,
          "mac": mac,
          "address_type": item.get("address_type"),
          "name": item.get("name"),
          "service_uuid": item.get("service_uuid"),
          "characteristic_uuid": item.get("characteristic_uuid"),
          "rssi": item.get("rssi"),
          "payload": item,
        }
      )

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_gateway_candidates
          SET active = FALSE
          WHERE gateway_id = %s
          """,
          (gateway_id,),
        )
        for candidate in normalized:
          cur.execute(
            """
            INSERT INTO lumentree_gateway_candidates
              (
                gateway_id, device_id, mac, address_type, name, service_uuid,
                characteristic_uuid, rssi, payload, first_seen_at, last_seen_at, active
              )
            VALUES
              (%s, %s, %s, %s, %s, %s, %s, %s, %s, NOW(), NOW(), TRUE)
            ON CONFLICT DO NOTHING
            """,
            (
              gateway_id,
              candidate["device_id"],
              candidate["mac"],
              candidate.get("address_type"),
              candidate.get("name"),
              candidate.get("service_uuid"),
              candidate.get("characteristic_uuid"),
              candidate.get("rssi"),
              Jsonb(candidate["payload"]),
            ),
          )
          cur.execute(
            """
            UPDATE lumentree_gateway_candidates
            SET address_type = %s,
                name = %s,
                service_uuid = %s,
                characteristic_uuid = %s,
                rssi = %s,
                payload = %s,
                last_seen_at = NOW(),
                active = TRUE
            WHERE gateway_id = %s
              AND device_id = %s
              AND mac = %s
            """,
            (
              candidate.get("address_type"),
              candidate.get("name"),
              candidate.get("service_uuid"),
              candidate.get("characteristic_uuid"),
              candidate.get("rssi"),
              Jsonb(candidate["payload"]),
              gateway_id,
              candidate["device_id"],
              candidate["mac"],
            ),
          )
      conn.commit()
    return {"ok": True, "gateway_id": gateway_id, "candidate_count": len(normalized)}

  def latest_gateway_candidate(self, gateway_id: str, device_id: str) -> dict[str, Any] | None:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT gateway_id, device_id, mac, address_type, name, service_uuid,
                 characteristic_uuid, rssi, payload, first_seen_at, last_seen_at, active
          FROM lumentree_gateway_candidates
          WHERE gateway_id = %s
            AND device_id = %s
            AND active = TRUE
          ORDER BY last_seen_at DESC, id DESC
          LIMIT 1
          """,
          (gateway_id, device_id),
        )
        return cur.fetchone()

  def current_gateway_binding(self, gateway_id: str, device_id: str) -> dict[str, Any] | None:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT gateway_id, device_id, target_mac, pairing_status, wifi_connected, updated_at
          FROM lumentree_gateway_status
          WHERE gateway_id = %s
            AND device_id = %s
            AND updated_at > NOW() - interval '5 minutes'
          LIMIT 1
          """,
          (gateway_id, device_id),
        )
        row = cur.fetchone()
    if row is None:
      return None
    target_mac = str(row.get("target_mac") or "").strip().lower()
    if not target_mac:
      return None
    if row.get("pairing_status") not in {"paired", "multiple_candidates", "scanning_no_candidate"}:
      return None
    if row.get("wifi_connected") is False:
      return None
    return row

  def audit_write_event(
    self,
    cur: Any,
    event: str,
    *,
    device_id: str | None = None,
    gateway_id: str | None = None,
    detail: dict[str, Any] | None = None,
  ) -> None:
    cur.execute(
      """
      INSERT INTO lumentree_write_audit (device_id, gateway_id, event, detail)
      VALUES (%s, %s, %s, %s)
      """,
      (device_id, gateway_id, event, Jsonb(detail or {})),
    )

  def audit_auth_event(
    self,
    cur: Any,
    event: str,
    *,
    device_id: str | None = None,
    gateway_id: str | None = None,
    mac: str | None = None,
    detail: dict[str, Any] | None = None,
  ) -> None:
    cur.execute(
      """
      INSERT INTO lumentree_auth_audit (device_id, gateway_id, mac, event, detail)
      VALUES (%s, %s, %s, %s, %s)
      """,
      (device_id, gateway_id, mac, event, Jsonb(detail or {})),
    )

  def create_read_pairing_token(self, gateway_id: str, payload: dict[str, Any]) -> dict[str, Any]:
    gateway_id = gateway_id.strip()
    if not gateway_id:
      raise ValueError("gateway_id is required")
    device_id = payload.get("device_id")
    token = payload.get("token")
    mac = payload.get("mac")
    if not isinstance(device_id, str) or not device_id.strip():
      raise ValueError("device_id is required")
    if not isinstance(mac, str) or not mac.strip():
      raise ValueError("mac is required")
    if not isinstance(token, str) or not token.strip():
      raise ValueError("token is required")
    device_id = device_id.strip()
    mac = mac.strip().lower()
    token = normalize_pairing_secret(token)

    candidate = self.latest_gateway_candidate(gateway_id, device_id)
    if candidate is not None and str(candidate.get("mac") or "").lower() == mac:
      binding_ok = True
    else:
      binding = self.current_gateway_binding(gateway_id, device_id)
      binding_ok = binding is not None and str(binding.get("target_mac") or "").lower() == mac
    if not binding_ok:
      raise ValueError("gateway candidate or current gateway binding for device_id and mac was not found")

    expires_at = utc_now() + timedelta(seconds=READ_PAIRING_TOKEN_TTL_SECONDS)
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_read_pairing_tokens
          SET consumed_at = NOW()
          WHERE gateway_id = %s
            AND device_id = %s
            AND mac = %s
            AND consumed_at IS NULL
            AND expires_at > NOW()
          """,
          (gateway_id, device_id, mac),
        )
        cur.execute(
          """
          INSERT INTO lumentree_read_pairing_tokens
            (gateway_id, device_id, mac, token_hash, expires_at, firmware, payload)
          VALUES
            (%s, %s, %s, %s, %s, %s, %s)
          RETURNING id, gateway_id, device_id, mac, expires_at, created_at
          """,
          (
            gateway_id,
            device_id,
            mac,
            scoped_read_token_hash(gateway_id, device_id, mac, token),
            expires_at,
            payload.get("firmware"),
            Jsonb({key: value for key, value in payload.items() if key != "token"}),
          ),
        )
        row = cur.fetchone()
        self.audit_auth_event(
          cur,
          "read_pairing_token_created",
          device_id=device_id,
          gateway_id=gateway_id,
          mac=mac,
          detail={"expires_at": expires_at.isoformat()},
        )
      conn.commit()
    return {
      "ok": True,
      "read_pairing_token": {
        **row,
        "ttl_seconds": READ_PAIRING_TOKEN_TTL_SECONDS,
      },
    }

  def create_write_pairing_code(self, gateway_id: str, payload: dict[str, Any]) -> dict[str, Any]:
    if not gateway_id.strip():
      raise ValueError("gateway_id is required")
    gateway_id = gateway_id.strip()
    device_id = payload.get("device_id")
    code = payload.get("code")
    if not isinstance(device_id, str) or not device_id.strip():
      raise ValueError("device_id is required")
    if not isinstance(code, str) or not code.strip():
      raise ValueError("code is required")
    device_id = device_id.strip()
    code = normalize_pairing_secret(code)
    if len(code) < 6 or len(code) > 16 or not code.isalnum():
      raise ValueError("code must be 6 to 16 alphanumeric characters")

    expires_at = utc_now() + timedelta(seconds=WRITE_PAIRING_CODE_TTL_SECONDS)
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_write_pairing_codes
          SET consumed_at = NOW()
          WHERE gateway_id = %s
            AND device_id = %s
            AND consumed_at IS NULL
            AND expires_at > NOW()
          """,
          (gateway_id, device_id),
        )
        cur.execute(
          """
          INSERT INTO lumentree_write_pairing_codes
            (gateway_id, device_id, code_hash, expires_at, firmware, payload)
          VALUES
            (%s, %s, %s, %s, %s, %s)
          RETURNING id, gateway_id, device_id, expires_at, created_at
          """,
          (
            gateway_id,
            device_id,
            scoped_code_hash(gateway_id, device_id, code),
            expires_at,
            payload.get("firmware"),
            Jsonb({key: value for key, value in payload.items() if key != "code"}),
          ),
        )
        row = cur.fetchone()
        self.audit_write_event(
          cur,
          "write_pairing_code_created",
          device_id=device_id,
          gateway_id=gateway_id,
          detail={"expires_at": expires_at.isoformat(), "firmware": payload.get("firmware")},
        )
      conn.commit()
    return {
      "ok": True,
      "pairing_code": {
        **row,
        "ttl_seconds": WRITE_PAIRING_CODE_TTL_SECONDS,
        "code_length": len(code),
      },
    }

  def claim_write_grant(self, device_id: str, payload: dict[str, Any]) -> dict[str, Any]:
    if not device_id.strip():
      raise ValueError("device_id is required")
    device_id = device_id.strip()
    code = payload.get("code")
    if not isinstance(code, str) or not code.strip():
      raise ValueError("code is required")
    code = normalize_pairing_secret(code)

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT id, gateway_id, device_id, code_hash, expires_at
          FROM lumentree_write_pairing_codes
          WHERE device_id = %s
            AND consumed_at IS NULL
            AND expires_at > NOW()
          ORDER BY created_at DESC
          LIMIT 5
          FOR UPDATE
          """,
          (device_id,),
        )
        code_rows = cur.fetchall()
        matched = None
        for row in code_rows:
          expected_hash = scoped_code_hash(row["gateway_id"], device_id, code)
          if constant_time_equal(row["code_hash"], expected_hash):
            matched = row
            break
        if matched is None:
          self.audit_write_event(
            cur,
            "write_grant_claim_failed",
            device_id=device_id,
            detail={"reason": "invalid_or_expired_code"},
          )
          conn.commit()
          raise ValueError("invalid or expired write pairing code")

        cur.execute(
          """
          SELECT gateway_id, device_id, updated_at, wifi_connected
          FROM lumentree_gateway_status
          WHERE gateway_id = %s
            AND device_id = %s
            AND updated_at > NOW() - interval '5 minutes'
          """,
          (matched["gateway_id"], device_id),
        )
        gateway_row = cur.fetchone()
        if gateway_row is None:
          self.audit_write_event(
            cur,
            "write_grant_claim_failed",
            device_id=device_id,
            gateway_id=matched["gateway_id"],
            detail={"reason": "gateway_not_online"},
          )
          conn.commit()
          raise ValueError("gateway is not online for this Device ID")

        grant_token = secrets.token_urlsafe(32)
        cur.execute(
          """
          UPDATE lumentree_write_pairing_codes
          SET consumed_at = NOW()
          WHERE id = %s
          """,
          (matched["id"],),
        )
        cur.execute(
          """
          INSERT INTO lumentree_write_grants
            (gateway_id, device_id, grant_token_hash, scope, enabled)
          VALUES
            (%s, %s, %s, %s, TRUE)
          RETURNING id, gateway_id, device_id, scope, enabled, created_at, revoked_at, last_used_at
          """,
          (matched["gateway_id"], device_id, token_hash(grant_token), WRITE_GRANT_SCOPE),
        )
        grant = cur.fetchone()
        self.audit_write_event(
          cur,
          "write_grant_created",
          device_id=device_id,
          gateway_id=matched["gateway_id"],
          detail={"scope": WRITE_GRANT_SCOPE},
        )
      conn.commit()
    return {"ok": True, "write_grant": grant, "grant_token": grant_token}

  def validate_write_grant(self, device_id: str, grant_token: str | None) -> dict[str, Any] | None:
    if not grant_token:
      return None
    token_digest = token_hash(grant_token)
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_write_grants
          SET last_used_at = NOW()
          WHERE device_id = %s
            AND grant_token_hash = %s
            AND enabled = TRUE
            AND revoked_at IS NULL
          RETURNING id, gateway_id, device_id, scope, enabled, created_at, revoked_at, last_used_at
          """,
          (device_id, token_digest),
        )
        grant = cur.fetchone()
        if grant is not None and grant.get("scope") == "dry_run":
          cur.execute(
            """
            UPDATE lumentree_write_grants
            SET scope = %s
            WHERE id = %s
            RETURNING id, gateway_id, device_id, scope, enabled, created_at, revoked_at, last_used_at
            """,
            (WRITE_GRANT_SCOPE, grant["id"]),
          )
          grant = cur.fetchone()
      conn.commit()
    return grant

  def validate_read_grant(self, device_id: str, grant_token: str | None) -> dict[str, Any] | None:
    if not grant_token:
      return None
    token_digest = read_grant_token_hash(grant_token)
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_read_grants
          SET last_used_at = NOW()
          WHERE device_id = %s
            AND grant_token_hash = %s
            AND enabled = TRUE
            AND revoked_at IS NULL
          RETURNING id, gateway_id, device_id, mac, enabled, created_at, revoked_at, last_used_at
          """,
          (device_id, token_digest),
        )
        grant = cur.fetchone()
      conn.commit()
    return grant

  def claim_read_grant(self, device_id: str, payload: dict[str, Any]) -> dict[str, Any]:
    if not device_id.strip():
      raise ValueError("device_id is required")
    device_id = device_id.strip()
    token = payload.get("token")
    if not isinstance(token, str) or not token.strip():
      raise ValueError("token is required")
    token = normalize_pairing_secret(token)

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT id, gateway_id, device_id, mac, token_hash, expires_at
          FROM lumentree_read_pairing_tokens
          WHERE device_id = %s
            AND consumed_at IS NULL
            AND expires_at > NOW()
          ORDER BY created_at DESC
          LIMIT 10
          FOR UPDATE
          """,
          (device_id,),
        )
        token_rows = cur.fetchall()
        matched = None
        for row in token_rows:
          expected_hash = scoped_read_token_hash(row["gateway_id"], device_id, row["mac"], token)
          if constant_time_equal(row["token_hash"], expected_hash):
            matched = row
            break
        if matched is None:
          self.audit_auth_event(
            cur,
            "read_grant_claim_failed",
            device_id=device_id,
            detail={"reason": "invalid_or_expired_token"},
          )
          conn.commit()
          raise ValueError("invalid or expired read pairing token")

        gateway_row = self.current_gateway_binding(matched["gateway_id"], device_id)
        if gateway_row is None:
          self.audit_auth_event(
            cur,
            "read_grant_claim_failed",
            device_id=device_id,
            gateway_id=matched["gateway_id"],
            mac=matched["mac"],
            detail={"reason": "gateway_not_online"},
          )
          conn.commit()
          raise ValueError("gateway is not online for this Device ID")

        candidate = self.latest_gateway_candidate(matched["gateway_id"], device_id)
        candidate_mac_matches = candidate is not None and str(candidate.get("mac") or "").lower() == str(matched["mac"]).lower()
        gateway_mac_matches = str(gateway_row.get("target_mac") or "").lower() == str(matched["mac"]).lower()
        if not candidate_mac_matches and not gateway_mac_matches:
          self.audit_auth_event(
            cur,
            "read_grant_claim_failed",
            device_id=device_id,
            gateway_id=matched["gateway_id"],
            mac=matched["mac"],
            detail={"reason": "candidate_mac_not_found"},
          )
          conn.commit()
          raise ValueError("gateway candidate MAC for this Device ID was not found")

        observed_at = utc_now()
        cur.execute(
          UPSERT_DEVICE_SQL,
          (
            device_id,
            matched["mac"],
            matched["gateway_id"],
            observed_at,
            observed_at,
            Jsonb({"auth_binding": {"mac": matched["mac"], "gateway_id": matched["gateway_id"]}}),
          ),
        )

        read_grant_token = secrets.token_urlsafe(32)
        cur.execute(
          """
          UPDATE lumentree_read_pairing_tokens
          SET consumed_at = NOW()
          WHERE id = %s
          """,
          (matched["id"],),
        )
        cur.execute(
          """
          INSERT INTO lumentree_read_grants
            (gateway_id, device_id, mac, grant_token_hash, enabled)
          VALUES
            (%s, %s, %s, %s, TRUE)
          RETURNING id, gateway_id, device_id, mac, enabled, created_at, revoked_at, last_used_at
          """,
          (matched["gateway_id"], device_id, matched["mac"], read_grant_token_hash(read_grant_token)),
        )
        grant = cur.fetchone()
        self.audit_auth_event(
          cur,
          "read_grant_created",
          device_id=device_id,
          gateway_id=matched["gateway_id"],
          mac=matched["mac"],
          detail={},
        )
      conn.commit()
    return {"ok": True, "read_grant": grant, "grant_token": read_grant_token}

  def write_grant_status(self, device_id: str, grant_token: str | None = None) -> dict[str, Any]:
    device_id = device_id.strip()
    if not device_id:
      raise ValueError("device_id is required")
    grant = self.validate_write_grant(device_id, grant_token)
    gateway = self.gateway_status_for_device(device_id)
    gateway_online = bool(gateway and gateway.get("updated_at") and gateway["updated_at"] > utc_now() - timedelta(minutes=5))
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT COUNT(*) AS count
          FROM lumentree_write_grants
          WHERE device_id = %s
            AND enabled = TRUE
            AND revoked_at IS NULL
          """,
          (device_id,),
        )
        active_count = int(cur.fetchone()["count"])
    return {
      "device_id": device_id,
      "write_available": gateway_online,
      "write_enabled": grant is not None,
      "grant_active": grant is not None,
      "active_grant_count": active_count,
      "scope": grant.get("scope") if grant else None,
      "gateway_id": gateway.get("gateway_id") if gateway else None,
      "gateway_online": gateway_online,
      "gateway_updated_at": gateway.get("updated_at") if gateway else None,
      "command_mode": "guarded_write" if grant is not None else "read_only",
      "safety": "allowlisted_semantic_write_only" if grant is not None else "no_write_grant",
    }

  def revoke_write_grant(self, device_id: str, grant_token: str | None) -> dict[str, Any]:
    if not device_id.strip():
      raise ValueError("device_id is required")
    if not grant_token:
      raise PermissionError("write grant token is required")
    device_id = device_id.strip()
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_write_grants
          SET enabled = FALSE,
              revoked_at = NOW()
          WHERE device_id = %s
            AND grant_token_hash = %s
            AND revoked_at IS NULL
          RETURNING id, gateway_id, device_id, scope, enabled, created_at, revoked_at, last_used_at
          """,
          (device_id, token_hash(grant_token)),
        )
        grant = cur.fetchone()
        if grant is None:
          raise PermissionError("active write grant not found")
        self.audit_write_event(
          cur,
          "write_grant_revoked",
          device_id=device_id,
          gateway_id=grant["gateway_id"],
          detail={"scope": grant["scope"]},
        )
      conn.commit()
    return {"ok": True, "write_grant": grant}

  def revoke_read_grant(self, device_id: str, grant_token: str | None) -> dict[str, Any]:
    if not device_id.strip():
      raise ValueError("device_id is required")
    if not grant_token:
      raise PermissionError("read grant token is required")
    device_id = device_id.strip()
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_read_grants
          SET enabled = FALSE,
              revoked_at = NOW()
          WHERE device_id = %s
            AND grant_token_hash = %s
            AND revoked_at IS NULL
          RETURNING id, gateway_id, device_id, mac, enabled, created_at, revoked_at, last_used_at
          """,
          (device_id, read_grant_token_hash(grant_token)),
        )
        grant = cur.fetchone()
        if grant is None:
          raise PermissionError("active read grant not found")
        self.audit_auth_event(
          cur,
          "read_grant_revoked",
          device_id=device_id,
          gateway_id=grant["gateway_id"],
          mac=grant["mac"],
          detail={},
        )
      conn.commit()
    return {"ok": True, "read_grant": grant}

  def create_command(self, payload: dict[str, Any]) -> dict[str, Any]:
    device_id = payload.get("device_id")
    command = payload.get("command")
    mode = payload.get("mode", "dry_run")
    gateway_id = payload.get("gateway_id")
    requested_by = payload.get("requested_by", "api")
    command_payload = payload.get("payload") or {}

    if not isinstance(device_id, str) or not device_id.strip():
      raise ValueError("device_id is required")
    if not isinstance(command, str) or not command.strip():
      raise ValueError("command is required")
    if not isinstance(mode, str) or mode not in {"dry_run", "write"}:
      raise ValueError("mode must be dry_run or write")
    if mode == "dry_run" and command not in ALLOWED_DRY_RUN_COMMANDS:
      raise ValueError(f"unsupported dry-run command: {command}")
    if mode == "write" and command not in ALLOWED_WRITE_COMMANDS:
      raise ValueError(f"unsupported write command: {command}")
    if gateway_id is not None and not isinstance(gateway_id, str):
      raise ValueError("gateway_id must be a string")
    if not isinstance(command_payload, dict):
      raise ValueError("payload must be an object")

    device_id = device_id.strip()
    command = command.strip()
    gateway_id = gateway_id.strip() if isinstance(gateway_id, str) and gateway_id.strip() else None
    if mode == "write":
      if command == "set_first_discharge_target_soc":
        target_soc = command_payload.get("target_soc")
        if not isinstance(target_soc, int) or isinstance(target_soc, bool):
          raise ValueError("payload.target_soc must be an integer")
        if target_soc < TARGET_SOC_MIN or target_soc > TARGET_SOC_MAX:
          raise ValueError(f"payload.target_soc must be {TARGET_SOC_MIN}..{TARGET_SOC_MAX}")
      elif command == "set_discharge_target_soc":
        slot = command_payload.get("slot")
        target_soc = command_payload.get("target_soc")
        if not isinstance(slot, int) or isinstance(slot, bool):
          raise ValueError("payload.slot must be an integer")
        if slot not in DISCHARGE_TIME_SLOTS:
          raise ValueError(f"payload.slot must be one of {sorted(DISCHARGE_TIME_SLOTS)}")
        if not isinstance(target_soc, int) or isinstance(target_soc, bool):
          raise ValueError("payload.target_soc must be an integer")
        if target_soc < TARGET_SOC_MIN or target_soc > TARGET_SOC_MAX:
          raise ValueError(f"payload.target_soc must be {TARGET_SOC_MIN}..{TARGET_SOC_MAX}")
      elif command == "set_discharge_power":
        slot = command_payload.get("slot")
        power = command_payload.get("power")
        if not isinstance(slot, int) or isinstance(slot, bool):
          raise ValueError("payload.slot must be an integer")
        if slot not in DISCHARGE_POWER_SLOTS:
          raise ValueError(f"payload.slot must be one of {sorted(DISCHARGE_POWER_SLOTS)}")
        if not isinstance(power, int) or isinstance(power, bool):
          raise ValueError("payload.power must be an integer")
        if power < DISCHARGE_POWER_MIN or power > DISCHARGE_POWER_MAX:
          raise ValueError(f"payload.power must be {DISCHARGE_POWER_MIN}..{DISCHARGE_POWER_MAX}")
      elif command == "set_discharge_time_enable":
        slot = command_payload.get("slot")
        enabled = command_payload.get("enabled")
        if not isinstance(slot, int) or isinstance(slot, bool):
          raise ValueError("payload.slot must be an integer")
        if slot not in DISCHARGE_TIME_SLOTS:
          raise ValueError(f"payload.slot must be one of {sorted(DISCHARGE_TIME_SLOTS)}")
        if not isinstance(enabled, int) or isinstance(enabled, bool):
          raise ValueError("payload.enabled must be an integer")
        if enabled not in (0, 1):
          raise ValueError("payload.enabled must be 0 or 1")
      elif command in {"set_discharge_time_start", "set_discharge_time_end"}:
        slot = command_payload.get("slot")
        time_value = command_payload.get("time")
        if not isinstance(slot, int) or isinstance(slot, bool):
          raise ValueError("payload.slot must be an integer")
        if slot not in DISCHARGE_TIME_SLOTS:
          raise ValueError(f"payload.slot must be one of {sorted(DISCHARGE_TIME_SLOTS)}")
        if not isinstance(time_value, int) or isinstance(time_value, bool):
          raise ValueError("payload.time must be an integer")
        if not is_valid_hhmm(time_value):
          raise ValueError("payload.time must be a valid HHMM value from 0 to 2359")
      elif command == "set_mains_charge_target_soc":
        slot = command_payload.get("slot")
        target_soc = command_payload.get("target_soc")
        if not isinstance(slot, int) or isinstance(slot, bool):
          raise ValueError("payload.slot must be an integer")
        if slot not in MAINS_CHARGE_TIME_SLOTS:
          raise ValueError(f"payload.slot must be one of {sorted(MAINS_CHARGE_TIME_SLOTS)}")
        if not isinstance(target_soc, int) or isinstance(target_soc, bool):
          raise ValueError("payload.target_soc must be an integer")
        if target_soc < TARGET_SOC_MIN or target_soc > TARGET_SOC_MAX:
          raise ValueError(f"payload.target_soc must be {TARGET_SOC_MIN}..{TARGET_SOC_MAX}")
      elif command == "set_mains_charge_time_enable":
        slot = command_payload.get("slot")
        enabled = command_payload.get("enabled")
        if not isinstance(slot, int) or isinstance(slot, bool):
          raise ValueError("payload.slot must be an integer")
        if slot not in MAINS_CHARGE_TIME_SLOTS:
          raise ValueError(f"payload.slot must be one of {sorted(MAINS_CHARGE_TIME_SLOTS)}")
        if not isinstance(enabled, int) or isinstance(enabled, bool):
          raise ValueError("payload.enabled must be an integer")
        if enabled not in (0, 1):
          raise ValueError("payload.enabled must be 0 or 1")
      elif command in {"set_mains_charge_time_start", "set_mains_charge_time_end"}:
        slot = command_payload.get("slot")
        time_value = command_payload.get("time")
        if not isinstance(slot, int) or isinstance(slot, bool):
          raise ValueError("payload.slot must be an integer")
        if slot not in MAINS_CHARGE_TIME_SLOTS:
          raise ValueError(f"payload.slot must be one of {sorted(MAINS_CHARGE_TIME_SLOTS)}")
        if not isinstance(time_value, int) or isinstance(time_value, bool):
          raise ValueError("payload.time must be an integer")
        if not is_valid_hhmm(time_value):
          raise ValueError("payload.time must be a valid HHMM value from 0 to 2359")

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          INSERT INTO lumentree_commands
            (device_id, gateway_id, command, mode, status, requested_by, payload)
          VALUES
            (%s, %s, %s, %s, 'requested', %s, %s)
          RETURNING *
          """,
          (device_id, gateway_id, command, mode, requested_by, Jsonb(command_payload)),
        )
        command_row = cur.fetchone()
      conn.commit()
    return {"ok": True, "command": command_row}

  def next_command(self, gateway_id: str, device_id: str | None) -> dict[str, Any]:
    gateway_id = gateway_id.strip()
    if not gateway_id:
      raise ValueError("gateway_id is required")
    if isinstance(device_id, str):
      device_id = device_id.strip() or None

    params: list[Any] = [gateway_id]
    device_clause = ""
    if device_id:
      device_clause = "AND device_id = %s"
      params.append(device_id)

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          f"""
          WITH next AS (
            SELECT id
            FROM lumentree_commands
            WHERE status = 'requested'
              AND mode IN ('dry_run', 'write')
              AND (gateway_id IS NULL OR gateway_id = %s)
              {device_clause}
            ORDER BY requested_at ASC, id ASC
            LIMIT 1
            FOR UPDATE SKIP LOCKED
          )
          UPDATE lumentree_commands
          SET status = 'sent',
              gateway_id = %s,
              claimed_at = NOW(),
              updated_at = NOW()
          WHERE id = (SELECT id FROM next)
          RETURNING *
          """,
          [*params, gateway_id],
        )
        command_row = cur.fetchone()
      conn.commit()

    return {"command": command_row}

  def complete_command(self, command_id: int, payload: dict[str, Any]) -> dict[str, Any]:
    status = payload.get("status", "dry_run_completed")
    result = payload.get("result") or {}
    error = payload.get("error")
    gateway_id = payload.get("gateway_id")

    if status not in {"dry_run_completed", "completed", "failed", "rejected"}:
      raise ValueError("invalid command result status")
    if not isinstance(result, dict):
      raise ValueError("result must be an object")
    if error is not None and not isinstance(error, str):
      raise ValueError("error must be a string")

    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          UPDATE lumentree_commands
          SET status = %s,
              gateway_id = COALESCE(%s, gateway_id),
              result = %s,
              error = %s,
              completed_at = NOW(),
              updated_at = NOW()
          WHERE id = %s
          RETURNING *
          """,
          (status, gateway_id, Jsonb(result), error, command_id),
        )
        command_row = cur.fetchone()
      conn.commit()

    if command_row is None:
      raise ValueError("command not found")
    return {"ok": True, "command": command_row}

  def list_commands(self, device_id: str, limit: int) -> dict[str, Any]:
    limit = max(1, min(limit, 100))
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT *
          FROM lumentree_commands
          WHERE device_id = %s
          ORDER BY requested_at DESC, id DESC
          LIMIT %s
          """,
          (device_id, limit),
        )
        rows = cur.fetchall()
    return {"commands": rows}

  def command_status(self, device_id: str) -> dict[str, Any]:
    device_id = device_id.strip()
    if not device_id:
      raise ValueError("device_id is required")
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT id, device_id, gateway_id, command, mode, status, requested_by,
                 result, error, requested_at, claimed_at, completed_at, updated_at
          FROM lumentree_commands
          WHERE device_id = %s
          ORDER BY requested_at DESC, id DESC
          LIMIT 1
          """,
          (device_id,),
        )
        row = cur.fetchone()

    if row is None:
      return {
        "device_id": device_id,
        "has_command": False,
        "last_command": None,
      }

    result = row.get("result") or {}
    if not isinstance(result, dict):
      result = {}
    return {
      "device_id": device_id,
      "has_command": True,
      "last_command": {
        "id": row.get("id"),
        "gateway_id": row.get("gateway_id"),
        "command": row.get("command"),
        "mode": row.get("mode"),
        "status": row.get("status"),
        "requested_by": row.get("requested_by"),
        "requested_at": row.get("requested_at"),
        "claimed_at": row.get("claimed_at"),
        "completed_at": row.get("completed_at"),
        "updated_at": row.get("updated_at"),
        "error": row.get("error"),
        "dry_run": result.get("dry_run"),
        "firmware": result.get("firmware"),
        "ble_write": result.get("ble_write"),
        "modbus_write": result.get("modbus_write"),
        "write_enabled": result.get("write_enabled"),
        "would_execute": result.get("would_execute"),
        "register": result.get("register"),
        "requested_value": result.get("requested_value"),
        "before_value": result.get("before_value"),
        "after_value": result.get("after_value"),
        "write_ack": result.get("write_ack"),
        "verified": result.get("verified"),
        "safety": result.get("safety"),
      },
    }

  def update_energy_aggregate(self, cur: Any, device_id: str, observed_at: datetime, metrics: dict[str, Any]) -> None:
    if not metrics:
      return
    cur.execute(
      """
      SELECT observed_at, metrics
      FROM lumentree_telemetry
      WHERE device_id = %s
        AND observed_at < %s
        AND metrics <> '{}'::jsonb
      ORDER BY observed_at DESC, id DESC
      LIMIT 1
      """,
      (device_id, observed_at),
    )
    previous = cur.fetchone()
    if not previous:
      return

    previous_at = previous["observed_at"]
    previous_metrics = previous["metrics"] or {}
    if not isinstance(previous_metrics, dict):
      return

    seconds = (observed_at - previous_at).total_seconds()
    if seconds <= 0 or seconds > MAX_AGGREGATION_INTERVAL_SECONDS:
      return

    pv_kwh = positive_kwh(avg_power(previous_metrics, metrics, "pv_power"), seconds)
    load_kwh = positive_kwh(avg_power(previous_metrics, metrics, "load_power"), seconds)
    grid_avg = avg_power(previous_metrics, metrics, "grid_power")
    battery_avg = avg_power(previous_metrics, metrics, "battery_power")
    ac_input_kwh = positive_kwh(avg_power(previous_metrics, metrics, "ac_input_power"), seconds)
    ac_output_kwh = positive_kwh(avg_power(previous_metrics, metrics, "ac_output_power"), seconds)

    for day, segment_seconds, segment_start, segment_end in local_day_segments(previous_at, observed_at):
      ratio = segment_seconds / seconds
      cur.execute(
        UPSERT_ENERGY_DAILY_SQL,
        (
          device_id,
          day,
          pv_kwh * ratio,
          load_kwh * ratio,
          positive_kwh(grid_avg, segment_seconds),
          negative_kwh(grid_avg, segment_seconds),
          positive_kwh(battery_avg, segment_seconds),
          negative_kwh(battery_avg, segment_seconds),
          ac_input_kwh * ratio,
          ac_output_kwh * ratio,
          segment_seconds,
          segment_start,
          segment_end,
        ),
      )

  def list_devices(self) -> dict[str, Any]:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT device_id, mac, gateway_id, first_seen_at, last_seen_at, metadata
          FROM lumentree_devices
          ORDER BY last_seen_at DESC
          """
        )
        devices = cur.fetchall()
    return {"devices": devices}

  def latest(self, device_id: str) -> dict[str, Any] | None:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT id, device_id, mac, gateway_id, observed_at, firmware, uptime_ms, metrics, raw, payload
          FROM lumentree_telemetry
          WHERE device_id = %s
            AND metrics <> '{}'::jsonb
            AND COALESCE(raw->'decode'->>'data_type', '') = 'main'
          ORDER BY observed_at DESC, id DESC
          LIMIT 1
          """,
          (device_id,),
        )
        latest_main = hydrate_telemetry_metrics(cur.fetchone())
        cur.execute(
          """
          SELECT id, device_id, mac, gateway_id, observed_at, firmware, uptime_ms, metrics, raw, payload
          FROM lumentree_telemetry
          WHERE device_id = %s
            AND (
              COALESCE(raw->'decode'->>'data_type', '') = 'statistics'
              OR COALESCE(raw->>'label', '') = 'today_statistics_0_7'
            )
          ORDER BY observed_at DESC, id DESC
          LIMIT 1
          """,
          (device_id,),
        )
        latest_statistics = hydrate_telemetry_metrics(cur.fetchone())
        merged = merge_latest_telemetry(latest_main, latest_statistics)
        if merged is not None:
          return merged
        cur.execute(
          """
          SELECT id, device_id, mac, gateway_id, observed_at, firmware, uptime_ms, metrics, raw, payload
          FROM lumentree_telemetry
          WHERE device_id = %s
          ORDER BY observed_at DESC, id DESC
          LIMIT 1
          """,
          (device_id,),
        )
        return hydrate_telemetry_metrics(cur.fetchone())

  def latest_settings(self, device_id: str) -> dict[str, Any] | None:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT id, device_id, mac, gateway_id, observed_at, firmware, uptime_ms,
                 raw->'decode'->'diagnostics'->'settings' AS settings,
                 raw->'decode'->'diagnostics'->'setting_registers' AS setting_registers,
                 raw
          FROM lumentree_telemetry
          WHERE device_id = %s
            AND raw->'decode'->>'data_type' = 'settings'
          ORDER BY observed_at DESC, id DESC
          LIMIT 1
          """,
          (device_id,),
        )
        row = cur.fetchone()
    if row is None:
      return None
    settings = row.get("settings") or {}
    setting_registers = row.get("setting_registers") or {}
    if not isinstance(settings, dict):
      settings = {}
    if not isinstance(setting_registers, dict):
      setting_registers = {}
    return {
      "id": row.get("id"),
      "device_id": row.get("device_id"),
      "mac": row.get("mac"),
      "gateway_id": row.get("gateway_id"),
      "observed_at": row.get("observed_at"),
      "firmware": row.get("firmware"),
      "uptime_ms": row.get("uptime_ms"),
      "settings": settings,
      "setting_registers": setting_registers,
      "raw": row.get("raw"),
    }

  def device_health(self, device_id: str) -> dict[str, Any]:
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          WITH latest AS (
            SELECT id, device_id, mac, gateway_id, observed_at, firmware, uptime_ms
            FROM lumentree_telemetry
            WHERE device_id = %s AND metrics <> '{}'::jsonb
            ORDER BY observed_at DESC, id DESC
            LIMIT 1
          ),
          recent_desc AS (
            SELECT observed_at
            FROM lumentree_telemetry
            WHERE device_id = %s AND metrics <> '{}'::jsonb
            ORDER BY observed_at DESC, id DESC
            LIMIT 25
          ),
          recent AS (
            SELECT observed_at
            FROM recent_desc
            ORDER BY observed_at ASC
          ),
          gaps AS (
            SELECT EXTRACT(EPOCH FROM observed_at - LAG(observed_at) OVER (ORDER BY observed_at ASC)) AS gap_seconds
            FROM recent
          ),
          stats AS (
            SELECT
              COUNT(*) FILTER (WHERE observed_at >= NOW() - INTERVAL '1 hour') AS samples_last_hour,
              COUNT(*) AS recent_sample_count
            FROM recent
          ),
          totals AS (
            SELECT COUNT(*) AS total_sample_count
            FROM lumentree_telemetry
            WHERE device_id = %s AND metrics <> '{}'::jsonb
          )
          SELECT
            latest.*,
            EXTRACT(EPOCH FROM NOW() - latest.observed_at) AS latest_age_seconds,
            stats.samples_last_hour,
            stats.recent_sample_count,
            totals.total_sample_count,
            AVG(gaps.gap_seconds) FILTER (
              WHERE gaps.gap_seconds IS NOT NULL AND gaps.gap_seconds <= 120
            ) AS avg_sample_interval_seconds,
            MAX(gaps.gap_seconds) FILTER (WHERE gaps.gap_seconds IS NOT NULL) AS max_sample_gap_seconds,
            MIN(gaps.gap_seconds) FILTER (WHERE gaps.gap_seconds IS NOT NULL) AS min_sample_gap_seconds
          FROM latest
          CROSS JOIN stats
          CROSS JOIN totals
          LEFT JOIN gaps ON TRUE
          GROUP BY
            latest.id, latest.device_id, latest.mac, latest.gateway_id, latest.observed_at,
            latest.firmware, latest.uptime_ms, stats.samples_last_hour,
            stats.recent_sample_count, totals.total_sample_count
          """,
          (device_id, device_id, device_id),
        )
        row = cur.fetchone()

    gateway_status = self.gateway_status_for_device(device_id)

    if row is None:
      return {
        "device_id": device_id,
        "online": False,
        "latest_age_seconds": None,
        "samples_last_hour": 0,
        "recent_sample_count": 0,
        "total_sample_count": 0,
        "pairing_status": gateway_status.get("pairing_status") if gateway_status else "waiting_for_gateway_pairing",
        "gateway_status": gateway_status,
      }

    latest_age = row.get("latest_age_seconds")
    online = latest_age is not None and float(latest_age) <= 120.0
    pairing_status = "paired" if online else "upload_stale"
    if gateway_status and gateway_status.get("pairing_status"):
      pairing_status = gateway_status["pairing_status"]
    return {
      "device_id": device_id,
      "online": online,
      "latest_id": row.get("id"),
      "latest_observed_at": row.get("observed_at"),
      "latest_age_seconds": round(float(latest_age), 1) if latest_age is not None else None,
      "mac": row.get("mac"),
      "gateway_id": row.get("gateway_id"),
      "firmware": row.get("firmware"),
      "uptime_ms": row.get("uptime_ms"),
      "uptime_seconds": round(float(row["uptime_ms"]) / 1000.0, 1) if row.get("uptime_ms") is not None else None,
      "samples_last_hour": int(row.get("samples_last_hour") or 0),
      "recent_sample_count": int(row.get("recent_sample_count") or 0),
      "total_sample_count": int(row.get("total_sample_count") or 0),
      "avg_sample_interval_seconds": round(float(row["avg_sample_interval_seconds"]), 1)
      if row.get("avg_sample_interval_seconds") is not None
      else None,
      "max_sample_gap_seconds": round(float(row["max_sample_gap_seconds"]), 1)
      if row.get("max_sample_gap_seconds") is not None
      else None,
      "min_sample_gap_seconds": round(float(row["min_sample_gap_seconds"]), 1)
      if row.get("min_sample_gap_seconds") is not None
      else None,
      "online_threshold_seconds": 120,
      "pairing_status": pairing_status,
      "gateway_status": gateway_status,
    }

  def events(self, device_id: str | None, limit: int) -> dict[str, Any]:
    limit = max(1, min(limit, 500))
    params: list[Any] = []
    where = ""
    if device_id:
      where = "WHERE device_id = %s"
      params.append(device_id)
    params.append(limit)
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          f"""
          SELECT id, device_id, mac, gateway_id, observed_at, firmware, uptime_ms, metrics, raw
          FROM lumentree_telemetry
          {where}
          ORDER BY observed_at DESC, id DESC
          LIMIT %s
          """,
          params,
        )
        return {"events": cur.fetchall()}

  def energy(self, device_id: str) -> dict[str, Any]:
    now_local = datetime.now(timezone.utc).astimezone(LOCAL_TIMEZONE)
    today = now_local.date()
    month_start, month_end = local_month_range(now_local)
    year_start, year_end = local_year_range(now_local)
    with self.connect() as conn:
      with conn.cursor() as cur:
        cur.execute(
          """
          SELECT *
          FROM lumentree_energy_daily
          WHERE device_id = %s AND day = %s
          """,
          (device_id, today),
        )
        daily = rounded_energy(cur.fetchone())

        cur.execute(
          """
          SELECT
            SUM(pv_kwh) AS pv_kwh,
            SUM(load_kwh) AS load_kwh,
            SUM(grid_in_kwh) AS grid_in_kwh,
            SUM(grid_out_kwh) AS grid_out_kwh,
            SUM(battery_charge_kwh) AS battery_charge_kwh,
            SUM(battery_discharge_kwh) AS battery_discharge_kwh,
            SUM(ac_input_kwh) AS ac_input_kwh,
            SUM(ac_output_kwh) AS ac_output_kwh,
            SUM(sample_count) AS sample_count,
            SUM(covered_seconds) AS covered_seconds,
            MIN(first_observed_at) AS first_observed_at,
            MAX(last_observed_at) AS last_observed_at
          FROM lumentree_energy_daily
          WHERE device_id = %s
            AND day >= %s
            AND day < %s
          """,
          (device_id, month_start, month_end),
        )
        monthly = rounded_energy(cur.fetchone())

        cur.execute(
          """
          SELECT
            SUM(pv_kwh) AS pv_kwh,
            SUM(load_kwh) AS load_kwh,
            SUM(grid_in_kwh) AS grid_in_kwh,
            SUM(grid_out_kwh) AS grid_out_kwh,
            SUM(battery_charge_kwh) AS battery_charge_kwh,
            SUM(battery_discharge_kwh) AS battery_discharge_kwh,
            SUM(ac_input_kwh) AS ac_input_kwh,
            SUM(ac_output_kwh) AS ac_output_kwh,
            SUM(sample_count) AS sample_count,
            SUM(covered_seconds) AS covered_seconds,
            MIN(first_observed_at) AS first_observed_at,
            MAX(last_observed_at) AS last_observed_at
          FROM lumentree_energy_daily
          WHERE device_id = %s
            AND day >= %s
            AND day < %s
          """,
          (device_id, year_start, year_end),
        )
        yearly = rounded_energy(cur.fetchone())

        cur.execute(
          """
          SELECT
            SUM(pv_kwh) AS pv_kwh,
            SUM(load_kwh) AS load_kwh,
            SUM(grid_in_kwh) AS grid_in_kwh,
            SUM(grid_out_kwh) AS grid_out_kwh,
            SUM(battery_charge_kwh) AS battery_charge_kwh,
            SUM(battery_discharge_kwh) AS battery_discharge_kwh,
            SUM(ac_input_kwh) AS ac_input_kwh,
            SUM(ac_output_kwh) AS ac_output_kwh,
            SUM(sample_count) AS sample_count,
            SUM(covered_seconds) AS covered_seconds,
            MIN(first_observed_at) AS first_observed_at,
            MAX(last_observed_at) AS last_observed_at
          FROM lumentree_energy_daily
          WHERE device_id = %s
          """,
          (device_id,),
        )
        total = rounded_energy(cur.fetchone())

    return {
      "device_id": device_id,
      "timezone": LOCAL_TIMEZONE_NAME,
      "daily_reset_time": "00:00",
      "daily": daily,
      "monthly": monthly,
      "yearly": yearly,
      "total": total,
    }


def make_handler(app: LumentreeServer):
  class Handler(BaseHTTPRequestHandler):
    server_version = "LumentreeLocalServer/0.1"

    def log_message(self, fmt: str, *args: Any) -> None:
      print("%s - %s" % (self.address_string(), fmt % args), file=sys.stderr)

    def send_json(self, status: int, payload: dict[str, Any]) -> None:
      body = json.dumps(payload, default=json_default, separators=(",", ":")).encode("utf-8")
      self.send_response(status)
      self.send_header("Content-Type", "application/json")
      self.send_header("Content-Length", str(len(body)))
      self.end_headers()
      self.wfile.write(body)

    def read_json(self) -> dict[str, Any]:
      length = int(self.headers.get("Content-Length", "0"))
      if length <= 0:
        raise ValueError("request body is required")
      raw = self.rfile.read(length)
      payload = json.loads(raw.decode("utf-8"))
      if not isinstance(payload, dict):
        raise ValueError("request body must be a JSON object")
      return payload

    def bearer_token(self) -> str:
      header = self.headers.get("Authorization", "")
      if header.startswith("Bearer "):
        return header[7:].strip()
      return ""

    def has_server_auth(self) -> bool:
      if not app.token:
        return True
      return self.bearer_token() == app.token

    def require_auth(self) -> bool:
      if self.has_server_auth():
        return True
      self.send_json(HTTPStatus.UNAUTHORIZED, {"ok": False, "error": "unauthorized"})
      return False

    def require_device_read_auth(self, device_id: str) -> bool:
      if self.has_server_auth():
        return True
      read_grant = app.validate_read_grant(device_id, self.bearer_token())
      if read_grant is not None:
        return True
      write_grant = app.validate_write_grant(device_id, self.bearer_token())
      if write_grant is not None:
        return True
      self.send_json(HTTPStatus.UNAUTHORIZED, {"ok": False, "error": "unauthorized"})
      return False

    def require_command_auth(self, device_id: str, mode: str) -> bool:
      if self.has_server_auth():
        return True
      grant = app.validate_write_grant(device_id, self.bearer_token())
      if grant is not None:
        if mode == "write" and grant.get("scope") != WRITE_GRANT_SCOPE:
          self.send_json(HTTPStatus.UNAUTHORIZED, {"ok": False, "error": "write grant does not allow production write"})
          return False
        return True
      self.send_json(HTTPStatus.UNAUTHORIZED, {"ok": False, "error": "write grant required"})
      return False

    def do_GET(self) -> None:
      parsed = urlparse(self.path)
      path = parsed.path.rstrip("/") or "/"

      try:
        if path == "/health":
          self.send_json(HTTPStatus.OK, {"ok": True, "database": app.check_database()})
          return

        if path == "/api/lumentree/devices":
          if not self.require_auth():
            return
          self.send_json(HTTPStatus.OK, app.list_devices())
          return

        prefix = "/api/lumentree/devices/"
        suffix = "/latest"
        if path.startswith(prefix) and path.endswith(suffix):
          device_id = unquote(path[len(prefix) : -len(suffix)])
          if not self.require_device_read_auth(device_id):
            return
          latest = app.latest(device_id)
          if latest is None:
            self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "device telemetry not found"})
            return
          self.send_json(HTTPStatus.OK, sanitize_latest_response(latest))
          return

        settings_prefix = "/api/lumentree/devices/"
        settings_suffix = "/settings"
        if path.startswith(settings_prefix) and path.endswith(settings_suffix):
          device_id = unquote(path[len(settings_prefix) : -len(settings_suffix)])
          if not self.require_device_read_auth(device_id):
            return
          settings = app.latest_settings(device_id)
          if settings is None:
            self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "device settings not found"})
            return
          self.send_json(HTTPStatus.OK, sanitize_settings_response(settings))
          return

        energy_prefix = "/api/lumentree/devices/"
        energy_suffix = "/energy"
        if path.startswith(energy_prefix) and path.endswith(energy_suffix):
          device_id = unquote(path[len(energy_prefix) : -len(energy_suffix)])
          if not self.require_device_read_auth(device_id):
            return
          self.send_json(HTTPStatus.OK, app.energy(device_id))
          return

        health_prefix = "/api/lumentree/devices/"
        health_suffix = "/health"
        if path.startswith(health_prefix) and path.endswith(health_suffix):
          device_id = unquote(path[len(health_prefix) : -len(health_suffix)])
          if not self.require_device_read_auth(device_id):
            return
          self.send_json(HTTPStatus.OK, app.device_health(device_id))
          return

        write_status_prefix = "/api/lumentree/devices/"
        write_status_suffix = "/write-grants/status"
        if path.startswith(write_status_prefix) and path.endswith(write_status_suffix):
          device_id = unquote(path[len(write_status_prefix) : -len(write_status_suffix)])
          if not self.require_device_read_auth(device_id):
            return
          self.send_json(HTTPStatus.OK, app.write_grant_status(device_id, self.bearer_token()))
          return

        command_status_prefix = "/api/lumentree/devices/"
        command_status_suffix = "/commands/status"
        if path.startswith(command_status_prefix) and path.endswith(command_status_suffix):
          device_id = unquote(path[len(command_status_prefix) : -len(command_status_suffix)])
          if not self.require_device_read_auth(device_id):
            return
          self.send_json(HTTPStatus.OK, app.command_status(device_id))
          return

        gateway_prefix = "/api/lumentree/gateways/"
        gateway_suffix = "/health"
        if path.startswith(gateway_prefix) and path.endswith(gateway_suffix):
          gateway_id = unquote(path[len(gateway_prefix) : -len(gateway_suffix)])
          status = app.gateway_status(gateway_id)
          if status is None:
            self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "gateway status not found"})
            return
          self.send_json(HTTPStatus.OK, status)
          return

        if path == "/api/lumentree/events":
          if not self.require_auth():
            return
          query = parse_qs(parsed.query)
          device_id = query.get("device_id", [""])[0] or None
          limit = int(query.get("limit", ["100"])[0])
          self.send_json(HTTPStatus.OK, app.events(device_id, limit))
          return

        commands_prefix = "/api/lumentree/devices/"
        commands_suffix = "/commands"
        if path.startswith(commands_prefix) and path.endswith(commands_suffix):
          if not self.require_auth():
            return
          device_id = unquote(path[len(commands_prefix) : -len(commands_suffix)])
          query = parse_qs(parsed.query)
          limit = int(query.get("limit", ["20"])[0])
          self.send_json(HTTPStatus.OK, app.list_commands(device_id, limit))
          return

        gateway_command_prefix = "/api/lumentree/gateways/"
        gateway_command_suffix = "/commands/next"
        if path.startswith(gateway_command_prefix) and path.endswith(gateway_command_suffix):
          if not self.require_auth():
            return
          gateway_id = unquote(path[len(gateway_command_prefix) : -len(gateway_command_suffix)])
          query = parse_qs(parsed.query)
          device_id = query.get("device_id", [""])[0] or None
          self.send_json(HTTPStatus.OK, app.next_command(gateway_id, device_id))
          return

        self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})
      except Exception as exc:
        self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"ok": False, "error": str(exc)})

    def do_POST(self) -> None:
      parsed = urlparse(self.path)
      path = parsed.path.rstrip("/") or "/"

      try:
        if path == "/api/lumentree/events":
          if not self.require_auth():
            return
          self.send_json(HTTPStatus.CREATED, app.store_event(self.read_json()))
          return

        if path == "/api/lumentree/gateways/status":
          if not self.require_auth():
            return
          self.send_json(HTTPStatus.CREATED, app.store_gateway_status(self.read_json()))
          return

        candidate_prefix = "/api/lumentree/gateways/"
        candidate_suffix = "/candidates"
        if path.startswith(candidate_prefix) and path.endswith(candidate_suffix):
          if not self.require_auth():
            return
          gateway_id = unquote(path[len(candidate_prefix) : -len(candidate_suffix)])
          self.send_json(HTTPStatus.CREATED, app.store_gateway_candidates(gateway_id, self.read_json()))
          return

        read_token_prefix = "/api/lumentree/gateways/"
        read_token_suffix = "/read-pairing-token"
        if path.startswith(read_token_prefix) and path.endswith(read_token_suffix):
          if not self.require_auth():
            return
          gateway_id = unquote(path[len(read_token_prefix) : -len(read_token_suffix)])
          self.send_json(HTTPStatus.CREATED, app.create_read_pairing_token(gateway_id, self.read_json()))
          return

        write_code_prefix = "/api/lumentree/gateways/"
        write_code_suffix = "/write-pairing-code"
        if path.startswith(write_code_prefix) and path.endswith(write_code_suffix):
          if not self.require_auth():
            return
          gateway_id = unquote(path[len(write_code_prefix) : -len(write_code_suffix)])
          self.send_json(HTTPStatus.CREATED, app.create_write_pairing_code(gateway_id, self.read_json()))
          return

        claim_prefix = "/api/lumentree/devices/"
        read_claim_suffix = "/read-grants/claim"
        if path.startswith(claim_prefix) and path.endswith(read_claim_suffix):
          device_id = unquote(path[len(claim_prefix) : -len(read_claim_suffix)])
          self.send_json(HTTPStatus.CREATED, app.claim_read_grant(device_id, self.read_json()))
          return

        claim_prefix = "/api/lumentree/devices/"
        claim_suffix = "/write-grants/claim"
        if path.startswith(claim_prefix) and path.endswith(claim_suffix):
          device_id = unquote(path[len(claim_prefix) : -len(claim_suffix)])
          self.send_json(HTTPStatus.CREATED, app.claim_write_grant(device_id, self.read_json()))
          return

        read_revoke_prefix = "/api/lumentree/devices/"
        read_revoke_suffix = "/read-grants/revoke"
        if path.startswith(read_revoke_prefix) and path.endswith(read_revoke_suffix):
          device_id = unquote(path[len(read_revoke_prefix) : -len(read_revoke_suffix)])
          self.send_json(HTTPStatus.OK, app.revoke_read_grant(device_id, self.bearer_token()))
          return

        revoke_prefix = "/api/lumentree/devices/"
        revoke_suffix = "/write-grants/revoke"
        if path.startswith(revoke_prefix) and path.endswith(revoke_suffix):
          device_id = unquote(path[len(revoke_prefix) : -len(revoke_suffix)])
          self.send_json(HTTPStatus.OK, app.revoke_write_grant(device_id, self.bearer_token()))
          return

        if path == "/api/lumentree/commands":
          payload = self.read_json()
          device_id = payload.get("device_id")
          mode = payload.get("mode", "dry_run")
          if not isinstance(device_id, str) or not isinstance(mode, str) or not self.require_command_auth(device_id, mode):
            return
          self.send_json(HTTPStatus.CREATED, app.create_command(payload))
          return

        command_prefix = "/api/lumentree/commands/"
        command_suffix = "/result"
        if path.startswith(command_prefix) and path.endswith(command_suffix):
          if not self.require_auth():
            return
          command_id_text = path[len(command_prefix) : -len(command_suffix)]
          self.send_json(HTTPStatus.OK, app.complete_command(int(command_id_text), self.read_json()))
          return

        self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})
      except json.JSONDecodeError:
        self.send_json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "invalid json"})
      except ValueError as exc:
        self.send_json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": str(exc)})
      except PermissionError as exc:
        self.send_json(HTTPStatus.UNAUTHORIZED, {"ok": False, "error": str(exc)})
      except Exception as exc:
        self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"ok": False, "error": str(exc)})

  return Handler


def main() -> int:
  args = parse_args()
  if args.init_db:
    init_db(args.postgres_dsn)
  app = LumentreeServer(args.postgres_dsn, args.token)
  httpd = ThreadingHTTPServer((args.host, args.port), make_handler(app))
  print(f"listening on http://{args.host}:{args.port}", flush=True)
  httpd.serve_forever()
  return 0


if __name__ == "__main__":
  try:
    raise SystemExit(main())
  except KeyboardInterrupt:
    print("\nserver stopped", file=sys.stderr)
    raise SystemExit(130)
