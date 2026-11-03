import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

MI = {
    "M_WALL_A": "/Game/WFCKit/Materials/MI_WFC_Wall_A.MI_WFC_Wall_A",
    "M_WALL_B": "/Game/WFCKit/Materials/MI_WFC_Wall_B.MI_WFC_Wall_B",
    "M_WALL_C": "/Game/WFCKit/Materials/MI_WFC_Wall_C.MI_WFC_Wall_C",
    "M_WALL_D": "/Game/WFCKit/Materials/MI_WFC_Wall_D.MI_WFC_Wall_D",
    "M_DARK": "/Game/WFCKit/Materials/MI_WFC_Dark.MI_WFC_Dark",
    "M_TRIM": "/Game/WFCKit/Materials/MI_WFC_Dark.MI_WFC_Dark",
    "M_RUBBER": "/Game/WFCKit/Materials/MI_WFC_Dark.MI_WFC_Dark",
    "M_FLOOR_A": "/Game/WFCKit/Materials/MI_WFC_Floor_A.MI_WFC_Floor_A",
    "M_FLOOR_B": "/Game/WFCKit/Materials/MI_WFC_Floor_B.MI_WFC_Floor_B",
    "M_FLOOR_C": "/Game/WFCKit/Materials/MI_WFC_Floor_C.MI_WFC_Floor_C",
    "M_CEIL": "/Game/WFCKit/Materials/MI_WFC_Ceil.MI_WFC_Ceil",
}

m = MCP()
assert m.init()
paths = json.load(open(r"D:\EbbingRainbow\Scripts\wfc_asset_paths.json", encoding="utf-8"))["asset_paths"]

script = """
import json
PATHS = json.loads(%s)
MI = json.loads(%s)

def slots(mesh):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_material_slots",
        json.dumps({"mesh": mesh}))["returnValue"]

def setmat(mesh, slot, mat):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.set_material",
        json.dumps({"mesh": mesh, "slot_name": slot, "material": mat}))

def run():
    changed = 0
    errors = []
    for mesh in PATHS:
        try:
            for slot in slots(mesh):
                mat = MI.get(slot)
                if mat:
                    setmat(mesh, slot, mat)
                    changed += 1
        except Exception as exc:
            errors.append(mesh + ": " + str(exc)[:160])
    return {"changed": changed, "errors": errors[:20], "error_count": len(errors)}
""" % (json.dumps(json.dumps(paths)), json.dumps(json.dumps(MI)))
res = m.call("call_tool", {
    "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
    "tool_name": "execute_tool_script",
    "arguments": {"script": script},
})
print("REMAP", text_of(res)[:1000])

save = {"asset_paths": paths + [
    "/Game/WFCKit/Materials/M_WFC_TintedPBR.M_WFC_TintedPBR",
    "/Game/WFCKit/Materials/MI_WFC_Wall_A.MI_WFC_Wall_A",
    "/Game/WFCKit/Materials/MI_WFC_Wall_B.MI_WFC_Wall_B",
    "/Game/WFCKit/Materials/MI_WFC_Wall_C.MI_WFC_Wall_C",
    "/Game/WFCKit/Materials/MI_WFC_Wall_D.MI_WFC_Wall_D",
    "/Game/WFCKit/Materials/MI_WFC_Dark.MI_WFC_Dark",
    "/Game/WFCKit/Materials/MI_WFC_Floor_A.MI_WFC_Floor_A",
    "/Game/WFCKit/Materials/MI_WFC_Floor_B.MI_WFC_Floor_B",
    "/Game/WFCKit/Materials/MI_WFC_Floor_C.MI_WFC_Floor_C",
    "/Game/WFCKit/Materials/MI_WFC_Ceil.MI_WFC_Ceil",
]}
r = m.call("call_tool", {
    "toolset_name": "editor_toolset.toolsets.asset.AssetTools",
    "tool_name": "save_assets",
    "arguments": save,
})
print("SAVE", text_of(r)[:500])
