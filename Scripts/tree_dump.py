# -*- coding: utf-8 -*-
"""只读: dump Fishermans_Cabin 树/灌木网格的 Nanite / 碰撞 / 包围盒 / LOD / 材质, 以及 Lvl_Legion 里的 Landscape 信息。"""

import unreal

DIRS = ["/Game/Fishermans_Cabin/Meshes/Foliage/Tree", "/Game/Fishermans_Cabin/Meshes/Foliage/Bush"]


def log(m):
    unreal.log("[TreeDump] " + m)


def main():
    for d in DIRS:
        log("==== " + d)
        for a in unreal.EditorAssetLibrary.list_assets(d, recursive=False, include_folder=False):
            o = unreal.EditorAssetLibrary.load_asset(a)
            if not isinstance(o, unreal.StaticMesh):
                continue
            b = o.get_bounds()
            e = b.box_extent
            try:
                nan = o.get_editor_property("nanite_settings").enabled
            except Exception:
                nan = "?"
            try:
                cc = o.get_editor_property("body_setup").get_editor_property("collision_trace_flag")
            except Exception:
                cc = "?"
            try:
                nbody = len(o.get_editor_property("body_setup").agg_geom.convex_elems)
            except Exception:
                nbody = "?"
            lods = o.get_num_lods()
            tris = o.get_num_triangles(0) if hasattr(o, "get_num_triangles") else "?"
            log("  %s | extent=(%.0f,%.0f,%.0f) | nanite=%s | lods=%s | collision=%s convex=%s"
                % (o.get_name(), e.x, e.y, e.z, nan, lods, cc, nbody))

    unreal.EditorLevelLibrary.load_level("/Game/Battle/Maps/Lvl_Legion")
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        c = a.get_class().get_name()
        if "Landscape" in c:
            o, ex = a.get_actor_bounds(False)
            log("actor %s | %s | origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f)" % (a.get_actor_label(), c, o.x, o.y, o.z, ex.x, ex.y, ex.z))
            if c == "Landscape":
                log("  material=%s" % a.get_editor_property("landscape_material"))


main()
