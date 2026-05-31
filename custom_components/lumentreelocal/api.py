"""Client for the local Lumentree API server."""

from __future__ import annotations

from collections.abc import AsyncIterator
import json
from typing import Any

from aiohttp import ClientError, ClientSession


class LumentreeLocalApiError(Exception):
    """Base API error."""


class LumentreeLocalAuthError(LumentreeLocalApiError):
    """Authentication failed."""


class LumentreeLocalNotFoundError(LumentreeLocalApiError):
    """Requested resource was not found."""


class LumentreeLocalApiClient:
    """Small async client for the local server."""

    def __init__(
        self,
        session: ClientSession,
        api_url: str,
        read_token: str | None = None,
        write_token: str | None = None,
    ) -> None:
        self._session = session
        self._api_url = api_url.rstrip("/")
        self._read_token = read_token or ""
        self._write_token = write_token or ""

    @property
    def api_url(self) -> str:
        """Return configured API URL."""
        return self._api_url

    def _headers(self, token: str | None = None) -> dict[str, str]:
        headers: dict[str, str] = {}
        effective_token = token if token is not None else self._read_token
        if effective_token:
            headers["Authorization"] = f"Bearer {effective_token}"
        return headers

    @staticmethod
    def _summarize_error_text(text: str) -> str:
        """Collapse huge HTML/proxy failures into a short readable message."""
        compact = " ".join(text.split())
        if not compact:
            return ""
        lowered = compact.lower()
        if "<html" in lowered and "bad gateway" in lowered:
            return "bad gateway HTML response"
        if len(compact) > 200:
            return compact[:200].rstrip() + "..."
        return compact

    async def _get(self, path: str, token: str | None = None) -> dict[str, Any]:
        url = f"{self._api_url}{path}"
        try:
            async with self._session.get(url, headers=self._headers(token), timeout=10) as response:
                if response.status in (401, 403):
                    raise LumentreeLocalAuthError("invalid API token")
                if response.status == 404:
                    text = await response.text()
                    raise LumentreeLocalNotFoundError(
                        f"GET {path} failed: 404 {self._summarize_error_text(text)}"
                    )
                if response.status >= 400:
                    text = await response.text()
                    raise LumentreeLocalApiError(
                        f"GET {path} failed: {response.status} {self._summarize_error_text(text)}"
                    )
                data = await response.json()
        except LumentreeLocalApiError:
            raise
        except (ClientError, TimeoutError) as err:
            raise LumentreeLocalApiError(f"cannot connect to local Lumentree server: {err}") from err

        if not isinstance(data, dict):
            raise LumentreeLocalApiError(f"GET {path} returned non-object JSON")
        return data

    async def _post(self, path: str, payload: dict[str, Any], token: str | None = None) -> dict[str, Any]:
        url = f"{self._api_url}{path}"
        try:
            async with self._session.post(url, headers=self._headers(token), json=payload, timeout=10) as response:
                if response.status in (401, 403):
                    raise LumentreeLocalAuthError("invalid API token")
                if response.status == 404:
                    text = await response.text()
                    raise LumentreeLocalNotFoundError(
                        f"POST {path} failed: 404 {self._summarize_error_text(text)}"
                    )
                if response.status >= 400:
                    text = await response.text()
                    raise LumentreeLocalApiError(
                        f"POST {path} failed: {response.status} {self._summarize_error_text(text)}"
                    )
                data = await response.json()
        except LumentreeLocalApiError:
            raise
        except (ClientError, TimeoutError) as err:
            raise LumentreeLocalApiError(f"cannot connect to local Lumentree server: {err}") from err

        if not isinstance(data, dict):
            raise LumentreeLocalApiError(f"POST {path} returned non-object JSON")
        return data

    async def health(self) -> dict[str, Any]:
        """Fetch health status."""
        return await self._get("/health")

    async def devices(self) -> list[dict[str, Any]]:
        """Fetch known devices."""
        data = await self._get("/api/lumentree/devices")
        devices = data.get("devices", [])
        if not isinstance(devices, list):
            raise LumentreeLocalApiError("devices response is invalid")
        return [item for item in devices if isinstance(item, dict)]

    async def latest(self, device_id: str) -> dict[str, Any]:
        """Fetch latest metrics for one device."""
        return await self._get(f"/api/lumentree/devices/{device_id}/latest")

    async def latest_stream(self, device_id: str) -> AsyncIterator[dict[str, Any]]:
        """Stream latest telemetry updates via SSE."""
        url = f"{self._api_url}/api/lumentree/devices/{device_id}/stream"
        try:
            async with self._session.get(url, headers=self._headers(), timeout=None) as response:
                if response.status in (401, 403):
                    raise LumentreeLocalAuthError("invalid API token")
                if response.status >= 400:
                    text = await response.text()
                    raise LumentreeLocalApiError(
                        f"GET /api/lumentree/devices/{device_id}/stream failed: "
                        f"{response.status} {self._summarize_error_text(text)}"
                    )

                event_name = "message"
                data_lines: list[str] = []
                async for raw_line in response.content:
                    line = raw_line.decode("utf-8", errors="ignore").strip("\r\n")
                    if line == "":
                        if data_lines:
                            payload_text = "\n".join(data_lines)
                            try:
                                payload = json.loads(payload_text)
                            except json.JSONDecodeError:
                                payload = {"raw": payload_text}
                            yield {"event": event_name, "data": payload}
                        event_name = "message"
                        data_lines = []
                        continue
                    if line.startswith(":"):
                        continue
                    if line.startswith("event:"):
                        event_name = line[6:].strip() or "message"
                        continue
                    if line.startswith("data:"):
                        data_lines.append(line[5:].lstrip())
        except LumentreeLocalApiError:
            raise
        except (ClientError, TimeoutError) as err:
            raise LumentreeLocalApiError(f"cannot connect to local Lumentree server: {err}") from err

    async def settings(self, device_id: str) -> dict[str, Any]:
        """Fetch latest settings snapshot for one device."""
        return await self._get(f"/api/lumentree/devices/{device_id}/settings")

    async def energy(self, device_id: str) -> dict[str, Any]:
        """Fetch aggregated energy metrics for one device."""
        return await self._get(f"/api/lumentree/devices/{device_id}/energy")

    async def device_health(self, device_id: str) -> dict[str, Any]:
        """Fetch telemetry health diagnostics for one device."""
        return await self._get(f"/api/lumentree/devices/{device_id}/health")

    async def write_grant_status(self, device_id: str) -> dict[str, Any]:
        """Fetch write grant status for one device."""
        return await self._get(
            f"/api/lumentree/devices/{device_id}/write-grants/status",
            token=self._write_token or None,
        )

    async def command_status(self, device_id: str) -> dict[str, Any]:
        """Fetch latest redacted command status for one device."""
        return await self._get(f"/api/lumentree/devices/{device_id}/commands/status")

    async def claim_read_grant(self, device_id: str, token: str) -> dict[str, Any]:
        """Claim a scoped read grant using a one-time ESP32 portal token."""
        return await self._post(
            f"/api/lumentree/devices/{device_id}/read-grants/claim",
            {"token": token},
        )

    async def claim_write_grant(self, device_id: str, code: str) -> dict[str, Any]:
        """Claim a scoped write grant using a one-time ESP32 portal code."""
        return await self._post(
            f"/api/lumentree/devices/{device_id}/write-grants/claim",
            {"code": code},
        )

    async def revoke_write_grant(self, device_id: str) -> dict[str, Any]:
        """Revoke the configured scoped write grant."""
        return await self._post(
            f"/api/lumentree/devices/{device_id}/write-grants/revoke",
            {},
            token=self._write_token,
        )

    async def revoke_read_grant(self, device_id: str) -> dict[str, Any]:
        """Revoke the configured scoped read grant."""
        return await self._post(
            f"/api/lumentree/devices/{device_id}/read-grants/revoke",
            {},
            token=self._read_token,
        )

    async def create_dry_run_command(
        self,
        device_id: str,
        command: str = "dry_run_noop",
        payload: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        """Create a dry-run command."""
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": command,
                "mode": "dry_run",
                "requested_by": "home_assistant",
                "payload": payload or {},
            },
            token=self._write_token,
        )

    async def create_target_soc_command(self, device_id: str, target_soc: int) -> dict[str, Any]:
        """Create a guarded first target SOC write command."""
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": "set_first_discharge_target_soc",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"target_soc": target_soc},
            },
            token=self._write_token,
        )

    async def create_discharge_target_soc_command(self, device_id: str, slot: int, target_soc: int) -> dict[str, Any]:
        """Create a guarded discharge target SOC write command."""
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": "set_discharge_target_soc",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"slot": slot, "target_soc": target_soc},
            },
            token=self._write_token,
        )

    async def create_discharge_power_command(self, device_id: str, slot: int, power: int) -> dict[str, Any]:
        """Create a guarded discharge power write command."""
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": "set_discharge_power",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"slot": slot, "power": power},
            },
            token=self._write_token,
        )

    async def create_discharge_time_enable_command(self, device_id: str, slot: int, enabled: bool) -> dict[str, Any]:
        """Create a guarded discharge time enable write command."""
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": "set_discharge_time_enable",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"slot": slot, "enabled": 1 if enabled else 0},
            },
            token=self._write_token,
        )

    async def create_discharge_time_command(
        self,
        device_id: str,
        slot: int,
        field: str,
        time_value: int,
    ) -> dict[str, Any]:
        """Create a guarded discharge time start/end write command."""
        if field not in {"start", "end"}:
            raise ValueError("field must be start or end")
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": f"set_discharge_time_{field}",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"slot": slot, "time": time_value},
            },
            token=self._write_token,
        )

    async def create_mains_charge_target_soc_command(self, device_id: str, slot: int, target_soc: int) -> dict[str, Any]:
        """Create a guarded mains-charge target SOC write command."""
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": "set_mains_charge_target_soc",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"slot": slot, "target_soc": target_soc},
            },
            token=self._write_token,
        )

    async def create_mains_charge_time_enable_command(self, device_id: str, slot: int, enabled: bool) -> dict[str, Any]:
        """Create a guarded mains-charge time enable write command."""
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": "set_mains_charge_time_enable",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"slot": slot, "enabled": 1 if enabled else 0},
            },
            token=self._write_token,
        )

    async def create_mains_charge_time_command(
        self,
        device_id: str,
        slot: int,
        field: str,
        time_value: int,
    ) -> dict[str, Any]:
        """Create a guarded mains-charge time start/end write command."""
        if field not in {"start", "end"}:
            raise ValueError("field must be start or end")
        return await self._post(
            "/api/lumentree/commands",
            {
                "device_id": device_id,
                "command": f"set_mains_charge_time_{field}",
                "mode": "write",
                "requested_by": "home_assistant",
                "payload": {"slot": slot, "time": time_value},
            },
            token=self._write_token,
        )
