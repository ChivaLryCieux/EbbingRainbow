# -*- coding: utf-8 -*-
"""
build_painterly_postprocess.py
==============================

在编辑器里一键构建「厚涂手绘」后处理材质：

    /Game/Style/Painterly/M_PP_Painterly        后处理母材质（Custom 节点 + HLSL 引用）
    /Game/Style/Painterly/MI_PP_Painterly       默认参数实例（可直接拖进 PostProcessVolume）
    /Game/Style/Painterly/MI_PP_Painterly_Ink   偏「墨线」预设
    /Game/Style/Painterly/MI_PP_Painterly_Soft  偏「柔和湿画」预设

运行方式（三选一）：
  1) 编辑器 Output Log 的 Python 模式：
         exec(open(r"D:/EbbingRainbow/Scripts/Painterly/build_painterly_postprocess.py").read())
  2) 编辑器菜单 Tools > Execute Python Script... 选择本文件
  3) 命令行（无界面）：
         UnrealEditor-Cmd.exe D:/EbbingRainbow/EbbingRainbow.uproject -run=pythonscript
             -Script="D:/EbbingRainbow/Scripts/Painterly/build_painterly_postprocess.py"
             -unattended -nosplash -nop4

前置条件：
  * 工程已编译（Source/EbbingRainbow/EbbingRainbow.cpp 里注册了 /Project 着色器目录）
  * 已启用 PythonScriptPlugin（见 EbbingRainbow.uproject）
  * 着色器文件位于 D:/EbbingRainbow/Shaders/Painterly/
"""

import unreal

# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
PACKAGE_PATH = "/Game/Style/Painterly"
MATERIAL_NAME = "M_PP_Painterly"
SHADER_INCLUDE = "/Project/Painterly/PainterlyPostProcess.ush"

# (参数名, 默认值, 说明)
SCALAR_PARAMS = [
    ("P_BrushRadius",     8.0,   u"笔刷半径（像素）"),
    ("P_StrokeStretch",   0.65,  u"笔触拉伸 0..1"),
    ("P_Patchiness",      2.8,   u"色块硬朗度 0.5..8"),
    ("P_TensorRadius",    1.8,   u"方向场采样半径（像素）"),
    ("P_FlowFreq",        4.5,   u"平坦区流动频率"),
    ("P_FlowJitter",      0.28,  u"笔触方向抖动"),
    ("P_DepthScale",      1.30,  u"景深大笔触比例 0..3"),
    ("P_ImpastoStrength", 0.60,  u"堆料立体感 0..1"),
    ("P_ImpastoAngle",    3.927, u"假想光方向（弧度，屏幕空间）"),
    ("P_ImpastoScale",    1.10,  u"笔刷纹理密度"),
    ("P_BristleStrength", 0.45,  u"刷毛丝缕痕强度 0..1"),
    ("P_CanvasStrength",  0.30,  u"画布纹理 0..1"),
    ("P_InkStrength",     0.35,  u"枯笔飞白与暗缝强度 0..1"),
    ("P_InkWidth",        0.90,  u"边缘采样粗细"),
    ("P_QuantSteps",      14.0,  u"色阶数"),
    ("P_QuantStrength",   0.28,  u"色阶化强度 0..1"),
    ("P_ChromaVibe",      0.70,  u"色彩振荡与点彩提鲜 0..2"),
    ("P_Saturation",      1.10,  u"饱和度"),
    ("P_Warmth",          0.20,  u"冷暖偏移 -1..1"),
    ("P_Vignette",        0.12,  u"暗角 0..1"),
    ("P_DebugMode",       0.0,   u"调试视图 0..5"),
]

# Custom 节点输入引脚
PIN_SCENE_COLOR = "InSceneColor"
PIN_DEPTH = "InDepth"
PIN_NORMAL = "InNormal"

CUSTOM_CODE = u"""
FPaintParams P;
P.ScreenSize        = View.BufferSizeAndInvSize.xy;
P.BrushRadius       = P_BrushRadius;
P.StrokeStretch     = P_StrokeStretch;
P.Patchiness        = P_Patchiness;
P.TensorRadius      = P_TensorRadius;
P.FlowFreq          = P_FlowFreq;
P.FlowJitter        = P_FlowJitter;
P.DepthScale        = P_DepthScale;
P.ImpastoStrength   = P_ImpastoStrength;
P.ImpastoAngle      = P_ImpastoAngle;
P.ImpastoScale      = P_ImpastoScale;
P.BristleStrength   = P_BristleStrength;
P.CanvasStrength    = P_CanvasStrength;
P.InkStrength       = P_InkStrength;
P.InkWidth          = P_InkWidth;
P.QuantSteps        = P_QuantSteps;
P.QuantStrength     = P_QuantStrength;
P.ChromaVibe        = P_ChromaVibe;
P.Saturation        = P_Saturation;
P.Warmth            = P_Warmth;
P.Vignette          = P_Vignette;
P.DebugMode         = P_DebugMode;

float2 UV = GetDefaultSceneTextureUV(Parameters, 14);
return Paint_Main(UV, InSceneColor.rgb, P);
"""

