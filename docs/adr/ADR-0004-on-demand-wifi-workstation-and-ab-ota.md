[Back to ADRs](README.md) | [Back to Root README](../../README.md)

# ADR-0004: On-Demand Wi-Fi SoftAP Workstation, WebSocket, and Authenticated A/B OTA

- **Status:** Accepted
- **Date:** 2026-11-22

## Context

On the ESP32-S3 N16R8, users want an on-device OtterDeck web UI, low-latency mouse/command streaming over WebSocket, and wireless firmware updates without external servers. However, an always-on Wi-Fi AP increases power draw, leaves an RF footprint, and exposes an unauthenticated USB HID injection surface if left open.

## Decision

Gate the Wi-Fi Workstation strictly to N16R8 (`-DRO_ENABLE_WIFI_WORKSTATION`) with defense-in-depth defaults:
- **Off by default on boot:** `WiFi.mode(WIFI_OFF)` until explicitly activated via `wifi on` or `wifi ap "<ssid>" "<pass>"`.
- **Mandatory WPA2-PSK:** Rejects SoftAP passwords shorter than 8 characters; unencrypted open APs are disallowed.
- **Dual-port server:** Built-in `WebServer` on port `80` (PROGMEM single-file OtterDeck UI, `/api/cmd`, `/api/scripts`, `/api/ota`) plus `links2004/WebSockets` on port `81` for low-latency live mouse and ACK streaming.
- **Authenticated API and A/B OTA:** Every HTTP API, WebSocket session (`auth <token>`), and OTA upload (`X-Orca-Token`) verifies against an NVS-stored token (`>= 12` chars) using `constant_time_equals()`. `/api/ota` returns `403` when no token is configured and writes to the passive `ota_0`/`ota_1` partition via `Update` for automatic bootloader rollback on corrupted images.

## Consequences

- **Positive:** Zero RF/power overhead during normal BLE-only usage, and hardened authentication before any HTTP/WS command or OTA write executes.
- **Positive:** N16R8 firmware stays at 20.0% of its 6.5 MB `ota_0` partition and 27.9% RAM.
- **Trade-off:** Adds one external dependency (`links2004/WebSockets`) scoped exclusively to `[env:esp32s3_n16r8]`.
