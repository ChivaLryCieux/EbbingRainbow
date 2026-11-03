import base64
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

FLOOR = "/Game/WFCKit/Meshes/Floors/SM_floor_plain.SM_floor_plain"
WALL = "/Game/WFCKit/Meshes/Walls/SM_wall_dowo.SM_wall_dowo"

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:300])
    return r


def add(asset, name, loc):
    xf = {"location": {"x": loc[0], "y": loc[1], "z": loc[2]},
          "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
          "scale": {"x": 100, "y": 100, "z": 100}}
    r = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.scene.SceneTools",
        "tool_name": "add_to_scene_from_asset",
        "arguments": {"asset_path": asset, "name": name, "xform": xf},
    })
    return json.loads(text_of(r)).get("returnValue")


floor = add(FLOOR, "TwoSidedFloor", (0, 0, 0))
wall = add(WALL, "TwoSidedWall", (0, 0, 0))
ann = {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0,
       "maxLabelDistance": 0, "classFilter": {"refPath": "/Script/Engine.Actor"},
       "maxLabels": 0}

for tag, y, yaw in (("front", 700, -90), ("back", -700, 90)):
    cam = {"location": {"x": 200, "y": y, "z": 220},
           "rotation": {"pitch": -6, "yaw": yaw, "roll": 0},
           "scale": {"x": 1, "y": 1, "z": 1}}
    r = m.call("call_tool", {
        "toolset_name": "EditorToolset.EditorAppToolset",
        "tool_name": "CaptureViewport",
        "arguments": {"bShowUI": False, "captureTransform": cam, "annotations": ann},
    })
    try:
        data = json.loads(text_of(r))["returnValue"]["image"]["data"]
        out = rf"D:\EbbingRainbow\Saved\Screenshots\arch_twosided_{tag}.png"
        open(out, "wb").write(base64.b64decode(data))
        print("WROTE", out)
    except Exception as exc:
        print("capture failed", tag, exc)

for actor in (floor, wall):
    if actor:
        call("editor_toolset.toolsets.scene.SceneTools", "remove_from_scene",
             {"actor": actor})
