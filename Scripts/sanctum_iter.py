#!/usr/bin/env python3
"""Iterate on Lvl_Sanctum look: replace SkyAtmosphere with soft sky dome."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_shot import connect, call, capture

m = connect()
SCENE = "editor_toolset.toolsets.scene.SceneTools"
ACTOR = "editor_toolset.toolsets.actor.ActorTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"


def setp(ref, values):
    return call(m, OBJ, "set_properties", {"instance": ref, "values": json.dumps(values)})


def comp0(actor_ref):
    return json.loads(call(m, ACTOR, "get_components", {"actor": {"refPath": actor_ref}}))["returnValue"][0]["refPath"]


def find(name):
    r = call(m, SCENE, "find_actors", {"name": name, "tag": "", "collision_channels": []})
    return json.loads(r)["returnValue"]


def find_one(name):
    for a in find(name):
        return a["refPath"]
    return None


# 1) remove SkyAtmosphere -> maroon sky came from it
atm = find_one("SkyAtmosphere")
if atm:
    print("del atm", call(m, SCENE, "remove_from_scene", {"actor": {"refPath": atm}})[:120])

# 2) soft sky dome
xf = {"location": {"x": 0, "y": 3000, "z": -3000},
      "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
      "scale": {"x": 600, "y": 600, "z": 400}}
r = call(m, SCENE, "add_to_scene_from_asset",
         {"asset_path": "/Engine/MapTemplates/Sky/SM_SkySphere", "name": "SkyDome", "xform": xf})
dome = json.loads(r)["returnValue"]
setp(comp0(dome["refPath"]), {
    "OverrideMaterials": [{"refPath": "/Game/Environment/Sky/MI_Sky_SoftSolid.MI_Sky_SoftSolid"}],
    "CastShadow": False, "bCastDynamicShadow": False,
    "bUseAsOccluder": False})
call(m, SCENE, "set_actor_folder", {"actor": dome, "folder_path": "Sanctum"})
print("dome", dome["refPath"])

# 3) sea -> dark concrete (glass speckled under lumen)
sea = find_one("Sea")
if sea:
    setp(comp0(sea), {"OverrideMaterials": [{"refPath": "/Game/Materials/Architecture/MI_Concrete_1.MI_Concrete_1"}]})

# 4) fog lighter + cooler
fog = find_one("ExponentialHeightFog")
setp(comp0(fog), {
    "FogDensity": 0.0012, "FogHeightFalloff": 0.2,
    "FogInscatteringColor": {"r": 0.82, "g": 0.84, "b": 0.86, "a": 1.0},
    "bEnableVolumetricFog": True, "VolumetricFogScatteringDistribution": 0.85,
    "StartDistance": 3500.0})

# 5) skylight recapture
sky = find_one("SkyLight")
setp(comp0(sky), {"Intensity": 0.9, "bRealTimeCapture": True})

print("done")
