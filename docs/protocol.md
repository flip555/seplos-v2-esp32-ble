# Seplos V2 BLE protocol

## Frame

```
7E  <ver>  <addr>  <CID2>  <lchksum>  <len_hi>  <len_lo>  <INFO>  <CRC16>  0D
```

- `7E` start, `0D` end
- `CID2` sits at **offset 3** (this is the field that identifies the message)
- `len_hi/len_lo` at offsets 5–6 = length of `INFO`
- total frame length = `10 + len`
- `CRC16` is CRC16-XMODEM over bytes 1 … (total − 4)

## Command IDs observed

| CID2 | Direction | Contents |
|---|---|---|
| `0x61` | response | live status: cells, temperatures, current, voltage, capacity, SOC, SOH, cycles, port voltage |
| `0x51` | response | device info: model, hardware and software version |
| `0x47` | response | settings block: two header bytes then 16-bit big-endian words |
| `0x44` | request | alarm block — **no response observed over BLE** |
| `0xA2` / `0xA3` | push | unsolicited state change |
| `0x81` | response | error |

## Status payload (0x61)

Parsed in order:

1. `cells-1` bytes of cell voltages (millivolts, 16-bit)
2. one byte temperature count `t`, then `t` × 16-bit values in tenths of Kelvin
   (subtract 2731, divide by 10). The block is **not** all cell sensors: the
   first `t-2` are the battery temperatures and the last two are ambient and
   power/MOSFET (16S profile: `t = 6`, i.e. 4 battery + ambient + power).
3. current (signed 16-bit, 0.01 A)
4. pack voltage (0.01 V)
5. remaining capacity (0.01 Ah)
6. custom counter (1 byte)
7. capacity (0.01 Ah)
8. SOC (0.1 %)
9. rated capacity (0.01 Ah)
10. cycles
11. SOH (0.1 %)
12. port voltage (0.01 V)

### State tail (39 bytes for 16 cells / 6 temp words)

Everything after byte `o+19` is the state tail. With `c` cells and `t` temp
words the layout is:

| Offset | Size | Field |
|---|---|---|
| +0 | `c` | battery warning byte, one per cell |
| +c | `t-2` | temperature warning byte, one per battery temp |
| +c+t-2 | 1 | ambient temperature warning |
| +c+t-1 | 1 | power temperature warning |
| +c+t | 1 | charge current warning |
| +c+t+1 | 1 | reserved |
| +c+t+2 | 1 | system state (bitmask, table below) |
| +c+t+3 | 1 | switch state (bit0 charge switch closed, bit1 discharge switch closed) |
| +c+t+4 | 1 | `n`, number of event bytes |
| +c+t+5 | `n` | event bits, 8 per byte, LSB first |
| … | ceil(c/8) | per-cell balance bits |
| … | ceil(c/8) | per-cell broken/open bits |

For 16 cells / 6 temp words this is 16+4+1+1+1+1+1+1+1+8+2+2 = 39 bytes. A
healthy discharging pack reads `00 ×24  01 03 08  00 ×12`: `01` = discharge,
`03` = charge and discharge switches closed, `08` = eight event bytes, all zero.

System state (bitmask values are hex):

| Value | State |
|---|---|
| 0x01 | Discharge |
| 0x02 | Charge |
| 0x04 | Float |
| 0x08 | Fully charged |
| 0x10 | Standby |
| 0x20 | Shutdown |

The event bits are ordered as the vendor alarm table: byte 0 bit 0 is
*voltage sensor invalidation*, bit 3 *button switch invalidation*, bit 5
*charge switch invalidation*, bit 6 *discharge switch invalidation*, then cell
and pack OV/UV, charge/discharge temperature warn+protect, ambient/power
temperature, heating, overcurrent, short circuit and the remaining flags.

## Settings payload (0x47)

Two leading bytes, then big-endian words. Word order (validated against the
pack's own RS485 values):

| Word | Meaning | Scale |
|---|---|---|
| 0–1 | cell over-voltage alarm / recovery | mV |
| 2–3 | cell under-voltage alarm / recovery | mV |
| 4–5 | cell over-voltage protection / recovery | mV |
| 6–7 | cell under-voltage protection / recovery | mV |
| 8 | balance start | mV |
| 9 | charge forbidden below | mV |
| 10–11 | pack over-voltage alarm / recovery | 10 mV |
| 12–13 | pack under-voltage alarm / recovery | 10 mV |
| 14–15 | pack over-voltage protection / recovery | 10 mV |
| 16–17 | pack under-voltage protection / recovery | 10 mV |
| 18–19 | charging over-voltage protection / recovery | 10 mV |
| 20+ | temperature thresholds | tenths of Kelvin |

Words 20 and onward carry charge/discharge high/low warning and protection
temperatures; the component exposes the first 18 words after the temperature
block, at indices 20–37.

