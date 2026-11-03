import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

MI = {
    "M_EMIT_A": "/Game/WFCKit/Materials/MI_WFC_Emit_A.MI_WFC_Emit_A",
    "M_EMIT_C": "/Game/WFCKit/Materials/MI_WFC_Emit_C.MI_WFC_Emit_C",
    "M_EMIT_W": "/Game/WFCKit/Materials/MI_WFC_Emit_W.MI_WFC_Emit_W",
    "M_LIGHT": "/Game/WFCKit/Materials/MI_WFC_Emit_W.MI_WFC_Emit_W",
}
STRENGTH = {"MI_WFC_Emit_A": 5.0, "MI_WFC_Emit_C": 4.0, "MI_WFC_Emit_W": 4.0}

m = MCP()
assert m.init()


def call(ts, tool, args):
    return m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})


def text(res):
    return text_of(res)


# Lower strengths first (12 clips to white).
for name, val in STRENGTH.items():
    mi = f"/Game/WFCKit/Materials/{name}.{name}"
    print(name, text(call(
        "editor_toolset.toolsets.material_instance.MaterialInstanceTools",
        "set_scalar_parameter",
        {"instance": mi, "name": "EmissiveStrength", "value": val}))[:200])

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
print("REMAP", text(res)[:1000])

save = {"asset_paths": paths + [
    "/Game/WFCKit/Materials/M_WFC_Emissive.M_WFC_Emissive",
    "/Game/WFCKit/Materials/MI_WFC_Emit_A.MI_WFC_Emit_A",
    "/Game/WFCKit/Materials/MI_WFC_Emit_C.MI_WFC_Emit_C",
    "/Game/WFCKit/Materials/MI_WFC_Emit_W.MI_WFC_Emit_W",
]}
print("SAVE", text(call("editor_toolset.toolsets.asset.AssetTools", "save_assets", save))[:500])
