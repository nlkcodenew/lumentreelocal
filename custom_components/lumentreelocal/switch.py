"""Switch controls for Lumentree Local."""

from __future__ import annotations

from dataclasses import dataclass
from time import monotonic
from typing import Any

from homeassistant.components.switch import SwitchEntity, SwitchEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.entity import EntityCategory
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .api import LumentreeLocalApiError, LumentreeLocalAuthError
from .const import DOMAIN, SWITCH_WRITE_CONFIRM_TIMEOUT_SECONDS
from .coordinator import LumentreeLocalCoordinator
from .entity_helpers import lumentree_device_info, settings_available, settings_value


@dataclass(frozen=True, kw_only=True)
class LumentreeLocalSwitchDescription(SwitchEntityDescription):
    """Description of a Lumentree Local switch control."""

    slot: int
    command_group: str


SWITCH_DESCRIPTIONS: tuple[LumentreeLocalSwitchDescription, ...] = (
    *(
        LumentreeLocalSwitchDescription(
            key=f"mains_charge_slot_{slot}_enabled",
            translation_key=f"mains_charge_slot_{slot}_enabled",
            slot=slot,
            command_group="mains_charge",
            icon="mdi:battery-plus-outline",
            entity_category=EntityCategory.CONFIG,
        )
        for slot in range(1, 3)
    ),
    *(
        LumentreeLocalSwitchDescription(
            key=f"discharge_slot_{slot}_enabled",
            translation_key=f"discharge_slot_{slot}_enabled",
            slot=slot,
            command_group="discharge",
            icon="mdi:timer-check-outline",
            entity_category=EntityCategory.CONFIG,
        )
        for slot in range(1, 5)
    )
)


def _hhmm_to_minutes(value: int) -> int:
    return (value // 100 * 60) + (value % 100)


def _expand_window(start: int, end: int) -> list[tuple[int, int]]:
    start_minutes = _hhmm_to_minutes(start)
    end_minutes = _hhmm_to_minutes(end)
    if end_minutes <= start_minutes:
        return [(start_minutes, 24 * 60), (0, end_minutes)]
    return [(start_minutes, end_minutes)]


def _windows_overlap(left_start: int, left_end: int, right_start: int, right_end: int) -> bool:
    for left_begin, left_finish in _expand_window(left_start, left_end):
        for right_begin, right_finish in _expand_window(right_start, right_end):
            if left_begin < right_finish and right_begin < left_finish:
                return True
    return False


def _format_hhmm(value: int) -> str:
    return f"{value // 100:02d}:{value % 100:02d}"


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up switch controls."""
    coordinator: LumentreeLocalCoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    async_add_entities(LumentreeLocalSwitch(coordinator, description) for description in SWITCH_DESCRIPTIONS)


class LumentreeLocalSwitch(CoordinatorEntity[LumentreeLocalCoordinator], SwitchEntity):
    """Lumentree Local switch control."""

    entity_description: LumentreeLocalSwitchDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: LumentreeLocalCoordinator,
        description: LumentreeLocalSwitchDescription,
    ) -> None:
        super().__init__(coordinator)
        self.entity_description = description
        self._device_id = coordinator.device_id
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"
        self._attr_device_info = lumentree_device_info(coordinator)
        self._pending_enabled: bool | None = None
        self._pending_started_monotonic: float | None = None

    def _snapshot_value(self) -> bool | None:
        value = settings_value(self.coordinator, self.entity_description.key)
        return value if isinstance(value, bool) else None

    def _pending_active(self) -> bool:
        if self._pending_enabled is None or self._pending_started_monotonic is None:
            return False
        if monotonic() - self._pending_started_monotonic > SWITCH_WRITE_CONFIRM_TIMEOUT_SECONDS:
            self._pending_enabled = None
            self._pending_started_monotonic = None
            return False
        return True

    def _sync_pending_with_snapshot(self) -> None:
        if self._pending_enabled is None:
            return
        snapshot_value = self._snapshot_value()
        if snapshot_value is not None and snapshot_value == self._pending_enabled:
            self._pending_enabled = None
            self._pending_started_monotonic = None

    @property
    def is_on(self) -> bool | None:
        """Return the latest known switch state."""
        self._sync_pending_with_snapshot()
        if self._pending_active():
            return self._pending_enabled
        return self._snapshot_value()

    @property
    def available(self) -> bool:
        """Return whether a real settings snapshot is available."""
        return super().available and settings_available(self.coordinator) and self.is_on is not None

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Queue an enable command."""
        await self._set_enabled(True)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Queue a disable command."""
        await self._set_enabled(False)

    async def _set_enabled(self, enabled: bool) -> None:
        if self._pending_active():
            raise HomeAssistantError("A previous switch change is still waiting for inverter confirmation.")
        if enabled and self.entity_description.command_group == "mains_charge":
            self._raise_if_mains_charge_overlaps_discharge()
        try:
            if self.entity_description.command_group == "mains_charge":
                await self.coordinator.client.create_mains_charge_time_enable_command(
                    self._device_id,
                    self.entity_description.slot,
                    enabled,
                )
            else:
                await self.coordinator.client.create_discharge_time_enable_command(
                    self._device_id,
                    self.entity_description.slot,
                    enabled,
                )
        except LumentreeLocalAuthError as err:
            raise HomeAssistantError(
                "Write access is required for Lumentree Local write commands. "
                "Generate a write pairing code from the ESP32 portal and enter it "
                "in Lumentree Local options."
            ) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

        self._pending_enabled = enabled
        self._pending_started_monotonic = monotonic()
        self.async_write_ha_state()
        await self.coordinator.async_request_refresh()

    def _handle_coordinator_update(self) -> None:
        self._sync_pending_with_snapshot()
        super()._handle_coordinator_update()

    def _raise_if_mains_charge_overlaps_discharge(self) -> None:
        charge_start = settings_value(self.coordinator, f"mains_charge_slot_{self.entity_description.slot}_start_time")
        charge_end = settings_value(self.coordinator, f"mains_charge_slot_{self.entity_description.slot}_end_time")
        if not isinstance(charge_start, int) or not isinstance(charge_end, int):
            raise HomeAssistantError("Cannot enable mains charge before its time window is loaded from settings.")

        overlapping_slots: list[str] = []
        for slot in range(1, 5):
            enabled = settings_value(self.coordinator, f"discharge_slot_{slot}_enabled")
            if enabled is not True:
                continue
            discharge_start = settings_value(self.coordinator, f"discharge_slot_{slot}_start_time")
            discharge_end = settings_value(self.coordinator, f"discharge_slot_{slot}_end_time")
            if not isinstance(discharge_start, int) or not isinstance(discharge_end, int):
                raise HomeAssistantError("Cannot enable mains charge before discharge schedules are loaded from settings.")
            if _windows_overlap(charge_start, charge_end, discharge_start, discharge_end):
                overlapping_slots.append(
                    f"discharge slot {slot} ({_format_hhmm(discharge_start)}-{_format_hhmm(discharge_end)})"
                )

        if overlapping_slots:
            raise HomeAssistantError(
                "Cannot enable mains charge because it overlaps enabled "
                f"{', '.join(overlapping_slots)}. Disable or adjust the overlapping discharge schedule first."
            )

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return diagnostic attributes."""
        return {
            "slot": self.entity_description.slot,
            "command": f"set_{self.entity_description.command_group}_time_enable",
            "queued_via": "lumentree_command_api",
            "loaded_from_settings_snapshot": self.is_on is not None,
            "write_access_required": True,
            "write_warning": "Changing this entity writes directly to the inverter schedule.",
            "pending_confirmation": self._pending_active(),
        }
