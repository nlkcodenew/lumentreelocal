"""Lumentree Local integration."""

from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.const import Platform
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import ConfigEntryNotReady, HomeAssistantError
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers import entity_registry as er
import voluptuous as vol

from .api import LumentreeLocalApiClient, LumentreeLocalApiError, LumentreeLocalAuthError
from .const import (
    CONF_API_TOKEN,
    CONF_API_URL,
    CONF_DEVICE_ID,
    CONF_READ_GRANT_TOKEN,
    CONF_WRITE_GRANT_TOKEN,
    DEFAULT_API_URL,
    DOMAIN,
    SERVICE_DRY_RUN_COMMAND,
    SERVICE_SET_DISCHARGE_POWER,
    SERVICE_SET_DISCHARGE_TARGET_SOC,
    SERVICE_SET_DISCHARGE_TIME_ENABLE,
    SERVICE_SET_DISCHARGE_TIME_END,
    SERVICE_SET_DISCHARGE_TIME_START,
    SERVICE_SET_FIRST_DISCHARGE_TARGET_SOC,
)
from .coordinator import LumentreeLocalCoordinator

PLATFORMS: list[Platform] = [
    Platform.SENSOR,
    Platform.BINARY_SENSOR,
    Platform.NUMBER,
    Platform.SWITCH,
    Platform.TIME,
]

DRY_RUN_SERVICE_SCHEMA = vol.Schema(
    {
        vol.Optional(CONF_DEVICE_ID): str,
        vol.Optional("command", default="dry_run_noop"): str,
    }
)

TARGET_SOC_SERVICE_SCHEMA = vol.Schema(
    {
        vol.Optional(CONF_DEVICE_ID): str,
        vol.Required("target_soc"): vol.All(vol.Coerce(int), vol.Range(min=5, max=100)),
    }
)

DISCHARGE_TARGET_SOC_SERVICE_SCHEMA = vol.Schema(
    {
        vol.Optional(CONF_DEVICE_ID): str,
        vol.Required("slot"): vol.All(vol.Coerce(int), vol.Range(min=1, max=4)),
        vol.Required("target_soc"): vol.All(vol.Coerce(int), vol.Range(min=5, max=100)),
    }
)

DISCHARGE_POWER_SERVICE_SCHEMA = vol.Schema(
    {
        vol.Optional(CONF_DEVICE_ID): str,
        vol.Required("slot"): vol.All(vol.Coerce(int), vol.Range(min=1, max=4)),
        vol.Required("power"): vol.All(vol.Coerce(int), vol.Range(min=500, max=5000)),
    }
)

DISCHARGE_TIME_ENABLE_SERVICE_SCHEMA = vol.Schema(
    {
        vol.Optional(CONF_DEVICE_ID): str,
        vol.Required("slot"): vol.All(vol.Coerce(int), vol.Range(min=1, max=4)),
        vol.Required("enabled"): bool,
    }
)


def _valid_hhmm(value: int) -> int:
    hours = value // 100
    minutes = value % 100
    if hours > 23 or minutes > 59:
        raise vol.Invalid("time must be a valid HHMM value from 0000 to 2359")
    return value


DISCHARGE_TIME_SERVICE_SCHEMA = vol.Schema(
    {
        vol.Optional(CONF_DEVICE_ID): str,
        vol.Required("slot"): vol.All(vol.Coerce(int), vol.Range(min=1, max=4)),
        vol.Required("time"): vol.All(vol.Coerce(int), vol.Range(min=0, max=2359), _valid_hhmm),
    }
)


def _entry_client_and_device(hass: HomeAssistant, device_id: str | None):
    entries = hass.data.get(DOMAIN, {})
    if not entries:
        raise HomeAssistantError("Lumentree Local is not configured")

    first_entry = next(iter(entries.values()))
    coordinator = first_entry["coordinator"]
    client = first_entry["client"]
    return client, device_id or coordinator.device_id


def _write_access_error(err: LumentreeLocalAuthError) -> HomeAssistantError:
    return HomeAssistantError(
        "Write access is required for Lumentree Local write commands. "
        "Generate a write pairing code from the ESP32 portal and enter it "
        "in Lumentree Local options."
    )


