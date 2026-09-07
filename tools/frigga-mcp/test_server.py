#!/usr/bin/env python3
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock

from server import EditorRpc, McpServer


class FakeRpc:
    def __init__(self):
        self.calls = []

    def call(self, method, params):
        self.calls.append((method, params))
        return {"result": {"ok": True, "data": {"method": method}}}


class McpServerTests(unittest.TestCase):
    def setUp(self):
        self.rpc = FakeRpc()
        self.server = McpServer(self.rpc)

    def test_initialize(self):
        response = self.server.handle({
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {"protocolVersion": "2024-11-05"},
        })
        self.assertEqual(response["id"], 1)
        self.assertIn("tools", response["result"]["capabilities"])

    def test_tools_list(self):
        response = self.server.handle({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
        self.assertTrue(response["result"]["tools"])
        names = {tool["name"] for tool in response["result"]["tools"]}
        self.assertIn("scene.inspect", names)
        self.assertIn("modules.create", names)
        self.assertIn("modules.list", names)
        self.assertIn("modules.build", names)
        self.assertIn("modules.reload", names)

    def test_tool_call_is_forwarded(self):
        response = self.server.handle({
            "jsonrpc": "2.0",
            "id": 3,
            "method": "tools/call",
            "params": {"name": "scene.inspect", "arguments": {}},
        })
        self.assertEqual(self.rpc.calls, [("scene.inspect", {})])
        self.assertFalse(response["result"]["isError"])
        self.assertEqual(json.loads(response["result"]["content"][0]["text"])["ok"], True)

    def test_modules_create_is_forwarded(self):
        response = self.server.handle({
            "jsonrpc": "2.0",
            "id": 4,
            "method": "tools/call",
            "params": {"name": "modules.create", "arguments": {"name": "Player Animation"}},
        })
        self.assertEqual(self.rpc.calls, [("modules.create", {"name": "Player Animation"})])
        self.assertFalse(response["result"]["isError"])

    def test_modules_set_enabled_underscore_alias(self):
        response = self.server.handle({
            "jsonrpc": "2.0",
            "id": 5,
            "method": "tools/call",
            "params": {
                "name": "modules_set_enabled",
                "arguments": {"id": "playeranimation", "enabled": True},
            },
        })
        self.assertEqual(
            self.rpc.calls,
            [("modules.set_enabled", {"id": "playeranimation", "enabled": True})],
        )
        self.assertFalse(response["result"]["isError"])


class EditorRpcReconnectTests(unittest.TestCase):
    def test_call_reconnects_after_connection_drop(self):
        rpc = EditorRpc(Path("/tmp/frigga-mcp-test.endpoint"), timeout=1.0)
        rpc.sock = MagicMock()
        reads = {"n": 0}
        connects = {"n": 0}

        def fake_send(_message):
            return None

        def fake_read():
            reads["n"] += 1
            if reads["n"] == 1:
                raise ConnectionError("Editor closed the MCP connection")
            return {"result": {"ok": True, "data": {}}}

        def fake_connect():
            connects["n"] += 1
            rpc.sock = MagicMock()

        rpc._send = fake_send
        rpc._read = fake_read
        rpc.connect = fake_connect

        result = rpc.call("scene.inspect", {})
        self.assertTrue(result["result"]["ok"])
        self.assertEqual(reads["n"], 2)
        self.assertEqual(connects["n"], 1)
        self.assertIsNotNone(rpc.sock)

    def test_call_without_editor_raises_clear_error(self):
        missing = Path(tempfile.gettempdir()) / "frigga-mcp-test-missing.endpoint"
        if missing.exists():
            missing.unlink()
        rpc = EditorRpc(missing, timeout=0.2)
        with self.assertRaises(RuntimeError) as ctx:
            rpc.call("scene.inspect", {})
        self.assertIn("not running", str(ctx.exception))

    def test_mcp_server_survives_editor_unavailable(self):
        missing = Path(tempfile.gettempdir()) / "frigga-mcp-test-missing.endpoint"
        if missing.exists():
            missing.unlink()
        rpc = EditorRpc(missing, timeout=0.2)
        server = McpServer(rpc)

        init = server.handle({
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {"protocolVersion": "2024-11-05"},
        })
        self.assertIn("result", init)

        call = server.handle({
            "jsonrpc": "2.0",
            "id": 2,
            "method": "tools/call",
            "params": {"name": "scene.inspect", "arguments": {}},
        })
        self.assertIn("error", call)
        self.assertIn("not running", call["error"]["message"])

        again = server.handle({
            "jsonrpc": "2.0",
            "id": 3,
            "method": "initialize",
            "params": {"protocolVersion": "2024-11-05"},
        })
        self.assertIn("result", again)


if __name__ == "__main__":
    unittest.main()
