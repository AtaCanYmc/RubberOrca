import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from rubberorca.cli import build_command_from_args, build_parser, main
from rubberorca.protocol import (
    ETX,
    PAYLOAD_MAX,
    STX,
    VERSION,
    Ack,
    ProtocolError,
    build_frame,
    escape_text,
    parse_ack,
)


class TestProtocol(unittest.TestCase):
    def test_build_frame_and_xor(self):
        frame = build_frame("enter", seq=7)
        self.assertEqual(frame[0], STX)
        self.assertEqual(frame[1], VERSION)
        self.assertEqual(frame[2], 7)
        self.assertEqual(frame[3:5], bytes([0x00, 0x05]))
        self.assertEqual(frame[5:10], b"enter")
        expected_xor = 0
        for b in b"enter":
            expected_xor ^= b
        self.assertEqual(frame[10], expected_xor)
        self.assertEqual(frame[11], ETX)

    def test_oversized_payload_raises(self):
        with self.assertRaises(ProtocolError):
            build_frame("a" * (PAYLOAD_MAX + 1))

    def test_parse_ack_valid_and_invalid(self):
        ack = parse_ack(bytes([STX, VERSION, 9, 1, 0, ETX]))
        self.assertEqual(ack, Ack(seq=9, ok=True, code=0))
        with self.assertRaises(ProtocolError):
            parse_ack(bytes([STX, VERSION, 9]))
        with self.assertRaises(ProtocolError):
            parse_ack(bytes([0x00, VERSION, 9, 1, 0, ETX]))

    def test_escape_text(self):
        self.assertEqual(escape_text('a"b\nc\td\\e'), 'a\\"b\\nc\\td\\\\e')


class TestCli(unittest.TestCase):
    def test_command_builders(self):
        parser = build_parser()
        cases = [
            (["type", 'hi "orca"\n'], 'type "hi \\"orca\\"\\n"'),
            (["key", "enter"], "enter"),
            (["press", "gui l", "80"], "press gui l 80"),
            (["media", "play_pause"], "media play_pause"),
            (["mouse", "move", "20", "-10"], "mouse move 20 -10"),
            (["mouse", "click", "right"], "mouse click right"),
            (["mouse", "scroll", "-2"], "mouse scroll -2"),
            (["jiggler", "toggle"], "jiggler toggle"),
            (["macro", "define", "m0", "enter"], "macro define m0 { enter }"),
            (["macro", "run", "m15"], "macro run m15"),
            (["ble-name", "OrcaMini"], 'ble name "OrcaMini"'),
            (["ble-passkey", "482913"], "ble passkey 482913"),
            (["ble-passkey", "off"], "ble passkey off"),
            (["layout", "tr_q"], "layout tr_q"),
        ]
        for argv, expected in cases:
            args = parser.parse_args(argv)
            self.assertEqual(build_command_from_args(args), expected)

    @patch("rubberorca.cli.send_command", return_value=Ack(seq=1, ok=True, code=0))
    def test_main_dispatches_and_returns_zero(self, mock_send):
        rc = main(["--port", "/dev/null", "jiggler", "on"])
        self.assertEqual(rc, 0)
        mock_send.assert_called_once()

    @patch("rubberorca.cli.send_command", return_value=Ack(seq=1, ok=False, code=1))
    def test_main_returns_two_on_nack(self, _mock_send):
        self.assertEqual(main(["key", "enter"]), 2)


if __name__ == "__main__":
    unittest.main()
