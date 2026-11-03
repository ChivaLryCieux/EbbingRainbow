#!/usr/bin/env python3
"""Create M_Sanctum_Sky: a controlled vertical-gradient sky material.

Unlit, two-sided, bIsSky: zenith->horizon gradient driven by dome normal Z.
Then bind it on the SkyDome actor.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_shot import connect, call

m = connect()
MAT = "/Game/Environment/Sky/M_Sanctum_Sky"
FOLDER = "/Game/Environment/Sky"

graph = r'''
import json

def call(tool, args):
    return execute_tool(tool, json.dumps(args))

def ref(x):
    return x["returnValue"]["refPath"]

def setp(obj, values):
    return execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties",
        json.dumps({"instance": obj, "values": json.dumps(values)}))

def add(mat, cls):
    return ref(execute_tool("editor_toolset.toolsets.material.MaterialTools.add_expression",
        json.dumps({"material_or_function": mat, "expression_class": {"refPath": cls}})))

def conn(a, ao, b, bi):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_expressions",
        json.dumps({"from_expression": a, "from_output_name": ao,
                    "to_expression": b, "to_input_name": bi}))

def out(e, prop):
    return execute_tool("editor_toolset.toolsets.material.MaterialTools.connect_to_output",
        json.dumps({"expression": e, "output_name": "", "material_property": prop}))

def run():
    try:
        mat = ref(execute_tool("editor_toolset.toolsets.material.MaterialTools.create_material",
            json.dumps({"folder_path": "%s", "asset_name": "M_Sanctum_Sky"})))
    except Exception:
        mat = "%s/M_Sanctum_Sky.M_Sanctum_Sky"
    setp(mat, {"shadingModel": "MSM_Unlit", "twoSided": True, "bIsSky": True})
    old = execute_tool("editor_toolset.toolsets.material.MaterialTools.get_expressions",
        json.dumps({"material_or_function": mat}))["returnValue"]
    for e in old:
        execute_tool("editor_toolset.toolsets.material.MaterialTools.delete_expression",
            json.dumps({"material_or_function": mat, "expression": {"refPath": e["refPath"]}}))
    N = add(mat, "/Script/Engine.MaterialExpressionVertexNormalWS")
    MK = add(mat, "/Script/Engine.MaterialExpressionComponentMask")
    conn(N, "", MK, "")
    setp(MK, {"B": True})
    SAT = add(mat, "/Script/Engine.MaterialExpressionSaturate")
    conn(MK, "", SAT, "")
    POW = add(mat, "/Script/Engine.MaterialExpressionPower")
    conn(SAT, "", POW, "Base")
    GP = add(mat, "/Script/Engine.MaterialExpressionScalarParameter")
    setp(GP, {"ParameterName": "GradientPower", "DefaultValue": 0.7})
    conn(GP, "", POW, "Exp")
    LERP = add(mat, "/Script/Engine.MaterialExpressionLinearInterpolate")
    HC = add(mat, "/Script/Engine.MaterialExpressionVectorParameter")
    setp(HC, {"ParameterName": "HorizonColor",
              "DefaultValue": {"r": 0.98, "g": 0.88, "b": 0.72, "a": 1.0}})
    conn(HC, "", LERP, "A")
    ZC = add(mat, "/Script/Engine.MaterialExpressionVectorParameter")
    setp(ZC, {"ParameterName": "ZenithColor",
              "DefaultValue": {"r": 0.42, "g": 0.52, "b": 0.68, "a": 1.0}})
    conn(ZC, "", LERP, "B")
    conn(POW, "", LERP, "Alpha")
    MUL = add(mat, "/Script/Engine.MaterialExpressionMultiply")
    conn(LERP, "", MUL, "A")
    BR = add(mat, "/Script/Engine.MaterialExpressionScalarParameter")
    setp(BR, {"ParameterName": "SkyBrightness", "DefaultValue": 1.0})
    conn(BR, "", MUL, "B")
    out(MUL, "MP_EmissiveColor")
    execute_tool("editor_toolset.toolsets.material.MaterialTools.layout_expressions",
        json.dumps({"material_or_function": mat}))
    execute_tool("editor_toolset.toolsets.material.MaterialTools.recompile",
        json.dumps({"material_or_function": mat}))
    execute_tool("editor_toolset.toolsets.asset.AssetTools.save_assets",
        json.dumps({"asset_paths": [mat.split(".")[0]]}))
    return {"mat": mat}
''' % (FOLDER, FOLDER)

r = m.call("call_tool", {
    "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
    "tool_name": "execute_tool_script",
    "arguments": {"script": graph},
})
txt = ""
for c in r.get("content", []):
    if c.get("type") == "text":
        txt += c["text"]
print(txt[:2000])

# bind to dome
SCENE = "editor_toolset.toolsets.scene.SceneTools"
ACTOR = "editor_toolset.toolsets.actor.ActorTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"

r = call(m, SCENE, "find_actors", {"name": "SkyDome", "tag": "", "collision_channels": []})
dome = json.loads(r)["returnValue"][0]["refPath"]
comp = json.loads(call(m, ACTOR, "get_components", {"actor": {"refPath": dome}}))["returnValue"][0]["refPath"]
call(m, OBJ, "set_properties", {"instance": comp, "values": json.dumps(
    {"OverrideMaterials": [{"refPath": MAT + ".M_Sanctum_Sky"}]})})
print("bound", dome)
