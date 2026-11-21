import io
import json
import sys
import unittest
from pathlib import Path
from unittest.mock import MagicMock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from rubberorca.mcp_server import TOOLS, RubberOrcaMcpServer
from rubberorca.protocol import Ack


class TestMcpServer(unittest.TestCase):
    def setUp(self):
        self.sender = MagicMock(return_value=Ack(seq=1, ok=True, code=0))
        self.server = RubberOrcaMcpServer(default_port="/dev/null", sender=self.sender)

    def test_initialize_and_tools_list(self):
        init = self.server.handle_request({"jsonrpc": "2.0", "id": 1, "method": "initialize"})
        self.assertEqual(init["result"]["serverInfo"]["name"], "rubberorca-mcp")

        listed = self.server.handle_request({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
        self.assertEqual(len(listed["result"]["tools"]), len(TOOLS))

    def test_notification_returns_none(self):
        self.assertIsNone(self.server.handle_request({"jsonrpc": "2.0", "method": "notifications/initialized"}))

    def test_tools_call_dispatches_expected_wire_commands(self):
        cases = [
            ("type_text", {"text": 'hi "orca"\n'}, 'type "hi \\"orca\\"\\n"'),
            ("press_key", {"key": "enter"}, "enter"),
            ("press_key", {"key": "gui l", "ms": 80}, "press gui l 80"),
            ("media_control", {"action": "mute"}, "media mute"),
            ("mouse_action", {"action": "move", "dx": 10, "dy": -5}, "mouse move 10 -5"),
            ("mouse_action", {"action": "click", "button": "right"}, "mouse click right"),
            ("mouse_action", {"action": "scroll", "amount": 2}, "mouse scroll 2"),
            ("jiggler", {"state": "toggle"}, "jiggler toggle"),
            ("macro_define", {"slot": "m0", "body": "enter"}, "macro define m0 { enter }"),
            ("macro_run", {"slot": "m0"}, "macro run m0"),
            ("send_command", {"command": "delay 50 && enter"}, "delay 50 && enter"),
        ]
        for idx, (tool_name, arguments, expected_cmd) in enumerate(cases, start=10):
            self.sender.reset_mock()
            resp = self.server.handle_request(
                {
                    "jsonrpc": "2.0",
                    "id": idx,
                    "method": "tools/call",
                    "params": {"name": tool_name, "arguments": arguments},
                }
            )
            self.assertFalse(resp["result"]["isError"])
            self.assertEqual(self.sender.call_args.args[0], expected_cmd)

    def test_unknown_tool_and_stdio_loop(self):
        stdin = io.StringIO(
            json.dumps({"jsonrpc": "2.0", "id": 1, "method": "ping"})
            + "\n"
            + json.dumps(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "tools/call",
                    "params": {"name": "nonexistent", "arguments": {}},
                }
            )
            + "\n"
        )
        stdout = io.StringIO()
        self.server.serve(stdin=stdin, stdout=stdout)
        lines = [json.loads(line) for line in stdout.getvalue().strip().splitlines()]
        self.assertEqual(lines[0]["result"], {})
        self.assertEqual(lines[1]["error"]["code"], -32602)


if __name__ == "__main__":
    unittest.main()
