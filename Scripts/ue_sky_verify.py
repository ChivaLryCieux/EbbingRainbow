import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_shot import connect, call

m = connect()
OBJ = "editor_toolset.toolsets.object.ObjectTools"
MI = "editor_toolset.toolsets.material_instance.MaterialInstanceTools"
BASE = "/Game/Battle/Maps/Lvl_Arena.Lvl_Arena:PersistentLevel."
SKY = BASE + "SkyLight_0.SkyLightComponent0"
DOME = BASE + "StaticMeshActor_0.StaticMeshComponent0"
PPV = BASE + "PostProcessVolume_1"
MIP = "/Game/Environment/Sky/MI_Sky_SoftSolid.MI_Sky_SoftSolid"


def get(ref, props):
    txt = call(m, OBJ, "get_properties", {"instance": {"refPath": ref}, "properties": props})
    try:
        return json.loads(json.loads(txt)["returnValue"])
    except Exception:
        return txt[:200]


print("SKYLIGHT", json.dumps(get(SKY, ["SourceType", "Cubemap", "Intensity",
                                       "bLowerHemisphereIsBlack", "Mobility", "LightColor"]),
                             ensure_ascii=False))
print("DOME", json.dumps(get(DOME, ["OverrideMaterials", "CastShadow", "bUseAsOccluder",
                                    "bAffectDistanceFieldLighting"]), ensure_ascii=False))

txt = call(m, OBJ, "get_properties", {"instance": {"refPath": PPV}, "properties": ["Settings"]})
s = json.loads(json.loads(txt)["returnValue"])["Settings"]
keys = ["autoExposureMethod", "autoExposureBias", "autoExposureApplyPhysicalCameraExposure",
        "bloomIntensity", "vignetteIntensity", "filmGrainIntensity",
        "colorSaturation", "colorContrast", "lumenSkylightLeaking", "tonemappingMethod"]
print("PPV", json.dumps({k: s.get(k) for k in keys}, ensure_ascii=False))

for name in ["ZenithColor", "HorizonColor"]:
    print(name, call(m, MI, "get_vector_parameter",
                     {"instance": {"refPath": MIP}, "name": name})[:200])
for name in ["SkyBrightness", "GradientPower"]:
    print(name, call(m, MI, "get_scalar_parameter",
                     {"instance": {"refPath": MIP}, "name": name})[:200])
