from __future__ import annotations

import argparse
import asyncio
import json
import os
import sys
from typing import Any, Callable, Optional, TextIO

from . import __version__
from .protocol import ProtocolError, escape_text
from .transport import list_serial_ports, scan_ble, send_command

MCP_PROTOCOL_VERSION = "2024-11-05"

TOOLS: list[dict[str, Any]] = [
    {
        "name": "scan_devices",
        "description": "Scan for nearby RubberOrca BLE devices (Service 0xFFE0) and local USB serial ports.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "timeout": {"type": "number", "minimum": 0.5, "maximum": 15.0, "default": 4.0},
            },
            "additionalProperties": False,
        },
    },
    {
        "name": "type_text",
        "description": "Type ASCII text onto the target host over wired USB HID keyboard.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "text": {"type": "string", "minLength": 1},
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["text"],
            "additionalProperties": False,
        },
    },
    {
        "name": "press_key",
        "description": "Send enter/tab/backspace or press a modifier/shortcut combo (shift, ctrl, alt, gui, right, left, f5, 'gui l', 'gui d', 'ctrl shift esc').",
        "inputSchema": {
            "type": "object",
            "properties": {
                "key": {"type": "string", "minLength": 1},
                "ms": {"type": "integer", "minimum": 1, "maximum": 5000},
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["key"],
            "additionalProperties": False,
        },
    },
    {
        "name": "media_control",
        "description": "Send a USB HID Consumer Control media key.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": {
                    "type": "string",
                    "enum": ["play_pause", "volume_up", "volume_down", "next", "prev", "mute"],
                },
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["action"],
            "additionalProperties": False,
        },
    },
    {
        "name": "mouse_action",
        "description": "Move, click, or scroll the wired USB HID mouse.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "action": {"type": "string", "enum": ["move", "click", "scroll"]},
                "dx": {"type": "integer", "minimum": -127, "maximum": 127},
                "dy": {"type": "integer", "minimum": -127, "maximum": 127},
                "button": {"type": "string", "enum": ["left", "right", "middle"]},
                "amount": {"type": "integer", "minimum": -127, "maximum": 127},
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["action"],
            "additionalProperties": False,
        },
    },
    {
        "name": "jiggler",
        "description": "Turn the RubberOrca mouse jiggler on, off, or toggle it.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "state": {"type": "string", "enum": ["on", "off", "toggle"]},
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["state"],
            "additionalProperties": False,
        },
    },
    {
        "name": "macro_define",
        "description": "Persist a command chain into an ESP32-S3 NVS macro slot (m0..m15).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "slot": {"type": "string", "pattern": "^m([0-9]|1[0-5])$"},
                "body": {"type": "string", "minLength": 1},
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["slot", "body"],
            "additionalProperties": False,
        },
    },
    {
        "name": "macro_run",
        "description": "Execute a stored NVS macro slot (m0..m15).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "slot": {"type": "string", "pattern": "^m([0-9]|1[0-5])$"},
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["slot"],
            "additionalProperties": False,
        },
    },
    {
        "name": "send_command",
        "description": "Send a raw RubberOrca command or && / ; command chain.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {"type": "string", "minLength": 1},
                "port": {"type": "string"},
                "address": {"type": "string"},
            },
            "required": ["command"],
            "additionalProperties": False,
        },
    },
]


