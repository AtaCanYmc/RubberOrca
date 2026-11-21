import sys
import unittest
from pathlib import Path
from unittest.mock import MagicMock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from rubberorca import Ack, RubberOrca


class TestSdk(unittest.TestCase):
    def setUp(self):
        self.sender = MagicMock(return_value=Ack(seq=1, ok=True, code=0))
        self.raw_sender = MagicMock()
        self.orca = RubberOrca(port="/dev/null", sender=self.sender, raw_sender=self.raw_sender)

    def test_sdk_methods_emit_expected_wire_commands(self):
        calls = [
            (lambda: self.orca.type('hi "orca"\n'), 'type "hi \\"orca\\"\\n"'),
            (lambda: self.orca.delay(150), "delay 150"),
            (lambda: self.orca.key("enter"), "enter"),
            (lambda: self.orca.press("gui l", 80), "press gui l 80"),
            (lambda: self.orca.media("play_pause"), "media play_pause"),
            (lambda: self.orca.mouse_move(10, -20), "mouse move 10 -20"),
            (lambda: self.orca.mouse_click("right"), "mouse click right"),
            (lambda: self.orca.mouse_scroll(-3), "mouse scroll -3"),
            (lambda: self.orca.jiggler("on"), "jiggler on"),
            (lambda: self.orca.macro_define("m0", "enter"), "macro define m0 { enter }"),
            (lambda: self.orca.macro_run("m0"), "macro run m0"),
            (lambda: self.orca.set_ble_name("OrcaMini"), 'ble name "OrcaMini"'),
            (lambda: self.orca.set_ble_passkey(482913), "ble passkey 482913"),
            (lambda: self.orca.set_layout("tr_q"), "layout tr_q"),
        ]
        for fn, expected in calls:
            self.sender.reset_mock()
            ack = fn()
            self.assertTrue(ack.ok)
            self.assertEqual(self.sender.call_args.args[0], expected)

    def test_sequence_increments_and_raw_byte(self):
        self.orca.key("enter")
        self.assertEqual(self.sender.call_args.kwargs["seq"], 1)
        self.orca.key("tab")
        self.assertEqual(self.sender.call_args.kwargs["seq"], 2)
        self.orca.raw_byte(0x32)
        self.raw_sender.assert_called_once()


if __name__ == "__main__":
    unittest.main()