PRESETS = {
    "MI_PP_Painterly_ConceptArt": {
        "P_BrushRadius": 9.0,
        "P_StrokeStretch": 0.70,
        "P_Patchiness": 3.2,
        "P_DepthScale": 1.6,
        "P_ImpastoStrength": 0.65,
        "P_BristleStrength": 0.50,
        "P_ChromaVibe": 0.85,
        "P_InkStrength": 0.25,
        "P_QuantSteps": 16.0,
        "P_QuantStrength": 0.25,
        "P_Saturation": 1.15,
        "P_CanvasStrength": 0.35,
    },
    "MI_PP_Painterly_Ink": {
        "P_BrushRadius": 5.0,
        "P_Patchiness": 3.6,
        "P_ImpastoStrength": 0.45,
        "P_InkStrength": 0.85,
        "P_InkWidth": 1.4,
        "P_QuantSteps": 8.0,
        "P_QuantStrength": 0.6,
        "P_Saturation": 1.12,
        "P_CanvasStrength": 0.2,
    },
    "MI_PP_Painterly_Soft": {
        "P_BrushRadius": 11.0,
        "P_StrokeStretch": 0.35,
        "P_Patchiness": 1.3,
        "P_ImpastoStrength": 0.40,
        "P_InkStrength": 0.20,
        "P_QuantSteps": 24.0,
        "P_QuantStrength": 0.12,
        "P_CanvasStrength": 0.45,
        "P_Vignette": 0.25,
    },
}


# ---------------------------------------------------------------------------
# 小工具
# ---------------------------------------------------------------------------
def log(msg):
    unreal.log(u"[Painterly] " + str(msg))


def warn(msg):
    unreal.log_warning(u"[Painterly] " + str(msg))


_ENUM_PREFIXES = ("blend", "bl", "md", "cmot", "cmci", "mp", "ppi", "ecustom", "st", "rt")


def _norm_enum(name):
    """归一化：小写、去掉下划线，再剥掉版本之间会变的枚举前缀。"""
    text = name.replace("_", "").lower()
    changed = True
    while changed:
        changed = False
        for prefix in _ENUM_PREFIXES:
            if text.startswith(prefix) and len(text) > len(prefix) + 3:
                text = text[len(prefix):]
                changed = True
    return text


def enum_value(enum_type, *candidates):
    """按名字取 UENUM 值，兼容 BL_OPAQUE / BLEND_OPAQUE 这类版本差异。"""
    available = [a for a in dir(enum_type) if not a.startswith("_")]
    wanted = [_norm_enum(c) for c in candidates]

    for candidate in candidates:
        for name in available:
            if name.replace("_", "").lower() == candidate.replace("_", "").lower():
                return getattr(enum_type, name)

    for want in wanted:
        for name in available:
            if _norm_enum(name) == want:
                return getattr(enum_type, name)

    raise RuntimeError(u"找不到枚举值 %s 中的 %s（可用：%s）" % (enum_type, candidates, available))


def make_expr(material, expr_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, expr_class, x, y)


def safe_set(obj, value, *names):
    """按若干候选属性名依次尝试赋值；布尔属性在不同版本里可能叫 bFiltered / filtered。"""
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:  # noqa: BLE001
            continue
    return None


def connect_to_custom(custom, pin_name, source_expr, source_output=""):
    """把 source_expr 接到 Custom 节点的 pin_name 上。"""
    try:
        ok = unreal.MaterialEditingLibrary.connect_material_expressions(
            source_expr, source_output, custom, pin_name)
        if ok:
            return "named"
    except Exception as exc:  # noqa: BLE001
        warn(u"按名字连接 %s 失败：%s" % (pin_name, exc))
    return "failed"


def save_asset(path):
    try:
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    except Exception as exc:  # noqa: BLE001
        warn(u"保存 %s 失败：%s" % (path, exc))


