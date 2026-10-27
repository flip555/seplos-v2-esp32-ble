# Seplos V2 BLE bridge for ESPHome

A read-only ESPHome component that talks to Seplos / PACE V2 battery BMS units
over Bluetooth LE from an ESP32 and exposes typed entities to Home Assistant.

- Pack voltage, current, power, SOC, SOH, cycles, capacity, port voltage
- Every cell voltage, every temperature probe, min/max/delta, derived energy
- Device identity and the settings block, decoded and read-only
- Multiple packs on one ESP32: persistent links, staggered polling
- Sub-devices, so each pack groups as its own device in Home Assistant

Configuration writes are intentionally not implemented.

## Requirements

- ESP32 with BLE (classic ESP32 or ESP32-S3)
- ESPHome 2026.5 or newer
- A Seplos / PACE V2 BMS advertising the BLE service `FF00`
  (characteristics `FF01` notify, `FF02` write)

## Install

```yaml
external_components:
  - source: github://flip555/seplos-v2-esp32-ble@main
    components: [bms_bridge]
```

## Minimal configuration

```yaml
esphome:
  name: van-bms
  devices:
    - id: pack1_live
      name: "Pack 1 Live"

external_components:
  - source: github://flip555/seplos-v2-esp32-ble@main
    components: [bms_bridge]

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:
  level: INFO
  logs:
    esp32_ble_client: WARN
    bms_bridge.ble: WARN

api:
  encryption:
    key: "REPLACE_WITH_A_BASE64_KEY"

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

esp32_ble_tracker:
  scan_parameters:
    interval: 1100ms
    window: 1100ms
  on_ble_advertise:
    - lambda: |-
        id(hub1)->note_advert(x.address_str(), x.get_rssi(), x.get_name(),
                              (int) x.get_service_uuids().size());

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: pack1

bms_bridge:
  id: hub1
  transport: ble
  ble_client_id: pack1
  profile: seplos_v2_ble
  address: 0x00
  update_interval: 10s

sensor:
  - platform: bms_bridge
    bms_bridge_id: hub1
    device_id: pack1_live
    type: total_voltage
    name: "Pack voltage"
  - platform: bms_bridge
    bms_bridge_id: hub1
    device_id: pack1_live
    type: current
    name: "Current"
  - platform: bms_bridge
    bms_bridge_id: hub1
    device_id: pack1_live
    type: soc
    name: "SOC"
  - platform: bms_bridge
    bms_bridge_id: hub1
    device_id: pack1_live
    type: cell_voltage
    index: 1
    name: "Cell 1"
```

`examples/van-bms.yaml` is a complete two-pack configuration, including the
settings and diagnostics entities.

## Entity model

Entities are declared as `type` plus an optional `index`, so the platform is
shared across protocols:

| Type | Unit | Notes |
|---|---|---|
| `total_voltage` | V | pack voltage |
| `current` | A | positive = charging |
| `power` | W | pack voltage × current |
| `soc`, `soh` | % | |
| `cycles` | | |
| `capacity`, `remaining_capacity`, `rated_capacity` | Ah | |
| `port_voltage` | V | DC output side |
| `cell_voltage` | V | needs `index: 1..N` |
| `temperature` | °C | needs `index: 1..N` |
| `cell_voltage_min` / `_max` / `_delta` | V | |
| `temperature_min` / `_max` | °C | |
| `full_charge` | Ah | derived: capacity − remaining |
| `full_charge_watts`, `remaining_watts`, `capacity_watts` | Wh | derived |
| `battery_watts` | W | port voltage × current |
| `cell_max_number`, `cell_min_number` | | 1-based |
| `temp_max_number`, `temp_min_number` | | 1-based |
| `temp_delta` | °C | |
| `setting_*` | V / °C | read-only thresholds from the settings block |

Text sensors: `profile`, `settings_raw`, `scan_results`, `output`, `status_tail`.
Buttons: scan, run console command, read settings, read alarms.


## Diagnostics

The component logs a per-hub line every 30 seconds:

```
hub seplos_v2_ble: chunks=41 bytes=1093 frames=6 buf=0 crc=0 drops=0 valid=1
```

`frames` should keep climbing and `crc`/`drops` should stay near zero. A rising
`drops` with few `frames` means the link is losing frame starts, usually a
range problem or burst pressure on a second concurrent connection.

## Protocol

See [docs/protocol.md](docs/protocol.md) for the frame layout, command IDs and
field offsets, and [docs/entities.md](docs/entities.md) for the full entity list.
