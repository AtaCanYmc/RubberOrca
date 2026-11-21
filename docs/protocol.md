[Back to Documentation](README.md) | [Back to Root README](../README.md)

# Wire Protocol Specification

RubberOrca implements the RubberOtter v1 framed protocol over BLE GATT and USB CDC Serial.

## BLE GATT Profile

| Attribute | Value |
| :--- | :--- |
| Service UUID | `0000ffe0-0000-1000-8000-00805f9b34fb` (`0xFFE0`) |
| Characteristic UUID | `0000ffe1-0000-1000-8000-00805f9b34fb` (`0xFFE1`) |
| Properties | `READ`, `WRITE`, `WRITE_NR`, `NOTIFY` |
| Notify Chunk Size | 20 bytes |
| Default Device Name | `RubberOrca` |

## Framed Packet Format

```text
Byte:   0      1      2      3       4       5 .. 5+N-1   5+N    6+N
      +------+------+------+-------+-------+------------+------+------+
      | STX  | VER  | SEQ  | LEN_H | LEN_L |  PAYLOAD   | XOR  | ETX  |
      +------+------+------+-------+-------+------------+------+------+
        0x02   0x01   0..255    big-endian    N <= 1024   byte    0x03
```

- `LEN`: 16-bit big-endian payload length (`0` to `1024`).
- `XOR`: bitwise XOR of all `N` payload bytes (`0x00` when `N == 0`).

## ACK Frame Format

Every framed command produces a 6-byte ACK over BLE notify and USB CDC:

```text
[0x02, 0x01, SEQ, OK, CODE, 0x03]
```

| `OK` | `CODE` | Meaning |
| :--- | :--- | :--- |
| `1` | `0` | Command or command chain executed successfully. |
| `0` | `1` | Unsupported protocol version, unknown command, invalid argument, or out-of-range value. |
| `0` | `2` | Declared payload length exceeds `PAYLOAD_MAX` (1024 bytes). |
| `0` | `3` | Computed XOR checksum does not match frame checksum byte. |

## Single-Byte Fallback Commands

When the parser is in `WAIT_STX`, raw bytes in `0x10`..`0x8F` trigger direct actions without framing or ACK:

| Byte | Action | Byte | Action |
| :--- | :--- | :--- | :--- |
| `0x11` | Media Play / Pause | `0x31` | Lock screen (`Win+L` and `Ctrl+Cmd+Q`) |
| `0x12` | Media Next | `0x32` | Toggle mouse jiggler |
| `0x13` | Media Previous | `0x33` | Task Manager / Force Quit |
| `0x14` | Volume Up | `0x34` | Show Desktop (`Win+D` and `Cmd+F3`) |
| `0x15` | Volume Down | `0x35` | Vibrate 100 ms |
| `0x16` | Volume Mute | `0x41` | Macro chain `b` -> `4` -> `2` |
| `0x21` | Right Arrow | `0x81` | Mouse Left Click |
| `0x22` | Left Arrow | `0x82` | Mouse Right Click |
| `0x23` | F5 | `0x83` | Mouse Middle Click |
| `0x24` | Key `b` | `0x84` | Mouse Scroll Up (+1) |
| | | `0x85` | Mouse Scroll Down (-1) |

## Security and Input Bounds

| Parameter | Limit | Exceeding Behavior |
| :--- | :--- | :--- |
| `PAYLOAD_MAX` | 1024 bytes | NACK (`code 2`) |
| `delay <ms>` | `1` to `30000` ms | NACK (`code 1`) |
| `press <mod> [ms]` | `1` to `5000` ms (default `50`) | NACK (`code 1`) |
| `vibrate <ms>` | `0` to `5000` ms | NACK (`code 1`) |
| `mouse move` / `scroll` | `-127` to `127` | Clamped to `[-127, 127]` |
| `ble name "..."` | 1 to 29 printable ASCII chars (`0x20`..`0x7E`) | NACK (`code 1`) |
| `ble passkey <pin\|off>` | `off`, `0`, or 6-digit integer `100000`..`999999` | NACK (`code 1`) |
| `layout <us\|tr_q>` | `us`, `tr_q`, `tr-q`, or `tr` | NACK (`code 1`) |
| `macro run` nesting | Max depth `2` | NACK (`code 1`) |

## BLE Passkey Bonding and Keyboard Layouts

- **BLE Passkey Bonding:** Setting `ble passkey 482913` persists the 6-digit passkey in NVS. On the next boot, `BleTransport` enables BLE Secure Connections, MITM, bonding, and encrypted GATT permissions on `0xFFE1`. Run `ble passkey off` and reboot to restore open HM-10 compatibility.
- **Turkish Q Layout (`layout tr_q`):** Decodes UTF-8 payloads in `type "..."` and transmits raw USB HID Boot Keyboard usage IDs plus `LSHIFT` (`0x02`) and `RALT` (`0x40`) modifier reports for Turkish letters (`ğ`, `Ğ`, `ü`, `Ü`, `ş`, `Ş`, `ı`, `İ`, `ö`, `Ö`, `ç`, `Ç`) and Turkish Q punctuation/AltGr symbols (`@`, `\`, `[`, `]`, `{`, `}`, `|`, `#`, `$`, `~`).
