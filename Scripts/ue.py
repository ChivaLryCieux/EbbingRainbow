"""Compatibility shim — CLI now lives in SteerAgents (python -m steeragents)."""
import os
import sys
from mcp_call import _steer_src

_src = _steer_src()
if _src:
    sys.path.insert(0, _src)

from steeragents.cli import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
