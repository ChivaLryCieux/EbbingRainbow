#!/usr/bin/env python3
"""Print input schema of specific tools (short names) inside a toolset."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of


def main():
    m = MCP()
    assert m.init()
    toolset = sys.argv[1]
    wanted = set(sys.argv[2:])
    res = m.call("describe_toolset", {"toolset_name": toolset})
    obj = json.loads(text_of(res))
    for tool in obj.get("tools", []):
        short = tool["name"].split(".")[-1]
        if wanted and short not in wanted:
            continue
        print("==", short)
        print(json.dumps(tool.get("inputSchema", {}), ensure_ascii=False, indent=1))
        out = tool.get("outputSchema")
        if out:
            print("-- output:")
            print(json.dumps(out, ensure_ascii=False, indent=1)[:800])


if __name__ == "__main__":
    main()
