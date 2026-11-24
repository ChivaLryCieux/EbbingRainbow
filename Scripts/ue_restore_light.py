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

raw = text_of(call("editor_toolset.toolsets.object.ObjectTools", "get_properties",
                   {"instance": PPV, "properties": ["bUnbound", "Settings"]}))
try:
    ppv = json.loads(json.loads(raw)["returnValue"])
    s = ppv.get("Settings", {})
    s["bOverride_AutoExposureMethod"] = True
    s["AutoExposureMethod"] = "AEM_Histogram"
    s["bOverride_AutoExposureBias"] = True
    s["AutoExposureBias"] = 0.0
    s["bOverride_AutoExposureMinBrightness"] = False
    s["bOverride_AutoExposureMaxBrightness"] = False
    s["bOverride_WhiteTemp"] = False
    s["bOverride_ColorGain"] = False
    s["bOverride_BloomIntensity"] = False
    s["bOverride_VignetteIntensity"] = False
    setp(PPV, {"bUnbound": True, "BlendWeight": 1.0, "Settings": s})
except Exception as exc:
    print("PPV restore failed", exc, raw[:400])

setp(SUN, {"Intensity": 10.0})
setp(SKY, {"Intensity": 10.0})
setp(HDRI_GEO, {"bVisible": True})
setp(HDRI_SKY, {"Intensity": 1.0})
call("editor_toolset.toolsets.material_instance.MaterialInstanceTools",
     "set_scalar_parameter",
     {"instance": MI_W, "name": "EmissiveStrength", "value": 3.0})
call("editor_toolset.toolsets.material_instance.MaterialInstanceTools",
     "set_vector_parameter",
     {"instance": MI_W, "name": "EmissiveColor",
      "value": {"r": 1.0, "g": 0.86, "b": 0.62, "a": 1.0}})
