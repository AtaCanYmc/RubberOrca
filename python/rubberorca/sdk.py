from __future__ import annotations

import asyncio
from typing import Any, Callable, Optional

from .protocol import Ack, escape_text
from .transport import list_serial_ports, scan_ble, send_command, send_raw_byte


class RubberOrca:
    """High-level Python SDK client for RubberOrca ESP32-S3 USB HID."""

    def __init__(
        self,
        *,
        port: Optional[str] = None,
        address: Optional[str] = None,
        baud: int = 115200,
        timeout: float = 3.0,
        sender: Callable[..., Ack] = send_command,
        raw_sender: Callable[..., None] = send_raw_byte,
    ) -> None:
        self.port = port
        self.address = address
        self.baud = baud
        self.timeout = timeout
        self._sender = sender
        self._raw_sender = raw_sender
        self._seq = 1

    def _next_seq(self) -> int:
        seq = self._seq
        self._seq = (self._seq + 1) & 0xFF
        return seq

    @staticmethod
    def scan(timeout: float = 4.0) -> dict[str, list[tuple[str, str]]]:
        """Discover nearby BLE 0xFFE0 devices and local serial ports."""
        return {
            "ble": asyncio.run(scan_ble(timeout=timeout)),
            "serial": list_serial_ports(),
        }

    def send(self, command: str) -> Ack:
        return self._sender(
            command,
            port=self.port,
            address=self.address,
            baud=self.baud,
            timeout=self.timeout,
            seq=self._next_seq(),
        )

    def raw_byte(self, value: int) -> None:
        self._raw_sender(
            value,
            port=self.port,
            address=self.address,
            baud=self.baud,
            timeout=self.timeout,
        )

    def type(self, text: str) -> Ack:
        return self.send(f'type "{escape_text(text)}"')

    def delay(self, ms: int) -> Ack:
        return self.send(f"delay {int(ms)}")

    def key(self, name: str) -> Ack:
        return self.send(name)

    def press(self, mod: str, ms: Optional[int] = None) -> Ack:
        return self.send(f"press {mod} {int(ms)}" if ms is not None else f"press {mod}")

    def media(self, action: str) -> Ack:
        return self.send(f"media {action}")

    def mouse_move(self, dx: int, dy: int) -> Ack:
        return self.send(f"mouse move {int(dx)} {int(dy)}")

    def mouse_click(self, button: str = "left") -> Ack:
        return self.send(f"mouse click {button}")

    def mouse_scroll(self, amount: int) -> Ack:
        return self.send(f"mouse scroll {int(amount)}")

    def jiggler(self, state: str = "toggle") -> Ack:
        return self.send(f"jiggler {state}")

    def macro_define(self, slot: str, body: str) -> Ack:
        return self.send(f"macro define {slot} {{ {body} }}")

    def macro_run(self, slot: str) -> Ack:
        return self.send(f"macro run {slot}")

    def set_ble_name(self, name: str) -> Ack:
        return self.send(f'ble name "{name}"')

    def set_ble_passkey(self, passkey: str | int) -> Ack:
        return self.send(f"ble passkey {passkey}")

    def set_layout(self, layout: str) -> Ack:
        return self.send(f"layout {layout}")

    def script_save(self, name: str, body: str, append: bool = False) -> Ack:
        verb = "append" if append else "save"
        return self.send(f"script {verb} {name} {{ {body} }}")

    def script_run(self, name: str) -> Ack:
        return self.send(f"script run {name}")

    def script_delete(self, name: str) -> Ack:
        return self.send(f"script del {name}")

    def script_list(self) -> Ack:
        return self.send("script list")

    def wifi_on(self) -> Ack:
        return self.send("wifi on")

    def wifi_off(self) -> Ack:
        return self.send("wifi off")

    def wifi_ap(self, ssid: str, password: str) -> Ack:
        return self.send(f'wifi ap "{ssid}" "{password}"')

    def set_ota_token(self, token: str) -> Ack:
        if token.lower() == "off":
            return self.send("ota token off")
        return self.send(f'ota token "{token}"')
