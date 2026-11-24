"""Compatibility shim — real implementation lives in the SteerAgents repo."""
import os
import sys


def _steer_src():
    """Locate the steeragents package: STEERAGENTS_HOME env -> sibling repo
    -> legacy in-tree copy -> already-installed package."""
    here = os.path.dirname(os.path.abspath(__file__))
    roots = [os.environ.get("STEERAGENTS_HOME"),
             os.path.join(here, "..", "..", "SteerAgents"),
             os.path.join(here, "..", "SteerAgents")]
    for root in filter(None, roots):
        for sub in ("client/src", "client"):
            p = os.path.join(root, sub)
            if os.path.isdir(os.path.join(p, "steeragents")):
                return p
    return None


_src = _steer_src()
if _src:
    sys.path.insert(0, _src)

from steeragents.mcp import MCP  # noqa: F401,E402
