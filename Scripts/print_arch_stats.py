import glob
import json
import os

root = r"D:\EbbingRainbow\sci_fi_interior_20fbx\WFC_Kit\levels"
for p in sorted(glob.glob(os.path.join(root, "arch_seed*.json"))):
    with open(p, encoding="utf-8") as f:
        s = json.load(f)["stats"]
    print(os.path.basename(p), "rooms", s["rooms"], "doors", s["doors"],
          "windows", s["windows"], "cols", s["columns"], "props", s["props"],
          "reach", f"{s['reachable']}/{s['cells']}", "tri", s["triangles_l0_l1"],
          "fb", s["fallbacks"])
