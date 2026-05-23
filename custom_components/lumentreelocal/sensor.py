"""Sensors for Lumentree Local."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from homeassistant.components.sensor import SensorDeviceClass, SensorEntity, SensorEntityDescription, SensorStateClass
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import (
    PERCENTAGE,
    UnitOfApparentPower,
    UnitOfElectricCurrent,
    UnitOfElectricPotential,
    UnitOfEnergy,
    UnitOfFrequency,
    UnitOfPower,
    UnitOfTemperature,
    UnitOfTime,
)
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity import EntityCategory
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import LumentreeLocalCoordinator


BILLING_CYCLE_RESET_DAY = 22
EQUIVALENT_BILL_RATE_VND_PER_KWH = 2864


@dataclass(frozen=True, kw_only=True)
class LumentreeLocalSensorDescription(SensorEntityDescription):
    """Description of a Lumentree Local sensor."""

    metric_key: str
    source: str = "metrics"
    period: str | None = None
    value_fn: Any = None


def energy_sensor(period: str, metric_key: str, name_key: str) -> LumentreeLocalSensorDescription:
    """Create an energy sensor description."""
    return LumentreeLocalSensorDescription(
        key=f"{period}_{name_key}",
        metric_key=metric_key,
        source="energy",
        period=period,
        translation_key=f"{period}_{name_key}",
        native_unit_of_measurement=UnitOfEnergy.KILO_WATT_HOUR,
        device_class=SensorDeviceClass.ENERGY,
        state_class=SensorStateClass.TOTAL_INCREASING,
        suggested_display_precision=3,
    )


def metric_energy_sensor(metric_key: str) -> LumentreeLocalSensorDescription:
    """Create a direct metrics-backed energy sensor description."""
    return LumentreeLocalSensorDescription(
        key=metric_key,
        metric_key=metric_key,
        translation_key=metric_key,
        native_unit_of_measurement=UnitOfEnergy.KILO_WATT_HOUR,
        device_class=SensorDeviceClass.ENERGY,
        state_class=SensorStateClass.TOTAL_INCREASING,
        suggested_display_precision=1,
    )


def billing_cycle_energy_sensor(metric_key: str, name_key: str) -> LumentreeLocalSensorDescription:
    """Create a billing-cycle energy sensor description."""
    return LumentreeLocalSensorDescription(
        key=f"billing_cycle_{name_key}",
        metric_key=metric_key,
        source="energy",
        period="billing_cycle",
        translation_key=f"billing_cycle_{name_key}",
        native_unit_of_measurement=UnitOfEnergy.KILO_WATT_HOUR,
        device_class=SensorDeviceClass.ENERGY,
        state_class=SensorStateClass.TOTAL_INCREASING,
        suggested_display_precision=3,
    )


def billing_cycle_bill_sensor() -> LumentreeLocalSensorDescription:
    """Estimated bill for the current EVN billing cycle."""
    return LumentreeLocalSensorDescription(
        key="billing_cycle_equivalent_bill_vnd",
        metric_key="billing_cycle_equivalent_bill_vnd",
        source="derived",
        translation_key="billing_cycle_equivalent_bill_vnd",
        native_unit_of_measurement="VND",
        device_class=SensorDeviceClass.MONETARY,
        state_class=SensorStateClass.TOTAL,
        suggested_display_precision=0,
    )


def compute_billing_cycle_equivalent_bill(data: dict[str, Any]) -> float | None:
    """Estimate the current billing-cycle bill from load energy."""
    energy = data.get("energy", {})
    if not isinstance(energy, dict):
        return None
    billing_cycle = energy.get("billing_cycle", {})
    if not isinstance(billing_cycle, dict):
        return None
    load_kwh = billing_cycle.get("load_kwh")
    try:
        return round(float(load_kwh) * EQUIVALENT_BILL_RATE_VND_PER_KWH, 0)
    except (TypeError, ValueError):
        return None


def health_sensor(
    metric_key: str,
    *,
    unit: str | None = None,
    icon: str | None = None,
    device_class: SensorDeviceClass | None = None,
    state_class: SensorStateClass | None = None,
) -> LumentreeLocalSensorDescription:
    """Create a health diagnostic sensor description."""
    return LumentreeLocalSensorDescription(
        key=metric_key,
        metric_key=metric_key,
        source="health",
        translation_key=metric_key,
        native_unit_of_measurement=unit,
        icon=icon,
        device_class=device_class,
        state_class=state_class,
        entity_category=EntityCategory.DIAGNOSTIC,
    )


def diagnostic_sensor(
    metric_key: str,
    *,
    unit: str | None = None,
    icon: str | None = None,
    device_class: SensorDeviceClass | None = None,
) -> LumentreeLocalSensorDescription:
    """Create a generic diagnostic sensor description."""
    return LumentreeLocalSensorDescription(
        key=metric_key,
        metric_key=metric_key,
        translation_key=metric_key,
        native_unit_of_measurement=unit,
        icon=icon,
        device_class=device_class,
        entity_category=EntityCategory.DIAGNOSTIC,
    )


def command_sensor(metric_key: str, *, icon: str | None = None) -> LumentreeLocalSensorDescription:
    """Create a command diagnostic sensor description."""
    return LumentreeLocalSensorDescription(
        key=metric_key,
        metric_key=metric_key,
        source="command_status",
        translation_key=metric_key,
        icon=icon,
        entity_category=EntityCategory.DIAGNOSTIC,
    )


SENSORS: tuple[LumentreeLocalSensorDescription, ...] = (
    LumentreeLocalSensorDescription(
        key="pv_power",
        metric_key="pv_power",
        translation_key="pv_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="pv1_power",
        metric_key="pv1_power",
        translation_key="pv1_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="pv2_power",
        metric_key="pv2_power",
        translation_key="pv2_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="grid_power",
        metric_key="grid_power",
        translation_key="grid_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="ac_input_power",
        metric_key="ac_input_power",
        translation_key="ac_input_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="ac_output_power",
        metric_key="ac_output_power",
        translation_key="ac_output_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="ac_output_va",
        metric_key="ac_output_va",
        translation_key="ac_output_va",
        native_unit_of_measurement=UnitOfApparentPower.VOLT_AMPERE,
        device_class=SensorDeviceClass.APPARENT_POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="load_power",
        metric_key="load_power",
        translation_key="load_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="battery_power",
        metric_key="battery_power",
        translation_key="battery_power",
        native_unit_of_measurement=UnitOfPower.WATT,
        device_class=SensorDeviceClass.POWER,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="battery_soc",
        metric_key="battery_soc",
        translation_key="battery_soc",
        native_unit_of_measurement=PERCENTAGE,
        device_class=SensorDeviceClass.BATTERY,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="battery_voltage",
        metric_key="battery_voltage",
        translation_key="battery_voltage",
        native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="battery_current",
        metric_key="battery_current",
        translation_key="battery_current",
        native_unit_of_measurement=UnitOfElectricCurrent.AMPERE,
        device_class=SensorDeviceClass.CURRENT,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="grid_voltage",
        metric_key="grid_voltage",
        translation_key="grid_voltage",
        native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="ac_input_voltage",
        metric_key="ac_input_voltage",
        translation_key="ac_input_voltage",
        native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="ac_output_voltage",
        metric_key="ac_output_voltage",
        translation_key="ac_output_voltage",
        native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="pv1_voltage",
        metric_key="pv1_voltage",
        translation_key="pv1_voltage",
        native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="pv2_voltage",
        metric_key="pv2_voltage",
        translation_key="pv2_voltage",
        native_unit_of_measurement=UnitOfElectricPotential.VOLT,
        device_class=SensorDeviceClass.VOLTAGE,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="ac_input_frequency",
        metric_key="ac_input_frequency",
        translation_key="ac_input_frequency",
        native_unit_of_measurement=UnitOfFrequency.HERTZ,
        device_class=SensorDeviceClass.FREQUENCY,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="ac_output_frequency",
        metric_key="ac_output_frequency",
        translation_key="ac_output_frequency",
        native_unit_of_measurement=UnitOfFrequency.HERTZ,
        device_class=SensorDeviceClass.FREQUENCY,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="device_temperature",
        metric_key="device_temperature",
        translation_key="device_temperature",
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    LumentreeLocalSensorDescription(
        key="battery_status",
        metric_key="battery_status",
        translation_key="battery_status",
        device_class=SensorDeviceClass.ENUM,
    ),
    LumentreeLocalSensorDescription(
        key="grid_status",
        metric_key="grid_status",
        translation_key="grid_status",
        device_class=SensorDeviceClass.ENUM,
    ),
    LumentreeLocalSensorDescription(
        key="battery_type",
        metric_key="battery_type",
        translation_key="battery_type",
        device_class=SensorDeviceClass.ENUM,
        entity_category=EntityCategory.DIAGNOSTIC,
    ),
    LumentreeLocalSensorDescription(
        key="mqtt_device_sn",
        metric_key="mqtt_device_sn",
        translation_key="mqtt_device_sn",
        icon="mdi:barcode-scan",
        entity_category=EntityCategory.DIAGNOSTIC,
    ),
    LumentreeLocalSensorDescription(
        key="master_slave_status",
        metric_key="master_slave_status",
        translation_key="master_slave_status",
        icon="mdi:account-multiple",
        entity_category=EntityCategory.DIAGNOSTIC,
    ),
    diagnostic_sensor(
        "device_type_code",
        icon="mdi:chip",
    ),
    diagnostic_sensor(
        "device_power_rating_code",
        icon="mdi:identifier",
    ),
    diagnostic_sensor(
        "device_power_rating_w",
        unit=UnitOfPower.WATT,
        icon="mdi:flash-outline",
    ),
    metric_energy_sensor("today_pv_generation_kwh"),
    metric_energy_sensor("today_essential_load_kwh"),
    metric_energy_sensor("today_total_load_kwh"),
    metric_energy_sensor("today_grid_import_kwh"),
    metric_energy_sensor("today_battery_charge_kwh"),
    metric_energy_sensor("today_battery_discharge_kwh"),
    energy_sensor("daily", "pv_kwh", "pv_kwh"),
    energy_sensor("daily", "load_kwh", "load_kwh"),
    energy_sensor("daily", "grid_in_kwh", "grid_in_kwh"),
    energy_sensor("daily", "grid_out_kwh", "grid_out_kwh"),
    energy_sensor("daily", "battery_charge_kwh", "battery_charge_kwh"),
    energy_sensor("daily", "battery_discharge_kwh", "battery_discharge_kwh"),
    energy_sensor("daily", "ac_input_kwh", "ac_input_kwh"),
    energy_sensor("daily", "ac_output_kwh", "ac_output_kwh"),
    billing_cycle_energy_sensor("load_kwh", "load_kwh"),
    energy_sensor("monthly", "pv_kwh", "pv_kwh"),
    energy_sensor("monthly", "load_kwh", "load_kwh"),
    energy_sensor("monthly", "grid_in_kwh", "grid_in_kwh"),
    energy_sensor("monthly", "grid_out_kwh", "grid_out_kwh"),
    energy_sensor("monthly", "battery_charge_kwh", "battery_charge_kwh"),
    energy_sensor("monthly", "battery_discharge_kwh", "battery_discharge_kwh"),
    energy_sensor("monthly", "ac_input_kwh", "ac_input_kwh"),
    energy_sensor("monthly", "ac_output_kwh", "ac_output_kwh"),
    energy_sensor("yearly", "pv_kwh", "pv_kwh"),
    energy_sensor("yearly", "load_kwh", "load_kwh"),
    energy_sensor("yearly", "grid_in_kwh", "grid_in_kwh"),
    energy_sensor("yearly", "grid_out_kwh", "grid_out_kwh"),
    energy_sensor("yearly", "battery_charge_kwh", "battery_charge_kwh"),
    energy_sensor("yearly", "battery_discharge_kwh", "battery_discharge_kwh"),
    energy_sensor("yearly", "ac_input_kwh", "ac_input_kwh"),
    energy_sensor("yearly", "ac_output_kwh", "ac_output_kwh"),
    energy_sensor("total", "pv_kwh", "pv_kwh"),
    energy_sensor("total", "load_kwh", "load_kwh"),
    energy_sensor("total", "grid_in_kwh", "grid_in_kwh"),
    energy_sensor("total", "grid_out_kwh", "grid_out_kwh"),
    energy_sensor("total", "battery_charge_kwh", "battery_charge_kwh"),
    energy_sensor("total", "battery_discharge_kwh", "battery_discharge_kwh"),
    energy_sensor("total", "ac_input_kwh", "ac_input_kwh"),
    energy_sensor("total", "ac_output_kwh", "ac_output_kwh"),
    billing_cycle_bill_sensor(),
    health_sensor(
        "latest_age_seconds",
        unit=UnitOfTime.SECONDS,
        icon="mdi:timer-outline",
        device_class=SensorDeviceClass.DURATION,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    health_sensor(
        "avg_sample_interval_seconds",
        unit=UnitOfTime.SECONDS,
        icon="mdi:timer-sync-outline",
        device_class=SensorDeviceClass.DURATION,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    health_sensor(
        "max_sample_gap_seconds",
        unit=UnitOfTime.SECONDS,
        icon="mdi:timer-alert-outline",
        device_class=SensorDeviceClass.DURATION,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    health_sensor(
        "samples_last_hour",
        icon="mdi:counter",
        state_class=SensorStateClass.MEASUREMENT,
    ),
    health_sensor(
        "total_sample_count",
        icon="mdi:counter",
        state_class=SensorStateClass.TOTAL_INCREASING,
    ),
    health_sensor(
        "uptime_seconds",
        unit=UnitOfTime.SECONDS,
        icon="mdi:clock-outline",
        device_class=SensorDeviceClass.DURATION,
        state_class=SensorStateClass.MEASUREMENT,
    ),
    health_sensor("gateway_id", icon="mdi:router-wireless"),
    health_sensor("firmware", icon="mdi:chip"),
    health_sensor("pairing_status", icon="mdi:bluetooth-connect"),
    health_sensor("target_mac", icon="mdi:bluetooth"),
    health_sensor("candidate_count", icon="mdi:format-list-numbered"),
    LumentreeLocalSensorDescription(
        key="write_access_status",
        metric_key="write_enabled",
        source="write_grant",
        translation_key="write_access_status",
        icon="mdi:shield-key-outline",
    ),
    command_sensor("last_command_status", icon="mdi:progress-check"),
    command_sensor("last_command_name", icon="mdi:console"),
    command_sensor("last_command_time", icon="mdi:clock-check-outline"),
    command_sensor("last_command_safety", icon="mdi:shield-check-outline"),
    command_sensor("last_command_error", icon="mdi:alert-circle-outline"),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up sensors."""
    coordinator: LumentreeLocalCoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    async_add_entities(LumentreeLocalSensor(coordinator, entry, description) for description in SENSORS)


