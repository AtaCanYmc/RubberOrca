[Back to Documentation](README.md) | [Back to Root README](../README.md)

# Hardware Guide

RubberOrca compiles from one `platformio.ini` for two ESP32-S3 boards.

## Board Comparison

| Specification | ESP32-S3 Super Mini | ESP32-S3 N16R8 DevKitC |
| :--- | :--- | :--- |
| PlatformIO Environment | `esp32s3_supermini` | `esp32s3_n16r8` |
| Board ID | `lolin_s3_mini` | `esp32-s3-devkitc1-n16r8` |
| Flash | 4 MB QIO | 16 MB QIO |
| PSRAM | 2 MB QSPI | 8 MB OPI |
| Partition Table | `huge_app.csv` (3 MB app) | `default_16MB.csv` (6.5 MB app) |
| USB-C Ports | 1 (Native USB OTG) | 2 (`USB` OTG and `UART` bridge) |
| Form Factor | Compact USB dongle | Development board |

## USB-C Port Selection

Both environments set `-DARDUINO_USB_MODE=0` and `-DARDUINO_USB_CDC_ON_BOOT=1`. TinyUSB enumerates a composite USB device providing HID Keyboard, HID Mouse, HID Consumer Control, and USB CDC Serial over the native USB OTG pins (GPIO19 and GPIO20).

On the **ESP32-S3 N16R8 DevKitC**, plug the host cable into the **`USB`** port. The **`UART`** port connects only to the onboard USB-to-UART chip and cannot transmit USB HID reports.

## Flashing and Bootloader Recovery

```bash
# List detected serial devices
make devices

# Flash target board
make flash-supermini
make flash-n16r8

# Specify an explicit port if auto-detection picks the wrong interface
make flash-n16r8 PORT=/dev/cu.usbmodem1101
```

If TinyUSB prevents automatic bootloader entry:

1. Hold the `BOOT` button.
2. Press and release `RST` (or plug the USB cable in while holding `BOOT`).
3. Release `BOOT` and run `make flash-supermini` or `make flash-n16r8`.

## Optional Haptic Motor Pin

By default, `vibrate <ms>` is a safe no-op. To drive a coin vibration motor through a MOSFET or transistor stage, add `-DVIB_PIN=<GPIO>` to `build_flags` in `platformio.ini`. Never drive a motor directly from an ESP32-S3 GPIO pin without current limiting and a flyback diode.
