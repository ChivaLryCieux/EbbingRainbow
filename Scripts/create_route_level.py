"""在编辑器内创建 /Game/Battle/Maps/Lvl_Route 并设置默认 GameMode。

用法（编辑器启动时执行）：
  UnrealEditor.exe EbbingRainbow.uproject -ExecutePythonScript="D:/EbbingRainbow/Scripts/create_route_level.py"

也可以从编辑器 Python 控制台执行。
"""

import unreal

LEVEL_PATH = "/Game/Battle/Maps/Lvl_Route"


def log(msg):
    unreal.log("[RouteLevel] " + msg)


def log_warn(msg):
    unreal.log_warning("[RouteLevel] " + msg)


def find_world_settings():
    """WorldSettings 是关卡里的一个 Actor，直接按类名找。"""
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            if actor.get_class().get_name() == "WorldSettings":
                return actor
        except Exception:
            continue
    return None


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        log("level already exists: %s" % LEVEL_PATH)
        unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    else:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        ok = les.new_level(LEVEL_PATH)
        log("new_level(%s) -> %s" % (LEVEL_PATH, ok))
        if not ok:
            log_warn("new_level failed")
            return

    # 默认 GameMode：让直接 PIE Lvl_Route 也能跑起来
    ws = find_world_settings()
    if ws is None:
        log_warn("WorldSettings not found; PIE 时需要手动设置 GameMode")
    else:
        try:
            ws.set_editor_property("default_game_mode", unreal.RouteGameMode)
            log("WorldSettings.DefaultGameMode = RouteGameMode")
        except Exception as exc:
            log_warn("set default_game_mode failed: %s" % exc)

    # 出生点（相机 Pawn 用）
    has_start = False
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if isinstance(actor, unreal.PlayerStart):
            has_start = True
            break
    if not has_start:
        unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        log("spawned PlayerStart at origin")

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.save_current_level()
    log("saved %s" % LEVEL_PATH)

    # 诊断输出，便于确认关卡内容
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        log("actor: %s (%s)" % (actor.get_name(), actor.get_class().get_name()))


main()
