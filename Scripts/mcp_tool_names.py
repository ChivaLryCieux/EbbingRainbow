#!/usr/bin/env python3
"""Print tool names (and short descriptions) for one or more toolsets."""
import json
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of


def main():
    m = MCP()
    assert m.init()
    for name in sys.argv[1:]:
        res = m.call("describe_toolset", {"toolset_name": name})
        txt = text_of(res)
        try:
            obj = json.loads(txt)
        except Exception:
            print("RAW", txt[:2000])
            continue
        print("==", name)
        for tool in obj.get("tools", []):
            desc = (tool.get("description") or "").strip().splitlines()
            print(" -", tool.get("name"), "::", desc[0] if desc else "")


if __name__ == "__main__":
    main()