# ---------------------------------------------------------------------------
# 材质
# ---------------------------------------------------------------------------
def build_material():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_path = "%s/%s" % (PACKAGE_PATH, MATERIAL_NAME)

    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        unreal.EditorAssetLibrary.delete_asset(material_path)
        log(u"已删除旧的 %s" % material_path)

    material = asset_tools.create_asset(
        MATERIAL_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(u"创建材质失败：%s" % material_path)

    material.set_editor_property("material_domain",
                                 enum_value(unreal.MaterialDomain, "MD_POST_PROCESS"))
    material.set_editor_property("blendable_location",
                                 enum_value(unreal.BlendableLocation,
                                            "BL_SCENE_COLOR_AFTER_TONEMAPPING"))
    material.set_editor_property("blend_mode", enum_value(unreal.BlendMode, "BL_OPAQUE"))
    log(u"材质域=%s 混合位置=%s" % (
        material.get_editor_property("material_domain"),
        material.get_editor_property("blendable_location")))

    # --- 场景纹理节点（必须存在，着色器才会绑定这些纹理）-------------------
    scene_color = make_expr(material, unreal.MaterialExpressionSceneTexture, -900, -260)
    scene_color.set_editor_property(
        "scene_texture_id", enum_value(unreal.SceneTextureId, "PPI_PostProcessInput0"))
    safe_set(scene_color, True, "b_filtered", "bFiltered", "filtered")

    scene_depth = make_expr(material, unreal.MaterialExpressionSceneTexture, -900, -120)
    scene_depth.set_editor_property(
        "scene_texture_id", enum_value(unreal.SceneTextureId, "PPI_SceneDepth"))
    safe_set(scene_depth, False, "b_filtered", "bFiltered", "filtered")

    scene_normal = make_expr(material, unreal.MaterialExpressionSceneTexture, -900, 20)
    scene_normal.set_editor_property(
        "scene_texture_id", enum_value(unreal.SceneTextureId, "PPI_WorldNormal"))
    safe_set(scene_normal, False, "b_filtered", "bFiltered", "filtered")

    # --- Custom 节点 -------------------------------------------------------
    custom = make_expr(material, unreal.MaterialExpressionCustom, -400, -120)
    custom.set_editor_property("code", CUSTOM_CODE)
    custom.set_editor_property("description", u"Painterly Impasto Filter")
    custom.set_editor_property("output_type",
                               enum_value(unreal.CustomMaterialOutputType, "CMOT_Float3"))
    custom.set_editor_property("include_file_paths", [SHADER_INCLUDE])
    safe_set(custom, enum_value(unreal.CustomMaterialClipInstruction, "CMCI_No"),
             "contains_clip_instruction")

    pin_names = [PIN_SCENE_COLOR, PIN_DEPTH, PIN_NORMAL] + [p[0] for p in SCALAR_PARAMS]
    custom_inputs = []
    for name in pin_names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        custom_inputs.append(entry)
    custom.set_editor_property("inputs", custom_inputs)

    # --- 参数节点 + 连线 ---------------------------------------------------
    method_counts = {}
    y = 120
    scalar_exprs = []
    for name, default, _desc in SCALAR_PARAMS:
        node = make_expr(material, unreal.MaterialExpressionScalarParameter, -900, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", float(default))
        safe_set(node, "Painterly", "group")
        scalar_exprs.append(node)
        y += 90

    connections = [
        (PIN_SCENE_COLOR, scene_color),
        (PIN_DEPTH, scene_depth),
        (PIN_NORMAL, scene_normal),
    ]
    for offset, node in enumerate(scalar_exprs):
        connections.append((SCALAR_PARAMS[offset][0], node))

    for pin_name, node in connections:
        how = connect_to_custom(custom, pin_name, node)
        method_counts[how] = method_counts.get(how, 0) + 1

    log(u"Custom 节点连线结果：%s（共 %d 个输入）" % (method_counts, len(connections)))
    if method_counts.get("failed", 0) > 0:
        warn(u"有输入未能连线，材质可能无法编译")

    # --- 输出 --------------------------------------------------------------
    ok = unreal.MaterialEditingLibrary.connect_material_property(
        custom, "", enum_value(unreal.MaterialProperty, "MP_EMISSIVE_COLOR"))
    log(u"Emissive 输出连接：%s" % ok)
    if not ok:
        raise RuntimeError(u"无法连接 Emissive Color 输出")

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    save_asset(material_path)

    log(u"材质已生成：%s（表达式 %d 个）" %
        (material_path, len(unreal.MaterialEditingLibrary.get_material_expressions(material))))
    return material


# ---------------------------------------------------------------------------
# 材质实例
# ---------------------------------------------------------------------------
def build_instance(material, name, overrides=None):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    instance_path = "%s/%s" % (PACKAGE_PATH, name)

    if unreal.EditorAssetLibrary.does_asset_exist(instance_path):
        unreal.EditorAssetLibrary.delete_asset(instance_path)

    instance = asset_tools.create_asset(
        name, PACKAGE_PATH, unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        raise RuntimeError(u"创建材质实例失败：%s" % instance_path)

    instance.set_editor_property("parent", material)

    values = {p[0]: p[1] for p in SCALAR_PARAMS}
    if overrides:
        values.update(overrides)

    for param_name, value in values.items():
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            instance, param_name, float(value))

    unreal.MaterialEditingLibrary.update_material_instance(instance)
    save_asset(instance_path)
    log(u"材质实例已生成：%s" % instance_path)
    return instance


# ---------------------------------------------------------------------------
# 入口
# ---------------------------------------------------------------------------
def main():
    log(u"开始构建厚涂后处理材质…")
    if not unreal.EditorAssetLibrary.does_directory_exist(PACKAGE_PATH):
        unreal.EditorAssetLibrary.make_directory(PACKAGE_PATH)

    material = build_material()
    build_instance(material, "MI_PP_Painterly")
    for name, overrides in PRESETS.items():
        build_instance(material, name, overrides)

    log(u"完成。把 MI_PP_Painterly 拖进 PostProcessVolume 的 Post Process Materials 即可；"
        u"工程默认由 UPainterlyStyleSubsystem 自动挂载（可用控制台 Paint.Style 0/1 开关）。")


main()
