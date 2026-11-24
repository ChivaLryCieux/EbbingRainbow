import base64
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_call import MCP
from mcp_admin import text_of

PPV = "/Game/Battle/Maps/UEDPIE_0_Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.PostProcessVolume_1"
SUN = "/Game/Battle/Maps/UEDPIE_0_Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.DirectionalLight_0.LightComponent0"
SKY = "/Game/Battle/Maps/UEDPIE_0_Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.SkyLight_0.SkyLightComponent0"
HDRI_GEO = "/Game/Battle/Maps/UEDPIE_0_Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.HDRIBackdrop_C_1.Geometry"
HDRI_SKY = "/Game/Battle/Maps/UEDPIE_0_Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel.HDRIBackdrop_C_1.Skylight"

m = MCP()
assert m.init()


def call(ts, tool, args):
    r = m.call("call_tool", {"toolset_name": ts, "tool_name": tool, "arguments": args})
    print(tool, text_of(r)[:800])
    return r


def setp(obj, values):
    return call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
                {"instance": obj, "values": json.dumps(values)})


def getp(obj, props):
    return text_of(call("editor_toolset.toolsets.object.ObjectTools", "get_properties",
                        {"instance": obj, "properties": props}))


# --- post process: manual exposure, mild bloom + vignette ---
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
    setp(PPV, {"bUnbound": True, "Settings": settings})
except Exception as exc:
    print("PPV patch failed", exc, raw[:500])

# --- sun / sky ---
setp(SUN, {"Intensity": 2.5})
setp(SKY, {"Intensity": 0.6})

# --- hide the daylight HDRI sky dome, keep only a dim skylight ---
setp(HDRI_GEO, {"bVisible": False})
setp(HDRI_SKY, {"Intensity": 0.2})

# capture
res = call("EditorToolset.EditorAppToolset", "CaptureEditorImage", {})
try:
    data = json.loads(text_of(res))["returnValue"]["data"]
    out = r"D:\EbbingRainbow\Saved\Screenshots\arch_dark.png"
    open(out, "wb").write(base64.b64decode(data))
    print("WROTE", out)
except Exception as exc:
    print("capture failed", exc)
