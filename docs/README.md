[Back to Root README](../README.md)

# RubberOrca Documentation

Technical reference for the RubberOrca ESP32-S3 firmware, hardware targets, and wire protocol.

| Document | Scope |
| :--- | :--- |
| [architecture.md](architecture.md) | Dual-core BLE/HID data flow, TinyUSB composite device, NVS storage layout, and backend abstraction. |
| [hardware.md](hardware.md) | ESP32-S3 Super Mini and N16R8 DevKitC specifications, USB OTG port selection, flashing, and optional GPIO wiring. |
| [protocol.md](protocol.md) | Framed `STX/ETX` wire format, ACK error codes, single-byte fallback map, command syntax, and input bounds. |
