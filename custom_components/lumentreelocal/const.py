"""Constants for Lumentree Local."""

from __future__ import annotations

from datetime import timedelta
import logging

DOMAIN = "lumentreelocal"
LOGGER = logging.getLogger(__package__)

CONF_API_URL = "api_url"
CONF_API_TOKEN = "api_token"
CONF_DEVICE_ID = "device_id"
CONF_WRITE_GRANT_TOKEN = "write_grant_token"
SERVICE_DRY_RUN_COMMAND = "dry_run_command"
SERVICE_SET_FIRST_DISCHARGE_TARGET_SOC = "set_first_discharge_target_soc"
SERVICE_SET_DISCHARGE_TARGET_SOC = "set_discharge_target_soc"
SERVICE_SET_DISCHARGE_POWER = "set_discharge_power"
SERVICE_SET_DISCHARGE_TIME_ENABLE = "set_discharge_time_enable"
SERVICE_SET_DISCHARGE_TIME_START = "set_discharge_time_start"
SERVICE_SET_DISCHARGE_TIME_END = "set_discharge_time_end"

DEFAULT_API_URL = "https://lumentree.jonah.io.vn"
DEFAULT_SCAN_INTERVAL = timedelta(seconds=10)
SWITCH_WRITE_CONFIRM_TIMEOUT_SECONDS = 25
