"""Re-texture the WFC kit (Blackshard arena tiles) with the Futuristic texture set.

What it does, idempotently:
  1. makes M_WFC_TintedPBR fully parameter driven
       BaseColor = BaseColorTex * Tint
       Normal    = NormalTex
       ORM       = ORMTex  (R = AO, G = Roughness, B = Metallic)
  2. points every MI_WFC_* instance at one Futuristic BC/N/ORM set
  3. saves the master material and the instances, then dumps a report

The kit static meshes in /Game/WFCKit/Meshes keep their material slots, so every
tile the WFC generator instances (walls / floors / ceilings / props / stairs)
picks up the new look automatically - no C++ or blueprint change needed.

Run headless (editor must be closed):
  UnrealEditor-Cmd.exe EbbingRainbow.uproject ^
      -ExecutePythonScript="D:\\EbbingRainbow\\Scripts\\wfc_apply_futuristic_materials.py" ^
      -unattended -nosplash -nullrhi -stdout

Re-run it after replacing any PNG in Content/Futuristic: the texture assets are
re-imported by wfc_import_futuristic_textures.py, then this script re-links them.
"""
import json
import os
import stat

import unreal

ROOT = r"D:\EbbingRainbow"
MAT = "/Game/WFCKit/Materials"
TEX = "/Game/Futuristic"
REPORT = os.path.join(ROOT, "Scripts", "wfc_futuristic_report.json")


def make_writable(asset_path):
    """Clear the read-only flag so the editor can overwrite the .uasset."""
    local = os.path.join(ROOT, "Content", asset_path.replace("/Game/", "").replace("/", os.sep) + ".uasset")
    try:
        os.chmod(local, stat.S_IWRITE | stat.S_IREAD)
        return True
    except Exception as exc:
        unreal.log_warning("[WFC-MAT] cannot make %s writable: %s" % (local, exc))
        return False

# MI name -> (Futuristic texture base, tint)
MAPPING = {
    "MI_WFC_Wall_A": ("T_Wall_TechRecess", (1.0, 1.0, 1.0, 1.0)),
    "MI_WFC_Wall_B": ("T_Wall_PortalTechInset", (1.0, 1.0, 1.0, 1.0)),
    "MI_WFC_Wall_C": ("T_Wall_DarkStructuralMetal", (1.0, 1.0, 1.0, 1.0)),
    "MI_WFC_Wall_D": ("T_Wall_WarmSandstone", (1.0, 1.0, 1.0, 1.0)),
    "MI_WFC_Dark": ("T_Wall_DarkStructuralMetal", (0.55, 0.55, 0.60, 1.0)),
    "MI_WFC_Floor_A": ("T_Floor_LargeSlab", (1.0, 1.0, 1.0, 1.0)),
    "MI_WFC_Floor_B": ("T_Floor_DeckPlating", (1.0, 1.0, 1.0, 1.0)),
    "MI_WFC_Floor_C": ("T_Floor_TreadGrating", (1.0, 1.0, 1.0, 1.0)),
    "MI_WFC_Ceil": ("T_Ceiling_Panel", (1.0, 1.0, 1.0, 1.0)),
}

OUTPUTS = (("MP_BASE_COLOR", "BaseColor"), ("MP_NORMAL", "Normal"),
           ("MP_ROUGHNESS", "Roughness"), ("MP_METALLIC", "Metallic"),
           ("MP_AMBIENT_OCCLUSION", "AO"), ("MP_EMISSIVE_COLOR", "Emissive"))

mel = unreal.MaterialEditingLibrary
report = {"steps": [], "outputs": {}, "instances": {}, "errors": []}


def log(msg):
    unreal.log("[WFC-MAT] " + str(msg))


def safe(label, fn, *args):
    try:
        return fn(*args)
    except Exception as exc:
        report["errors"].append("%s: %s" % (label, exc))
        log("ERROR %s: %s" % (label, exc))
        return None


def tex(base, suffix):
    return unreal.EditorAssetLibrary.load_asset("%s/%s_%s" % (TEX, base, suffix))


