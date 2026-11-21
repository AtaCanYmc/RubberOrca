from __future__ import annotations

from dataclasses import dataclass

STX = 0x02
ETX = 0x03
VERSION = 0x01
PAYLOAD_MAX = 1024
ACK_LEN = 6
BLE_CHUNK_SIZE = 20

SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"
CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"


class ProtocolError(ValueError):
    """Raised when frame encoding or ACK decoding fails."""


@dataclass(frozen=True)
class Ack:
    seq: int
    ok: bool
    code: int


def escape_text(text: str) -> str:
    """Escape text for the RubberOrca `type "..."` command."""
    return (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\t", "\\t")
    )


def build_frame(payload: str, seq: int = 1) -> bytes:
    """Encode a command string into an STX/ETX XOR-checked binary frame."""
    data = payload.encode("utf-8")
    if len(data) > PAYLOAD_MAX:
        raise ProtocolError(f"Payload length {len(data)} exceeds PAYLOAD_MAX ({PAYLOAD_MAX})")
    xor = 0
    for b in data:
        xor ^= b
    header = bytes([STX, VERSION, seq & 0xFF, (len(data) >> 8) & 0xFF, len(data) & 0xFF])
    return header + data + bytes([xor, ETX])


def parse_ack(raw: bytes | bytearray) -> Ack:
    """Parse a 6-byte ACK frame `[STX, VER, SEQ, OK, CODE, ETX]`."""
    if len(raw) != ACK_LEN:
        raise ProtocolError(f"Expected {ACK_LEN} ACK bytes, got {len(raw)}")
    if raw[0] != STX or raw[5] != ETX:
        raise ProtocolError(f"Invalid ACK framing: {bytes(raw).hex()}")
    if raw[1] != VERSION:
        raise ProtocolError(f"Unsupported ACK version: {raw[1]}")
    return Ack(seq=raw[2], ok=bool(raw[3]), code=raw[4])
