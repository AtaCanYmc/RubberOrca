[Back to ADRs](README.md) | [Back to Root README](../../README.md)

# ADR-0002: Dual-Board PlatformIO Configuration (Super Mini and N16R8)

- **Status:** Accepted
- **Date:** 2026-11-21

## Context

RubberOrca targets two physically distinct ESP32-S3 boards: the compact 4 MB Flash / 2 MB PSRAM **ESP32-S3 Super Mini** (`lolin_s3_mini`) and the 16 MB Flash / 8 MB OPI PSRAM **ESP32-S3 N16R8 DevKitC** (`esp32-s3-devkitc1-n16r8`). Maintaining separate repositories or branches would duplicate protocol and HID code.

## Decision

Maintain one `platformio.ini` with shared core sources (`src/`), a host-only `native` Unity test environment, and compile-time feature flags:
- `esp32s3_supermini`: `huge_app.csv` (3 MB single app slot + 896 KB LittleFS), `-DRO_ENABLE_LITTLEFS`.
- `esp32s3_n16r8`: `default_16MB.csv` (dual 6.25 MB `ota_0`/`ota_1` slots + 3.375 MB LittleFS), `-DRO_ENABLE_LITTLEFS`, `-DRO_ENABLE_WIFI_WORKSTATION`.
- Pin `pioarduino` to `55.03.311` (`arduino-esp32 3.3.11`) instead of floating `stable` to prevent upstream LDF regressions (such as `arduino-esp32 4.0.0` `USBHostMSC.cpp -> FS.h`).

## Consequences

- **Positive:** One codebase, deterministic CI builds, and zero feature drift between boards.
- **Positive:** Super Mini stays lightweight without Wi-Fi/OTA flash bloat, while N16R8 utilizes its 16 MB flash for A/B OTA and embedded Web UI.
- **Trade-off:** Super Mini does not support wireless A/B OTA due to 4 MB flash partition limits; firmware updates on Super Mini use USB-C Web Serial or PlatformIO upload.
