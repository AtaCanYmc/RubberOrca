[Back to Root README](../README.md)

# RubberOrca CLI

Python command-line client for RubberOrca over BLE 5.0 GATT (`0xFFE0` / `0xFFE1`) and USB CDC Serial.

## Installation

```bash
pip install -e ./cli
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

## Testing

```bash
python3 -m unittest discover -s cli/tests -v
```
