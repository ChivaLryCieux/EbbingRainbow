import bpy
from mathutils import Vector

for cname in ("WFC_walls", "WFC_floors", "WFC_ceilings", "WFC_props"):
    c = bpy.data.collections.get(cname)
    print(f"\n== {cname} ==", len(c.objects) if c else "MISSING")
    if not c:
        continue
    names = sorted(o.name for o in c.objects)
    print("first:", names[:8])
    print("last :", names[-8:])
    for probe in ("wall_powo", "wall_owww_stair_ramp", "wall_oooo", "wall_dowo", "wall_owww"):
        o = c.objects.get(probe)
        if o:
            bb = [o.matrix_world @ Vector(v) for v in o.bound_box]
            mn = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
            mx = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
            print(f"  {probe}: min={tuple(round(x,2) for x in mn)} max={tuple(round(x,2) for x in mx)} mats={[m.name for m in o.data.materials]}")

print("\nSCENE", bpy.context.scene.name, "engine", bpy.context.scene.render.engine)
print("objects total", len(bpy.data.objects), "materials", len(bpy.data.materials))
print("images", len(bpy.data.images), "missing", sum(1 for i in bpy.data.images if not i.has_data))
