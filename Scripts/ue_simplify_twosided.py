import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

MAT = "/Game/WFCKit/Materials/M_WFC_TintedPBR.M_WFC_TintedPBR"
RG = MAT + ":MaterialExpressionTextureSampleParameter2D_2"
MT = MAT + ":MaterialExpressionTextureSampleParameter2D_3"
EMIT = "/Game/WFCKit/Materials/M_WFC_Emissive.M_WFC_Emissive"
FAB_MASTER = "/Game/Fab/Sci-fi_Interior_20/M_SciFi_Master.M_SciFi_Master"

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:400])
    return r


def setp(obj, values):
    return call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
                {"instance": obj, "values": json.dumps(values)})


# Drop the two texture-driven scalar inputs (the source of the MI compile
# failures) and use scalar parameters instead.
for prop in ("MP_Roughness", "MP_Metallic"):
    call("editor_toolset.toolsets.material.MaterialTools", "disconnect_from_output",
         {"material": MAT, "material_property": prop})
for expr in (RG, MT):
    call("editor_toolset.toolsets.material.MaterialTools", "delete_expression",
         {"material_or_function": MAT, "expression": expr})

for name, default, prop in (("Roughness", 0.55, "MP_Roughness"),
                            ("Metallic", 0.0, "MP_Metallic")):
    r = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.material.MaterialTools",
        "tool_name": "add_expression",
        "arguments": {"material_or_function": MAT,
                      "expression_class": {"refPath": "/Script/Engine.MaterialExpressionScalarParameter"}},
    })
    expr = json.loads(text_of(r))["returnValue"]["refPath"]
    setp(expr, {"ParameterName": name, "DefaultValue": default})
    call("editor_toolset.toolsets.material.MaterialTools", "connect_to_output",
         {"expression": expr, "output_name": "", "material_property": prop})

# Two-sided: walls are thin single-sided planes; UE culls the back face by default.
setp(MAT, {"TwoSided": True})
setp(EMIT, {"TwoSided": True})
setp(FAB_MASTER, {"TwoSided": True})

call("editor_toolset.toolsets.material.MaterialTools", "recompile",
     {"material_or_function": MAT})

paths = [MAT, EMIT, FAB_MASTER] + [
    f"/Game/WFCKit/Materials/{n}.{n}" for n in (
        "MI_WFC_Wall_A", "MI_WFC_Wall_B", "MI_WFC_Wall_C", "MI_WFC_Wall_D",
        "MI_WFC_Dark", "MI_WFC_Floor_A", "MI_WFC_Floor_B", "MI_WFC_Floor_C",
        "MI_WFC_Ceil")
]
call("editor_toolset.toolsets.asset.AssetTools", "save_assets",
     {"asset_paths": paths})