class LumentreeLocalSensor(CoordinatorEntity[LumentreeLocalCoordinator], SensorEntity):
    """Lumentree Local metric sensor."""

    entity_description: LumentreeLocalSensorDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: LumentreeLocalCoordinator,
        entry: ConfigEntry,
        description: LumentreeLocalSensorDescription,
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
    def native_value(self) -> Any:
        """Return native sensor value."""
        if self.entity_description.source == "energy" and self.entity_description.period:
            energy = self.coordinator.data.get("energy", {})
            if not isinstance(energy, dict):
                return None
            period_data = energy.get(self.entity_description.period, {})
            if not isinstance(period_data, dict):
                return None
            return period_data.get(self.entity_description.metric_key)

        if self.entity_description.source == "health":
            health = self.coordinator.data.get("health", {})
            if not isinstance(health, dict):
                return None
            if self.entity_description.metric_key in {"target_mac", "candidate_count"}:
                gateway_status = health.get("gateway_status", {})
                if isinstance(gateway_status, dict):
                    if self.entity_description.metric_key == "candidate_count":
                        candidates = gateway_status.get("candidates")
                        return len(candidates) if isinstance(candidates, list) else 0
                    return gateway_status.get("target_mac")
                if self.entity_description.metric_key == "candidate_count":
                    return 0
                return health.get("mac")
            return health.get(self.entity_description.metric_key)

        if self.entity_description.source == "write_grant":
            write_grant = self.coordinator.data.get("write_grant", {})
            if not isinstance(write_grant, dict):
                return None
            if write_grant.get("write_enabled"):
                return "enabled"
            if write_grant.get("write_available"):
                return "available"
            return "unavailable"

        if self.entity_description.source == "command_status":
            command_status = self.coordinator.data.get("command_status", {})
            if not isinstance(command_status, dict):
                return None
            command = command_status.get("last_command")
            if not isinstance(command, dict):
                return None
            if self.entity_description.metric_key == "last_command_status":
                return command.get("status")
            if self.entity_description.metric_key == "last_command_name":
                return command.get("command")
            if self.entity_description.metric_key == "last_command_time":
                return command.get("completed_at") or command.get("updated_at") or command.get("requested_at")
            if self.entity_description.metric_key == "last_command_safety":
                return command.get("safety")
            if self.entity_description.metric_key == "last_command_error":
                return command.get("error")
            return None

        if self.entity_description.source == "derived":
            if callable(self.entity_description.value_fn):
                return self.entity_description.value_fn(self.coordinator.data)
            if self.entity_description.metric_key == "billing_cycle_equivalent_bill_vnd":
                return compute_billing_cycle_equivalent_bill(self.coordinator.data)
            return None

        metrics = self.coordinator.data.get("metrics", {})
        if not isinstance(metrics, dict):
            return None
        return metrics.get(self.entity_description.metric_key)

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return diagnostic attributes."""
        data = self.coordinator.data
        attrs = {
            "observed_at": data.get("observed_at"),
            "mac": data.get("mac"),
            "gateway_id": data.get("gateway_id"),
            "source_id": data.get("id"),
        }
        if self.entity_description.source == "energy" and self.entity_description.period:
            energy = data.get("energy", {})
            period_data = energy.get(self.entity_description.period, {}) if isinstance(energy, dict) else {}
            if isinstance(period_data, dict):
                attrs.update(
                    {
                        "aggregation_period": self.entity_description.period,
                        "covered_seconds": period_data.get("covered_seconds"),
                        "sample_count": period_data.get("sample_count"),
                        "first_observed_at": period_data.get("first_observed_at"),
                        "last_observed_at": period_data.get("last_observed_at"),
                    }
                )
                if self.entity_description.period == "billing_cycle":
                    attrs.update(
                        {
                            "billing_cycle_reset_day": energy.get("billing_cycle_reset_day"),
                            "billing_cycle_start": energy.get("billing_cycle_start"),
                            "billing_cycle_end": energy.get("billing_cycle_end"),
                        }
                    )
        if self.entity_description.source == "health":
            health = data.get("health", {})
            if isinstance(health, dict):
                attrs.update(
                    {
                        "online": health.get("online"),
                        "latest_id": health.get("latest_id"),
                        "latest_observed_at": health.get("latest_observed_at"),
                        "online_threshold_seconds": health.get("online_threshold_seconds"),
                        "recent_sample_count": health.get("recent_sample_count"),
                        "pairing_status": health.get("pairing_status"),
                    }
                )
                gateway_status = health.get("gateway_status", {})
                if isinstance(gateway_status, dict):
                    attrs.update(
                        {
                            "gateway_updated_at": gateway_status.get("updated_at"),
                            "target_mac": gateway_status.get("target_mac"),
                            "candidates": gateway_status.get("candidates"),
                        }
                    )
        if self.entity_description.source == "write_grant":
            write_grant = data.get("write_grant", {})
            if isinstance(write_grant, dict):
                attrs.update(
                    {
                        "write_available": write_grant.get("write_available"),
                        "write_enabled": write_grant.get("write_enabled"),
                        "grant_active": write_grant.get("grant_active"),
                        "active_grant_count": write_grant.get("active_grant_count"),
                        "scope": write_grant.get("scope"),
                        "gateway_id": write_grant.get("gateway_id"),
                        "gateway_online": write_grant.get("gateway_online"),
                        "gateway_updated_at": write_grant.get("gateway_updated_at"),
                        "command_mode": write_grant.get("command_mode"),
                        "safety": write_grant.get("safety"),
                    }
                )
        if self.entity_description.source == "command_status":
            command_status = data.get("command_status", {})
            command = command_status.get("last_command") if isinstance(command_status, dict) else None
            if isinstance(command, dict):
                attrs.update(
                    {
                        "command_id": command.get("id"),
                        "command": command.get("command"),
                        "command_mode": command.get("mode"),
                        "command_status": command.get("status"),
                        "requested_by": command.get("requested_by"),
                        "requested_at": command.get("requested_at"),
                        "claimed_at": command.get("claimed_at"),
                        "completed_at": command.get("completed_at"),
                        "updated_at": command.get("updated_at"),
                        "dry_run": command.get("dry_run"),
                        "firmware": command.get("firmware"),
                        "ble_write": command.get("ble_write"),
                        "modbus_write": command.get("modbus_write"),
                        "write_enabled": command.get("write_enabled"),
                        "would_execute": command.get("would_execute"),
                        "register": command.get("register"),
                        "requested_value": command.get("requested_value"),
                        "before_value": command.get("before_value"),
                        "after_value": command.get("after_value"),
                        "write_ack": command.get("write_ack"),
                        "verified": command.get("verified"),
                        "safety": command.get("safety"),
                    }
                )
        if self.entity_description.source == "derived":
            if self.entity_description.metric_key == "billing_cycle_equivalent_bill_vnd":
                energy = data.get("energy", {})
                billing_cycle = energy.get("billing_cycle", {}) if isinstance(energy, dict) else {}
                attrs.update(
                    {
                        "rate_vnd_per_kwh": EQUIVALENT_BILL_RATE_VND_PER_KWH,
                        "billing_cycle_reset_day": energy.get("billing_cycle_reset_day") if isinstance(energy, dict) else BILLING_CYCLE_RESET_DAY,
                        "billing_cycle_start": energy.get("billing_cycle_start") if isinstance(energy, dict) else None,
                        "billing_cycle_end": energy.get("billing_cycle_end") if isinstance(energy, dict) else None,
                        "billing_cycle_load_kwh": billing_cycle.get("load_kwh") if isinstance(billing_cycle, dict) else None,
                    }
                )
        return attrs
