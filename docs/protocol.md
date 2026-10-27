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
2. one byte temperature count, then count × 16-bit values in tenths of Kelvin
   (subtract 2731, divide by 10)
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

Everything after byte `o+19` is the unmapped state tail. In a healthy pack it
is mostly zero with a few state bytes, for example:

```
00 ×24  01 03 08  00 ×12
```

Observed correspondence with the RS485 status page: `01` = system state
(discharge), `03` = both charge and discharge switches closed. It is **not**
the alarm event table, which only exists in the RS485 teledata frame.

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

