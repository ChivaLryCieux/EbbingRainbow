# -*- coding: utf-8 -*-
"""检查 Lvl_Legion 内容: actor 列表 + DefaultGameMode。"""
import unreal

LEVEL_PATH = "/Game/Battle/Maps/Lvl_Legion"

unreal.log_warning("[Check] exists=%s" % unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH))
unreal.EditorLevelLibrary.load_level(LEVEL_PATH)

for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    try:
        unreal.log_warning("[Check] actor: %s (%s)" % (actor.get_actor_label(), actor.get_class().get_name()))
    except Exception as e:
        unreal.log_warning("[Check] actor err: %s" % e)

world = unreal.EditorLevelLibrary.get_editor_world()
ws = world.get_world_settings()
gm = ws.get_editor_property("default_game_mode")
unreal.log_warning("[Check] default_game_mode=%s" % gm)
