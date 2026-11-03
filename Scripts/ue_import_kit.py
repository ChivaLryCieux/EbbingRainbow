#!/usr/bin/env python3
"""Import WFC_Kit FBX tiles into /Game/WFCKit and remap slots to Fab materials.

Usage:
  python ue_import_kit.py --plan <arch_seed7.json> --limit 3
  python ue_import_kit.py --plan <arch_seed7.json>
  python ue_import_kit.py --all --group walls
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

KIT = r"D:\EbbingRainbow\sci_fi_interior_20fbx\WFC_Kit"
FBX = os.path.join(KIT, "FBX")

FAB = "/Game/Fab/Sci-fi_Interior_20"
MAT = {
    "M_WALL_A": f"{FAB}/walls_1_mat_PBS.walls_1_mat_PBS",
    "M_WALL_B": f"{FAB}/walls_2_mat_PBS.walls_2_mat_PBS",
    "M_WALL_C": f"{FAB}/walls_4_mat_PBS.walls_4_mat_PBS",
    "M_WALL_D": f"{FAB}/walls_3_mat_PBS.walls_3_mat_PBS",
    "M_DARK": f"{FAB}/walls_5_mat_PBS.walls_5_mat_PBS",
    "M_TRIM": f"{FAB}/walls_5_mat_PBS.walls_5_mat_PBS",
    "M_RUBBER": f"{FAB}/walls_5_mat_PBS.walls_5_mat_PBS",
    "M_FLOOR_A": f"{FAB}/floor_1_mat_PBS.floor_1_mat_PBS",
    "M_FLOOR_B": f"{FAB}/floor_2_mat_PBS.floor_2_mat_PBS",
    "M_FLOOR_C": f"{FAB}/floor_3_mat_PBS.floor_3_mat_PBS",
    "M_CEIL": f"{FAB}/ceiling_mat_PBS.ceiling_mat_PBS",
    "M_GLASS": f"{FAB}/glass_mat_PBS.glass_mat_PBS",
    "M_LIGHT": f"{FAB}/lights_mat_PBS.lights_mat_PBS",
    "M_EMIT_A": f"{FAB}/lights_mat_PBS.lights_mat_PBS",
    "M_EMIT_C": f"{FAB}/lights_mat_PBS.lights_mat_PBS",
    "M_EMIT_W": f"{FAB}/lights_mat_PBS.lights_mat_PBS",
}
LAYERS = {
    "wall": ("Walls", "/Game/WFCKit/Meshes/Walls"),
    "floor": ("Floors", "/Game/WFCKit/Meshes/Floors"),
    "ceil": ("Ceilings", "/Game/WFCKit/Meshes/Ceilings"),
    "prop": ("Props", "/Game/WFCKit/Meshes/Props"),
}


def collect(plan_path=None, group=None):
    ids = {"wall": set(), "floor": set(), "ceil": set(), "prop": set()}
    if plan_path:
        d = json.load(open(plan_path, encoding="utf-8"))
        for c in d["cells"]:
            for k in ("wall", "floor", "ceiling", "prop"):
                if c.get(k):
                    ids["ceil" if k == "ceiling" else k].add(c[k])
            u = c.get("upper") or {}
            for k in ("wall", "floor", "ceiling", "railing", "railing2"):
                if u.get(k):
                    layer = "ceil" if k == "ceiling" else ("wall" if k in ("wall", "railing", "railing2") else "floor")
                    ids[layer].add(u[k])
    else:
        for layer, (folder, _) in LAYERS.items():
            for f in os.listdir(os.path.join(FBX, folder)):
                if f.lower().endswith(".fbx") and f.startswith("SM_"):
                    ids[layer].add(f[3:-4])
    out = []
    for layer, (folder, dest) in LAYERS.items():
        if group and layer != group:
            continue
        for tid in sorted(ids[layer]):
            src = os.path.join(FBX, folder, f"SM_{tid}.fbx")
            if not os.path.exists(src):
                continue
            out.append((layer, dest, f"SM_{tid}", src.replace("\\", "/")))
    return out


def build_script(items, limit=None):
    if limit:
        items = items[:limit]
    rows = json.dumps([[l, d, a, s] for (l, d, a, s) in items], ensure_ascii=False)
    mats = json.dumps(MAT, ensure_ascii=False)
    return f"""
import json

ITEMS = json.loads({json.dumps(rows)})
MATS = json.loads({json.dumps(mats)})

def imp(folder, name, src):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file",
        json.dumps({{"folder_path": folder, "asset_name": name, "source_file": src}}))

def slots(mesh):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_material_slots",
        json.dumps({{"mesh": mesh}}))["returnValue"]

def setmat(mesh, slot, mat):
    return execute_tool("editor_toolset.toolsets.static_mesh.StaticMeshTools.set_material",
        json.dumps({{"mesh": mesh, "slot_name": slot, "material": mat}}))

def run():
    imported = 0
    replaced = 0
    errors = []
    for layer, folder, name, src in ITEMS:
        try:
            r = imp(folder, name, src)
            refs = r.get("returnValue") or []
            if not refs:
                errors.append("no-ref " + name)
                continue
            mesh = refs[0]["refPath"]
            imported += 1
            for slot in slots(mesh):
                mat = MATS.get(slot)
                if not mat:
                    mat = MATS["M_TRIM"]
                setmat(mesh, slot, mat)
                replaced += 1
        except Exception as exc:
            errors.append(name + ": " + str(exc)[:200])
    return {{"imported": imported, "slots": replaced, "errors": errors[:30],
             "error_count": len(errors)}}
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--plan", default=None)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--group", default=None)
    ap.add_argument("--limit", type=int, default=None)
    args = ap.parse_args()
    items = collect(None if args.all else args.plan, args.group)
    print("items", len(items), "limit", args.limit)
    if not items:
        return 1
    script = build_script(items, args.limit)
    m = MCP()
    if not m.init():
        return 1
    res = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
        "tool_name": "execute_tool_script",
        "arguments": {"script": script},
    })
    print(json.dumps(res, ensure_ascii=False, indent=1)[:12000])
    return 0


if __name__ == "__main__":
    sys.exit(main())
