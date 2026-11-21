from __future__ import annotations

import asyncio
from typing import Optional

from .protocol import (
    ACK_LEN,
    BLE_CHUNK_SIZE,
    CHAR_UUID,
    SERVICE_UUID,
    Ack,
    ProtocolError,
    build_frame,
    parse_ack,
)


async def scan_ble(timeout: float = 4.0) -> list[tuple[str, str]]:
    """Return `(address, name)` pairs advertising Service 0xFFE0."""
    from bleak import BleakScanner

    discovered = await BleakScanner.discover(timeout=timeout, return_adv=True)
    matches: list[tuple[str, str]] = []
    for device, adv in discovered.values():
        uuids = [u.lower() for u in (adv.service_uuids or [])]
        if SERVICE_UUID in uuids:
            matches.append((device.address, device.name or adv.local_name or "Unknown"))
    return matches


def list_serial_ports() -> list[tuple[str, str]]:
    """Return `(device, description)` pairs for available serial ports."""
    from serial.tools import list_ports

    return [(p.device, p.description) for p in list_ports.comports()]


async def send_ble_frame(
    frame: bytes,
    address: Optional[str] = None,
    timeout: float = 3.0,
    expect_ack: bool = True,
) -> Optional[Ack]:
    """Send raw bytes over BLE GATT 0xFFE1 and optionally await a 6-byte ACK."""
    from bleak import BleakClient, BleakScanner

    target = address
    if not target:
        device = await BleakScanner.find_device_by_filter(
            lambda _d, adv: SERVICE_UUID in [u.lower() for u in (adv.service_uuids or [])],
            timeout=timeout,
        )
        if not device:
            raise RuntimeError("No RubberOrca BLE device found (Service 0xFFE0)")
        target = device

    ack_event = asyncio.Event()
    rx_buf = bytearray()

    def on_notify(_sender: object, data: bytearray) -> None:
        rx_buf.extend(data)
        if len(rx_buf) >= ACK_LEN:
            ack_event.set()

    async with BleakClient(target, timeout=timeout) as client:
        if expect_ack:
            await client.start_notify(CHAR_UUID, on_notify)

        for i in range(0, len(frame), BLE_CHUNK_SIZE):
            await client.write_gatt_char(CHAR_UUID, frame[i : i + BLE_CHUNK_SIZE], response=False)

        if not expect_ack:
            return None

        await asyncio.wait_for(ack_event.wait(), timeout=timeout)
        return parse_ack(rx_buf[:ACK_LEN])


def send_serial_frame(
    frame: bytes,
    port: str,
    baud: int = 115200,
    timeout: float = 3.0,
    expect_ack: bool = True,
) -> Optional[Ack]:
    """Send raw bytes over USB CDC Serial and optionally read a 6-byte ACK."""
    import serial

    with serial.Serial(port, baud, timeout=timeout) as ser:
        ser.reset_input_buffer()
        ser.write(frame)
        ser.flush()
        if not expect_ack:
            return None
        raw = ser.read(ACK_LEN)
        if len(raw) != ACK_LEN:
            raise ProtocolError(f"Timed out waiting for ACK on {port} (got {len(raw)} bytes)")
        return parse_ack(raw)


def send_command(
    command: str,
    *,
    port: Optional[str] = None,
    address: Optional[str] = None,
    baud: int = 115200,
    timeout: float = 3.0,
    seq: int = 1,
) -> Ack:
    """Encode and transmit a command over Serial (if `port` given) or BLE."""
    frame = build_frame(command, seq=seq)
    if port:
        ack = send_serial_frame(frame, port=port, baud=baud, timeout=timeout, expect_ack=True)
    else:
        ack = asyncio.run(send_ble_frame(frame, address=address, timeout=timeout, expect_ack=True))
    assert ack is not None
    return ack


def send_raw_byte(
    byte_val: int,
    *,
    port: Optional[str] = None,
    address: Optional[str] = None,
    baud: int = 115200,
    timeout: float = 3.0,
) -> None:
    """Send a single-byte fallback command (0x10..0x8F) without framing."""
    if not (0x10 <= byte_val <= 0x8F):
        raise ProtocolError(f"Fallback byte 0x{byte_val:02X} outside 0x10..0x8F")
    raw = bytes([byte_val])
    if port:
        send_serial_frame(raw, port=port, baud=baud, timeout=timeout, expect_ack=False)
    else:
        asyncio.run(send_ble_frame(raw, address=address, timeout=timeout, expect_ack=False))
