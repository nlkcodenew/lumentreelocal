"""Binary sensors for Lumentree Local."""

from __future__ import annotations

from typing import Any

from homeassistant.components.binary_sensor import (
    BinarySensorDeviceClass,
    BinarySensorEntity,
    BinarySensorEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity import EntityCategory
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import LumentreeLocalCoordinator


class LumentreeLocalBinarySensor(CoordinatorEntity[LumentreeLocalCoordinator], BinarySensorEntity):
    """Lumentree Local binary metric sensor."""

    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: LumentreeLocalCoordinator,
        description: BinarySensorEntityDescription,
    ) -> None:
        super().__init__(coordinator)
        self.entity_description = description
        self._device_id = coordinator.device_id
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"
        self._attr_device_info = DeviceInfo(
            identifiers={(DOMAIN, self._device_id)},
            name="Lumentree Local",
            manufacturer="Lumentree",
            model="Local BLE Gateway",
        )

    @property
    def is_on(self) -> bool | None:
        """Return binary sensor state."""
        if self.entity_description.key == "online_status":
            health = self.coordinator.data.get("health", {})
            if isinstance(health, dict) and isinstance(health.get("online"), bool):
                return self.coordinator.last_update_success and health["online"]
            return self.coordinator.last_update_success and bool(self.coordinator.data)
        if self.entity_description.key == "write_grant_active":
            write_grant = self.coordinator.data.get("write_grant", {})
            if isinstance(write_grant, dict):
                return bool(write_grant.get("grant_active"))
            return False
        metrics = self.coordinator.data.get("metrics", {})
        if not isinstance(metrics, dict):
            return None
        value: Any = metrics.get(self.entity_description.key)
        return value if isinstance(value, bool) else None

    @property
    def extra_state_attributes(self) -> dict[str, Any] | None:
        """Return extra attributes for write access guidance."""
        if self.entity_description.key != "write_grant_active":
            return None
        return {
            "write_access_scope": "direct_inverter_write",
            "setup_path": "ESP32 portal -> Write Access -> Generate write pairing code -> Lumentree Local options",
            "warning": "Write-capable entities directly change inverter settings.",
        }


BINARY_SENSORS: tuple[BinarySensorEntityDescription, ...] = (
    BinarySensorEntityDescription(
        key="online_status",
        translation_key="online_status",
        device_class=BinarySensorDeviceClass.CONNECTIVITY,
    ),
    BinarySensorEntityDescription(
        key="is_ups_mode",
        translation_key="is_ups_mode",
        icon="mdi:power-plug-outline",
    ),
    BinarySensorEntityDescription(
        key="write_grant_active",
        translation_key="write_grant_active",
        icon="mdi:shield-key",
    ),
    BinarySensorEntityDescription(
        key="battery_connected",
        translation_key="battery_connected",
        device_class=BinarySensorDeviceClass.PLUG,
        entity_category=EntityCategory.DIAGNOSTIC,
        icon="mdi:battery-check",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up binary sensors."""
    coordinator: LumentreeLocalCoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    async_add_entities(LumentreeLocalBinarySensor(coordinator, description) for description in BINARY_SENSORS)
