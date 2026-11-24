"""Compatibility shim — real implementation lives in the SteerAgents repo."""
import sys
from mcp_call import _steer_src

_src = _steer_src()
if _src:
    sys.path.insert(0, _src)

from steeragents.console import find_cmd_box, run_cmd, run_py  # noqa: F401,E402
from steeragents.toolsets import connect  # noqa: E402

if __name__ == "__main__":
    m = connect()
    cmd = " ".join(sys.argv[1:]) if len(sys.argv) > 1 else "py D:/EbbingRainbow/Scripts/trigger_lc.py"
    print(run_cmd(m, cmd))
