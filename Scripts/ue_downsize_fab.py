#!/usr/bin/env python3
"""Lower the Fab 8K textures to 2K so PIE does not exhaust GPU memory."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

FAB_DIR = r"D:\EbbingRainbow\Content\Fab\Sci-fi_Interior_20"
SUFFIX = ("_BaseColor", "_Normal", "_Roughness", "_Metallic", "_Emissive")


def collect():
    out = []
    for f in sorted(os.listdir(FAB_DIR)):
        if not f.endswith(".uasset"):
            continue
        name = f[:-7]
        if not name.endswith(SUFFIX):
            continue
        out.append(f"/Game/Fab/Sci-fi_Interior_20/{name}.{name}")
    return out


def main():
    paths = collect()
    print("textures", len(paths))
    script = """
import json
PATHS = json.loads(%s)

def set_max(p):
    return execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties",
        json.dumps({"instance": p, "values": {"maxTextureSize": 2048}}))

def save(p):
    return execute_tool("editor_toolset.toolsets.asset.AssetTools.save_assets",
        json.dumps({"asset_paths": [p]}))

def run():
    ok = 0
    errors = []
    for p in PATHS:
        try:
            set_max(p)
            ok += 1
        except Exception as exc:
            errors.append(p + ": " + str(exc)[:160])
    saved = 0
    try:
        save(PATHS[0])  # ensure package write path works
    except Exception:
        pass
    return {"ok": ok, "errors": errors[:20], "error_count": len(errors)}
""" % json.dumps(json.dumps(paths))
    m = MCP()
    if not m.init():
        return 1
    res = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
        "tool_name": "execute_tool_script",
        "arguments": {"script": script},
    })
    print(json.dumps(res, ensure_ascii=False, indent=1)[:8000])
    # Save the whole texture set via the asset tool in one call.
    save_args = {"asset_paths": paths}
    with open(r"D:\EbbingRainbow\Scripts\fab_tex_paths.json", "w", encoding="utf-8") as f:
        json.dump(save_args, f)
    res2 = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.asset.AssetTools",
        "tool_name": "save_assets",
        "arguments": save_args,
    })
    print("SAVE", json.dumps(res2, ensure_ascii=False)[:2000])
    return 0


if __name__ == "__main__":
    sys.exit(main())
