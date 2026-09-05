#!/usr/bin/env python3
"""MCP stdio bridge for a running Frigga Editor."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import socket
import tempfile
import time
from typing import Any

from transports import StdioMcpTransport


ENDPOINT = Path(tempfile.gettempdir()) / "frigga-editor-mcp.endpoint"

TOOLS = [
    {
        "name": "project.inspect",
        "description": "Inspect the open Frigga project and Editor status.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "scene.inspect",
        "description": "Return the active scene path and serialized scene snapshot.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "scene.open",
        "description": "Open a scene inside the current project.",
        "inputSchema": {"type": "object", "required": ["path"],
                        "properties": {"path": {"type": "string"}}},
    },
    {
        "name": "scene.create",
        "description": "Create and open a 3D scene in the current project.",
        "inputSchema": {"type": "object", "required": ["name"],
                        "properties": {"name": {"type": "string"}}},
    },
    {
        "name": "scene.save",
        "description": "Save the active scene.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "scene.replace_snapshot",
        "description": "Replace the active scene from a serialized snapshot; supports dry_run.",
        "inputSchema": {
            "type": "object",
            "required": ["snapshot"],
            "properties": {
                "snapshot": {"type": "object"},
                "dry_run": {"type": "boolean"},
            },
        },
    },
    {
        "name": "scene.update_component",
        "description": "Apply an edited serialized snapshot at the Editor main-thread boundary.",
        "inputSchema": {
            "type": "object",
            "required": ["snapshot"],
            "properties": {
                "snapshot": {"type": "object"},
                "entity": {"type": "integer"},
                "component": {"type": "string"},
                "dry_run": {"type": "boolean"},
            },
        },
    },
    {
        "name": "scene.delete_entity",
        "description": "Apply a serialized snapshot with an entity removed.",
        "inputSchema": {
            "type": "object",
            "required": ["snapshot"],
            "properties": {
                "snapshot": {"type": "object"},
                "entity": {"type": "integer"},
                "dry_run": {"type": "boolean"},
            },
        },
    },
    {
        "name": "assets.validate",
        "description": "Validate the current project's asset manifest.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "assets.list",
        "description": "List imported assets and their persistent IDs.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "assets.cook",
        "description": "Cook assets into a project-local destination; supports dry_run.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "destination": {"type": "string"},
                "dry_run": {"type": "boolean"},
            },
        },
    },
    {
        "name": "runtime.start",
        "description": "Start Editor Play mode.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "runtime.stop",
        "description": "Stop Editor Play mode.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "runtime.status",
        "description": "Inspect Editor Play mode state.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "editor.invoke",
        "description": "Invoke an allowlisted Editor action.",
        "inputSchema": {
            "type": "object",
            "required": ["action"],
            "properties": {"action": {"type": "string"}},
        },
    },
    {
        "name": "logs.recent",
        "description": "Read the Editor's latest status and error diagnostics.",
        "inputSchema": {"type": "object", "properties": {}},
    },
]

WIRE_TOOL_NAMES = {
    "scene_update_component": "scene.update_component",
    "scene_delete_entity": "scene.delete_entity",
    "scene_replace_snapshot": "scene.replace_snapshot",
}


class EditorRpc:
    def __init__(self, endpoint: Path, timeout: float = 5.0) -> None:
        self.endpoint = endpoint
        self.timeout = timeout
        self.sock: socket.socket | None = None
        self.counter = 0

    def connect(self) -> None:
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            try:
                config = json.loads(self.endpoint.read_text())
                self.sock = socket.create_connection(
                    (config["host"], int(config["port"])), timeout=self.timeout
                )
                self.sock.settimeout(self.timeout)
                self._send({"token": config["token"], "id": 0, "method": "handshake"})
                response = self._read()
                if not response.get("ok", False):
                    raise RuntimeError(response.get("message", "Editor authentication failed"))
                return
            except (FileNotFoundError, ConnectionError, TimeoutError, OSError, ValueError):
                time.sleep(0.1)
        raise RuntimeError(f"Frigga Editor MCP endpoint unavailable: {self.endpoint}")

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    def call(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        if self.sock is None:
            self.connect()
        self.counter += 1
        self._send({"id": self.counter, "method": method, "params": params})
        return self._read()

    def _send(self, message: dict[str, Any]) -> None:
        if self.sock is None:
            raise RuntimeError("Editor RPC is not connected")
        self.sock.sendall((json.dumps(message, separators=(",", ":")) + "\n").encode())

    def _read(self) -> dict[str, Any]:
        if self.sock is None:
            raise RuntimeError("Editor RPC is not connected")
        data = bytearray()
        while True:
            char = self.sock.recv(1)
            if not char:
                raise ConnectionError("Editor closed the MCP connection")
            if char == b"\n":
                return json.loads(data.decode())
            data.extend(char)
            if len(data) > 1024 * 1024:
                raise ValueError("Editor MCP response exceeds the payload limit")


class McpServer:
    def __init__(self, rpc: EditorRpc) -> None:
        self.rpc = rpc

    def handle(self, request: dict[str, Any]) -> dict[str, Any] | None:
        method = request.get("method")
        request_id = request.get("id")
        if method == "notifications/initialized":
            return None
        if method == "initialize":
            return {
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {
                    "protocolVersion": request.get("params", {}).get(
                        "protocolVersion", "2024-11-05"
                    ),
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "frigga-editor", "version": "0.1.0"},
                },
            }
        if method == "tools/list":
            return {"jsonrpc": "2.0", "id": request_id, "result": {"tools": TOOLS}}
        if method == "tools/call":
            params = request.get("params", {})
            name = params.get("name")
            arguments = params.get("arguments", {})
            try:
                # Cursor normalizes dotted MCP tool names in some surfaces.
                # Keep the wire protocol stable for the Editor either way.
                if isinstance(name, str):
                    wire_name = WIRE_TOOL_NAMES.get(name, name.replace("_", "."))
                else:
                    wire_name = name
                result = self.rpc.call(wire_name, arguments)
                payload = result.get("result", result)
                is_error = not payload.get("ok", False)
                return {
                    "jsonrpc": "2.0",
                    "id": request_id,
                    "result": {
                        "isError": is_error,
                        "content": [{"type": "text", "text": json.dumps(payload)}],
                    },
                }
            except (ConnectionError, OSError, RuntimeError, ValueError) as error:
                return {
                    "jsonrpc": "2.0",
                    "id": request_id,
                    "error": {"code": -32000, "message": str(error)},
                }
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "error": {"code": -32601, "message": f"Method not found: {method}"},
        }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--endpoint", type=Path, default=ENDPOINT)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    rpc = EditorRpc(args.endpoint, args.timeout)
    try:
        rpc.connect()
        StdioMcpTransport().serve(McpServer(rpc).handle)
    finally:
        rpc.close()


if __name__ == "__main__":
    main()
