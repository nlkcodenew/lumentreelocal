"""Shared entity helpers for Lumentree Local."""

from __future__ import annotations

from homeassistant.helpers.device_registry import DeviceInfo

from .const import DOMAIN
from .coordinator import LumentreeLocalCoordinator


def lumentree_device_info(coordinator: LumentreeLocalCoordinator) -> DeviceInfo:
    """Return the shared Lumentree Local device info."""
    return DeviceInfo(
        identifiers={(DOMAIN, coordinator.device_id)},
        name="Lumentree Local",
        manufacturer="Lumentree",
        model="Local BLE Gateway",
    )


def settings_value(coordinator: LumentreeLocalCoordinator, key: str):
    """Return a value from the latest settings snapshot."""
    snapshot = coordinator.data.get("settings", {})
    if not isinstance(snapshot, dict):
        return None
    settings = snapshot.get("settings", {})
    if not isinstance(settings, dict):
        return None
    return settings.get(key)


def settings_available(coordinator: LumentreeLocalCoordinator) -> bool:
    """Return whether a settings snapshot is available."""
    snapshot = coordinator.data.get("settings", {})
    if not isinstance(snapshot, dict):
        return False
    settings = snapshot.get("settings", {})
    return isinstance(settings, dict) and bool(settings)
