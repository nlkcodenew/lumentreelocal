"""Number controls for Lumentree Local."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from homeassistant.components.number import NumberEntity, NumberEntityDescription, NumberMode
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import PERCENTAGE, UnitOfPower
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.entity import EntityCategory
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .api import LumentreeLocalApiError, LumentreeLocalAuthError
from .const import DOMAIN
from .coordinator import LumentreeLocalCoordinator
from .entity_helpers import lumentree_device_info, settings_available, settings_value


@dataclass(frozen=True, kw_only=True)
class LumentreeLocalNumberDescription(NumberEntityDescription):
    """Description of a Lumentree Local number control."""

    command: str
    slot: int | None = None
    settings_key: str | None = None


def _discharge_number_descriptions() -> tuple[LumentreeLocalNumberDescription, ...]:
    descriptions: list[LumentreeLocalNumberDescription] = []
    for slot in range(1, 5):
        descriptions.append(
            LumentreeLocalNumberDescription(
                key=f"discharge_slot_{slot}_power",
                translation_key=f"discharge_slot_{slot}_power",
                command="discharge_power",
                slot=slot,
                native_min_value=500,
                native_max_value=5000,
                native_step=50,
                native_unit_of_measurement=UnitOfPower.WATT,
                mode=NumberMode.BOX,
                entity_category=EntityCategory.CONFIG,
            )
        )
        descriptions.append(
            LumentreeLocalNumberDescription(
                key=f"discharge_slot_{slot}_target_soc",
                translation_key=f"discharge_slot_{slot}_target_soc",
                command="target_soc",
                slot=slot,
                settings_key="first_discharge_target_soc" if slot == 1 else None,
                native_min_value=5,
                native_max_value=100,
                native_step=1,
                native_unit_of_measurement=PERCENTAGE,
                mode=NumberMode.SLIDER,
                entity_category=EntityCategory.CONFIG,
            )
        )
    return tuple(descriptions)


NUMBER_DESCRIPTIONS: tuple[LumentreeLocalNumberDescription, ...] = (
    *(
        LumentreeLocalNumberDescription(
            key=f"mains_charge_slot_{slot}_target_soc",
            translation_key=f"mains_charge_slot_{slot}_target_soc",
            command="mains_charge_target_soc",
            slot=slot,
            native_min_value=5,
            native_max_value=100,
            native_step=1,
            native_unit_of_measurement=PERCENTAGE,
            mode=NumberMode.SLIDER,
            entity_category=EntityCategory.CONFIG,
        )
        for slot in range(1, 3)
    ),
    *_discharge_number_descriptions(),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up number controls."""
    coordinator: LumentreeLocalCoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    async_add_entities(LumentreeLocalNumber(coordinator, description) for description in NUMBER_DESCRIPTIONS)


class LumentreeLocalNumber(CoordinatorEntity[LumentreeLocalCoordinator], NumberEntity):
    """Lumentree Local number control."""

    entity_description: LumentreeLocalNumberDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: LumentreeLocalCoordinator,
        description: LumentreeLocalNumberDescription,
    ) -> None:
        super().__init__(coordinator)
        self.entity_description = description
        self._device_id = coordinator.device_id
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"
        self._attr_device_info = lumentree_device_info(coordinator)

    def _settings_key(self) -> str:
        return self.entity_description.settings_key or self.entity_description.key

    @property
    def native_value(self) -> float | None:
        """Return the latest known control value."""
        value = settings_value(self.coordinator, self._settings_key())
        return float(value) if isinstance(value, int | float) and not isinstance(value, bool) else None

    @property
    def available(self) -> bool:
        """Return whether a real settings snapshot is available."""
        return super().available and settings_available(self.coordinator) and self.native_value is not None

    async def async_set_native_value(self, value: float) -> None:
        """Queue the corresponding Lumentree write command."""
        int_value = int(value)
        try:
            if self.entity_description.command == "target_soc":
                slot = self.entity_description.slot or 1
                await self.coordinator.client.create_discharge_target_soc_command(self._device_id, slot, int_value)
            elif self.entity_description.command == "mains_charge_target_soc" and self.entity_description.slot is not None:
                await self.coordinator.client.create_mains_charge_target_soc_command(
                    self._device_id,
                    self.entity_description.slot,
                    int_value,
                )
            elif self.entity_description.command == "discharge_power" and self.entity_description.slot is not None:
                await self.coordinator.client.create_discharge_power_command(
                    self._device_id,
                    self.entity_description.slot,
                    int_value,
                )
            else:
                raise HomeAssistantError("Unsupported Lumentree Local number command")
        except LumentreeLocalAuthError as err:
            raise HomeAssistantError(
                "Write access is required for Lumentree Local write commands. "
                "Generate a write pairing code from the ESP32 portal and enter it "
                "in Lumentree Local options."
            ) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

        await self.coordinator.async_request_refresh()

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return diagnostic attributes."""
        attrs: dict[str, Any] = {
            "command": self.entity_description.command,
            "queued_via": "lumentree_command_api",
            "write_access_required": True,
            "write_warning": "Changing this entity writes directly to the inverter settings.",
        }
        if self.entity_description.slot is not None:
            attrs["slot"] = self.entity_description.slot
        value = settings_value(self.coordinator, self._settings_key())
        attrs["loaded_from_settings_snapshot"] = value is not None
        return attrs
