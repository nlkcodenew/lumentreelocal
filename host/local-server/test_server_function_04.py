#!/usr/bin/env python3
"""Endpoint-level test for function 0x04 statistics ingestion."""

from __future__ import annotations

import json
import threading
import time
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer

from server import decode_payload, make_handler


FUNCTION_04_PAYLOAD_HEX = "010410007d005900200073004e00400000000040ad"


class FakeApp:
    """Small in-memory app to exercise the HTTP handler without Postgres."""

    def __init__(self, token: str):
        self.token = token
        self.rows: list[dict] = []

    def store_event(self, payload: dict) -> dict:
        raw = payload.get("raw") or {}
        decoded = decode_payload(raw, payload)
        if decoded is None:
            raise AssertionError("expected decoded payload")
        row = {
            "device_id": payload["device_id"],
            "metrics": decoded["metrics"],
            "raw": {
                **raw,
                "decode": {
                    "data_type": decoded["data_type"],
                    "diagnostics": decoded["diagnostics"],
                },
            },
        }
        self.rows.append(row)
        return {"ok": True, "id": len(self.rows), "device_id": payload["device_id"]}


def test_post_events_decodes_function_04_statistics():
    app = FakeApp(token="secret-token")
    handler = make_handler(app)
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()

    try:
        payload = {
            "device_id": "P240819130",
            "gateway_id": "esp32-lumentree",
            "raw": {
                "payload_hex": FUNCTION_04_PAYLOAD_HEX,
                "start_register": 0,
            },
        }

        conn = HTTPConnection("127.0.0.1", httpd.server_port, timeout=5)
        conn.request(
            "POST",
            "/api/lumentree/events",
            body=json.dumps(payload),
            headers={
                "Authorization": "Bearer secret-token",
                "Content-Type": "application/json",
            },
        )
        response = conn.getresponse()
        body = json.loads(response.read().decode("utf-8"))
        conn.close()

        assert response.status == 201, body
        assert body["ok"] is True
        assert len(app.rows) == 1

        row = app.rows[0]
        assert row["raw"]["decode"]["data_type"] == "statistics"
        assert row["metrics"]["today_pv_generation_kwh"] == 12.5
        assert row["metrics"]["today_essential_load_kwh"] == 8.9
        assert row["metrics"]["today_grid_import_kwh"] == 3.2
        assert row["metrics"]["today_total_load_kwh"] == 11.5
        assert row["metrics"]["today_battery_charge_kwh"] == 7.8
        assert row["metrics"]["today_battery_discharge_kwh"] == 6.4
    finally:
        httpd.shutdown()
        httpd.server_close()
        thread.join(timeout=5)
        time.sleep(0.05)
