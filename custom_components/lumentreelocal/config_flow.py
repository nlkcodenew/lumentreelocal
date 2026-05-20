"""Config flow for Lumentree Local."""

from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.config_entries import ConfigFlowResult
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import LumentreeLocalApiClient, LumentreeLocalApiError
from .const import (
    CONF_API_URL,
    CONF_DEVICE_ID,
    CONF_READ_GRANT_TOKEN,
    CONF_WRITE_GRANT_TOKEN,
    DEFAULT_API_URL,
    DOMAIN,
)


class InitialGrantClaimError(Exception):
    """Initial claim failed in a user-facing way."""

    def __init__(self, error_key: str) -> None:
        self.error_key = error_key
        super().__init__(error_key)


def device_id_schema(default: str | None = None) -> vol.Schema:
    """Return the Device ID form schema."""
    selector = vol.Required(CONF_DEVICE_ID, default=default) if default else vol.Required(CONF_DEVICE_ID)
    return vol.Schema(
        {
            selector: str,
            vol.Required("read_pairing_token"): str,
            vol.Optional("write_pairing_token"): str,
        }
    )


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


async def claim_initial_grants(hass, device_id: str, read_pairing_token: str, write_pairing_token: str = "") -> dict[str, str]:
    """Validate reachability and claim the required initial grants."""
    client = LumentreeLocalApiClient(async_get_clientsession(hass), DEFAULT_API_URL)
    try:
        await client.health()
    except LumentreeLocalApiError as err:
        raise InitialGrantClaimError("cannot_connect") from err

    try:
        read_claim = await client.claim_read_grant(device_id, read_pairing_token)
    except LumentreeLocalApiError as err:
        raise InitialGrantClaimError("invalid_read_pairing_token") from err

    read_grant_token = read_claim.get("grant_token")
    if not isinstance(read_grant_token, str) or not read_grant_token:
        raise InitialGrantClaimError("invalid_read_pairing_token")

    tokens = {CONF_READ_GRANT_TOKEN: read_grant_token}
    if not write_pairing_token:
        return tokens

    try:
        write_claim = await client.claim_write_grant(device_id, write_pairing_token)
    except LumentreeLocalApiError as err:
        read_client = LumentreeLocalApiClient(
            async_get_clientsession(hass),
            DEFAULT_API_URL,
            read_token=read_grant_token,
        )
        try:
            await read_client.revoke_read_grant(device_id)
        except LumentreeLocalApiError:
            pass
        raise InitialGrantClaimError("invalid_write_pairing_code") from err

    write_grant_token = write_claim.get("grant_token")
    if not isinstance(write_grant_token, str) or not write_grant_token:
        read_client = LumentreeLocalApiClient(
            async_get_clientsession(hass),
            DEFAULT_API_URL,
            read_token=read_grant_token,
        )
        try:
            await read_client.revoke_read_grant(device_id)
        except LumentreeLocalApiError:
            pass
        raise InitialGrantClaimError("invalid_write_pairing_code")
    tokens[CONF_WRITE_GRANT_TOKEN] = write_grant_token
    return tokens


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
            read_pairing_token = user_input["read_pairing_token"].strip()
            write_pairing_token = user_input.get("write_pairing_token", "").strip()
            await self.async_set_unique_id(device_id)
            self._abort_if_unique_id_configured()

            try:
                grants = await claim_initial_grants(self.hass, device_id, read_pairing_token, write_pairing_token)
            except InitialGrantClaimError as err:
                errors["base"] = err.error_key
            else:
                return self.async_create_entry(
                    title="Lumentree Local",
                    data={
                        CONF_API_URL: DEFAULT_API_URL,
                        CONF_DEVICE_ID: device_id,
                        CONF_READ_GRANT_TOKEN: grants[CONF_READ_GRANT_TOKEN],
                        CONF_WRITE_GRANT_TOKEN: grants.get(CONF_WRITE_GRANT_TOKEN, ""),
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
        current_read_grant = self._config_entry.options.get(
            CONF_READ_GRANT_TOKEN,
            self._config_entry.data.get(CONF_READ_GRANT_TOKEN, ""),
        )
        current_write_grant = self._config_entry.options.get(
            CONF_WRITE_GRANT_TOKEN,
            self._config_entry.data.get(CONF_WRITE_GRANT_TOKEN, ""),
        )

        if user_input is not None:
            device_id = user_input[CONF_DEVICE_ID].strip()
            write_pairing_code = user_input.get("write_pairing_code", "").strip()
            revoke_write_access = bool(user_input.get("revoke_write_access", False))
            keep_write_grant = bool(user_input.get("keep_write_grant", True))
            try:
                client = LumentreeLocalApiClient(
                    async_get_clientsession(self.hass),
                    DEFAULT_API_URL,
                    read_token=current_read_grant,
                    write_token=current_write_grant,
                )
                await client.health()
            except LumentreeLocalApiError:
                errors["base"] = "cannot_connect"
            else:
                options = {
                    CONF_DEVICE_ID: device_id,
                    CONF_READ_GRANT_TOKEN: current_read_grant,
                }
                if revoke_write_access and current_write_grant:
                    try:
                        await client.revoke_write_grant(device_id)
                    except LumentreeLocalApiError:
                        errors["base"] = "invalid_write_grant"
                    else:
                        current_write_grant = ""
                if not errors and write_pairing_code:
                    claim_client = LumentreeLocalApiClient(async_get_clientsession(self.hass), DEFAULT_API_URL)
                    try:
                        claim = await claim_client.claim_write_grant(device_id, write_pairing_code)
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
