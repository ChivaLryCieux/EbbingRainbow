import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_shot import connect, call, capture, stats

m = connect()
OBJ = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"
APP = "EditorToolset.EditorAppToolset"
BASE = "/Game/Battle/Maps/Lvl_ConcreteArena.Lvl_ConcreteArena:PersistentLevel."
SKY = BASE + "SkyLight_0.SkyLightComponent0"
PPV = BASE + "PostProcessVolume_1"

# final skylight settings in the editor world
print("skylight", call(m, OBJ, "set_properties", {"instance": {"refPath": SKY}, "values": json.dumps({
    "SourceType": "SLS_SpecifiedCubemap",
    "Cubemap": {"refPath": "/Game/Environment/Sky/T_SkyAmbient.T_SkyAmbient"},
    "bRealTimeCapture": False,
    "Mobility": "Movable",
    "Intensity": 2.0,
    "bLowerHemisphereIsBlack": False,
    "IndirectLightingIntensity": 1.0,
    "LightColor": {"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0},
})})[:200])

# final post process: stable manual exposure, soft bloom, light grain
txt = call(m, OBJ, "get_properties", {"instance": {"refPath": PPV}, "properties": ["Settings"]})
s = json.loads(json.loads(txt)["returnValue"])["Settings"]
s.update({
    "bOverride_AutoExposureMethod": True, "autoExposureMethod": "AEM_Manual",
    "bOverride_AutoExposureBias": True, "autoExposureBias": 0.0,
    "bOverride_AutoExposureApplyPhysicalCameraExposure": True,
    "autoExposureApplyPhysicalCameraExposure": False,
    "bOverride_BloomIntensity": True, "bloomIntensity": 0.35,
    "bOverride_VignetteIntensity": True, "vignetteIntensity": 0.30,
    "bOverride_FilmGrainIntensity": True, "filmGrainIntensity": 0.15,
    "bOverride_ColorSaturation": True, "colorSaturation": {"x": 1.05, "y": 1.05, "z": 1.08, "w": 1},
    "bOverride_ColorContrast": True, "colorContrast": {"x": 1.05, "y": 1.05, "z": 1.05, "w": 1},
    "bOverride_LumenSkylightLeaking": True, "lumenSkylightLeaking": 0.2,
})
print("ppv", call(m, OBJ, "set_properties", {"instance": {"refPath": PPV},
                                             "values": json.dumps({"Settings": s})})[:200])

print("save assets", call(m, ASSET, "save_assets", {"asset_paths": [
    "/Game/Environment/Sky/M_Sky_SoftSolid",
    "/Game/Environment/Sky/MI_Sky_SoftSolid",
    "/Game/Environment/Sky/T_SkyAmbient",
    "/Game/Battle/Maps/Lvl_ConcreteArena",
]})[:300])
