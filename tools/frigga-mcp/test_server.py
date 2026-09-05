#!/usr/bin/env python3
import json
import unittest

from server import McpServer


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
        self.assertIn("scene.inspect", {tool["name"] for tool in response["result"]["tools"]})

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


if __name__ == "__main__":
    unittest.main()