def pname(expr):
    try:
        return str(expr.get_editor_property("parameter_name"))
    except Exception:
        return None


def describe(expr):
    if expr is None:
        return None
    d = {"class": expr.get_class().get_name(), "name": expr.get_name(), "param": pname(expr)}
    try:
        t = expr.get_editor_property("texture")
        if t:
            d["texture"] = t.get_path_name()
    except Exception:
        pass
    return d


def out_node(material, prop):
    return safe("read " + prop, mel.get_material_property_input_node,
                material, getattr(unreal.MaterialProperty, prop))


def find_param(material, name):
    for expr in safe("expressions", mel.get_material_expressions, material) or []:
        if pname(expr) == name:
            return expr
    return None


def make_param(material, name, sampler, default_tex, x, y):
    expr = mel.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("sampler_type", sampler)
    if default_tex:
        expr.set_editor_property("texture", default_tex)
    report["steps"].append("created %s parameter" % name)
    return expr


def ensure_base_color(material):
    """BaseColor = BaseColorTex * Tint."""
    node = out_node(material, "MP_BASE_COLOR")
    if node is None:
        report["errors"].append("BaseColor has no input")
        return
    param = find_param(material, "BaseColorTex")
    if param is None:
        param = make_param(material, "BaseColorTex",
                           unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                           tex("T_Wall_TechRecess", "BC"), -1100, 150)
    param.set_editor_property("texture", tex("T_Wall_TechRecess", "BC"))

    if node.get_class().get_name() != "MaterialExpressionMultiply":
        ok = safe("BaseColorTex->BaseColor", mel.connect_material_property,
                  param, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        report["steps"].append("BaseColorTex -> BaseColor (%s)" % ok)
    else:
        ok = safe("BaseColorTex->Multiply.A", mel.connect_material_expressions,
                  param, "RGB", node, "A")
        report["steps"].append("BaseColorTex -> Multiply.A (%s)" % ok)


def ensure_normal(material):
    node = out_node(material, "MP_NORMAL")
    if pname(node) == "NormalTex":
        report["steps"].append("NormalTex already wired")
        return
    param = find_param(material, "NormalTex")
    if param is None:
        param = make_param(material, "NormalTex",
                           unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                           tex("T_Wall_TechRecess", "N"), -1100, 450)
    param.set_editor_property("texture", tex("T_Wall_TechRecess", "N"))
    ok = safe("NormalTex->Normal", mel.connect_material_property,
              param, "RGB", unreal.MaterialProperty.MP_NORMAL)
    report["steps"].append("NormalTex -> Normal (%s)" % ok)
    if ok and node is not None:
        safe("delete plain normal", mel.delete_material_expression, material, node)
        report["steps"].append("removed the hard-coded normal sample")


def ensure_orm(material):
    """Roughness / Metallic / AO all come from one packed ORM map."""
    rough = out_node(material, "MP_ROUGHNESS")
    metal = out_node(material, "MP_METALLIC")
    ao = out_node(material, "MP_AMBIENT_OCCLUSION")
    if pname(rough) == pname(metal) == pname(ao) == "ORMTex":
        report["steps"].append("ORMTex already wired")
        return
    param = find_param(material, "ORMTex")
    if param is None:
        param = make_param(material, "ORMTex",
                           unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                           tex("T_Wall_TechRecess", "ORM"), -1100, 750)
    param.set_editor_property("texture", tex("T_Wall_TechRecess", "ORM"))
    links = {}
    for channel, prop in (("R", "MP_AMBIENT_OCCLUSION"), ("G", "MP_ROUGHNESS"),
                          ("B", "MP_METALLIC")):
        links[channel] = bool(safe("ORM." + channel, mel.connect_material_property,
                                   param, channel, getattr(unreal.MaterialProperty, prop)))
    report["steps"].append("ORM channels -> AO/Roughness/Metallic %s" % links)
    for old in (rough, metal):
        if old is not None and old is not param:
            safe("delete legacy sample", mel.delete_material_expression, material, old)


def apply_instance(name, base, tint):
    mi = unreal.EditorAssetLibrary.load_asset("%s/%s" % (MAT, name))
    if not mi:
        report["errors"].append("missing MI " + name)
        return
    for param, suffix in (("BaseColorTex", "BC"), ("NormalTex", "N"), ("ORMTex", "ORM")):
        t = tex(base, suffix)
        if not t:
            report["errors"].append("missing texture %s_%s" % (base, suffix))
            continue
        safe("%s.%s" % (name, param), mel.set_material_instance_texture_parameter_value,
             mi, param, t)
    safe("%s.Tint" % name, mel.set_material_instance_vector_parameter_value, mi, "Tint",
         unreal.LinearColor(tint[0], tint[1], tint[2], tint[3]))
    safe("%s.update" % name, mel.update_material_instance, mi)
    path = "%s/%s" % (MAT, name)
    make_writable(path)
    saved = safe("%s.save" % name, unreal.EditorAssetLibrary.save_asset, path, False)
    if not saved:
        saved = safe("%s.save_loaded" % name, unreal.EditorAssetLibrary.save_loaded_asset, mi, False)
    report["steps"].append("%s -> %s (saved=%s)" % (name, base, saved))


def read_instance(name):
    mi = unreal.EditorAssetLibrary.load_asset("%s/%s" % (MAT, name))
    entry = {"textures": {}, "vectors": {}}
    for v in mi.get_editor_property("texture_parameter_values"):
        p = str(v.get_editor_property("parameter_info").get_editor_property("name"))
        t = v.get_editor_property("parameter_value")
        entry["textures"][p] = t.get_path_name() if t else None
    for v in mi.get_editor_property("vector_parameter_values"):
        p = str(v.get_editor_property("parameter_info").get_editor_property("name"))
        c = v.get_editor_property("parameter_value")
        entry["vectors"][p] = [round(c.r, 3), round(c.g, 3), round(c.b, 3), round(c.a, 3)]
    return entry


def probe_links(material):
    """Best-effort connection dump (API differs between engine versions)."""
    out = {}
    get_input_node = getattr(mel, "get_input_node", None)
    for expr in safe("expressions", mel.get_material_expressions, material) or []:
        names = []
        try:
            names = [str(n) for n in mel.get_material_expression_input_names(material, expr)]
        except Exception:
            pass
        links = {}
        for n in names:
            node = None
            if get_input_node is not None:
                node = safe("get_input_node", get_input_node, material, expr, n)
            if node is not None:
                links[n] = "%s[%s]" % (node.get_name(), pname(node))
        if links:
            out[expr.get_name()] = links
    return out


def main():
    material = unreal.EditorAssetLibrary.load_asset("%s/M_WFC_TintedPBR" % MAT)
    if not material:
        report["errors"].append("M_WFC_TintedPBR missing")
    else:
        ensure_base_color(material)
        ensure_normal(material)
        ensure_orm(material)
        # drop nodes that no longer feed the output (the old Fab normal /
        # roughness / metallic samples), so nothing references the old pack
        safe("prune unused", mel.delete_unused_expressions, material)
        safe("recompile", mel.recompile_material, material)
        make_writable("%s/M_WFC_TintedPBR" % MAT)
        saved = safe("save master", unreal.EditorAssetLibrary.save_asset,
                     "%s/M_WFC_TintedPBR" % MAT, False)
        if not saved:
            saved = safe("save master loaded", unreal.EditorAssetLibrary.save_loaded_asset,
                         material, False)
        report["steps"].append("master saved=%s" % saved)
        for prop, label in OUTPUTS:
            report["outputs"][label] = describe(out_node(material, prop))
        report["links"] = probe_links(material)

    for name, (base, tint) in MAPPING.items():
        apply_instance(name, base, tint)

    for name in MAPPING:
        report["instances"][name] = safe("verify " + name, read_instance, name) or {}

    with open(REPORT, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    log("report -> %s" % REPORT)
    log("DONE steps=%d errors=%d" % (len(report["steps"]), len(report["errors"])))
    for e in report["errors"]:
        log("ERR " + e)


main()
