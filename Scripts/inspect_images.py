import bpy, os
seen = {}
for img in bpy.data.images:
    p = bpy.path.abspath(img.filepath) if img.filepath else ""
    seen.setdefault(os.path.dirname(p), []).append((img.name, os.path.exists(p), img.filepath))
for d, items in seen.items():
    print("DIR", d, "count", len(items), "exists", sum(1 for _, e, _ in items if e))
    for n, e, fp in items[:3]:
        print("   ", n, e, fp)
print("MATS")
for m in list(bpy.data.materials)[:30]:
    print("  ", m.name, m.use_nodes, [n.type for n in m.node_tree.nodes][:6] if m.use_nodes else "")
