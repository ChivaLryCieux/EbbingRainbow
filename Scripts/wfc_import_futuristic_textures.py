"""Import / re-import the Futuristic texture set into /Game/Futuristic.

Reads every PNG in Content/Futuristic and applies the right sampler setup:
  *_BC, *_E  -> sRGB colour, default compression
  *_N        -> normal map, sRGB off
  *_ORM      -> packed mask (R=AO, G=Roughness, B=Metallic), sRGB off, TC_Masks

Run headless (editor must be closed):
  UnrealEditor-Cmd.exe EbbingRainbow.uproject ^
      -ExecutePythonScript="D:\\EbbingRainbow\\Scripts\\wfc_import_futuristic_textures.py" ^
      -unattended -nosplash -nullrhi -stdout

Follow up with wfc_apply_futuristic_materials.py to re-link the material instances.
"""
import json
import os

import unreal

ROOT = r"D:\EbbingRainbow"
SRC = os.path.join(ROOT, "Content", "Futuristic")
DST = "/Game/Futuristic"
REPORT = os.path.join(ROOT, "Scripts", "wfc_futuristic_import.json")

report = {"imported": [], "failed": [], "textures": {}}


def log(msg):
    unreal.log("[WFC-TEX] " + str(msg))


def main():
    files = sorted(f for f in os.listdir(SRC) if f.lower().endswith(".png"))
    tasks = []
    for f in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SRC, f))
        task.set_editor_property("destination_path", DST)
        task.set_editor_property("destination_name", os.path.splitext(f)[0])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    for f in files:
        name = os.path.splitext(f)[0]
        tex = unreal.EditorAssetLibrary.load_asset("%s/%s" % (DST, name))
        if not tex:
            report["failed"].append(name)
            continue
        try:
            if name.endswith("_N"):
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings",
                                        unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
            elif name.endswith("_ORM"):
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings",
                                        unreal.TextureCompressionSettings.TC_MASKS)
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            else:
                tex.set_editor_property("srgb", True)
                tex.set_editor_property("compression_settings",
                                        unreal.TextureCompressionSettings.TC_DEFAULT)
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            tex.set_editor_property("never_stream", False)
        except Exception as exc:
            report["failed"].append("%s: %s" % (name, exc))
        unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
        report["imported"].append(name)
        report["textures"][name] = {
            "srgb": bool(tex.get_editor_property("srgb")),
            "compression": str(tex.get_editor_property("compression_settings")),
        }

    with open(REPORT, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    log("imported=%d failed=%d report=%s" % (len(report["imported"]),
                                             len(report["failed"]), REPORT))


main()
