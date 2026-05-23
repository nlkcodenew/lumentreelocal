# Battery Card And Billing-Cycle Refactor

Date: 2026-05-23

## Summary

- The Solar battery summary card no longer treats the legacy
  `sensor.battery_power_adjusted` helper as the source of truth.
- The correct source of truth is the inverter-native pair:
  - `sensor.lumentree_local_p240819130_battery_power`
  - `sensor.lumentree_local_p240819130_battery_status`
- The local server and integration now expose billing-cycle metrics for the
  EVN-style cycle that starts at `00:00` on the 22nd of each month and ends at
  `00:00` on the next 22nd.

## What Changed

### Battery summary card

- The card now reads charge/discharge state directly from the inverter status.
- Displayed power is derived from the inverter-native battery power metric.
- The card no longer infers mode from the sign of the legacy adjusted helper.

### Billing-cycle energy and bill

- The local server energy payload now includes:
  - `billing_cycle_reset_day`
  - `billing_cycle_start`
  - `billing_cycle_end`
  - `billing_cycle`
- The Home Assistant integration now exposes:
  - `sensor.lumentree_local_<device_id>_billing_cycle_load_energy`
  - `sensor.lumentree_local_<device_id>_billing_cycle_equivalent_bill`

### Dashboard compatibility

- The Solar dashboard config was updated to prefer the new billing-cycle
  entities.
- A temporary fallback to the old helper entities remains in the dashboard
  config so the live UI does not break before the Home Assistant integration is
  upgraded to `0.14.17`.

## Legacy Helper Status

The following helpers are no longer the preferred data path for the Solar
dashboard:

- `sensor.battery_power_adjusted`
- `sensor.tai_tong_nha`
- `sensor.tien_phai_tra_neu_khong_co_nlmt`

They may still exist in Home Assistant, but the new integration/server path is
the intended long-term source of truth.

## Validation

- `python3 -m py_compile custom_components/lumentreelocal/*.py`
- `python3 -m py_compile host/local-server/server.py host/local-server/test_billing_cycle_range.py`
- `python3 host/local-server/test_billing_cycle_range.py`
- Manual energy payload verification against the live Postgres database for
  `device_id = P240819130`
