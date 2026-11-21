[Back to Documentation](README.md) | [Back to Root README](../README.md)

# Architecture

RubberOrca consolidates the two-chip `ATmega32U4` plus `HM-10` UART bridge from RubberOtter onto a single ESP32-S3 SoC while preserving host-side stealth and client-side protocol compatibility.

## System Data Flow

```mermaid
sequenceDiagram
    participant Client as RubberOtter Client
    participant BLE as BleTransport (Core 0 NimBLE)
    participant Loop as main.cpp / PacketParser (Core 1)
    participant Exec as CommandExecutor
    participant HID as TinyUSB HID
    participant Host as Target Host PC

    Client->>BLE: Write GATT 0xFFE1 (STX..ETX)
    BLE->>BLE: Push bytes into rxBuf under portMUX_TYPE
    Loop->>BLE: bleTransport_read()
    Loop->>Loop: Verify VERSION, LEN, XOR checksum
    Loop->>Exec: execute_command(payload, seq)
    Exec->>HID: Keyboard / Mouse / Consumer report
    HID->>Host: Wired USB HID packet
    Exec->>BLE: sendAck(seq, ok, code) via 0xFFE1 Notify
    BLE-->>Client: 6-byte ACK frame
```

## Concurrency and Thread Safety

- **Core 0 (NimBLE Host Task):** Executes `CharCallbacks::onWrite` and `ServerCallbacks::onConnect` / `onDisconnect` in `src/BleTransport.cpp`. Incoming bytes are copied into a 2048-byte circular buffer protected by a FreeRTOS `portMUX_TYPE` spinlock.
- **Core 1 (Arduino `loopTask`):** Drains `bleTransport_read()` and USB CDC `Serial`, feeds `PacketParser`, runs `CommandExecutor`, and ticks `jiggler_poll()`.
- **Stack Safety:** `CommandExecutor` places the 1025-byte `type` scratch buffer and 1024-byte `macro define` buffer in static storage and caps nested `macro run` recursion at depth 2 to stay well inside the 8 KB `loopTask` stack.

## Hardware Abstraction Layer

Protocol parsing (`src/PacketParser.cpp`), command dispatch (`src/CommandExecutor.cpp`), and ACK serialization (`src/Utils.cpp`) depend only on `include/Backend.h` and `include/Protocol.h`.

- On ESP32-S3 (`ARDUINO_ARCH_ESP32`), `src/Esp32Backend.cpp` implements `Backend.h` using Arduino-ESP32 `USBHIDKeyboard`, `USBHIDMouse`, `USBHIDConsumerControl`, `Preferences`, and `BleTransport`.
- On the host (`native` PlatformIO environment), `test/test_protocol/test_protocol.cpp` provides an in-memory spy backend for deterministic Unity testing.

## NVS Storage Layout

Persistent state is stored in ESP32 NVS via `Preferences` under namespace `orca`:

| NVS Key | Type | Max Size | Default | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `ble_name` | String | 29 bytes | `RubberOrca` | BLE GAP advertising name persisted by `ble name "..."`. |
| `ble_passkey` | UInt32 | 4 bytes | `0` (disabled) | Optional 6-digit static BLE bonding passkey (`100000`..`999999`). |
| `kbd_layout` | UInt8 | 1 byte | `0` (`us`) | Active USB HID keyboard layout (`0` = US, `1` = Turkish Q). |
| `m0` .. `m15` | String | 1023 bytes | Empty | Macro command chains persisted by `macro define mX { ... }`. |
