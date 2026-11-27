# Roadmap

Milestones reflect planned engineering work grouped by release target. Items are grounded in hardware reliability, host-side stealth, and operator ergonomics.

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

## v2.1.0: Reliability, Diagnostics and Field-Ready Polish

- [ ] Device health telemetry command (`status` / `info`) reporting heap, LittleFS usage, uptime, BLE RSSI, and active layout.
- [ ] BLE MTU negotiation and chunked write reassembly for long macro/script payloads without fragmentation drops.
- [ ] Hardware watchdog (`esp_task_wdt`) and brownout recovery guards to prevent lockups during long `delay` or macro chains.
- [ ] Automated Hardware-in-the-Loop (HIL) smoke test suite in Python (`pytest` + USB CDC + BLE) for pre-release gating.

## v2.2.0: Multi-Layout, DuckyScript 3.0 and Macro Ergonomics

- [ ] Additional ISO/ANSI keyboard layout tables (`de`, `fr`, `uk`) with compile-time selection to preserve Super Mini flash budget.
- [ ] Extended DuckyScript control flow (`REPEAT`, `DEFAULT_DELAY`, `STRINGLN`) in `script run`.
- [ ] Portable JSON backup/restore (`rubberorca export` / `rubberorca import`) for NVS macros, LittleFS scripts, and BLE settings.
- [ ] Physical `BOOT` button short-press trigger to run a designated macro slot (`m0`) without a connected client.

## v3.0.0: Enclosure, Web Studio and Signed OTA

- [ ] Reference 3D-printable and CNC aluminum enclosure files (STEP/STL) for Super Mini and N16R8 form factors.
- [ ] In-browser Script & Macro Studio in `site/` (live syntax check, keystroke timing preview, and direct Web Serial / BLE deploy).
- [ ] Signed OTA firmware manifests (Ed25519 signature verification before A/B partition switch).
