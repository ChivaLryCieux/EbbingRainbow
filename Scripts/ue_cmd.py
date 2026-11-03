"""Compatibility shim — real implementation lives in SteerAgents."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "SteerAgents", "client"))
from steeragents.console import find_cmd_box, run_cmd, run_py  # noqa: F401
from steeragents.toolsets import connect

if __name__ == "__main__":
    m = connect()
    cmd = " ".join(sys.argv[1:]) if len(sys.argv) > 1 else "py D:/EbbingRainbow/Scripts/trigger_lc.py"
    print(run_cmd(m, cmd))
