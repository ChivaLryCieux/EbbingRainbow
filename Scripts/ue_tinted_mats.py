#!/usr/bin/env python3
"""Create a tinted PBR master from the Fab texture sets and warm material
instances for walls / floors / ceiling, then remap the kit meshes."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

MAT_DIR = "/Game/WFCKit/Materials"
MASTER = "M_WFC_TintedPBR"
FAB = "/Game/Fab/Sci-fi_Interior_20"

WALLS = {
    "A": "walls_1", "B": "walls_2", "C": "walls_4", "D": "walls_3",
}
FLOORS = {"A": "floor_1", "B": "floor_2", "C": "floor_3"}

MI_SPECS = []
for key, prefix in WALLS.items():
    MI_SPECS.append((f"MI_WFC_Wall_{key}", prefix, {
        "A": (1.0, 0.78, 0.58, 1.0),
        "B": (1.0, 0.82, 0.62, 1.0),
        "C": (0.86, 0.82, 0.78, 1.0),
        "D": (1.0, 0.80, 0.60, 1.0),
    }[key]))
MI_SPECS.append(("MI_WFC_Dark", "walls_5", (0.55, 0.55, 0.60, 1.0)))
for key, prefix in FLOORS.items():
    MI_SPECS.append((f"MI_WFC_Floor_{key}", prefix, (1.0, 0.90, 0.80, 1.0)))
MI_SPECS.append(("MI_WFC_Ceil", "ceiling", (1.0, 0.86, 0.72, 1.0)))


def main():
    m = MCP()
    if not m.init():
        return 1

    graph = f"""
import json

def call(tool, args):
    return execute_tool(tool, json.dumps(args))

def ref(x):
    return x["returnValue"]["refPath"]

def setp(obj, values):
    return execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties",
        json.dumps({{"instance": obj, "values": json.dumps(values)}}))

def add(mat, cls):
    return ref(execute_tool("editor_toolset.toolsets.material.MaterialTools.add_expression",
        json.dumps({{"material_or_function": mat,
                     "expression_class": {{"refPath": cls}}}})))

def conn(a, ao, b, bi):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_expressions",
        json.dumps({{"from_expression": a, "from_output_name": ao,
                     "to_expression": b, "to_input_name": bi}}))

def out(e, prop):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_to_output",
        json.dumps({{"expression": e, "output_name": "",
                     "material_property": prop}}))

def run():
    mat = ref(execute_tool("editor_toolset.toolsets.material.MaterialTools.create_material",
        json.dumps({{"folder_path": "{MAT_DIR}", "asset_name": "{MASTER}"}})))
    BC = add(mat, "/Script/Engine.MaterialExpressionTextureSampleParameter2D")
    NM = add(mat, "/Script/Engine.MaterialExpressionTextureSampleParameter2D")
    RG = add(mat, "/Script/Engine.MaterialExpressionTextureSampleParameter2D")
    MT = add(mat, "/Script/Engine.MaterialExpressionTextureSampleParameter2D")
    TINT = add(mat, "/Script/Engine.MaterialExpressionVectorParameter")
    MUL = add(mat, "/Script/Engine.MaterialExpressionMultiply")
    setp(BC, {{"ParameterName": "BaseColorTex", "SamplerType": "SAMPLERTYPE_Color"}})
    setp(NM, {{"ParameterName": "NormalTex", "SamplerType": "SAMPLERTYPE_Normal"}})
    setp(RG, {{"ParameterName": "RoughnessTex", "SamplerType": "SAMPLERTYPE_LinearColor"}})
    setp(MT, {{"ParameterName": "MetallicTex", "SamplerType": "SAMPLERTYPE_LinearColor"}})
    setp(TINT, {{"ParameterName": "Tint"}})
    conn(BC, "RGB", MUL, "A")
    conn(TINT, "", MUL, "B")
    out(MUL, "MP_BaseColor")
    return {{"mat": mat, "BC": BC, "NM": NM, "RG": RG, "MT": MT, "MUL": MUL}}
"""
    res = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
        "tool_name": "execute_tool_script",
        "arguments": {"script": graph},
    })
    print("GRAPH", text_of(res)[:2000])
    try:
        data = json.loads(json.loads(text_of(res))["returnValue"])
        mat = data["mat"]
    except Exception as exc:
        print("graph parse failed", exc)
        return 1

    # Connect normal / roughness / metallic with a second simple script
    # (kept separate so expression refs are explicit).
    conn_script = f"""
import json

def conn(a, ao, b, bi):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_expressions",
        json.dumps({{"from_expression": a, "from_output_name": ao,
                     "to_expression": b, "to_input_name": bi}}))

def out(e, prop):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_to_output",
        json.dumps({{"expression": e, "output_name": "",
                     "material_property": prop}}))

def run():
    out("{data['NM']}", "MP_Normal")
    out("{data['RG']}", "MP_Roughness")
    out("{data['MT']}", "MP_Metallic")
    return {{"ok": True}}
"""
    # connect_to_output doesn't take an input pin; the normal sample's RGB is
    # the expression output, so the above is correct.
    print("CONNECT", text_of(m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
        "tool_name": "execute_tool_script",
        "arguments": {"script": conn_script},
    }))[:1000])

    m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.material.MaterialTools",
        "tool_name": "recompile",
        "arguments": {"material_or_function": mat},
    })

    # Create MIs and assign the Fab texture set + warm tint.
    for name, prefix, tint in MI_SPECS:
        cres = m.call("call_tool", {
            "toolset_name": "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
            "tool_name": "create",
            "arguments": {"folder_path": MAT_DIR, "asset_name": name,
                          "parent": {"refPath": mat}},
        })
        ctxt = text_of(cres)
        try:
            mi = json.loads(ctxt)["returnValue"]["refPath"]
        except Exception:
            print("MI create failed", name, ctxt[:300])
            continue
        for pname, suffix in (("BaseColorTex", "BaseColor"), ("NormalTex", "Normal"),
                              ("RoughnessTex", "Roughness"), ("MetallicTex", "Metallic")):
            tex = f"{FAB}/{prefix}_mat_{suffix}.{prefix}_mat_{suffix}"
            m.call("call_tool", {
                "toolset_name": "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
                "tool_name": "set_texture_parameter",
                "arguments": {"instance": mi, "name": pname,
                              "value": {"refPath": tex}},
            })
        m.call("call_tool", {
            "toolset_name": "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
            "tool_name": "set_vector_parameter",
            "arguments": {"instance": mi, "name": "Tint",
                          "value": {"r": tint[0], "g": tint[1], "b": tint[2], "a": 1.0}},
        })
        print("MI", name, mi)

    paths = [f"{MAT_DIR}/{MASTER}.{MASTER}"] + [
        f"{MAT_DIR}/{name}.{name}" for name, _, _ in MI_SPECS
    ]
    print("SAVE", text_of(m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.asset.AssetTools",
        "tool_name": "save_assets",
        "arguments": {"asset_paths": paths},
    }))[:500])
    return 0


if __name__ == "__main__":
    sys.exit(main())

