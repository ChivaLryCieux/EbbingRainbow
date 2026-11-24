# -*- coding: utf-8 -*-
"""只读: dump LGT_MWAM_Grass / Stones 的变体参数, 以及 Fishermans_Cabin Flower/Grass 网格的包围盒与材质。

用法:
  "D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" D:\\EbbingRainbow\\EbbingRainbow.uproject ^
      -ExecutePythonScript="D:\\EbbingRainbow\\Scripts\\grass_dump_lgt.py" -unattended -nosplash -stdout -FullStdOutLogOutput
"""

import unreal

LGT_PATHS = [
    "/Game/MWLandscapeAutoMaterial/Procedurals/LGT_MWAM_Grass",
    "/Game/MWLandscapeAutoMaterial/Procedurals/LGT_MWAM_Stones",
]
MESH_DIRS = [
    "/Game/Fishermans_Cabin/Meshes/Foliage/Grass",
    "/Game/Fishermans_Cabin/Meshes/Foliage/Flower",
]
PROPS = [
    "grass_mesh", "grass_density", "use_grid", "placement_jitter", "start_cull_distance",
    "end_cull_distance", "min_lod", "scaling", "scale_x", "scale_y", "scale_z",
    "random_rotation", "align_to_surface", "use_landscape_lightmap", "receives_decals",
    "cast_dynamic_shadow", "affects_distance_field_lighting", "shadow_cache_invalidation_behavior",
]


def log(m):
    unreal.log("[GrassDump] " + m)


def dump_lgt(path):
    lgt = unreal.EditorAssetLibrary.load_asset(path)
    if not lgt:
        log("MISSING " + path)
        return
    log("==== %s" % path)
    try:
        log("  grass_density_quality=%s" % lgt.get_editor_property("grass_density_quality"))
    except Exception as e:
        log("  (no grass_density_quality: %s)" % e)
    vs = lgt.get_editor_property("grass_varieties")
    log("  varieties=%d" % len(vs))
    for i, v in enumerate(vs):
        parts = []
        for p in PROPS:
            try:
                val = v.get_editor_property(p)
                if hasattr(val, "get_path_name"):
                    val = val.get_path_name()
                elif isinstance(val, unreal.PerPlatformFloat) or isinstance(val, unreal.PerPlatformInt):
                    val = val.default
                elif hasattr(val, "min") and hasattr(val, "max"):
                    val = "(%s..%s)" % (val.min, val.max)
                parts.append("%s=%s" % (p, val))
            except Exception as e:
                parts.append("%s=<err>" % p)
        log("  [%d] " % i + " | ".join(parts))


def dump_meshes(d):
    log("==== meshes in %s" % d)
    for a in unreal.EditorAssetLibrary.list_assets(d, recursive=False, include_folder=False):
        obj = unreal.EditorAssetLibrary.load_asset(a)
        if not isinstance(obj, unreal.StaticMesh):
            continue
        b = obj.get_bounds()
        ext = b.box_extent
        mats = [m.get_path_name() if m else None for m in [obj.get_material(i) for i in range(len(obj.static_materials))]]
        log("  %s | extent=(%.0f,%.0f,%.0f) | tris~ | mats=%s" % (obj.get_name(), ext.x, ext.y, ext.z, mats))


def main():
    for p in LGT_PATHS:
        dump_lgt(p)
    for d in MESH_DIRS:
        dump_meshes(d)


main()