class RubberOrcaMcpServer:
    """Minimal stdio JSON-RPC 2.0 MCP server for RubberOrca."""

    def __init__(
        self,
        *,
        default_port: Optional[str] = None,
        default_address: Optional[str] = None,
        baud: int = 115200,
        timeout: float = 3.0,
        sender: Callable[..., Any] = send_command,
    ) -> None:
        self.default_port = default_port
        self.default_address = default_address
        self.baud = baud
        self.timeout = timeout
        self._sender = sender
        self._seq = 1

    def _next_seq(self) -> int:
        seq = self._seq
        self._seq = (self._seq + 1) & 0xFF
        return seq

    def _dispatch_wire(self, wire_cmd: str, args: dict[str, Any]) -> dict[str, Any]:
        port = args.get("port") or self.default_port
        address = args.get("address") or self.default_address
        ack = self._sender(
            wire_cmd,
            port=port,
            address=address,
            baud=self.baud,
            timeout=self.timeout,
            seq=self._next_seq(),
        )
        text = f"ACK seq={ack.seq} {'OK' if ack.ok else f'ERR(code={ack.code})'}"
        return {"content": [{"type": "text", "text": text}], "isError": not ack.ok}

    def call_tool(self, name: str, args: dict[str, Any]) -> dict[str, Any]:
        if name == "scan_devices":
            timeout = float(args.get("timeout", 4.0))
            ble = asyncio.run(scan_ble(timeout=timeout))
            serial_ports = list_serial_ports()
            payload = {
                "ble": [{"address": a, "name": n} for a, n in ble],
                "serial": [{"port": d, "description": desc} for d, desc in serial_ports],
            }
            return {"content": [{"type": "text", "text": json.dumps(payload, indent=2)}], "isError": False}

        if name == "type_text":
            text = str(args["text"])
            return self._dispatch_wire(f'type "{escape_text(text)}"', args)

        if name == "press_key":
            key = str(args["key"]).strip()
            if key in ("enter", "tab", "backspace"):
                return self._dispatch_wire(key, args)
            ms = args.get("ms")
            cmd = f"press {key} {int(ms)}" if ms is not None else f"press {key}"
            return self._dispatch_wire(cmd, args)

        if name == "media_control":
            return self._dispatch_wire(f"media {args['action']}", args)

        if name == "mouse_action":
            action = args["action"]
            if action == "move":
                return self._dispatch_wire(f"mouse move {int(args.get('dx', 0))} {int(args.get('dy', 0))}", args)
            if action == "click":
                return self._dispatch_wire(f"mouse click {args.get('button', 'left')}", args)
            if action == "scroll":
                return self._dispatch_wire(f"mouse scroll {int(args.get('amount', 0))}", args)
            raise ValueError(f"Unsupported mouse action: {action}")

        if name == "jiggler":
            return self._dispatch_wire(f"jiggler {args['state']}", args)

        if name == "macro_define":
            return self._dispatch_wire(f"macro define {args['slot']} {{ {args['body']} }}", args)

        if name == "macro_run":
            return self._dispatch_wire(f"macro run {args['slot']}", args)

        if name == "send_command":
            return self._dispatch_wire(str(args["command"]), args)

        raise KeyError(f"Unknown tool: {name}")

    def handle_request(self, req: dict[str, Any]) -> Optional[dict[str, Any]]:
        req_id = req.get("id")
        method = req.get("method")
        params = req.get("params") or {}

        # Notifications have no id and expect no response.
        if req_id is None:
            return None

        if method == "initialize":
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "protocolVersion": MCP_PROTOCOL_VERSION,
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "rubberorca-mcp", "version": __version__},
                },
            }

        if method == "ping":
            return {"jsonrpc": "2.0", "id": req_id, "result": {}}

        if method == "tools/list":
            return {"jsonrpc": "2.0", "id": req_id, "result": {"tools": TOOLS}}

        if method == "tools/call":
            tool_name = params.get("name", "")
            tool_args = params.get("arguments") or {}
            try:
                result = self.call_tool(tool_name, tool_args)
                return {"jsonrpc": "2.0", "id": req_id, "result": result}
            except KeyError as exc:
                return {"jsonrpc": "2.0", "id": req_id, "error": {"code": -32602, "message": str(exc)}}
            except (ProtocolError, RuntimeError, ValueError, TimeoutError, asyncio.TimeoutError) as exc:
                return {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {"content": [{"type": "text", "text": f"Error: {exc}"}], "isError": True},
                }

        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "error": {"code": -32601, "message": f"Method not found: {method}"},
        }

    def serve(self, stdin: TextIO = sys.stdin, stdout: TextIO = sys.stdout) -> None:
        for line in stdin:
            line = line.strip()
            if not line:
                continue
            try:
                req = json.loads(line)
            except json.JSONDecodeError as exc:
                err = {"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": str(exc)}}
                stdout.write(json.dumps(err) + "\n")
                stdout.flush()
                continue

            resp = self.handle_request(req)
            if resp is not None:
                stdout.write(json.dumps(resp) + "\n")
                stdout.flush()


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        prog="rubberorca-mcp",
        description="Stdio MCP server for controlling RubberOrca ESP32-S3 USB HID.",
    )
    parser.add_argument("--port", default=os.environ.get("RUBBERORCA_PORT"), help="Default USB CDC serial port")
    parser.add_argument("--address", default=os.environ.get("RUBBERORCA_ADDRESS"), help="Default BLE address/UUID")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument(
        "--timeout",
        type=float,
        default=float(os.environ.get("RUBBERORCA_TIMEOUT", "3.0")),
        help="Transport timeout in seconds",
    )
    args = parser.parse_args(argv)

    server = RubberOrcaMcpServer(
        default_port=args.port,
        default_address=args.address,
        baud=args.baud,
        timeout=args.timeout,
    )
    server.serve()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
