"""Compatibility shim — real implementation lives in the SteerAgents repo.

Old API surface kept: connect / call / capture / stats, with APP and OUT
constants for the few scripts that touched them directly. stats() now
returns a dict; use print_stats() for the old text output.
"""
import sys
from mcp_call import _steer_src

_src = _steer_src()
if _src:
    sys.path.insert(0, _src)

from steeragents.capture import APP, capture, print_stats, stats  # noqa: F401,E402
from steeragents.config import config as _cfg  # noqa: E402
from steeragents.toolsets import call, connect  # noqa: F401,E402

OUT = str(_cfg.shot_dir)

if __name__ == "__main__":
    m = connect()
    p = capture(m, sys.argv[1] if len(sys.argv) > 1 else "shot.png")
    print(p)
    print(print_stats(stats(p)))
