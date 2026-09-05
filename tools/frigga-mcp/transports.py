"""Transport contracts for the Frigga MCP bridge.

The bridge currently uses newline-delimited stdio, while the MCP dispatcher is
kept independent so Streamable HTTP can be added without changing tool code.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any
import json
import sys


class McpTransport(ABC):
    @abstractmethod
    def serve(self, handler: Any) -> None:
        """Read MCP messages and pass them to a handler."""


class StdioMcpTransport(McpTransport):
    def serve(self, handler: Any) -> None:
        for line in sys.stdin:
            if not line.strip():
                continue
            try:
                request = json.loads(line)
                response = handler(request)
                if response is not None:
                    sys.stdout.write(json.dumps(response, separators=(",", ":")) + "\n")
                    sys.stdout.flush()
            except (ValueError, TypeError) as error:
                sys.stdout.write(json.dumps({
                    "jsonrpc": "2.0",
                    "id": None,
                    "error": {"code": -32700, "message": str(error)},
                }) + "\n")
                sys.stdout.flush()


class StreamableHttpMcpTransport(McpTransport):
    """Reserved adapter for the future MCP Streamable HTTP transport."""

    def serve(self, handler: Any) -> None:
        raise NotImplementedError(
            "Streamable HTTP is intentionally not enabled in the local-only bridge"
        )
