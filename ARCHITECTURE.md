# Architecture

```
  transport (BLE)
        │ raw bytes
        ▼
  protocol adapter (framing + field decoding)
        │ PackInfo, engineering units
        ▼
  platform entities (sensor / text_sensor / button)
```

The transport moves bytes; the adapter knows one wire format; the platform
publishes entities. Adding a new BMS family means writing an adapter, not a
new entity platform.

## Components

| File | Responsibility |
|---|---|
| `ble_transport.*` | GATT client node: connect, MTU, CCCD, notify → `on_bytes` |
| `ble_scan.*` | collects advertisements for the scan sensor |
| `protocol.h` | `Protocol` interface + `PackInfo` |
| `seplos_v2.*` | Seplos V2 framing, checksum, field decoding, settings block |
| `bms_bridge.*` | hub: polling, publishing, reconnect/backoff, diagnostics |
| `entities.h`, `*.py` | entity classes and YAML codegen |

## Data flow

1. `esp32_ble_tracker` sees advertisements; the YAML lambda calls
   `hub->note_advert()` for the scan sensor.
2. `ble_client` connects and enables notifications on `FF01`.
3. Notifications are handed to `BmsBridge::on_bytes()` untouched; the callback
   deliberately does no formatting or logging.
4. `SeplosV2::feed()` appends to a buffer, finds `0x7E`, validates length and
   CRC16-XMODEM, and decodes the payload into `PackInfo`.
5. On the configured interval the hub polls `0x61`/`0x51` and publishes the
   registered entities.
6. `0x47` is read once per boot for the settings block.

## Robustness

- Frame assembly tolerates arbitrary notification splits: a chunk without
  `0x7E` is kept, not discarded.
- An implausible length (`0` or `> 320`) advances one byte to resynchronise
  instead of waiting forever.
- Link liveness is judged by recent traffic, not only by the READY event,
  because a second concurrent connection can miss the CCCD completion.
- Five consecutive connection failures pause BLE for 60 s; a 30 s watchdog
  pauses a client that never establishes a link.
