import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_shot import connect, call, capture, stats

m = connect()
APP = "EditorToolset.EditorAppToolset"

print("start", call(m, APP, "StartPIE", {"options": {
    "bSimulate": True, "playMode": "PlayMode_InViewPort", "warmupSeconds": 12.0}})[:200])

views = [
    ("final_hero.png", {"location": {"x": 1200.0, "y": 1200.0, "z": 620.0},
                        "rotation": {"pitch": -4.0, "yaw": 45.0, "roll": 0.0}}),
    ("final_atrium.png", {"location": {"x": 3800.0, "y": 3800.0, "z": 800.0},
                          "rotation": {"pitch": 34.0, "yaw": 20.0, "roll": 0.0}}),
    ("final_wide.png", {"location": {"x": 7800.0, "y": 7800.0, "z": 3000.0},
                        "rotation": {"pitch": -14.0, "yaw": -135.0, "roll": 0.0}}),
]
for name, cam in views:
    stats(capture(m, name, cam))
