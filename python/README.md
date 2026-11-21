[Back to Root README](../README.md)

# RubberOrca Python SDK, CLI, and MCP Server

Unified Python package (`rubberorca`) providing a Python SDK, `rubberorca` CLI, and `rubberorca-mcp` stdio MCP server over BLE 5.0 GATT (`0xFFE0` / `0xFFE1`) and USB CDC Serial.

## Installation

```bash
pip install rubberorca
# Or editable from source:
pip install -e ./python
```

## Python SDK

```python
from rubberorca import RubberOrca

orca = RubberOrca()  # Auto-discovers BLE 0xFFE0 (or pass port="/dev/cu.usbmodem1101")
orca.type("hello from RubberOrca")
orca.key("enter")
orca.set_layout("tr_q")
orca.jiggler("toggle")
```

## Usage

```bash
# Discover BLE 0xFFE0 devices and local serial ports
rubberorca scan

# Send commands over BLE (default)
rubberorca type "hello from RubberOrca"
rubberorca key enter
rubberorca media play_pause
rubberorca mouse move 40 -20
rubberorca jiggler toggle
rubberorca macro define m0 'type "git status" && enter'
rubberorca macro run m0
rubberorca ble-name "OrcaMini"
rubberorca raw-byte 0x32

# Send over USB CDC Serial instead of BLE
rubberorca --port /dev/cu.usbmodem1101 send 'type "ok" && enter'
```

## MCP Server

`rubberorca-mcp` exposes RubberOrca tools over stdio JSON-RPC 2.0 with zero extra dependencies (`scan_devices`, `type_text`, `press_key`, `media_control`, `mouse_action`, `jiggler`, `macro_define`, `macro_run`, `send_command`).

Example MCP client configuration (`mcp_config.json`):

```json
{
  "mcpServers": {
    "rubberorca": {
      "command": "rubberorca-mcp",
      "args": []
    }
  }
}
```

Pass `--port /dev/cu.usbmodem1101` or `--address <BLE_ADDR>` in `args` (or set `RUBBERORCA_PORT` / `RUBBERORCA_ADDRESS`) to pin a specific device.

## Testing

```bash
python3 -m unittest discover -s python/tests -v
```
