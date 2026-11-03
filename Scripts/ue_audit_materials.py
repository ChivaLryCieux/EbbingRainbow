import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

m = MCP()
assert m.init()
paths = json.load(open(r"D:\EbbingRainbow\Scripts\wfc_asset_paths.json", encoding="utf-8"))["asset_paths"]

script = """
import json
PATHS = json.loads(%s)

def slots(mesh):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_material_slots",
        json.dumps({"mesh": mesh}))["returnValue"]

def mat(mesh, slot):
    r = execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_material",
        json.dumps({"mesh": mesh, "slot_name": slot}))
    v = r.get("returnValue")
    return v.get("refPath") if isinstance(v, dict) else v

def run():
    bad = []
    missing_slots = []
    total = 0
    for mesh in PATHS:
        ss = slots(mesh)
        if not ss:
            missing_slots.append(mesh)
            continue
        for slot in ss:
            total += 1
            ref = mat(mesh, slot)
            if not ref or "/Engine/" in ref or "WorldGrid" in ref or "DefaultMaterial" in ref:
                bad.append({"mesh": mesh, "slot": slot, "mat": ref})
    return {"total_slots": total, "bad": bad, "bad_count": len(bad),
            "no_slots": missing_slots}
""" % json.dumps(json.dumps(paths))
res = m.call("call_tool", {
    "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
    "tool_name": "execute_tool_script",
    "arguments": {"script": script},
})
print(text_of(res)[:8000])
