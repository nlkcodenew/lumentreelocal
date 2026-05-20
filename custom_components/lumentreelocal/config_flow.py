"""Config flow for Lumentree Local."""

from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.config_entries import ConfigFlowResult
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import LumentreeLocalApiClient, LumentreeLocalApiError, LumentreeLocalNotFoundError
from .const import CONF_API_URL, CONF_DEVICE_ID, CONF_WRITE_GRANT_TOKEN, DEFAULT_API_URL, DOMAIN


def device_id_schema(default: str | None = None) -> vol.Schema:
    """Return the Device ID form schema."""
    selector = vol.Required(CONF_DEVICE_ID, default=default) if default else vol.Required(CONF_DEVICE_ID)
    return vol.Schema({selector: str})


def options_schema(device_id: str | None = None, write_grant_configured: bool = False) -> vol.Schema:
    """Return the options form schema."""
    device_selector = vol.Required(CONF_DEVICE_ID, default=device_id) if device_id else vol.Required(CONF_DEVICE_ID)
    return vol.Schema(
        {
            device_selector: str,
            vol.Optional("write_pairing_code"): str,
            vol.Optional("revoke_write_access", default=False): bool,
            vol.Optional("keep_write_grant", default=write_grant_configured): bool,
        }
    )


async def validate_device_id(hass, device_id: str) -> str:
    """Validate server reachability and return the normalized Device ID."""
    client = LumentreeLocalApiClient(async_get_clientsession(hass), DEFAULT_API_URL)
    await client.health()
    try:
        latest = await client.latest(device_id)
    except LumentreeLocalNotFoundError:
        await client.device_health(device_id)
        return device_id
    return str(latest.get("device_id") or device_id)


class LumentreeLocalConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for Lumentree Local."""

    VERSION = 1

    @staticmethod
    def async_get_options_flow(config_entry: config_entries.ConfigEntry) -> config_entries.OptionsFlow:
        """Create the options flow."""
        return LumentreeLocalOptionsFlow(config_entry)

    async def async_step_user(self, user_input: dict[str, Any] | None = None) -> ConfigFlowResult:
        """Handle the initial step."""
        errors: dict[str, str] = {}

        if user_input is not None:
            device_id = user_input[CONF_DEVICE_ID].strip()

            try:
                server_device_id = await validate_device_id(self.hass, device_id)
            except LumentreeLocalApiError:
                errors["base"] = "cannot_connect"
            else:
                await self.async_set_unique_id(server_device_id)
                self._abort_if_unique_id_configured()
                return self.async_create_entry(
                    title="Lumentree Local",
                    data={
                        CONF_API_URL: DEFAULT_API_URL,
                        CONF_DEVICE_ID: server_device_id,
                    },
                )

        return self.async_show_form(step_id="user", data_schema=device_id_schema(), errors=errors)


class LumentreeLocalOptionsFlow(config_entries.OptionsFlow):
    """Handle Lumentree Local options."""

    def __init__(self, config_entry: config_entries.ConfigEntry) -> None:
        self._config_entry = config_entry

    async def async_step_init(self, user_input: dict[str, Any] | None = None) -> ConfigFlowResult:
        """Manage Lumentree Local options."""
        errors: dict[str, str] = {}
        current_device_id = self._config_entry.options.get(
            CONF_DEVICE_ID,
            self._config_entry.data.get(CONF_DEVICE_ID, ""),
        )
        current_write_grant = self._config_entry.options.get(CONF_WRITE_GRANT_TOKEN, "")

        if user_input is not None:
            device_id = user_input[CONF_DEVICE_ID].strip()
            write_pairing_code = user_input.get("write_pairing_code", "").strip()
            revoke_write_access = bool(user_input.get("revoke_write_access", False))
            keep_write_grant = bool(user_input.get("keep_write_grant", True))
            try:
                server_device_id = await validate_device_id(self.hass, device_id)
            except LumentreeLocalApiError:
                errors["base"] = "cannot_connect"
            else:
                options = {CONF_DEVICE_ID: server_device_id}
                if revoke_write_access and current_write_grant:
                    revoke_client = LumentreeLocalApiClient(
                        async_get_clientsession(self.hass),
                        DEFAULT_API_URL,
                        current_write_grant,
                    )
                    try:
                        await revoke_client.revoke_write_grant(server_device_id)
                    except LumentreeLocalApiError:
                        errors["base"] = "invalid_write_grant"
                    else:
                        current_write_grant = ""
                if not errors and write_pairing_code:
                    claim_client = LumentreeLocalApiClient(async_get_clientsession(self.hass), DEFAULT_API_URL)
                    try:
                        claim = await claim_client.claim_write_grant(server_device_id, write_pairing_code)
                    except LumentreeLocalApiError:
                        errors["base"] = "invalid_write_pairing_code"
                    else:
                        grant_token = claim.get("grant_token")
                        if isinstance(grant_token, str) and grant_token:
                            options[CONF_WRITE_GRANT_TOKEN] = grant_token
                        else:
                            errors["base"] = "invalid_write_pairing_code"
                elif not errors and keep_write_grant and current_write_grant:
                    options[CONF_WRITE_GRANT_TOKEN] = current_write_grant
                if errors:
                    return self.async_show_form(
                        step_id="init",
                        data_schema=options_schema(current_device_id, bool(current_write_grant)),
                        errors=errors,
                    )
                return self.async_create_entry(
                    title="",
                    data=options,
                )

        return self.async_show_form(
            step_id="init",
            data_schema=options_schema(current_device_id, bool(current_write_grant)),
            errors=errors,
        )
