[Back to Documentation](README.md) | [Back to Root README](../README.md)

# Hardware Verification Checklist

Repeatable bring-up and regression checklist for **ESP32-S3 Super Mini** and **ESP32-S3 N16R8 DevKitC** against the RubberOtter Web PWA, `rubberorca` Python CLI, LittleFS DuckyScript engine, and N16R8 Wi-Fi Workstation.

## Prerequisites

- Data-capable USB-C cable.
- On **ESP32-S3 N16R8 DevKitC**, use the native **`USB` (OTG)** port, not the `UART` port.
- Desktop Chrome, Edge, or Opera for Web Bluetooth and Web Serial.
- Local CLI installed via `make install-python` or `brew install AtaCanYmc/tap/rubberorca`.

## Stage 1: Flash and USB HID Enumeration (Both Boards)

1. Connect the native USB-C OTG port. If auto-bootloader fails, hold `BOOT`, tap `RST`, then release `BOOT`.
2. Verify port detection and flash:

```bash
make devices
make flash-supermini   # ESP32-S3 Super Mini
make flash-n16r8       # ESP32-S3 N16R8 DevKitC
```

3. Press `RST` and verify host enumeration:

```bash
system_profiler SPUSBDataType | grep -A 8 -i "RubberOrca"
```

Expected:
- Host enumerates `RubberOrca HID` (Keyboard, Mouse, Consumer Control, USB CDC) as a wired USB device.
- No Bluetooth HID pairing prompt appears on the host.
- Onboard RGB LED lights dim blue `(0, 0, 16)` while advertising.

## Stage 2: Python CLI over BLE 5.0 and USB CDC

```bash
rubberorca scan
rubberorca type "hello from RubberOrca BLE"
rubberorca key enter
rubberorca media volume_up
rubberorca mouse move 40 -20
rubberorca jiggler on
rubberorca jiggler off
rubberorca --port /dev/cu.usbmodemXXXX send 'type "usb cdc ok" && enter'
```

Expected:
- Every command returns `ACK seq=... OK`.
- RGB LED switches to dim green `(0, 16, 0)` on BLE connect and dim amber `(16, 10, 0)` while jiggler is active.
- NVS persistence check: run `rubberorca macro define m0 'type "nvs ok" && enter'` and `rubberorca ble-name "OrcaTest"`, unplug and replug USB, then confirm `rubberorca scan` shows `OrcaTest` and `rubberorca macro run m0` executes.

## Stage 3: RubberOtter Web PWA Compatibility

1. Open the RubberOtter Web PWA in Chrome/Edge and connect via Web Bluetooth (`0xFFE0` / `0xFFE1`).
2. Verify `type`, single-byte media fallbacks (`0x11`..`0x16`), mouse controls, and jiggler (`0x32`).
3. Disconnect the PWA and verify the LED returns to dim blue and reconnect succeeds immediately.

## Stage 4: v1.1 Ergonomics and BLE Hardening

```bash
# Turkish Q layout (with host OS set to Turkish Q)
rubberorca layout tr_q
rubberorca type "Merhaba İstanbul ğüşıöç @orca"
rubberorca layout us

# Static 6-digit BLE passkey bonding (applies after RST)
rubberorca ble-passkey 482913
# Reset board, verify OS/client prompts for 482913, then restore open mode:
rubberorca ble-passkey off
```

## Stage 5: v2.0 LittleFS DuckyScript (Both Boards)

```bash
rubberorca script save demo.orca 'REM hardware check'
rubberorca script append demo.orca 'DEFAULT_DELAY 40'
rubberorca script append demo.orca 'STRINGLN Hello from LittleFS'
rubberorca script list
rubberorca script run demo.orca
rubberorca script del demo.orca
```

Security check: `rubberorca send 'script save ../evil.orca { enter }'` must return `ERR(code=1)`.

## Stage 6: v2.0 Wi-Fi SoftAP, OtterDeck, and A/B OTA (N16R8 Only)

1. Confirm `RubberOrca-AP` is not visible on boot (`WIFI_OFF` default).
2. Configure token and WPA2-PSK SoftAP:

```bash
rubberorca ota-token "MyStrongOtaToken1"
rubberorca wifi ap "RubberOrca-Lab" "OrcaPass2026"
```

3. Join `RubberOrca-Lab`, open `http://192.168.4.1`, enter `MyStrongOtaToken1`, and click **CONNECT**.
4. Verify WebSocket `AUTH_OK`, live command execution, relative mouse drag pad, and LittleFS script save/run.
5. Verify OTA auth rejects an invalid token (`401`) and accepts `.pio/build/esp32s3_n16r8/firmware.bin` with the valid token (`200 OTA_OK_REBOOTING`), rebooting cleanly into the new slot.
6. Disable Wi-Fi when done: `rubberorca wifi off`.
