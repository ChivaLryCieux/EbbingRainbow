# -*- coding: utf-8 -*-
"""创建实验性军团作战关卡 /Game/Battle/Maps/Lvl_Legion。

内容:
  - WorldSettings.DefaultGameMode = LegionGameMode
  - 大平台 (240m x 240m 混凝土色平台块, 中心 z=0)
  - PlayerStart 原点
  - 基础光照: DirectionalLight + SkyLight + SkyAtmosphere + ExponentialHeightFog
    (氛围只给能看的默认值, 精修归用户手调 —— 与关卡自调约定一致)

用法:
  "D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor.exe" D:\\EbbingRainbow\\EbbingRainbow.uproject ^
      -ExecutePythonScript="D:\\EbbingRainbow\\Scripts\\legion_make_level.py" -unattended -nosplash

幂等: 已存在同名列会复用并补齐缺失项, 可反复跑。
"""

import unreal

LEVEL_PATH = "/Game/Battle/Maps/Lvl_Legion"


def log(msg):
    unreal.log("[LegionLevel] " + msg)


def log_warn(msg):
    unreal.log_warning("[LegionLevel] " + msg)


def find_world_settings():
    # get_all_level_actors() 不含 WorldSettings, 走 world.get_world_settings()
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world:
            return world.get_world_settings()
    except Exception:
        pass
    return None


def find_actor_by_label(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            if actor.get_actor_label() == label:
                return actor
        except Exception:
            continue
    return None


def set_gamemode():
    ws = find_world_settings()
    if ws is None:
        log_warn("WorldSettings not found")
        return
    gm = unreal.load_class(None, "/Script/EbbingRainbow.LegionGameMode")
    if gm is None:
        log_warn("cannot load /Script/EbbingRainbow.LegionGameMode (C++ not compiled?)")
        return
    try:
        ws.set_editor_property("default_game_mode", gm)
        log("WorldSettings.DefaultGameMode = LegionGameMode")
    except Exception as exc:
        log_warn("set default_game_mode failed: %s" % exc)


def ensure_player_start():
    if find_actor_by_label("LegionPlayerStart"):
        return
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0.0, 0.0, 200.0), unreal.Rotator())
    if a:
        a.set_actor_label("LegionPlayerStart")
        log("spawned PlayerStart")


def ensure_platform():
    """240x240x2m 大平台, 顶面 z=0; BasicShapeMaterial 混凝土白。"""
    if find_actor_by_label("LegionPlatform"):
        return
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, -100.0), unreal.Rotator())
    if not a:
        log_warn("platform spawn failed")
        return
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    smc = a.static_mesh_component
    smc.set_editor_property("static_mesh", cube)
    a.set_actor_scale3d(unreal.Vector(240.0, 240.0, 2.0))
    try:
        smc.set_collision_profile_name("BlockAll")
    except Exception:
        pass
    mat = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    if mat:
        smc.set_material(0, mat)
    a.set_actor_label("LegionPlatform")
    a.set_mobility(unreal.ComponentMobility.STATIC)
    log("spawned LegionPlatform 240x240m")


def ensure_lighting():
    """够用的默认三点: 低角度暖阳主光 + 天光 + 大气 + 轻雾。"""
    if not find_actor_by_label("LegionSun"):
        sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(-52.0, -28.0, 0.0))
        if sun:
            sun.set_actor_label("LegionSun")
            sun.light_component.set_editor_property("intensity", 4.0)
            sun.light_component.set_editor_property(
                "light_color", unreal.Color(255, 226, 190, 255))
            sun.light_component.set_editor_property("cast_shadows", True)
            log("spawned LegionSun")

    if not find_actor_by_label("LegionSkyLight"):
        sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyLight, unreal.Vector(0, 0, 3000), unreal.Rotator())
        if sky:
            sky.set_actor_label("LegionSkyLight")
            sky.light_component.set_editor_property("intensity", 0.6)
            log("spawned LegionSkyLight")

    if not find_actor_by_label("LegionAtmo"):
        atmo = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator())
        if atmo:
            atmo.set_actor_label("LegionAtmo")
            log("spawned LegionAtmo")

    if not find_actor_by_label("LegionFog"):
        fog = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.ExponentialHeightFog, unreal.Vector(0, 0, 600), unreal.Rotator())
        if fog:
            fog.set_actor_label("LegionFog")
            fog.component.set_editor_property("fog_density", 0.012)
            fog.component.set_editor_property("fog_height_falloff", 0.35)
            log("spawned LegionFog")


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        log("level exists: %s" % LEVEL_PATH)
        unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    else:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        ok = les.new_level(LEVEL_PATH)
        log("new_level(%s) -> %s" % (LEVEL_PATH, ok))
        if not ok:
            log_warn("new_level failed")
            return

    set_gamemode()
    ensure_player_start()
    ensure_platform()
    ensure_lighting()

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.save_current_level()
    log("saved %s" % LEVEL_PATH)

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        log("actor: %s (%s)" % (actor.get_actor_label(), actor.get_class().get_name()))


main()
