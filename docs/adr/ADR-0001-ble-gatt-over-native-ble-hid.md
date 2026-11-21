[Back to ADRs](README.md) | [Back to Root README](../../README.md)

# ADR-0001: BLE GATT `0xFFE0/0xFFE1` + Wired TinyUSB HID vs Native BLE HID

- **Status:** Accepted
- **Date:** 2026-11-21

## Context

Moving from RubberOtter (`ATmega32U4` + `HM-10` UART bridge) to a single ESP32-S3 SoC makes native BLE HID (HOGP) technically possible. However, RubberOrca's core requirement is zero host-side Bluetooth pairing and full backward compatibility with existing RubberOtter clients (Web PWA, Capacitor mobile apps, Python CLI/SDK/MCP).

## Decision

Expose wired USB-OTG composite HID (`USBHIDKeyboard`, `USBHIDMouse`, `USBHIDConsumerControl`) plus USB CDC Serial to the target host via TinyUSB (`-DARDUINO_USB_MODE=0`), while running a NimBLE GATT peripheral on Service `0xFFE0` and Characteristic `0xFFE1` for wireless control. Add optional NVS-persisted 6-digit static passkey bonding (`ble passkey <100000..999999|off>`) for hardened deployments.

## Consequences

- **Positive:** Target computers see only a standard wired USB HID peripheral. No OS Bluetooth pairing dialogs or host Bluetooth radio required. Existing RubberOtter clients connect without modification.
- **Positive:** Eliminates the 9600-baud UART bottleneck and physical wiring between two chips.
- **Trade-off:** Open GATT mode is default in v1 for HM-10 client drop-in compatibility; users requiring link-layer encryption must explicitly enable `ble passkey`.
