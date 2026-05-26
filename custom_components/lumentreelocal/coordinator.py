"""Coordinator for Lumentree Local."""

from __future__ import annotations

from time import monotonic
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
from .const import DEFAULT_SCAN_INTERVAL, DOMAIN, FAST_COMMAND_SCAN_INTERVAL, LOGGER

ENERGY_REFRESH_SECONDS = 60.0
SETTINGS_REFRESH_SECONDS = 300.0
WRITE_GRANT_REFRESH_SECONDS = 60.0
COMMAND_STATUS_REFRESH_SECONDS = 15.0


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
        self._aux_cache: dict[str, dict[str, Any]] = {}
        self._last_fetch_error: str | None = None

    async def _async_update_data(self) -> dict[str, Any]:
        latest = await self._fetch_latest_resilient()
        health = await self._fetch_required_with_cache("health", self.client.device_health, default={"device_id": self.device_id})
        latest["health"] = health

        latest["command_status"] = await self._fetch_optional_with_cache(
            "command_status",
            COMMAND_STATUS_REFRESH_SECONDS,
            self.client.command_status,
            default={"device_id": self.device_id, "has_command": False, "last_command": None},
            swallow_auth=True,
        )
        latest["write_grant"] = await self._fetch_optional_with_cache(
            "write_grant",
            WRITE_GRANT_REFRESH_SECONDS,
            self.client.write_grant_status,
            default={"device_id": self.device_id, "write_available": False, "write_enabled": False, "grant_active": False},
        )
        latest["energy"] = await self._fetch_optional_with_cache(
            "energy",
            ENERGY_REFRESH_SECONDS,
            self.client.energy,
            default={},
        )
        latest["settings"] = await self._fetch_optional_with_cache(
            "settings",
            SETTINGS_REFRESH_SECONDS,
            self.client.settings,
            default={},
            swallow_not_found=True,
        )

        self._adjust_update_interval(latest.get("command_status"))
        if isinstance(health, dict):
            self._maybe_notify_pairing_status(health)
        return latest

    async def _fetch_latest_resilient(self) -> dict[str, Any]:
        """Fetch the latest telemetry without dropping all entities on a transient timeout."""
        try:
            latest = await self.client.latest(self.device_id)
        except LumentreeLocalNotFoundError:
            latest = {
                "device_id": self.device_id,
                "metrics": {},
                "raw": {},
            }
        except LumentreeLocalApiError as err:
            previous = self.data if isinstance(self.data, dict) else None
            if previous:
                self._last_fetch_error = str(err)
                LOGGER.warning(
                    "Latest telemetry fetch failed for %s; keeping previous snapshot: %s",
                    self.device_id,
                    err,
                )
                latest = dict(previous)
            else:
                raise UpdateFailed(str(err)) from err
        else:
            self._last_fetch_error = None

        if not isinstance(latest, dict):
            raise UpdateFailed("latest telemetry response is invalid")
        latest.setdefault("device_id", self.device_id)
        latest.setdefault("metrics", {})
        latest.setdefault("raw", {})
        return latest

    async def _fetch_required_with_cache(
        self,
        cache_key: str,
        fetcher,
        *,
        default: dict[str, Any],
    ) -> dict[str, Any]:
        """Fetch a required surface but keep the last good value on transient failures."""
        try:
            data = await fetcher(self.device_id)
        except LumentreeLocalApiError as err:
            cached = self._cached_value(cache_key)
            if isinstance(cached, dict):
                LOGGER.warning(
                    "Required %s fetch failed for %s; keeping cached data: %s",
                    cache_key,
                    self.device_id,
                    err,
                )
                return cached
            if isinstance(self.data, dict):
                previous = self.data.get(cache_key)
                if isinstance(previous, dict):
                    LOGGER.warning(
                        "Required %s fetch failed for %s; reusing previous coordinator data: %s",
                        cache_key,
                        self.device_id,
                        err,
                    )
                    return previous
            LOGGER.warning(
                "Required %s fetch failed for %s; falling back to default payload: %s",
                cache_key,
                self.device_id,
                err,
            )
            return dict(default)

        if not isinstance(data, dict):
            LOGGER.warning("%s fetch returned invalid payload for %s; using default payload", cache_key, self.device_id)
            return dict(default)
        self._store_cached_value(cache_key, data)
        return data

    async def _fetch_optional_with_cache(
        self,
        cache_key: str,
        refresh_seconds: float,
        fetcher,
        *,
        default: dict[str, Any],
        swallow_not_found: bool = False,
        swallow_auth: bool = False,
    ) -> dict[str, Any]:
        """Fetch a slower-changing surface on its own cadence with cache fallback."""
        cached = self._cached_value(cache_key)
        if self._cache_is_fresh(cache_key, refresh_seconds) and isinstance(cached, dict):
            return cached

        try:
            data = await fetcher(self.device_id)
        except LumentreeLocalNotFoundError:
            if swallow_not_found:
                data = dict(default)
            else:
                raise
        except LumentreeLocalAuthError:
            if swallow_auth:
                data = dict(default)
            else:
                raise
        except LumentreeLocalApiError as err:
            if isinstance(cached, dict):
                LOGGER.warning(
                    "Optional %s fetch failed for %s; keeping cached data: %s",
                    cache_key,
                    self.device_id,
                    err,
                )
                return cached
            if isinstance(self.data, dict):
                previous = self.data.get(cache_key)
                if isinstance(previous, dict):
                    LOGGER.warning(
                        "Optional %s fetch failed for %s; reusing previous coordinator data: %s",
                        cache_key,
                        self.device_id,
                        err,
                    )
                    return previous
            LOGGER.warning(
                "Optional %s fetch failed for %s; falling back to default payload: %s",
                cache_key,
                self.device_id,
                err,
            )
            return dict(default)

        if not isinstance(data, dict):
            LOGGER.warning("%s fetch returned invalid payload for %s; using cached/default payload", cache_key, self.device_id)
            if isinstance(cached, dict):
                return cached
            return dict(default)
        self._store_cached_value(cache_key, data)
        return data

    def _cache_is_fresh(self, cache_key: str, refresh_seconds: float) -> bool:
        cache_entry = self._aux_cache.get(cache_key)
        if not isinstance(cache_entry, dict):
            return False
        fetched_at = cache_entry.get("fetched_at")
        if not isinstance(fetched_at, float):
            return False
        return monotonic() - fetched_at < refresh_seconds

    def _cached_value(self, cache_key: str) -> dict[str, Any] | None:
        cache_entry = self._aux_cache.get(cache_key)
        if not isinstance(cache_entry, dict):
            return None
        value = cache_entry.get("value")
        return value if isinstance(value, dict) else None

    def _store_cached_value(self, cache_key: str, value: dict[str, Any]) -> None:
        self._aux_cache[cache_key] = {
            "fetched_at": monotonic(),
            "value": value,
        }

    def _adjust_update_interval(self, command_status: dict[str, Any] | None) -> None:
        """Speed up polling while a write command is still pending."""
        next_interval = DEFAULT_SCAN_INTERVAL
        if isinstance(command_status, dict):
            command = command_status.get("last_command")
            if isinstance(command, dict) and command.get("status") in {"requested", "sent"}:
                next_interval = FAST_COMMAND_SCAN_INTERVAL
        self.update_interval = next_interval

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
