[Back to ADRs](README.md) | [Back to Root README](../../README.md)

# ADR-0003: Hybrid NVS and LittleFS Storage with Line-Streaming DuckyRunner

- **Status:** Accepted
- **Date:** 2026-11-22

## Context

ESP32 NVS (`Preferences`) is fast and power-loss safe for small key-value settings (`ble_name`, `ble_passkey`, `kbd_layout`, `m0..m15`), but the default NVS partition is only 20 KB (`0x5000`). Storing multi-kilobyte DuckyScript files in NVS quickly exhausts space, and loading entire scripts into RAM risks overflowing the 8 KB Arduino `loopTask` stack.

## Decision

Adopt a hybrid storage model:
- Keep configuration keys and v1 `m0..m15` macro slots in NVS (`orca` namespace) for backward compatibility.
- Store multi-kilobyte script files under LittleFS `/scripts/` (`src/ScriptStore.cpp`, up to 32 KB per file, 64 files max) on both Super Mini (896 KB) and N16R8 (3.375 MB).
- Stream scripts line by line through a fixed 256-byte buffer into `DuckyRunner` (`src/DuckyRunner.cpp`), which reuses `Backend.h` and `KeyLayout.h` (including `TR_Q`).
- Enforce strict filename validation (`^[a-zA-Z0-9_-]{1,32}(\.orca|\.txt)?$`) to block path traversal.

## Consequences

- **Positive:** Multi-kilobyte DuckyScript payloads execute with constant ~256 B stack overhead and full Turkish Q layout support.
- **Positive:** Zero external filesystem dependencies (`LittleFS` is built into Arduino-ESP32 3.3.11).
- **Trade-off:** Individual DuckyScript lines are capped at 255 bytes.
