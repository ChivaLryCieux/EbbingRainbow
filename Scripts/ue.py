"""Compatibility shim — CLI now lives in SteerAgents (python -m steeragents)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "SteerAgents", "client"))
from steeragents.__main__ import main

if __name__ == "__main__":
    sys.exit(main())
