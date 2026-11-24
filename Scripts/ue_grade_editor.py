import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

PPV = "/Game/Battle/Maps/Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.PostProcessVolume_1"
SUN = "/Game/Battle/Maps/Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.DirectionalLight_0.LightComponent0"
SKY = "/Game/Battle/Maps/Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.SkyLight_0.SkyLightComponent0"
HDRI_GEO = "/Game/Battle/Maps/Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.HDRIBackdrop_C_1.Geometry"
HDRI_SKY = "/Game/Battle/Maps/Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.HDRIBackdrop_C_1.Skylight"

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:300])
    return r


def setp(obj, values):
    return call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
                {"instance": obj, "values": json.dumps(values)})


def getp(obj, props):
    return text_of(call("editor_toolset.toolsets.object.ObjectTools", "get_properties",
                        {"instance": obj, "properties": props}))


raw = getp(PPV, ["bUnbound", "Settings"])
try:
    ppv = json.loads(json.loads(raw)["returnValue"])
    settings = ppv.get("Settings", {})
    settings["bOverride_AutoExposureMethod"] = True
    settings["AutoExposureMethod"] = "AEM_Manual"
    settings["bOverride_AutoExposureBias"] = True
    settings["AutoExposureBias"] = -2.5
    settings["bOverride_BloomIntensity"] = True
    settings["BloomIntensity"] = 0.6
    settings["bOverride_VignetteIntensity"] = True
    settings["VignetteIntensity"] = 0.35
    # warm the pale beige stone toward the Blender amber target
    settings["bOverride_WhiteTemp"] = True
    settings["WhiteTemp"] = 5600.0
    settings["bOverride_ColorGain"] = True
    settings["ColorGain"] = {"X": 1.10, "Y": 1.0, "Z": 0.86, "W": 1.0}
    settings["bOverride_ColorSaturation"] = True
    settings["ColorSaturation"] = {"X": 1.05, "Y": 0.98, "Z": 0.95, "W": 1.0}
    setp(PPV, {"bUnbound": True, "BlendWeight": 1.0, "Settings": settings})
except Exception as exc:
    print("PPV patch failed", exc, raw[:500])

setp(SUN, {"Intensity": 2.0, "LightColor": {"R": 255, "G": 214, "B": 170, "A": 255}})
setp(SKY, {"Intensity": 0.5})
setp(HDRI_GEO, {"bVisible": False})
setp(HDRI_SKY, {"Intensity": 0.15})

pie = {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort",
                   "warmupSeconds": 8,
                   "startTransform": {"location": {"x": 3800, "y": 7000, "z": 250},
                                      "rotation": {"pitch": 0, "yaw": -90, "roll": 0},
                                      "scale": {"x": 1, "y": 1, "z": 1}}}}
r = m.call("call_tool", {
    "toolset_name": "EditorToolset.EditorAppToolset",
    "tool_name": "StartPIE",
    "arguments": pie,
})
print("PIE", text_of(r)[:500])
