# -*- coding: utf-8 -*-
"""把 LGT_MWAM_Grass 的变体替换为 Fishermans_Cabin 的 Grass(7) + Flower(3)。

- 以原有第 0 个变体为模板继承 use_grid / 随机旋转 / 贴地对齐等设置, 只改 mesh/密度/缩放/剔除。
- 幂等: 每次都整表重建 grass_varieties。
- 密度单位: 每 10m x 10m (1000x1000 cm) 的实例数; Fishermans 草是整丛大簇, 密度远低于原 MWAM 草叶。

用法:
  "D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" D:\\EbbingRainbow\\EbbingRainbow.uproject ^
      -ExecutePythonScript="D:\\EbbingRainbow\\Scripts\\grass_apply_fishermans.py" -unattended -nosplash
"""

import os
import stat
import unreal

LGT_PATH = "/Game/MWLandscapeAutoMaterial/Procedurals/LGT_MWAM_Grass"
GRASS_DIR = "/Game/Fishermans_Cabin/Meshes/Foliage/Grass/"
FLOWER_DIR = "/Game/Fishermans_Cabin/Meshes/Foliage/Flower/"

# (mesh 路径, 密度, 缩放下限, 缩放上限, 近裁距离, 远裁距离, 分类说明)
VARIETIES = [
    (GRASS_DIR + "SM_Grass_01", 30.0, 0.7, 1.3, 2500, 7000),
    (GRASS_DIR + "SM_Grass_02", 30.0, 0.7, 1.3, 2500, 7000),
    (GRASS_DIR + "SM_Grass_03", 26.0, 0.7, 1.3, 2500, 7000),
    (GRASS_DIR + "SM_Grass_04", 26.0, 0.7, 1.3, 2500, 7000),
    (GRASS_DIR + "SM_Grass_05", 20.0, 0.8, 1.4, 2500, 7000),
    (GRASS_DIR + "SM_Grass_06", 40.0, 0.8, 1.6, 2500, 6000),   # 贴地平片, 铺底
    (GRASS_DIR + "SM_Grass_07", 30.0, 0.8, 1.5, 2500, 6000),   # 小簇
    (FLOWER_DIR + "SM_Flower_01", 5.0, 0.8, 1.5, 1500, 4500),
    (FLOWER_DIR + "SM_Flower_02", 5.0, 0.8, 1.5, 1500, 4500),
    (FLOWER_DIR + "SM_Flower_03", 5.0, 0.8, 1.5, 1500, 4500),
]


def log(m):
    unreal.log("[GrassApply] " + m)


def clear_readonly(asset_path):
    fs = unreal.SystemLibrary.get_project_directory() + "Content" + asset_path[len("/Game"):] + ".uasset"
    if os.path.exists(fs) and not os.access(fs, os.W_OK):
        os.chmod(fs, stat.S_IWRITE | stat.S_IREAD)
        log("cleared read-only: " + fs)


def per_platform_float(v):
    p = unreal.PerPlatformFloat()
    p.set_editor_property("default", v)
    return p


def per_platform_int(v):
    p = unreal.PerPlatformInt()
    p.set_editor_property("default", v)
    return p


def interval(lo, hi):
    i = unreal.FloatInterval()
    i.set_editor_property("min", lo)
    i.set_editor_property("max", hi)
    return i


def main():
    clear_readonly(LGT_PATH)
    lgt = unreal.EditorAssetLibrary.load_asset(LGT_PATH)
    if not lgt:
        log("ERROR: cannot load " + LGT_PATH)
        return

    old = list(lgt.get_editor_property("grass_varieties"))
    if not old:
        log("ERROR: template variety missing")
        return
    tpl = old[0]

    new = []
    for path, dens, smin, smax, cull_start, cull_end in VARIETIES:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if not mesh:
            log("ERROR: mesh missing " + path)
            return
        v = unreal.GrassVariety()
        # 继承模板的通用设置 (网格放置/旋转/阴影等), 再覆盖本变体特有项
        for prop in ("use_grid", "placement_jitter", "min_lod", "scaling", "random_rotation",
                     "align_to_surface", "use_landscape_lightmap", "receives_decals",
                     "cast_dynamic_shadow", "shadow_cache_invalidation_behavior"):
            try:
                v.set_editor_property(prop, tpl.get_editor_property(prop))
            except Exception as e:
                log("  skip prop %s: %s" % (prop, e))
        v.set_editor_property("grass_mesh", mesh)
        v.set_editor_property("grass_density", per_platform_float(dens))
        v.set_editor_property("placement_jitter", 1.0)
        v.set_editor_property("start_cull_distance", per_platform_int(cull_start))
        v.set_editor_property("end_cull_distance", per_platform_int(cull_end))
        for ax in ("scale_x", "scale_y", "scale_z"):
            v.set_editor_property(ax, interval(smin, smax))
        new.append(v)

    lgt.set_editor_property("grass_varieties", new)
    ok = unreal.EditorAssetLibrary.save_asset(LGT_PATH, only_if_is_dirty=False)
    log("saved %s -> %s (%d varieties)" % (LGT_PATH, ok, len(new)))

    chk = unreal.EditorAssetLibrary.load_asset(LGT_PATH).get_editor_property("grass_varieties")
    for i, v in enumerate(chk):
        log("  [%d] %s density=%s cull=%s..%s" % (
            i, v.get_editor_property("grass_mesh").get_name(),
            v.get_editor_property("grass_density").default,
            v.get_editor_property("start_cull_distance").default,
            v.get_editor_property("end_cull_distance").default))


main()
