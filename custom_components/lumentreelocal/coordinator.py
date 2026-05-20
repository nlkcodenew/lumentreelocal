"""Coordinator for Lumentree Local."""

from __future__ import annotations

from typing import Any

from homeassistant.components import persistent_notification
from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .api import (
    LumentreeLocalAuthError,
    LumentreeLocalApiClient,
    LumentreeLocalApiError,
    LumentreeLocalNotFoundError,
)
from .const import DEFAULT_SCAN_INTERVAL, DOMAIN, LOGGER


class LumentreeLocalCoordinator(DataUpdateCoordinator[dict[str, Any]]):
    """Fetch latest telemetry from the local server."""

    def __init__(
        self,
        hass: HomeAssistant,
        client: LumentreeLocalApiClient,
        device_id: str,
    ) -> None:
        super().__init__(
            hass,
            LOGGER,
            name=f"{DOMAIN}_{device_id}",
            update_interval=DEFAULT_SCAN_INTERVAL,
        )
        self.client = client
        self.device_id = device_id
        self._last_pairing_status: str | None = None

    async def _async_update_data(self) -> dict[str, Any]:
        try:
            try:
                latest = await self.client.latest(self.device_id)
            except LumentreeLocalNotFoundError:
                latest = {
                    "device_id": self.device_id,
                    "metrics": {},
                    "raw": {},
                }
            latest["energy"] = await self.client.energy(self.device_id)
            latest["health"] = await self.client.device_health(self.device_id)
            latest["write_grant"] = await self.client.write_grant_status(self.device_id)
            try:
                latest["command_status"] = await self.client.command_status(self.device_id)
            except LumentreeLocalAuthError:
                latest["command_status"] = {"device_id": self.device_id, "has_command": False, "last_command": None}
            try:
                latest["settings"] = await self.client.settings(self.device_id)
            except LumentreeLocalNotFoundError:
                latest["settings"] = {}
            self._maybe_notify_pairing_status(latest["health"])
            return latest
        except LumentreeLocalApiError as err:
            raise UpdateFailed(str(err)) from err

    def _maybe_notify_pairing_status(self, health: dict[str, Any]) -> None:
        """Create one-shot user notifications for actionable pairing states."""
        status = health.get("pairing_status")
        if not isinstance(status, str):
            return
        previous = self._last_pairing_status
        self._last_pairing_status = status
        if previous == status:
            return

        if status == "scanning_no_candidate":
            persistent_notification.async_create(
                self.hass,
                (
                    "No inverter candidate found. Check the Device ID, move ESP32 "
                    "closer to the inverter, then rescan from the ESP32 portal. "
                    "You can change the Device ID from Lumentree Local options."
                ),
                title="Lumentree Local pairing",
                notification_id=f"{DOMAIN}_{self.device_id}_pairing_no_candidate",
            )
        elif status == "multiple_candidates":
            persistent_notification.async_create(
                self.hass,
                (
                    "Multiple inverter BLE candidates were found. Open the ESP32 "
                    "portal, choose the correct inverter, then let telemetry upload resume."
                ),
                title="Lumentree Local pairing",
                notification_id=f"{DOMAIN}_{self.device_id}_pairing_multiple_candidates",
            )
