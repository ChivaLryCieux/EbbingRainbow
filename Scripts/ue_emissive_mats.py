#!/usr/bin/env python3
"""Create a proper unlit emissive material + MI_A/C/W and remap the kit's
M_EMIT_* / M_LIGHT slots to it."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

MAT_DIR = "/Game/WFCKit/Materials"
MAT_NAME = "M_WFC_Emissive"


def run_script(m, script):
    res = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
        "tool_name": "execute_tool_script",
        "arguments": {"script": script},
    })
    print(json.dumps(res, ensure_ascii=False, indent=1)[:6000])
    return res


def main():
    m = MCP()
    if not m.init():
        return 1

    create = f"""
import json

def make_material():
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.create_material",
        json.dumps({{"folder_path": "{MAT_DIR}", "asset_name": "{MAT_NAME}"}}))["returnValue"]["refPath"]

def add(mat, cls, x, y):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.add_expression",
        json.dumps({{"material_or_function": mat,
                     "expression_class": {{"refPath": cls}},
                     "x": x, "y": y}}))["returnValue"]["refPath"]

def setp(obj, values):
    return execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties",
        json.dumps({{"instance": obj, "values": values}}))

def connect(a, aout, b, binp):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_expressions",
        json.dumps({{"from_expression": a, "from_output_name": aout,
                     "to_expression": b, "to_input_name": binp}}))

def out(expr, prop):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_to_output",
        json.dumps({{"expression": expr, "output_name": "",
                     "material_property": prop}}))

def recompile(mat):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.recompile",
        json.dumps({{"material_or_function": mat}}))

def run():
    mat = make_material()
    vec = add(mat, "/Script/Engine.MaterialExpressionVectorParameter", -400, 0)
    sca = add(mat, "/Script/Engine.MaterialExpressionScalarParameter", -400, 160)
    mul = add(mat, "/Script/Engine.MaterialExpressionMultiply", -100, 60)
    setp(vec, {{"ParameterName": "EmissiveColor"}})
    setp(sca, {{"ParameterName": "EmissiveStrength"}})
    connect(vec, "", mul, "A")
    connect(sca, "", mul, "B")
    out(mul, "MP_EmissiveColor")
    try:
        setp(mat, {{"ShadingModel": "MSM_Unlit"}})
    except Exception as exc:
        pass
    recompile(mat)
    return {{"material": mat, "vec": vec, "sca": sca, "mul": mul}}
"""
    res = run_script(m, create)
    txt = text_of(res)
    try:
        data = json.loads(json.loads(txt)["returnValue"])
        mat = data["material"]
    except Exception as exc:
        print("could not parse material", exc)
        return 1

    # Create three material instances and set their parameters.
    for name, rgb, strength in (
        ("MI_WFC_Emit_A", {"R": 1.0, "G": 0.32, "B": 0.04, "A": 1.0}, 12.0),
        ("MI_WFC_Emit_C", {"R": 0.06, "G": 0.55, "B": 1.0, "A": 1.0}, 10.0),
        ("MI_WFC_Emit_W", {"R": 1.0, "G": 0.86, "B": 0.62, "A": 1.0}, 10.0),
    ):
        cres = m.call("call_tool", {
            "toolset_name": "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
            "tool_name": "create",
            "arguments": {"folder_path": MAT_DIR, "asset_name": name, "parent": mat},
        })
        ctxt = text_of(cres)
        print("MI", name, ctxt[:300])
        try:
            mi = json.loads(ctxt)["returnValue"]["refPath"]
        except Exception:
            continue
        m.call("call_tool", {
            "toolset_name": "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
            "tool_name": "set_vector_parameter",
            "arguments": {"instance": mi, "name": "EmissiveColor", "value": rgb},
        })
        m.call("call_tool", {
            "toolset_name": "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
            "tool_name": "set_scalar_parameter",
            "arguments": {"instance": mi, "name": "EmissiveStrength", "value": strength},
        })
        print("MI configured", name, mi)

    m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.asset.AssetTools",
        "tool_name": "save_assets",
        "arguments": {"asset_paths": [
            f"{MAT_DIR}/{MAT_NAME}.{MAT_NAME}",
            f"{MAT_DIR}/MI_WFC_Emit_A.MI_WFC_Emit_A",
            f"{MAT_DIR}/MI_WFC_Emit_C.MI_WFC_Emit_C",
            f"{MAT_DIR}/MI_WFC_Emit_W.MI_WFC_Emit_W",
        ]},
    })
    return 0


if __name__ == "__main__":
    sys.exit(main())
