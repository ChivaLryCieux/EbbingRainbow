#!/usr/bin/env python3
"""通过 MCP 把 Lvl_Route 的 WorldSettings.DefaultGameMode 设为 RouteGameMode。"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

SCENE = "editor_toolset.toolsets.scene.SceneTools"
OBJECT = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

LEVEL = "/Game/Battle/Maps/Lvl_Route"
GAMEMODE = "/Script/EbbingRainbow.RouteGameMode"


def call(m, toolset, tool, args):
    res = m.call("call_tool", {"toolset_name": toolset, "tool_name": tool, "arguments": args})
    txt = text_of(res)
    print(f"[{tool}] -> {txt[:700]}")
    return txt


def main():
    m = MCP()
    assert m.init()

    call(m, SCENE, "load_level", {"level_path": LEVEL})
    call(m, SCENE, "get_current_level", {})

    txt = call(m, SCENE, "find_actors", {"name": "WorldSettings", "tag": "", "collision_channels": []})
    try:
        actors = json.loads(txt).get("returnValue", [])
    except Exception:
        actors = []

    if not actors:
        print("WorldSettings not found by name; listing all actors:")
        all_txt = call(m, SCENE, "find_actors", {"name": "", "tag": "", "collision_channels": []})
        try:
            for a in json.loads(all_txt).get("returnValue", []):
                print("  actor:", a.get("refPath") or a)
        except Exception:
            pass
        return 1

    ws = actors[0]
    print("WorldSettings ref:", ws)

    call(m, OBJECT, "get_properties", {"instance": ws, "properties": ["default_game_mode"]})
    call(m, OBJECT, "set_properties", {
        "instance": ws,
        "values": json.dumps({"default_game_mode": {"refPath": GAMEMODE}}),
    })
    call(m, OBJECT, "get_properties", {"instance": ws, "properties": ["default_game_mode"]})

    call(m, ASSET, "save_assets", {"asset_paths": [LEVEL]})
    return 0


if __name__ == "__main__":
    sys.exit(main())
