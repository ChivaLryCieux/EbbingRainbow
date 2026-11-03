"""Viewport capture + numeric image inspection (the look loop's eyes)."""
import base64
import json
import os

from .config import config
from .toolsets import call

APP = "EditorToolset.EditorAppToolset"

# Standard annotation payload: no grid, no labels, pure pixels.
CLEAN_ANNOTATIONS = {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0,
                     "maxLabelDistance": 0, "classFilter": None, "maxLabels": 0}


def capture(m, name, transform=None, ui=False, out_dir=None):
    """Save the active viewport to <shot_dir>/<name>. Returns the path."""
    args = {"bShowUI": ui, "annotations": dict(CLEAN_ANNOTATIONS)}
    if transform:
        args["captureTransform"] = transform
    res = json.loads(call(m, APP, "CaptureViewport", args))
    rv = res["returnValue"]
    data = rv.get("data") or rv.get("image", {}).get("data")
    out = os.path.join(str(out_dir or config.shot_dir), name)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(base64.b64decode(data))
    return out


def stats(path, bands=6):
    """Per-band luminance + probe points + near-black ratio. Needs PIL."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    print(f"{os.path.basename(path)}  {w}x{h}")
    for b in range(bands):
        y0, y1 = h * b // bands, h * (b + 1) // bands
        vals = [px[x, y] for y in range(y0, y1, 6) for x in range(0, w, 6)]
        n = len(vals)
        r = sum(v[0] for v in vals) / n
        g = sum(v[1] for v in vals) / n
        b_ = sum(v[2] for v in vals) / n
        print(f"  band{b} y{y0:4d}-{y1:4d}  mean=({r:5.1f},{g:5.1f},{b_:5.1f})  "
              f"lum={0.2126 * r + 0.7152 * g + 0.0722 * b_:5.1f}")
    probes = {"top-center": (w // 2, max(2, h // 50)),
              "upper-left": (w // 8, h // 5),
              "center": (w // 2, h // 2),
              "lower-right": (7 * w // 8, 4 * h // 5)}
    for label, (x, y) in probes.items():
        print(f"  probe {label:12s} = {px[x, y]}")
    hist = im.convert("L").histogram()
    dark = sum(hist[:24]) / (w * h) * 100
    print(f"  near-black(<24) {dark:.1f}%")
