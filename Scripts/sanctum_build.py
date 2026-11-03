#!/usr/bin/env python3
"""Build Lvl_Sanctum: monumental marble sanctuary over a mirror-void.

Palette discipline: pale marble mass, dark concrete accents, ONE emissive
fissure inside the portal. Composition: processional axis (south->north),
rift cut + thin bridge, monumental stair, colossal portal, god-object in fog.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_shot import connect, call, capture, stats

LVL = "/Game/Battle/Maps/Lvl_Sanctum.Lvl_Sanctum:PersistentLevel"
MARBLE = "/Game/Materials/Architecture/MI_Marble_1.MI_Marble_1"
CONCRETE = "/Game/Materials/Architecture/MI_Concrete_1.MI_Concrete_1"
GLASS = "/Game/AdvancedRealisticGlass/Materials/MI_GlassClean_01.MI_GlassClean_01"
EMIT = "/Game/WFCKit/Materials/MI_WFC_Emit_W.MI_WFC_Emit_W"
CUBE = "/Engine/BasicShapes/Cube"
KIT = "/Game/FBX_Export"

m = connect()
SCENE = "editor_toolset.toolsets.scene.SceneTools"
ACTOR = "editor_toolset.toolsets.actor.ActorTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"


def xf(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    return {"location": {"x": loc[0], "y": loc[1], "z": loc[2]},
            "rotation": {"pitch": rot[0], "yaw": rot[1], "roll": rot[2]},
            "scale": {"x": scale[0], "y": scale[1], "z": scale[2]}}


def spawn(asset, name, loc, rot=(0, 0, 0), scale=(1, 1, 1), mat=None, folder="Sanctum"):
    r = call(m, SCENE, "add_to_scene_from_asset",
             {"asset_path": asset, "name": name, "xform": xf(loc, rot, scale)})
    try:
        actor = json.loads(r)["returnValue"]
    except Exception:
        print("SPAWN FAIL", name, r[:200])
        return None
    ref = actor["refPath"]
    comps = json.loads(call(m, ACTOR, "get_components", {"actor": actor}))["returnValue"]
    if mat:
        call(m, OBJ, "set_properties", {"instance": comps[0]["refPath"],
             "values": json.dumps({"OverrideMaterials": [{"refPath": mat}]})})
    if folder:
        call(m, SCENE, "set_actor_folder", {"actor": actor, "folder_path": folder})
    return ref


def cube(name, loc, scale, mat=MARBLE, rot=(0, 0, 0)):
    return spawn(CUBE, name, loc, rot, scale, mat)


def kit(mesh, name, loc, rot=(0, 0, 0), scale=(1, 1, 1), mat=MARBLE):
    return spawn(f"{KIT}/{mesh}", name, loc, rot, scale, mat)


def setp(ref, values):
    return call(m, OBJ, "set_properties", {"instance": ref, "values": json.dumps(values)})


def cleanup():
    """Delete inherited mesh actors + brush, keep env/volumes."""
    r = call(m, SCENE, "find_actors", {"name": "", "tag": "", "collision_channels": []})
    actors = json.loads(r)["returnValue"]
    n = 0
    for a in actors:
        path = a["refPath"]
        short = path.rsplit(".", 1)[-1]
        if short.startswith(("StaticMeshActor", "Brush_")):
            call(m, SCENE, "remove_from_scene", {"actor": a})
            n += 1
    print("cleanup removed", n)


def build():
    # ---------- masses ----------
    # south platform (top z=0): 48m x 26m x 12m thick
    cube("Slab_South", (0, -1500, -600), (48, 26, 12))
    # north lower court (top z=0): 48m x 6m
    cube("Slab_NorthLow", (0, 1100, -600), (48, 6, 12))
    # north terrace (top z=400): 48m x 14m
    cube("Slab_Terrace", (0, 2100, -200), (48, 14, 12))
    # rift: gap y in [-200, 800]
    # thin bridge across the rift at z=0, 8m wide, 80cm slab
    cube("Bridge", (0, 300, -40), (8, 12, 0.8))
    # monumental stair: 4 steps, 28m wide, 1m rise each, 0.75m tread
    for i in range(4):
        cube(f"Step_{i}", (0, 1475 + i * 75, i * 100 + 50), (28, 0.75, 1))
    # portal pylons: 4m x 3m x 24m, on terrace z=400
    for sgn, x in (("L", -700), ("R", 700)):
        cube(f"PortalPylon_{sgn}", (x, 2750, 1600), (4, 3, 24))
        # triangular fin on outer face, 24m tall
        kit("SM_Deco_Triangular_Fin_01", f"PortalFin_{sgn}",
            (x + (-1 if sgn == "L" else 1) * 210, 2750, 400),
            scale=(30, 4, 12), mat=CONCRETE)
    # portal lintel 18m x 4m x 3m on top (pylon tops at z=2800)
    cube("PortalLintel", (0, 2750, 2950), (18, 4, 3))
    # light fissure: thin emissive blade floating in the gap
    cube("Fissure", (0, 2750, 1600), (0.15, 0.5, 17), mat=EMIT)
    # columns: south court x=+-1700, scale 6 -> 12m tall, ~6m dia
    col_x = (-1700, 1700)
    for y in (-2400, -1600, -800):
        for x in col_x:
            kit("SM_Column_Octagonal_01", f"Col_S_{x}_{y}", (x, y, 0), scale=(6, 6, 6))
    # terrace colonnade
    for y in (1700, 2300):
        for x in col_x:
            kit("SM_Column_Octagonal_01", f"Col_N_{x}_{y}", (x, y, 400), scale=(6, 6, 6))
    # ---------- god objects in fog (no cast shadow: they float over the sea) ----------
    arch = kit("SM_Deco_Pyramid_Arch_01", "GodArch", (0, 9000, 400),
               rot=(0, 0, 8), scale=(45, 45, 45))
    shard = kit("SM_Deco_Pyramid_Tetrahedron_01", "GodShard",
                (-14000, 26000, 11000), rot=(0, -30, 12), scale=(20, 20, 20))
    for ref in (arch, shard):
        if ref:
            comps = json.loads(call(m, ACTOR, "get_components",
                                    {"actor": {"refPath": ref}}))["returnValue"]
            for c in comps:
                if "Sprite" not in c["refPath"]:
                    setp(c["refPath"], {"CastShadow": False, "bCastDynamicShadow": False})
    # mirror-sea far below
    cube("Sea", (0, 3000, -3300), (400, 400, 20), mat=GLASS)
    # scattered ruins: only tips break the sea surface
    ruins = [(-5200, 4200, -2400, (0, 0, 12), 9),
             (5800, -1800, -2600, (0, 0, -20), 8),
             (-6800, -3400, -2800, (6, 0, 30), 10),
             (6600, 5600, -3000, (0, -8, 0), 11)]
    for i, (x, y, z, rot, s) in enumerate(ruins):
        kit("SM_Column_Octagonal_01", f"Ruin_{i}", (x, y, z), rot=rot, scale=(s, s, s))
    print("build done")


def lighting():
    """Sun raking north->south through the portal; warm god-light."""
    r = call(m, SCENE, "add_to_scene_from_class",
             {"actor_type": {"refPath": "/Script/Engine.DirectionalLight"},
              "name": "Sun", "xform": xf((0, 4000, 3000), (-30, -155, 0))})
    print("sun", r[:200])
    sun = json.loads(r)["returnValue"]["refPath"]
    comps = json.loads(call(m, ACTOR, "get_components", {"actor": {"refPath": sun}}))["returnValue"]
    for c in comps:
        if "LightComponent" in c["refPath"]:
            setp(c["refPath"], {
                "Intensity": 5.5,
                "LightColor": {"r": 1.0, "g": 0.88, "b": 0.72, "a": 1.0},
                "bAtmosphereSunLight": True,
                "bCloudShadow": False})
    # fog: very thin, pale - tuned on the real HeightFogComponent (comps[0] is a Sprite!)
    r = call(m, SCENE, "find_actors", {"name": "ExponentialHeightFog", "tag": "", "collision_channels": []})
    fog = json.loads(r)["returnValue"][0]["refPath"]
    comps = json.loads(call(m, ACTOR, "get_components", {"actor": {"refPath": fog}}))["returnValue"]
    for c in comps:
        if "HeightFog" in c["refPath"]:
            setp(c["refPath"], {
                "FogDensity": 0.0006, "StartDistance": 4000.0, "FogHeightFalloff": 0.35,
                "FogInscatteringColor": {"r": 0.78, "g": 0.82, "b": 0.90, "a": 1.0},
                "bEnableVolumetricFog": True, "VolumetricFogScatteringDistribution": 0.8,
                "VolumetricFogAlbedo": {"r": 0.95, "g": 0.95, "b": 0.97, "a": 1.0}})
    # skylight: soft cool fill (skip Sprite/Billboard comps)
    r = call(m, SCENE, "find_actors", {"name": "SkyLight", "tag": "", "collision_channels": []})
    sky = json.loads(r)["returnValue"][0]["refPath"]
    comps = json.loads(call(m, ACTOR, "get_components", {"actor": {"refPath": sky}}))["returnValue"]
    for c in comps:
        if "SkyLightComponent" in c["refPath"]:
            setp(c["refPath"], {"Intensity": 0.85, "bRealTimeCapture": True,
                 "LightColor": {"r": 0.82, "g": 0.88, "b": 0.97, "a": 1.0}})
    # post process: manual exposure, gentle bloom, vignette, warm grade
    r = call(m, SCENE, "find_actors", {"name": "PostProcessVolume", "tag": "", "collision_channels": []})
    ppv = json.loads(r)["returnValue"][0]["refPath"]
    raw = call(m, OBJ, "get_properties", {"instance": ppv, "properties": ["bUnbound", "Settings"]})
    s = json.loads(json.loads(raw)["returnValue"]).get("Settings", {})
    s.update({
        "bOverride_AutoExposureMethod": True, "AutoExposureMethod": "AEM_Manual",
        "bOverride_AutoExposureBias": True, "AutoExposureBias": -1.2,
        "bOverride_BloomIntensity": True, "BloomIntensity": 0.55,
        "bOverride_VignetteIntensity": True, "VignetteIntensity": 0.3,
        "bOverride_WhiteTemp": True, "WhiteTemp": 5200.0,
        "bOverride_FilmGrainIntensity": True, "FilmGrainIntensity": 0.0})
    setp(ppv, {"bUnbound": True, "BlendWeight": 1.0, "Settings": s})
    print("lighting done")


def save():
    print(call(m, "editor_toolset.toolsets.asset.AssetTools", "save_assets",
               {"asset_paths": ["/Game/Battle/Maps/Lvl_Sanctum"]})[:300])


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    if which in ("clean", "all"):
        cleanup()
    if which in ("build", "all"):
        build()
    if which in ("light", "all"):
        lighting()
    if which in ("save", "all"):
        save()
