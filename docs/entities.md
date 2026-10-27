# Entities

All entities are declared with `platform: bms_bridge`, `bms_bridge_id` and an
optional `device_id` for sub-device grouping.

## Sensors

| `type` | Unit | Decimals | Index |
|---|---|---|---|
| `total_voltage` | V | 2 | |
| `current` | A | 2 | |
| `power` | W | 1 | |
| `soc` | % | 1 | |
| `soh` | % | 1 | |
| `cycles` | | 0 | |
| `capacity` | Ah | 2 | |
| `remaining_capacity` | Ah | 2 | |
| `rated_capacity` | Ah | 2 | |
| `port_voltage` | V | 2 | |
| `cell_voltage` | V | 3 | required |
| `temperature` | °C | 1 | required |
| `cell_voltage_min` | V | 3 | |
| `cell_voltage_max` | V | 3 | |
| `cell_voltage_delta` | V | 3 | |
| `temperature_min` | °C | 1 | |
| `temperature_max` | °C | 1 | |
| `full_charge` | Ah | 1 | derived |
| `full_charge_watts` | Wh | 0 | derived |
| `remaining_watts` | Wh | 0 | derived |
| `capacity_watts` | Wh | 0 | derived |
| `battery_watts` | W | 0 | derived |
| `cell_max_number` | | 0 | derived |
| `cell_min_number` | | 0 | derived |
| `temp_max_number` | | 0 | derived |
| `temp_min_number` | | 0 | derived |
| `temp_delta` | °C | 1 | derived |
| `setting_*` | V / °C | | read-only settings |

## Settings types

`setting_cell_ov_alarm`, `setting_cell_ov_rec`, `setting_cell_uv_alarm`,
`setting_cell_uv_rec`, `setting_cell_ovp`, `setting_cell_ovp_rec`,
`setting_cell_uvp`, `setting_cell_uvp_rec`, `setting_balance_start`,
`setting_charge_forbid`, `setting_pack_ov_alarm`, `setting_pack_ov_rec`,
`setting_pack_uv_alarm`, `setting_pack_uv_rec`, `setting_pack_ovp`,
`setting_pack_ovp_rec`, `setting_pack_uvp`, `setting_pack_uvp_rec`,
`setting_charge_ovp`, `setting_charge_ovp_rec`, `setting_charge_temp_*`,
`setting_discharge_temp_*`, `setting_cell_heat`, `setting_cell_heat_rec`.

## Text sensors

| `type` | Contents |
|---|---|
| `profile` | active protocol name |
| `settings_raw` | settings block as hex |
| `scan_results` | deduplicated advertisements with RSSI |
| `output` | last console/scan message |
| `status_tail` | unmapped bytes after the status fields |

## Buttons and text

| `type` | Action |
|---|---|
| `scan` | collect advertisements for 30 s |
| `run` | execute the `BLE command` text |
| `settings` | request the settings block (`0x47`) |
| `alarms` | request the alarm block (`0x44`) |

The console accepts `connect`, `disconnect`, `pair`, `dump` and `send <hex>`.
