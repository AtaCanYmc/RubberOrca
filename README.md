# RubberOrca

Single-chip **ESP32-S3** successor to [RubberOtter](https://github.com/AtaCanYmc/RubberOtter). Replaces the `ATmega32U4` + `HM-10` UART bridge with native TinyUSB HID + built-in BLE 5.0 GATT on one board, while remaining 100% compatible with existing RubberOtter clients (Web PWA, Capacitor Android/iOS, Python SDK/CLI/MCP/OtterDeck).

## Architecture

- **Host Stealth:** Appears to the target computer strictly as a wired USB HID Keyboard + Mouse + Consumer Control (+ USB CDC Serial). No host-side Bluetooth pairing.
- **Wireless Control:** Built-in BLE 5.0 GATT server exposing Service `0xFFE0` and Characteristic `0xFFE1` (`READ | WRITE | WRITE_NR | NOTIFY`), matching the HM-10 GATT profile used by RubberOtter clients.
- **Protocol:** `STX (0x02) | VER (0x01) | SEQ | LEN_HI | LEN_LO | PAYLOAD | XOR | ETX (0x03)` + 6-byte ACK frames + single-byte fallback commands (`0x11`–`0x85`) + `&&` / `;` command chaining.
- **Storage:** ESP32 NVS (`Preferences`, namespace `orca`) stores up to 16 macros (`m0`–`m15`, 1024 bytes each) and the BLE advertising name (`ble name "..."`).

## Supported Boards

| PlatformIO Env | Hardware | Flash / PSRAM | Partition Table |
| --- | --- | --- | --- |
| `esp32s3_supermini` | ESP32-S3 Super Mini (`lolin_s3_mini`) | 4 MB / 2 MB QSPI | `huge_app.csv` |
| `esp32s3_n16r8` | ESP32-S3 N16R8 DevKitC (`esp32-s3-devkitc1-n16r8`) | 16 MB / 8 MB OPI | `default_16MB.csv` |

> **USB-C Port Note (N16R8 DevKitC):** Always plug the **`USB` (OTG)** port—not the `UART` port—into the target host for native USB HID and CDC Serial.

## Build, Test & Flash

```bash
# Run host-side protocol & command unit tests
pio test -e native

# Compile firmware for both targets
pio run -e esp32s3_supermini
pio run -e esp32s3_n16r8

# Flash a board
pio run -e esp32s3_supermini -t upload
pio run -e esp32s3_n16r8 -t upload
```
