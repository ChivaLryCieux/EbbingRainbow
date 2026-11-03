import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

m = MCP()
assert m.init()
ACT = "editor_toolset.toolsets.actor.ActorTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"
SCENE = "editor_toolset.toolsets.scene.SceneTools"
BASE = "/Game/Battle/Maps/Lvl_Arena.Lvl_Arena:PersistentLevel."


def call(ts, tool, args=None):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args or {}})
    return text_of(r)


def comps(actor):
    print("###", actor)
    print(call(ACT, "get_components", {"actor": {"refPath": BASE + actor}})[:2000])


for a in ["SkyLight_0", "SkyAtmosphere_1", "PostProcessVolume_1", "Brush_0"]:
    comps(a)
