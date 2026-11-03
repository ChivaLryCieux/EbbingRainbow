import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

PPV = "/Game/Battle/Maps/Lvl_Arena.Lvl_Arena:PersistentLevel.PostProcessVolume_1"
SUN = "/Game/Battle/Maps/Lvl_Arena.Lvl_Arena:PersistentLevel.DirectionalLight_0.LightComponent0"
SKY = "/Game/Battle/Maps/Lvl_Arena.Lvl_Arena:PersistentLevel.SkyLight_0.SkyLightComponent0"
HDRI_GEO = "/Game/Battle/Maps/Lvl_Arena.Lvl_Arena:PersistentLevel.HDRIBackdrop_C_1.Geometry"
HDRI_SKY = "/Game/Battle/Maps/Lvl_Arena.Lvl_Arena:PersistentLevel.HDRIBackdrop_C_1.Skylight"
MI_W = "/Game/WFCKit/Materials/MI_WFC_Emit_W.MI_WFC_Emit_W"

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:300])
    return r


def setp(obj, values):
    return call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
                {"instance": obj, "values": json.dumps(values)})


call("EditorToolset.EditorAppToolset", "StopPIE", {})

# Large ceiling light panels should not be a white lightbox.
call("editor_toolset.toolsets.material_instance.MaterialInstanceTools",
     "set_vector_parameter",
     {"instance": MI_W, "name": "EmissiveColor",
      "value": {"r": 1.0, "g": 0.78, "b": 0.55, "a": 1.0}})
call("editor_toolset.toolsets.material_instance.MaterialInstanceTools",
     "set_scalar_parameter",
     {"instance": MI_W, "name": "EmissiveStrength", "value": 0.8})

# Manual exposure + warm grade on the level's post process volume.
raw = text_of(call("editor_toolset.toolsets.object.ObjectTools", "get_properties",
                   {"instance": PPV, "properties": ["bUnbound", "Settings"]}))
try:
    ppv = json.loads(json.loads(raw)["returnValue"])
    s = ppv.get("Settings", {})
    s["bOverride_AutoExposureMethod"] = True
    s["AutoExposureMethod"] = "AEM_Manual"
    s["bOverride_AutoExposureBias"] = True
    s["AutoExposureBias"] = -3.0
    s["bOverride_BloomIntensity"] = True
    s["BloomIntensity"] = 0.5
    s["bOverride_VignetteIntensity"] = True
    s["VignetteIntensity"] = 0.35
    s["bOverride_WhiteTemp"] = True
    s["WhiteTemp"] = 5600.0
    s["bOverride_ColorGain"] = True
    s["ColorGain"] = {"X": 1.10, "Y": 1.0, "Z": 0.86, "W": 1.0}
    setp(PPV, {"bUnbound": True, "BlendWeight": 1.0, "Settings": s})
except Exception as exc:
    print("PPV patch failed", exc, raw[:500])

setp(SUN, {"Intensity": 2.0})
setp(SKY, {"Intensity": 0.5})
setp(HDRI_GEO, {"bVisible": False})
setp(HDRI_SKY, {"Intensity": 0.15})

pie = {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort",
                   "warmupSeconds": 8,
                   "startTransform": {"location": {"x": 3800, "y": 7000, "z": 250},
                                      "rotation": {"pitch": 0, "yaw": -90, "roll": 0},
                                      "scale": {"x": 1, "y": 1, "z": 1}}}}
call("EditorToolset.EditorAppToolset", "StartPIE", pie)