async def async_setup(hass: HomeAssistant, config: dict) -> bool:
    """Set up integration services."""

    async def handle_dry_run(call) -> None:
        command = call.data.get("command", "dry_run_noop")
        client, device_id = _entry_client_and_device(hass, call.data.get(CONF_DEVICE_ID))

        try:
            await client.create_dry_run_command(device_id, command)
        except LumentreeLocalAuthError as err:
            raise HomeAssistantError(
                "Write access is required for Lumentree Local dry-run commands. "
                "Generate a write pairing code from the ESP32 portal and enter it "
                "in Lumentree Local options."
            ) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

    hass.services.async_register(
        DOMAIN,
        SERVICE_DRY_RUN_COMMAND,
        handle_dry_run,
        schema=DRY_RUN_SERVICE_SCHEMA,
    )

    async def handle_target_soc(call) -> None:
        client, device_id = _entry_client_and_device(hass, call.data.get(CONF_DEVICE_ID))
        target_soc = call.data["target_soc"]

        try:
            await client.create_target_soc_command(device_id, target_soc)
        except LumentreeLocalAuthError as err:
            raise _write_access_error(err) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

    hass.services.async_register(
        DOMAIN,
        SERVICE_SET_FIRST_DISCHARGE_TARGET_SOC,
        handle_target_soc,
        schema=TARGET_SOC_SERVICE_SCHEMA,
    )

    async def handle_discharge_target_soc(call) -> None:
        client, device_id = _entry_client_and_device(hass, call.data.get(CONF_DEVICE_ID))
        try:
            await client.create_discharge_target_soc_command(device_id, call.data["slot"], call.data["target_soc"])
        except LumentreeLocalAuthError as err:
            raise _write_access_error(err) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

    hass.services.async_register(
        DOMAIN,
        SERVICE_SET_DISCHARGE_TARGET_SOC,
        handle_discharge_target_soc,
        schema=DISCHARGE_TARGET_SOC_SERVICE_SCHEMA,
    )

    async def handle_discharge_power(call) -> None:
        client, device_id = _entry_client_and_device(hass, call.data.get(CONF_DEVICE_ID))
        try:
            await client.create_discharge_power_command(device_id, call.data["slot"], call.data["power"])
        except LumentreeLocalAuthError as err:
            raise _write_access_error(err) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

    hass.services.async_register(
        DOMAIN,
        SERVICE_SET_DISCHARGE_POWER,
        handle_discharge_power,
        schema=DISCHARGE_POWER_SERVICE_SCHEMA,
    )

    async def handle_discharge_time_enable(call) -> None:
        client, device_id = _entry_client_and_device(hass, call.data.get(CONF_DEVICE_ID))
        try:
            await client.create_discharge_time_enable_command(device_id, call.data["slot"], call.data["enabled"])
        except LumentreeLocalAuthError as err:
            raise _write_access_error(err) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

    hass.services.async_register(
        DOMAIN,
        SERVICE_SET_DISCHARGE_TIME_ENABLE,
        handle_discharge_time_enable,
        schema=DISCHARGE_TIME_ENABLE_SERVICE_SCHEMA,
    )

    async def handle_discharge_time_start(call) -> None:
        client, device_id = _entry_client_and_device(hass, call.data.get(CONF_DEVICE_ID))
        try:
            await client.create_discharge_time_command(device_id, call.data["slot"], "start", call.data["time"])
        except LumentreeLocalAuthError as err:
            raise _write_access_error(err) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

    hass.services.async_register(
        DOMAIN,
        SERVICE_SET_DISCHARGE_TIME_START,
        handle_discharge_time_start,
        schema=DISCHARGE_TIME_SERVICE_SCHEMA,
    )

    async def handle_discharge_time_end(call) -> None:
        client, device_id = _entry_client_and_device(hass, call.data.get(CONF_DEVICE_ID))
        try:
            await client.create_discharge_time_command(device_id, call.data["slot"], "end", call.data["time"])
        except LumentreeLocalAuthError as err:
            raise _write_access_error(err) from err
        except LumentreeLocalApiError as err:
            raise HomeAssistantError(str(err)) from err

    hass.services.async_register(
        DOMAIN,
        SERVICE_SET_DISCHARGE_TIME_END,
        handle_discharge_time_end,
        schema=DISCHARGE_TIME_SERVICE_SCHEMA,
    )
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Lumentree Local from a config entry."""
    if entry.title != "Lumentree Local":
        hass.config_entries.async_update_entry(entry, title="Lumentree Local")

    api_url = entry.data.get(CONF_API_URL, DEFAULT_API_URL)
    read_token = (
        entry.options.get(CONF_READ_GRANT_TOKEN)
        or entry.data.get(CONF_READ_GRANT_TOKEN, "")
        or entry.options.get(CONF_API_TOKEN)
        or entry.data.get(CONF_API_TOKEN, "")
    )
    write_token = (
        entry.options.get(CONF_WRITE_GRANT_TOKEN)
        or entry.data.get(CONF_WRITE_GRANT_TOKEN, "")
    )
    device_id = entry.options.get(CONF_DEVICE_ID) or entry.data[CONF_DEVICE_ID]

    entity_registry = er.async_get(hass)
    old_unique_id = f"{DOMAIN}_{device_id}_first_discharge_target_soc"
    new_unique_id = f"{DOMAIN}_{device_id}_discharge_slot_1_target_soc"
    old_entity_id = entity_registry.async_get_entity_id("number", DOMAIN, old_unique_id)
    existing_new_entity_id = entity_registry.async_get_entity_id("number", DOMAIN, new_unique_id)
    new_entity_id = f"number.lumentree_local_{device_id.lower()}_discharge_slot_1_target_soc"
    if old_entity_id and old_entity_id != new_entity_id and existing_new_entity_id is None:
        entity_registry.async_update_entity(
            old_entity_id,
            new_unique_id=new_unique_id,
            new_entity_id=new_entity_id,
        )

    client = LumentreeLocalApiClient(async_get_clientsession(hass), api_url, read_token, write_token)
    coordinator = LumentreeLocalCoordinator(hass, client, device_id)

    try:
        await coordinator.async_config_entry_first_refresh()
    except LumentreeLocalApiError as err:
        raise ConfigEntryNotReady(str(err)) from err
    await coordinator.async_start_stream()

    hass.data.setdefault(DOMAIN, {})[entry.entry_id] = {
        "client": client,
        "coordinator": coordinator,
    }

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    entry.async_on_unload(entry.add_update_listener(async_reload_entry))
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    coordinator = hass.data.get(DOMAIN, {}).get(entry.entry_id, {}).get("coordinator")
    if coordinator is not None:
        await coordinator.async_stop_stream()
    unload_ok = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unload_ok:
        hass.data.get(DOMAIN, {}).pop(entry.entry_id, None)
    return unload_ok


async def async_reload_entry(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """Reload the config entry after options change."""
    await hass.config_entries.async_reload(entry.entry_id)
