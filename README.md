# Seplos V2 BLE bridge for ESPHome

Read-only ESPHome component that talks to Seplos / PACE V2 battery BMS units
over Bluetooth LE on an ESP32 and exposes typed entities to Home Assistant.

**[Documentation and setup guide → wiki](../../wiki)**

## What it does

- Pack voltage, current, power, SOC, SOH, cycles, capacity, port voltage
- Every cell voltage and temperature probe, plus min/max/delta and derived energy
- Device identity and the settings block, decoded and read-only
- Multiple packs on one ESP32, grouped as sub-devices
- Diagnostics: per-hub frame counters and CRC-failure capture
- Stall recovery: cycles the BLE link if a module stops notifying

Configuration writes are deliberately not implemented.

## Requirements

- ESP32 with BLE (classic ESP32 or ESP32-S3)
- ESPHome 2026.5 or newer
- A Seplos / PACE V2 BMS advertising the BLE service `FF00`

## Install

```yaml
external_components:
  - source: github://flip555/seplos-v2-esp32-ble@main
    components: [bms_bridge]
```

See the [Installation](../../wiki/Installation) page for a complete first
configuration, and [Configuration](../../wiki/Configuration) for every option.

## Minimal example

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

api:
  encryption:
    key: "REPLACE_WITH_A_BASE64_KEY"

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: *** wifi_password

esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 30ms
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
```

`examples/van-bms.yaml` is a complete two-pack configuration.

## Documentation

- [Installation](../../wiki/Installation)
- [Configuration](../../wiki/Configuration)
- [Entities](../../wiki/Entities)
- [Protocol](../../wiki/Protocol)
- [Troubleshooting](../../wiki/Troubleshooting)

## Status

Read-only by design. Configuration writes and alarm events are tracked in the
[issue tracker](../../issues). Contributions and detailed reports are welcome.

## Support

If this saved you some time: [GitHub Sponsors](https://github.com/sponsors/flip555),
[Ko-fi](https://ko-fi.com/flip555) or
[Buy Me a Coffee](https://buymeacoffee.com/flip555).
