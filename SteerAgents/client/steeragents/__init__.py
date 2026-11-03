"""SteerAgents client library — talk to a running UE editor's MCP server.

Quick start:

    import steeragents as sa

    m = sa.connect()
    print(sa.call(m, "EditorToolset.EditorAppToolset", "GetLastLog", {"category": "LogTemp"}))
    sa.capture(m, "arena.png")
"""

from .config import config
from .mcp import MCP
from .toolsets import call, connect, describe, list_toolsets, text_of
from .capture import capture, stats
from .console import run_cmd, run_py

__all__ = [
    "MCP",
    "call",
    "capture",
    "config",
    "connect",
    "describe",
    "list_toolsets",
    "run_cmd",
    "run_py",
    "stats",
    "text_of",
]
