import bpy
from mathutils import Vector

def bb(o):
    pts = [o.matrix_world @ Vector(v) for v in o.bound_box]
    return (min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts),
            max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))

for name in ("wall_oooo_column", "wall_hooo", "wall_howo", "wall_owww", "wall_wwww",
             "ceil_open", "ceil_plain", "ceil_light", "floor_plain", "floor_tech",
             "prop_console", "prop_planter", "wall_wwow_stair_spiral"):
    o = bpy.data.objects.get(name)
    if not o:
        print("MISSING", name)
        continue
    b = bb(o)
    print(f"{name:26s} min=({b[0]:.2f},{b[1]:.2f},{b[2]:.2f}) max=({b[3]:.2f},{b[4]:.2f},{b[5]:.2f}) "
          f"tris={len(o.data.polygons)} mats={[m.name for m in o.data.materials]}")
