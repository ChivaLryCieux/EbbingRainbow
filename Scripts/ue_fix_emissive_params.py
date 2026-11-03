import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

MAT = "/Game/WFCKit/Materials/M_WFC_Emissive.M_WFC_Emissive"
VEC = MAT + ":MaterialExpressionVectorParameter_0"
SCA = MAT + ":MaterialExpressionScalarParameter_0"
MIS = {
    "MI_WFC_Emit_A": ({"r": 1.0, "g": 0.32, "b": 0.04, "a": 1.0}, 12.0),
    "MI_WFC_Emit_C": ({"r": 0.06, "g": 0.55, "b": 1.0, "a": 1.0}, 10.0),
    "MI_WFC_Emit_W": ({"r": 1.0, "g": 0.86, "b": 0.62, "a": 1.0}, 10.0),
}

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:500])
    return r


def setp(obj, values):
    return call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
                {"instance": obj, "values": json.dumps(values)})


setp(VEC, {"ParameterName": "EmissiveColor"})
setp(SCA, {"ParameterName": "EmissiveStrength"})
setp(MAT, {"ShadingModel": "MSM_Unlit"})
call("editor_toolset.toolsets.material.MaterialTools", "recompile",
     {"material_or_function": MAT})

for name, (rgb, strength) in MIS.items():
    mi = f"/Game/WFCKit/Materials/{name}.{name}"
    call("editor_toolset.toolsets.material_instance.MaterialInstanceTools",
         "set_vector_parameter", {"instance": mi, "name": "EmissiveColor", "value": rgb})
    call("editor_toolset.toolsets.material_instance.MaterialInstanceTools",
         "set_scalar_parameter", {"instance": mi, "name": "EmissiveStrength", "value": strength})

print("params now:")
call("editor_toolset.toolsets.material_instance.MaterialInstanceTools", "list_parameters",
     {"material": MAT})
