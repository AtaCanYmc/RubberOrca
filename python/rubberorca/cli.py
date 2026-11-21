from __future__ import annotations

import argparse
import asyncio
import sys
from typing import Optional

from .protocol import ProtocolError, escape_text
from .transport import list_serial_ports, scan_ble, send_command, send_raw_byte


def build_command_from_args(args: argparse.Namespace) -> str:
    """Convert parsed CLI subcommand arguments into a RubberOrca wire command."""
    cmd = args.command
    if cmd == "send":
        return args.payload
    if cmd == "type":
        return f'type "{escape_text(args.text)}"'
    if cmd == "key":
        return args.name
    if cmd == "press":
        if args.ms is not None:
            return f"press {args.mod} {args.ms}"
        return f"press {args.mod}"
    if cmd == "media":
        return f"media {args.action}"
    if cmd == "mouse":
        if args.mouse_cmd == "move":
            return f"mouse move {args.dx} {args.dy}"
        if args.mouse_cmd == "click":
            return f"mouse click {args.button}"
        if args.mouse_cmd == "scroll":
            return f"mouse scroll {args.amount}"
    if cmd == "jiggler":
        return f"jiggler {args.state}"
    if cmd == "macro":
        if args.macro_cmd == "define":
            return f"macro define {args.slot} {{ {args.body} }}"
        if args.macro_cmd == "run":
            return f"macro run {args.slot}"
    if cmd == "ble-name":
        return f'ble name "{args.name}"'
    if cmd == "ble-passkey":
        return f"ble passkey {args.passkey}"
    if cmd == "layout":
        return f"layout {args.name}"
    if cmd == "script":
        if args.script_cmd == "save":
            return f"script save {args.name} {{ {args.body} }}"
        if args.script_cmd == "append":
            return f"script append {args.name} {{ {args.body} }}"
        if args.script_cmd == "run":
            return f"script run {args.name}"
        if args.script_cmd == "del":
            return f"script del {args.name}"
        if args.script_cmd == "list":
            return "script list"
    if cmd == "wifi":
        if args.wifi_cmd == "on":
            return "wifi on"
        if args.wifi_cmd == "off":
            return "wifi off"
        if args.wifi_cmd == "ap":
            return f'wifi ap "{args.ssid}" "{args.password}"'
    if cmd == "ota-token":
        if args.token.lower() == "off":
            return "ota token off"
        return f'ota token "{args.token}"'
    raise ValueError(f"Unsupported command: {cmd}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="rubberorca",
        description="Control RubberOrca ESP32-S3 USB HID over BLE 5.0 GATT or USB CDC Serial.",
    )
    parser.add_argument("--port", help="USB CDC serial port (uses BLE GATT when omitted)")
    parser.add_argument("--address", help="Explicit BLE device address or UUID")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=3.0, help="Transport timeout in seconds")
    parser.add_argument("--seq", type=int, default=1, help="Packet sequence number (0-255)")

    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("scan", help="Scan for BLE 0xFFE0 devices and list serial ports")

    p_send = sub.add_parser("send", help="Send a raw command or &&/; chain")
    p_send.add_argument("payload", help="Raw command string")

    p_type = sub.add_parser("type", help="Type text over USB HID keyboard")
    p_type.add_argument("text", help="Text to type")

    p_key = sub.add_parser("key", help="Send a single special key")
    p_key.add_argument("name", choices=["enter", "tab", "backspace"])

    p_press = sub.add_parser("press", help="Press a modifier or shortcut combo")
    p_press.add_argument("mod", help="Modifier or combo (shift, ctrl, alt, gui, right, left, f5, 'gui l')")
    p_press.add_argument("ms", nargs="?", type=int, help="Optional hold duration in ms (1-5000)")

    p_media = sub.add_parser("media", help="Send a Consumer Control media key")
    p_media.add_argument(
        "action",
        choices=["play_pause", "volume_up", "volume_down", "next", "prev", "mute"],
    )

    p_mouse = sub.add_parser("mouse", help="Control USB HID mouse")
    mouse_sub = p_mouse.add_subparsers(dest="mouse_cmd", required=True)
    pm_move = mouse_sub.add_parser("move", help="Move relative pointer")
    pm_move.add_argument("dx", type=int)
    pm_move.add_argument("dy", type=int)
    pm_click = mouse_sub.add_parser("click", help="Click mouse button")
    pm_click.add_argument("button", choices=["left", "right", "middle"], default="left", nargs="?")
    pm_scroll = mouse_sub.add_parser("scroll", help="Scroll vertical wheel")
    pm_scroll.add_argument("amount", type=int)

    p_jig = sub.add_parser("jiggler", help="Control mouse jiggler")
    p_jig.add_argument("state", choices=["on", "off", "toggle"])

    p_macro = sub.add_parser("macro", help="Manage NVS macros (m0..m15)")
    macro_sub = p_macro.add_subparsers(dest="macro_cmd", required=True)
    pm_def = macro_sub.add_parser("define", help="Save macro body to NVS slot")
    pm_def.add_argument("slot", help="Slot name m0..m15")
    pm_def.add_argument("body", help="Command chain to store")
    pm_run = macro_sub.add_parser("run", help="Run stored NVS macro")
    pm_run.add_argument("slot", help="Slot name m0..m15")

    p_ble = sub.add_parser("ble-name", help="Update BLE advertising name in NVS")
    p_ble.add_argument("name", help="Printable ASCII name (1-29 chars)")

    p_pk = sub.add_parser("ble-passkey", help="Set 6-digit BLE bonding passkey or 'off'")
    p_pk.add_argument("passkey", help="6-digit passkey (100000-999999) or 'off'")

    p_lay = sub.add_parser("layout", help="Set USB HID keyboard layout in NVS")
    p_lay.add_argument("name", choices=["us", "tr_q"])

    p_scr = sub.add_parser("script", help="Manage LittleFS scripts (/scripts/)")
    scr_sub = p_scr.add_subparsers(dest="script_cmd", required=True)
    ps_save = scr_sub.add_parser("save", help="Save a script file to LittleFS")
    ps_save.add_argument("name")
    ps_save.add_argument("body")
    ps_app = scr_sub.add_parser("append", help="Append line(s) to a LittleFS script")
    ps_app.add_argument("name")
    ps_app.add_argument("body")
    ps_run = scr_sub.add_parser("run", help="Run a LittleFS DuckyScript/orca file")
    ps_run.add_argument("name")
    ps_del = scr_sub.add_parser("del", help="Delete a LittleFS script")
    ps_del.add_argument("name")
    scr_sub.add_parser("list", help="List stored LittleFS scripts")

    p_wifi = sub.add_parser("wifi", help="Control N16R8 Wi-Fi SoftAP Workstation")
    wifi_sub = p_wifi.add_subparsers(dest="wifi_cmd", required=True)
    wifi_sub.add_parser("on", help="Enable SoftAP Workstation")
    wifi_sub.add_parser("off", help="Disable SoftAP Workstation")
    pw_ap = wifi_sub.add_parser("ap", help="Configure WPA2-PSK SoftAP credentials and enable")
    pw_ap.add_argument("ssid")
    pw_ap.add_argument("password")

    p_ota = sub.add_parser("ota-token", help="Set authenticated OTA/API token (12+ chars) or 'off'")
    p_ota.add_argument("token")

    p_raw = sub.add_parser("raw-byte", help="Send a single-byte fallback command (e.g. 0x32)")
    p_raw.add_argument("value", help="Hex or decimal byte in 0x10..0x8F")

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        if args.command == "scan":
            ble_devices = asyncio.run(scan_ble(timeout=args.timeout))
            print("BLE devices (Service 0xFFE0):")
            if ble_devices:
                for addr, name in ble_devices:
                    print(f"  {addr}  {name}")
            else:
                print("  (none)")
            print("Serial ports:")
            ports = list_serial_ports()
            if ports:
                for dev, desc in ports:
                    print(f"  {dev}  {desc}")
            else:
                print("  (none)")
            return 0

        if args.command == "raw-byte":
            val = int(args.value, 0)
            send_raw_byte(
                val,
                port=args.port,
                address=args.address,
                baud=args.baud,
                timeout=args.timeout,
            )
            print(f"Sent fallback byte 0x{val:02X}")
            return 0

        wire_cmd = build_command_from_args(args)
        ack = send_command(
            wire_cmd,
            port=args.port,
            address=args.address,
            baud=args.baud,
            timeout=args.timeout,
            seq=args.seq,
        )
        status = "OK" if ack.ok else f"ERR(code={ack.code})"
        print(f"ACK seq={ack.seq} {status}")
        return 0 if ack.ok else 2
    except (ProtocolError, RuntimeError, ValueError, TimeoutError, asyncio.TimeoutError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
