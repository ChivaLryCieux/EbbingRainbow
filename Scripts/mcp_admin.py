"""Compatibility shim — real implementation lives in SteerAgents."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "SteerAgents", "client"))
from steeragents.toolsets import text_of  # noqa: F401

if __name__ == "__main__":
    from steeragents.__main__ import main
    sys.exit(main())
