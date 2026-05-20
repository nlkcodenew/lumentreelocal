#!/usr/bin/env python3
"""Endpoint-level tests for read-grant protected device reads."""

from __future__ import annotations

import json
import threading
import time
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer

from server import make_handler


class FakeApp:
    """Minimal app for handler auth tests."""

    def __init__(self, token: str):
        self.token = token

    def validate_read_grant(self, device_id: str, grant_token: str | None):
        if device_id == "P240819130" and grant_token == "read-secret":
            return {"device_id": device_id, "gateway_id": "esp32-lumentree", "mac": "d8:13:2a:ee:58:d6"}
        return None

    def validate_write_grant(self, device_id: str, grant_token: str | None):
        return None

    def latest(self, device_id: str):
        return {
            "id": 1,
            "device_id": device_id,
            "gateway_id": "esp32-lumentree",
            "observed_at": "2026-05-20T15:00:00Z",
            "firmware": "lumentree-ble-bridge/0.15.1",
            "uptime_ms": 1000,
            "metrics": {"battery_soc": 70},
        }


class FakePairingApp:
    """Minimal app for read pairing fallback tests."""

    def __init__(self):
        self.token = "server-secret"
        self.created = []
        self.claimed = []

    def create_read_pairing_token(self, gateway_id: str, payload: dict):
        if gateway_id != "esp32-lumentree":
            raise ValueError("bad gateway")
        if payload["device_id"] != "P240819130":
            raise ValueError("bad device")
        if payload["mac"] != "d8:13:2a:ee:58:d6":
            raise ValueError("bad mac")
        self.created.append((gateway_id, payload))
        return {"ok": True, "read_pairing_token": {"id": 1}, "grant_token": None}

    def claim_read_grant(self, device_id: str, payload: dict):
        if device_id != "P240819130" or payload.get("token", "").upper() != "PAIR1234":
            raise ValueError("bad claim")
        self.claimed.append((device_id, payload))
        return {"ok": True, "grant_token": "read-secret", "read_grant": {"id": 1}}


def _request(method: str, path: str, *, headers: dict[str, str] | None = None, body: dict | None = None):
    app = FakeApp(token="server-secret")
    handler = make_handler(app)
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    try:
        conn = HTTPConnection("127.0.0.1", httpd.server_port, timeout=5)
        payload = None if body is None else json.dumps(body)
        conn.request(method, path, body=payload, headers=headers or {})
        response = conn.getresponse()
        parsed = json.loads(response.read().decode("utf-8"))
        conn.close()
        return response.status, parsed
    finally:
        httpd.shutdown()
        httpd.server_close()
        thread.join(timeout=5)
        time.sleep(0.05)


def test_latest_requires_read_grant_when_no_server_token():
    status, body = _request("GET", "/api/lumentree/devices/P240819130/latest")
    assert status == 401, body
    assert body["ok"] is False
    assert body["error"] == "unauthorized"


def test_latest_allows_read_grant():
    status, body = _request(
        "GET",
        "/api/lumentree/devices/P240819130/latest",
        headers={"Authorization": "Bearer read-secret"},
    )
    assert status == 200, body
    assert body["device_id"] == "P240819130"
    assert body["metrics"]["battery_soc"] == 70


def test_read_pairing_and_claim_endpoints_exist():
    app = FakePairingApp()
    handler = make_handler(app)
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    try:
        conn = HTTPConnection("127.0.0.1", httpd.server_port, timeout=5)
        conn.request(
            "POST",
            "/api/lumentree/gateways/esp32-lumentree/read-pairing-token",
            body=json.dumps(
                {
                    "device_id": "P240819130",
                    "mac": "d8:13:2a:ee:58:d6",
                    "token": "PAIR1234",
                }
            ),
            headers={"Content-Type": "application/json", "Authorization": "Bearer server-secret"},
        )
        response = conn.getresponse()
        body = json.loads(response.read().decode("utf-8"))
        assert response.status == 201, body
        assert body["ok"] is True
        conn.close()

        conn = HTTPConnection("127.0.0.1", httpd.server_port, timeout=5)
        conn.request(
            "POST",
            "/api/lumentree/devices/P240819130/read-grants/claim",
            body=json.dumps({"token": "pair1234"}),
            headers={"Content-Type": "application/json"},
        )
        response = conn.getresponse()
        body = json.loads(response.read().decode("utf-8"))
        assert response.status == 201, body
        assert body["grant_token"] == "read-secret"
        conn.close()
    finally:
        httpd.shutdown()
        httpd.server_close()
        thread.join(timeout=5)
        time.sleep(0.05)
