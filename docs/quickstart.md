# Quick start

From zero to a working pack in about ten minutes, with **one flash**.

## 1. Requirements

- An ESP32 (classic or S3) with BLE, running ESPHome 2026.5+
- A Seplos / PACE V2 BMS advertising service `FF00`
- Home Assistant connected to the ESPHome device

## 2. Start from a full example

Don't hand-assemble the configuration: the repo ships complete, generated ones
with the BLE tracker hook, the `ble_client`, the MAC text entity, automatic
settings reads and every entity.

| File | Packs |
|---|---|
| [`examples/one-pack.yaml`](../examples/one-pack.yaml) | 1 |
| [`examples/two-packs.yaml`](../examples/two-packs.yaml) | 2 |
| [`examples/three-packs.yaml`](../examples/three-packs.yaml) | 3 |
| [`examples/four-packs.yaml`](../examples/four-packs.yaml) | 4 |

Copy one into ESPHome and change only:

- `substitutions.name` and the `esphome.devices` names
- `wifi_ssid` / `wifi_password` in `secrets.yaml`
- `api.encryption.key` (or remove the `encryption:` block)
- optionally `ble_client.mac_address` — leave the placeholder to pick the MAC at
  runtime

The pieces worth knowing about:

```yaml
text:
  - platform: bms_bridge
    bms_bridge_id: hub1
    type: mac
    mode: text
    name: "Pack MAC"
```

One per pack; each `bms_bridge` needs a unique `mac_slot`.

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

The value survives reboots and OTA updates; enter `clear` to erase it. Press **Scan** again any time to
re-check what is nearby.

**When it is re-read:** at every boot the stored MAC wins over the YAML value.
OTA keeps NVS, so a firmware update does not ask again. A full serial flash that
erases NVS (or an "erase flash" option) clears it — then either set **Pack MAC**
again or hardcode the real MAC in `ble_client`, which becomes the fallback. With
an all-zero placeholder and no stored MAC the client stays disabled, so there
are no futile connection attempts.

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
