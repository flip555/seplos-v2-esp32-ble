# Quick start

From zero to a working pack in about ten minutes, with **one flash**.

## 1. Requirements

- An ESP32 (classic or S3) with BLE, running ESPHome 2026.5+
- A Seplos / PACE V2 BMS advertising service `FF00`
- Home Assistant connected to the ESPHome device

## 2. Minimal configuration

The `ble_client` needs *a* MAC to compile, but you no longer need to know the
real one up front — the component can store it at runtime. Use a placeholder
for the first flash:

```yaml
substitutions:
  name: bms-bridge

esphome:
  name: ${name}
  friendly_name: BMS Bridge

esp32:
  board: esp32dev
  framework:
    type: esp-idf

external_components:
  - source: github://flip555/seplos-v2-esp32-ble@main
    components: [bms_bridge]

logger:
api:
ota:
  - platform: esphome
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

esp32_ble_tracker:
  on_ble_advertise:
    - lambda: |-
        id(hub1)->note_advert(x.address_str(), x.get_rssi(), x.get_name(),
                              (int) x.get_service_uuids().size());

ble_client:
  - mac_address: "00:00:00:00:00:00"   # replaced at runtime
    id: pack1_client

bms_bridge:
  - id: hub1
    transport: ble
    ble_client_id: pack1_client
    profile: seplos_v2_ble
    address: 0x00
    mac_slot: 1
    read_settings_on_connect: true
    settings_interval: 15min

text:
  - platform: bms_bridge
    bms_bridge_id: hub1
    type: mac
    mode: text
    name: "Pack MAC"

button:
  - platform: bms_bridge
    bms_bridge_id: hub1
    type: scan
    name: "Scan"

text_sensor:
  - platform: bms_bridge
    bms_bridge_id: hub1
    type: scan_results
    name: "BLE scan results"
```

Flash this once.

## 3. Find the pack's MAC

The component scans for 30 s after boot and whenever you press **Scan**. The
`BLE scan results` text sensor then lists what it saw:

```json
{"devices":[{"mac":"60:6E:41:00:12:34","name":"SP04S016","rssi":-58,"services":1}]}
```

Pick the entry that looks like the BMS (name, or the strongest RSSI near the
pack). If several packs are nearby, take one out of range at a time, or compare
RSSI while moving the ESP32.

Alternatives: the vendor phone app shows the MAC in its device list, or any BLE
scanner app will show it.

## 4. Store it — no reflash

In Home Assistant open the device and edit **Pack MAC**, entering the MAC from
step 3 (`AA:BB:CC:DD:EE:FF`). On submit the component:

1. writes the MAC to NVS,
2. points the BLE client at it,
3. reconnects.

The value survives reboots and OTA updates. Press **Scan** again any time to
re-check what is nearby.

## 5. Multiple packs

One `ble_client` + `bms_bridge` pair per pack, with a unique `mac_slot`:

```yaml
bms_bridge:
  - id: hub1
    ble_client_id: pack1_client
    ...
    mac_slot: 1
  - id: hub2
    ble_client_id: pack2_client
    ...
    mac_slot: 2
```

Each gets its own **Pack MAC** text entity. Give every pack the same
`update_interval` and stagger `poll_offset` (e.g. 10 s with offsets 0/5) so the
polls never collide.

## 6. Check it

- `Profile` should read `seplos_v2_ble`
- `System state` should show Discharge / Charge / Standby
- Cell voltages and temperatures should be plausible
- Settings populate automatically on connect (and every `settings_interval`)

If a pack shows no data, see [Troubleshooting](../../wiki/Troubleshooting).
