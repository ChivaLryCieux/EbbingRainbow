"""Compatibility shim — real implementation lives in SteerAgents.

Old API surface kept: connect / call / capture / stats, with APP and OUT
constants for the few scripts that touched them directly.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "SteerAgents", "client"))
from steeragents.config import config as _cfg
from steeragents.capture import APP, capture, stats  # noqa: F401
from steeragents.toolsets import call, connect  # noqa: F401

OUT = str(_cfg.shot_dir)

if __name__ == "__main__":
    m = connect()
    p = capture(m, sys.argv[1] if len(sys.argv) > 1 else "shot.png")
    print(p)
    stats(p)
