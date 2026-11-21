[Back to Documentation](README.md) | [Back to Root README](../README.md)

# Examples

Copy-paste examples for controlling RubberOrca over BLE 5.0 GATT, USB CDC Serial, Web Bluetooth, and RubberOtter clients.

## 1. Frame Builder (Reference)

Every framed packet follows `[0x02, 0x01, SEQ, LEN_HI, LEN_LO, PAYLOAD..., XOR, 0x03]`.

### Python (`bleak`) over BLE GATT

```python
import asyncio
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"
CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"


def build_frame(payload: str, seq: int = 1) -> bytes:
    data = payload.encode("utf-8")
    xor = 0
    for b in data:
        xor ^= b
    header = bytes([0x02, 0x01, seq & 0xFF, (len(data) >> 8) & 0xFF, len(data) & 0xFF])
    return header + data + bytes([xor, 0x03])


def parse_ack(raw: bytearray) -> tuple[int, bool, int]:
    if len(raw) != 6 or raw[0] != 0x02 or raw[5] != 0x03:
        raise ValueError(f"Unexpected ACK frame: {raw.hex()}")
    return raw[2], bool(raw[3]), raw[4]


async def main():
    device = await BleakScanner.find_device_by_filter(
        lambda d, adv: SERVICE_UUID in [u.lower() for u in adv.service_uuids]
    )
    if not device:
        raise RuntimeError("RubberOrca not found")

    ack_event = asyncio.Event()
    last_ack = bytearray()

    def on_notify(_sender, data: bytearray):
        last_ack.clear()
        last_ack.extend(data)
        ack_event.set()

    async with BleakClient(device) as client:
        await client.start_notify(CHAR_UUID, on_notify)

        frame = build_frame('type "hello from BLE" && enter', seq=1)
        for i in range(0, len(frame), 20):
            await client.write_gatt_char(CHAR_UUID, frame[i : i + 20], response=False)

        await asyncio.wait_for(ack_event.wait(), timeout=3.0)
        seq, ok, code = parse_ack(last_ack)
        print(f"ACK seq={seq} ok={ok} code={code}")


asyncio.run(main())
```

### Python (`pyserial`) over USB CDC Serial

```python
import serial

PORT = "/dev/cu.usbmodem1101"  # Replace with output of `make devices`


def build_frame(payload: str, seq: int = 1) -> bytes:
    data = payload.encode("utf-8")
    xor = 0
    for b in data:
        xor ^= b
    return bytes([0x02, 0x01, seq & 0xFF, len(data) >> 8, len(data) & 0xFF]) + data + bytes([xor, 0x03])


with serial.Serial(PORT, 115200, timeout=2) as ser:
    ser.write(build_frame('type "hello from USB CDC" && enter', seq=2))
    ack = ser.read(6)
    print(f"ACK raw={ack.hex()} seq={ack[2]} ok={bool(ack[3])} code={ack[4]}")
```

### Web Bluetooth (Browser / PWA)

```javascript
const SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
const CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";

function buildFrame(payload, seq = 1) {
  const bytes = new TextEncoder().encode(payload);
  let xor = 0;
  for (const b of bytes) xor ^= b;
  const frame = new Uint8Array(bytes.length + 7);
  frame[0] = 0x02;
  frame[1] = 0x01;
  frame[2] = seq & 0xff;
  frame[3] = (bytes.length >> 8) & 0xff;
  frame[4] = bytes.length & 0xff;
  frame.set(bytes, 5);
  frame[5 + bytes.length] = xor;
  frame[6 + bytes.length] = 0x03;
  return frame;
}

async function sendOverBle(command) {
  const device = await navigator.bluetooth.requestDevice({
    filters: [{ services: [SERVICE_UUID] }],
  });
  const server = await device.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  const ch = await service.getCharacteristic(CHAR_UUID);

  await ch.startNotifications();
  ch.addEventListener("characteristicvaluechanged", (e) => {
    const v = new Uint8Array(e.target.value.buffer);
    console.log("ACK:", { seq: v[2], ok: v[3] === 1, code: v[4] });
  });

  const frame = buildFrame(command, 1);
  for (let i = 0; i < frame.length; i += 20) {
    await ch.writeValueWithoutResponse(frame.slice(i, i + 20));
  }
}
```

## 2. Command and Macro Recipes

### Typing Text with Escapes

```text
type "line 1\nline 2\t\"quoted\"\\path"
```

### Chaining Commands

Use `&&` or `;` to run multiple actions inside one framed packet. One ACK is returned after the chain completes:

```text
press gui 50 && delay 250 && type "terminal" && delay 150 && enter
```

### Persisting and Running NVS Macros

Macros are stored in ESP32 NVS slots `m0` through `m15` (up to 1023 bytes each) and survive power cycles.

```text
macro define m0 { type "git status" && enter }
macro run m0
```

Macro bodies can call another slot up to nesting depth 2:

```text
macro define m1 { type "git pull --rebase" && enter }
macro define m2 { macro run m1 && delay 500 && macro run m0 }
macro run m2
```

### Media, Mouse, and Jiggler

```text
media play_pause
media volume_up
media mute
mouse move 40 -20
mouse click right
mouse scroll -3
jiggler on
jiggler toggle
jiggler off
```

### Renaming the BLE Device

Updates GAP advertising and persists the name in NVS (`ble_name`). Printable ASCII only, 1 to 29 characters:

```text
ble name "OrcaMini"
```

## 3. Single-Byte Fallback Triggers

Send a single raw byte while the parser is idle (no `STX/ETX` header, no ACK):

```python
# Toggle mouse jiggler (0x32) or trigger Play/Pause (0x11)
await client.write_gatt_char(CHAR_UUID, bytes([0x32]), response=False)
await client.write_gatt_char(CHAR_UUID, bytes([0x11]), response=False)
```

## 4. Using Existing RubberOtter Clients

RubberOrca advertises Service `0xFFE0` and Characteristic `0xFFE1` for drop-in compatibility with existing [RubberOtter](https://github.com/AtaCanYmc/RubberOtter) tools:

```bash
# RubberOtter Python CLI over BLE (auto-discovers 0xFFE0)
rubberotter --ble type "hello from RubberOrca"
rubberotter --ble media play_pause
rubberotter --ble jiggler toggle
```
