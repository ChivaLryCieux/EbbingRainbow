import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

TINT = "/Game/WFCKit/Materials/M_WFC_TintedPBR.M_WFC_TintedPBR"
EMIT = "/Game/WFCKit/Materials/M_WFC_Emissive.M_WFC_Emissive"
GLASS = "/Game/Fab/Sci-fi_Interior_20/glass_mat_PBS.glass_mat_PBS"

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:500])
    return r


def getp(obj, props):
    return text_of(call("editor_toolset.toolsets.object.ObjectTools", "get_properties",
                        {"instance": obj, "properties": props}))


def setp(obj, values):
    return call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
                {"instance": obj, "values": json.dumps(values)})


call("EditorToolset.EditorAppToolset", "StopPIE", {})

# Exact property name check
print("TINT props", getp(TINT, ["bUsedWithInstancedStaticMeshes"])[:600])
print("GLASS parent", getp(GLASS, ["Parent"])[:600])

# Enable the ISM usage flag on both WFC masters, then recompile.
setp(TINT, {"bUsedWithInstancedStaticMeshes": True})
setp(EMIT, {"bUsedWithInstancedStaticMeshes": True})
call("editor_toolset.toolsets.material.MaterialTools", "recompile",
     {"material_or_function": TINT})
call("editor_toolset.toolsets.material.MaterialTools", "recompile",
     {"material_or_function": EMIT})

paths = [TINT, EMIT] + [
    f"/Game/WFCKit/Materials/{n}.{n}" for n in (
        "MI_WFC_Wall_A", "MI_WFC_Wall_B", "MI_WFC_Wall_C", "MI_WFC_Wall_D",
        "MI_WFC_Dark", "MI_WFC_Floor_A", "MI_WFC_Floor_B", "MI_WFC_Floor_C",
        "MI_WFC_Ceil", "MI_WFC_Emit_A", "MI_WFC_Emit_C", "MI_WFC_Emit_W")
]
call("editor_toolset.toolsets.asset.AssetTools", "save_assets",
     {"asset_paths": paths})
