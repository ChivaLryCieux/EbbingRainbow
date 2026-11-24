"""Compatibility shim — real implementation lives in the SteerAgents repo."""
import os
import sys
from mcp_call import _steer_src  # noqa: F401 — resolves path for importers below

_src = _steer_src()
if _src:
    sys.path.insert(0, _src)

from steeragents.toolsets import text_of  # noqa: F401,E402

if __name__ == "__main__":
    from steeragents.cli import main
    sys.exit(main())
