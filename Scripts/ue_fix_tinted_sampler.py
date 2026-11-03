import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

MAT = "/Game/WFCKit/Materials/M_WFC_TintedPBR.M_WFC_TintedPBR"
RG = MAT + ":MaterialExpressionTextureSampleParameter2D_2"
MT = MAT + ":MaterialExpressionTextureSampleParameter2D_3"

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:400])
    return r


for expr in (RG, MT):
    call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
         {"instance": expr, "values": json.dumps({"SamplerType": "SAMPLERTYPE_Color"})})

call("editor_toolset.toolsets.material.MaterialTools", "recompile",
     {"material_or_function": MAT})

paths = [MAT] + [
    f"/Game/WFCKit/Materials/{n}.{n}" for n in (
        "MI_WFC_Wall_A", "MI_WFC_Wall_B", "MI_WFC_Wall_C", "MI_WFC_Wall_D",
        "MI_WFC_Dark", "MI_WFC_Floor_A", "MI_WFC_Floor_B", "MI_WFC_Floor_C",
        "MI_WFC_Ceil")
]
call("editor_toolset.toolsets.asset.AssetTools", "save_assets",
     {"asset_paths": paths})
