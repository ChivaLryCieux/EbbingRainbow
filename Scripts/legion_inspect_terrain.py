# -*- coding: utf-8 -*-
"""检查 Lvl_Legion 里的地形/平台 actor: 类、位置、包围盒、碰撞档位。
配合排查兵团贴地 trace 打不到地面的问题。"""

import unreal

LEVEL_PATH = "/Game/Battle/Maps/Lvl_Legion"


def log(m):
    unreal.log("[TerrainInspect] " + m)


def main():
    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            cls = a.get_class().get_name()
            loc = a.get_actor_location()
            scl = a.get_actor_scale3d()
            origin, extent = a.get_actor_bounds(False)
            log("actor: %s | class=%s | loc=(%.0f,%.0f,%.0f) | scale=(%.2f,%.2f,%.2f) | bounds extent=(%.0f,%.0f,%.0f)"
                % (a.get_actor_label(), cls, loc.x, loc.y, loc.z, scl.x, scl.y, scl.z, extent.x, extent.y, extent.z))
        except Exception as exc:
            log("actor err: %s" % exc)


main()
