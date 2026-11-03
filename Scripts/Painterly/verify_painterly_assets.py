# -*- coding: utf-8 -*-
"""
verify_painterly_assets.py
==========================

检查生成出来的材质/实例是否完整（不依赖渲染）：
  - 材质域 / Blendable Location / 混合模式
  - Custom 节点的 include 路径、输入引脚数量与名字
  - Emissive 是否接了 Custom 节点
  - 每个标量参数是否存在、默认值是多少
  - 实例的父材质与覆盖参数

用法（无界面）：
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -Script="<本文件>" -unattended -nosplash -nop4
"""

import unreal

MATERIAL_PATH = "/Game/Style/Painterly/M_PP_Painterly"
INSTANCE_PATHS = [
    "/Game/Style/Painterly/MI_PP_Painterly",
    "/Game/Style/Painterly/MI_PP_Painterly_ConceptArt",
    "/Game/Style/Painterly/MI_PP_Painterly_Ink",
    "/Game/Style/Painterly/MI_PP_Painterly_Soft",
]


def log(msg):
    unreal.log("[Verify] " + str(msg))


def main():
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if material is None:
        log("材质不存在：%s" % MATERIAL_PATH)
        return

    log("domain=%s blendable=%s blend_mode=%s" % (
        material.get_editor_property("material_domain"),
        material.get_editor_property("blendable_location"),
        material.get_editor_property("blend_mode")))

    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    custom = None
    scalars = []
    for expression in expressions:
        if isinstance(expression, unreal.MaterialExpressionCustom):
            custom = expression
        elif isinstance(expression, unreal.MaterialExpressionScalarParameter):
            scalars.append(expression)

    if custom is None:
        log("找不到 Custom 节点")
        return

    log("custom include=%s" % list(custom.get_editor_property("include_file_paths")))
    inputs = list(custom.get_editor_property("inputs"))
    log("custom 输入引脚 %d 个：%s" % (len(inputs), [i.get_editor_property("input_name") for i in inputs]))
    log("custom code 长度 %d 字符" % len(custom.get_editor_property("code")))

    # FCustomInput.Input 是 protected，Python 读不到，这里只做存在性检查
    log("输入引脚名字齐全：%s" % all(
        entry.get_editor_property("input_name") for entry in inputs))

    emissive = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    log("Emissive 输入 = %s" % (emissive.get_name() if emissive else "None"))

    log("标量参数 %d 个" % len(scalars))
    for node in scalars:
        log("   %-18s = %s" % (node.get_editor_property("parameter_name"),
                               node.get_editor_property("default_value")))

    for path in INSTANCE_PATHS:
        instance = unreal.EditorAssetLibrary.load_asset(path)
        if instance is None:
            log("实例缺失：%s" % path)
            continue
        parent = instance.get_editor_property("parent")
        log("实例 %s 父材质=%s" % (path, parent.get_name() if parent else "None"))


main()
