import json
import os

kit = r"D:\EbbingRainbow\sci_fi_interior_20fbx\WFC_Kit"
lvl = json.load(open(os.path.join(kit, "levels", "arch_seed7.json"), encoding="utf-8"))
man = json.load(open(os.path.join(kit, "tilesets", "ue5_manifest.json"), encoding="utf-8"))
tiles = {t.get("id") or t.get("name"): t for t in man.get("tiles", [])}
print("manifest tiles", len(tiles))
used = set()
for c in lvl["cells"]:
    for k in ("wall", "floor", "ceiling", "prop"):
        if c.get(k):
            used.add(c[k])
    u = c.get("upper") or {}
    for k in ("wall", "floor", "ceiling", "railing", "railing2"):
        if u.get(k):
            used.add(u[k])
print("used", len(used))
missing = sorted(t for t in used if t not in tiles)
print("missing", missing[:20], "count", len(missing))
for t in sorted(used):
    if t in tiles:
        cp = tiles[t].get("content_path") or tiles[t].get("asset") or ""
        if not cp:
            print("no content_path", t)
