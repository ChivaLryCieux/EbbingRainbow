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
    print(tool, text_of(r)[:500])
    return r


def add(asset, name, loc, yaw=0, scale=100.0):
    xf = {"location": {"x": loc[0], "y": loc[1], "z": loc[2]},
          "rotation": {"pitch": 0, "yaw": yaw, "roll": 0},
          "scale": {"x": scale, "y": scale, "z": scale}}
    r = m.call("call_tool", {
        "toolset_name": "editor_toolset.toolsets.scene.SceneTools",
        "tool_name": "add_to_scene_from_asset",
        "arguments": {"asset_path": asset, "name": name, "xform": xf},
    })
    print("add", name, text_of(r)[:300])
    return json.loads(text_of(r)).get("returnValue")


call("EditorToolset.EditorAppToolset", "StopPIE", {})
floor = add(FLOOR, "MatTestFloor", (0, 0, 0))
wall = add(WALL, "MatTestWall", (0, 0, 0))

# camera in front of the wall (wall front faces +Y)
cam = {"location": {"x": 200, "y": 700, "z": 220},
       "rotation": {"pitch": -6, "yaw": -90, "roll": 0},
       "scale": {"x": 1, "y": 1, "z": 1}}
ann = {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0,
       "maxLabelDistance": 0, "classFilter": {"refPath": "/Script/Engine.Actor"},
       "maxLabels": 0}
r = m.call("call_tool", {
    "toolset_name": "EditorToolset.EditorAppToolset",
    "tool_name": "CaptureViewport",
    "arguments": {"bShowUI": False, "captureTransform": cam, "annotations": ann},
})
txt = text_of(r)
try:
    data = json.loads(txt)["returnValue"]["image"]["data"]
    out = r"D:\EbbingRainbow\Saved\Screenshots\arch_material_test.png"
    open(out, "wb").write(base64.b64decode(data))
    print("WROTE", out)
except Exception as exc:
    print("capture failed", exc, txt[:500])

for actor in (floor, wall):
    if actor:
        call("editor_toolset.toolsets.scene.SceneTools", "remove_from_scene",
             {"actor": actor})
