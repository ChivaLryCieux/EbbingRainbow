"""Compatibility shim — real implementation lives in SteerAgents."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "SteerAgents", "client"))
from steeragents.mcp import MCP  # noqa: F401
