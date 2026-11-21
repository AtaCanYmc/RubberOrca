# Roadmap

Milestones reflect planned engineering work grouped by release target.

## v1.0.0: Core ESP32-S3 Parity

- [x] Dual-board PlatformIO setup (`esp32s3_supermini` and `esp32s3_n16r8`).
- [x] Native TinyUSB composite HID (Keyboard, Mouse, Consumer Control) and USB CDC Serial.
- [x] Built-in NimBLE GATT server exposing `0xFFE0` / `0xFFE1` with RubberOtter v1 protocol compatibility.
- [x] NVS persistence for 16 macro slots (`m0` to `m15`) and BLE advertising name.
- [x] Quote-aware and brace-aware `&&` / `;` command chaining and host Unity test suite.
- [ ] Physical hardware verification on ESP32-S3 Super Mini and ESP32-S3 N16R8 against Web PWA and Python CLI.

## v1.1.0: Hardware Ergonomics and BLE Hardening

- [x] Optional BLE passkey bonding mode configurable via NVS (`ble passkey <6-digit|off>`).
- [x] Status LED / NeoPixel feedback for BLE connection state and jiggler activity (`rgbLedWrite`).
- [x] Configurable USB VID/PID and HID string descriptors at build time (`RO_USB_*` flags).
- [x] Optional Turkish Q / non-US keyboard layout mapping table (`layout <us|tr_q>`).

## v2.0.0: Wi-Fi Workstation and Script Storage (N16R8 Focus)

- [x] LittleFS partition support for multi-kilobyte macro and DuckyScript files (`script save|append|run|list|del`).
- [x] Optional Wi-Fi SoftAP mode serving an on-device OtterDeck web UI (`:80`) and WebSocket transport (`:81`).
- [x] Authenticated Wi-Fi OTA firmware updates (`POST /api/ota` with constant-time `X-Orca-Token` and A/B rollback).
