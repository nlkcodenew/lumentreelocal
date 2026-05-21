"""Time controls for Lumentree Local."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import time
from typing import Any

from homeassistant.components.time import TimeEntity, TimeEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.entity import EntityCategory
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .api import LumentreeLocalApiError, LumentreeLocalAuthError
from .const import DOMAIN
from .coordinator import LumentreeLocalCoordinator
from .entity_helpers import lumentree_device_info, settings_available, settings_value
from .schedule_safety import apply_schedule_change, schedule_state_from_settings, slot_enabled, validate_schedule_conflicts


@dataclass(frozen=True, kw_only=True)
class LumentreeLocalTimeDescription(TimeEntityDescription):
    """Description of a Lumentree Local time control."""

    slot: int
    field: str
    command_group: str


TIME_DESCRIPTIONS: tuple[LumentreeLocalTimeDescription, ...] = (
    *(
        LumentreeLocalTimeDescription(
            key=f"mains_charge_slot_{slot}_{field}_time",
            translation_key=f"mains_charge_slot_{slot}_{field}_time",
            slot=slot,
            field=field,
            command_group="mains_charge",
            icon="mdi:battery-clock-outline",
            entity_category=EntityCategory.CONFIG,
        )
        for slot in range(1, 3)
        for field in ("start", "end")
    ),
    *(
        LumentreeLocalTimeDescription(
            key=f"discharge_slot_{slot}_{field}_time",
            translation_key=f"discharge_slot_{slot}_{field}_time",
            slot=slot,
            field=field,
            command_group="discharge",
            icon="mdi:clock-edit-outline",
            entity_category=EntityCategory.CONFIG,
        )
        for slot in range(1, 5)
        for field in ("start", "end")
    )
)


def time_to_hhmm(value: time) -> int:
    """Convert a Home Assistant time value to an HHMM register value."""
    return (value.hour * 100) + value.minute


def hhmm_to_time(value: int) -> time | None:
    """Convert an HHMM register value to a Home Assistant time value."""
    hours = value // 100
    minutes = value % 100
    if hours > 23 or minutes > 59:
        return None
    return time(hour=hours, minute=minutes)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up time controls."""
    coordinator: LumentreeLocalCoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    async_add_entities(LumentreeLocalTime(coordinator, description) for description in TIME_DESCRIPTIONS)


class LumentreeLocalTime(CoordinatorEntity[LumentreeLocalCoordinator], TimeEntity):
    """Lumentree Local time control."""

    entity_description: LumentreeLocalTimeDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: LumentreeLocalCoordinator,
        description: LumentreeLocalTimeDescription,
    ) -> None:
        super().__init__(coordinator)
        self.entity_description = description
        self._device_id = coordinator.device_id
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"
        self._attr_device_info = lumentree_device_info(coordinator)
        self._frontend_resync_nonce = 0

    @property
    def native_value(self) -> time | None:
        """Return the latest known time value."""
        value = settings_value(self.coordinator, self.entity_description.key)
        return hhmm_to_time(value) if isinstance(value, int) and not isinstance(value, bool) else None

    @property
    def available(self) -> bool:
        """Return whether a real settings snapshot is available."""
        return super().available and settings_available(self.coordinator) and self.native_value is not None

    async def async_set_value(self, value: time) -> None:
        """Queue the corresponding time write command."""
        hhmm = time_to_hhmm(value)
        try:
            self._raise_if_time_edit_is_unsafe(hhmm)
            if self.entity_description.command_group == "mains_charge":
                await self.coordinator.client.create_mains_charge_time_command(
                    self._device_id,
                    self.entity_description.slot,
                    self.entity_description.field,
                    hhmm,
                )
            else:
                await self.coordinator.client.create_discharge_time_command(
                    self._device_id,
                    self.entity_description.slot,
                    self.entity_description.field,
                    hhmm,
                )
        except LumentreeLocalAuthError as err:
            await self._reassert_snapshot_state()
            raise HomeAssistantError(
                "Write access is required for Lumentree Local write commands. "
                "Generate a write pairing code from the ESP32 portal and enter it "
                "in Lumentree Local options."
            ) from err
        except LumentreeLocalApiError as err:
            await self._reassert_snapshot_state()
            raise HomeAssistantError(str(err)) from err
        except HomeAssistantError:
            await self._reassert_snapshot_state()
            raise

        await self.coordinator.async_request_refresh()

    async def _reassert_snapshot_state(self) -> None:
        self._frontend_resync_nonce += 1
        self.async_write_ha_state()
        await self.coordinator.async_request_refresh()

    def _raise_if_time_edit_is_unsafe(self, hhmm: int) -> None:
        snapshot = self.coordinator.data.get("settings", {})
        settings = snapshot.get("settings", {}) if isinstance(snapshot, dict) else {}
        if not isinstance(settings, dict) or not settings:
            raise HomeAssistantError("Cannot validate schedule safety before the inverter settings snapshot is loaded.")
        state = schedule_state_from_settings(settings)
        group = self.entity_description.command_group
        slot = self.entity_description.slot
        if slot_enabled(state, group, slot):
            if group == "mains_charge":
                raise HomeAssistantError(f"Turn off mains charge slot {slot} before changing its time window.")
            raise HomeAssistantError(f"Turn off discharge slot {slot} before changing its time window.")
        next_state = apply_schedule_change(state, group, slot, self.entity_description.field, hhmm)
        if validate_schedule_conflicts(next_state):
            if group == "mains_charge":
                raise HomeAssistantError(
                    f"Cannot change mains charge slot {slot} {self.entity_description.field} time because the resulting schedule would overlap an enabled discharge window."
                )
            raise HomeAssistantError(
                f"Cannot change discharge slot {slot} {self.entity_description.field} time because the resulting schedule would overlap an enabled mains charge window."
            )

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return diagnostic attributes."""
        return {
            "slot": self.entity_description.slot,
            "field": self.entity_description.field,
            "command": f"set_{self.entity_description.command_group}_time_{self.entity_description.field}",
            "queued_via": "lumentree_command_api",
            "loaded_from_settings_snapshot": self.native_value is not None,
            "write_access_required": True,
            "write_warning": "Changing this entity writes directly to the inverter schedule.",
            "frontend_resync_nonce": self._frontend_resync_nonce,
        }
