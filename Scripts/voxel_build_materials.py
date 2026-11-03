"""Build the four Voxel arena materials under /Game/Voxel/Materials.

Idempotent: existing assets are deleted and rebuilt from scratch.

Run headless:
    UnrealEditor-Cmd.exe EbbingRainbow.uproject ^
        -ExecutePythonScript=D:/EbbingRainbow/Scripts/voxel_build_materials.py ^
        -unattended -nop4
"""
import unreal

PKG = "/Game/Voxel/Materials"
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def delete_if_exists(name):
    path = "%s/%s.%s" % (PKG, name, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)


def new_material(name):
    delete_if_exists(name)
    return tools.create_asset(name, PKG, unreal.Material, unreal.MaterialFactoryNew())


def expr(material, cls, x, y, **props):
    e = mel.create_material_expression(material, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def scalar_param(material, name, default, x, y):
    return expr(material, unreal.MaterialExpressionScalarParameter, x, y,
                parameter_name=name, default_value=default)


def vector_param(material, name, default, x, y):
    return expr(material, unreal.MaterialExpressionVectorParameter, x, y,
                parameter_name=name, default_value=default)


def const3(material, r, g, b, x, y):
    return expr(material, unreal.MaterialExpressionConstant3Vector, x, y,
                constant=unreal.LinearColor(r, g, b))


def const1(material, v, x, y):
    return expr(material, unreal.MaterialExpressionConstant, x, y, r=v)


def link(a, a_out, b, b_in):
    return mel.connect_material_expressions(a, a_out, b, b_in)


def build_gradient(material, x0, y0):
    """G = 0.5 + 0.5 * sin(dot(WorldPos, norm(GradientDir))/GradientScale
       + Time*FlowSpeed). Returns (lerp_node,)."""
    wp = expr(material, unreal.MaterialExpressionWorldPosition, x0, y0)
    gdir = vector_param(material, "GradientDir",
                        unreal.LinearColor(1.0, 0.3, 0.2, 0.0), x0, y0 + 160)
    norm = expr(material, unreal.MaterialExpressionNormalize, x0 + 200, y0 + 160)
    link(gdir, "", norm, "")
    dot = expr(material, unreal.MaterialExpressionDotProduct, x0 + 400, y0 + 60)
    link(wp, "", dot, "A")
    link(norm, "", dot, "B")
    scale = scalar_param(material, "GradientScale", 3000.0, x0 + 400, y0 + 240)
    div = expr(material, unreal.MaterialExpressionDivide, x0 + 600, y0 + 60)
    link(dot, "", div, "A")
    link(scale, "", div, "B")
    time = expr(material, unreal.MaterialExpressionTime, x0 + 200, y0 + 400)
    speed = scalar_param(material, "FlowSpeed", 0.3, x0 + 400, y0 + 480)
    mul = expr(material, unreal.MaterialExpressionMultiply, x0 + 600, y0 + 420)
    link(time, "", mul, "A")
    link(speed, "", mul, "B")
    add = expr(material, unreal.MaterialExpressionAdd, x0 + 800, y0 + 120)
    link(div, "", add, "A")
    link(mul, "", add, "B")
    sine = expr(material, unreal.MaterialExpressionSine, x0 + 1000, y0 + 120)
    link(add, "", sine, "")
    half = const1(material, 0.5, x0 + 1000, y0 + 300)
    mul2 = expr(material, unreal.MaterialExpressionMultiply, x0 + 1200, y0 + 120)
    link(sine, "", mul2, "A")
    link(half, "", mul2, "B")
    add2 = expr(material, unreal.MaterialExpressionAdd, x0 + 1400, y0 + 120)
    link(mul2, "", add2, "A")
    link(half, "", add2, "B")
    return add2


def build_lerp(material, g, x, y):
    ca = vector_param(material, "ColorA", unreal.LinearColor(1, 0.1, 0.1, 1), x, y)
    cb = vector_param(material, "ColorB", unreal.LinearColor(0.15, 0.3, 1, 1), x, y + 160)
    lerp = expr(material, unreal.MaterialExpressionLinearInterpolate, x + 260, y + 60)
    link(ca, "", lerp, "A")
    link(cb, "", lerp, "B")
    link(g, "", lerp, "Alpha")
    return lerp


def main():
    unreal.log("[voxel_mats] building materials in %s" % PKG)

    # M_Voxel_Concrete: 不透明 0.55 灰, Roughness 0.85
    m = new_material("M_Voxel_Concrete")
    mel.connect_material_property(const3(m, 0.55, 0.55, 0.55, -400, 0), "",
                                  unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const1(m, 0.85, -400, 160), "",
                                  unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const1(m, 0.0, -400, 320), "",
                                  unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)

    # M_Voxel_Marble: 不透明 0.9 近白, Roughness 0.25
    m = new_material("M_Voxel_Marble")
    mel.connect_material_property(const3(m, 0.9, 0.9, 0.9, -400, 0), "",
                                  unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const1(m, 0.25, -400, 160), "",
                                  unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const1(m, 0.0, -400, 320), "",
                                  unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)

    # M_Voxel_Glow: 不透明, Emissive = Lerp(A,B,G) * EmissiveStrength(6)
    m = new_material("M_Voxel_Glow")
    mel.connect_material_property(const3(m, 0.05, 0.05, 0.05, -1400, -200), "",
                                  unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const1(m, 0.4, -1400, -60), "",
                                  unreal.MaterialProperty.MP_ROUGHNESS)
    g = build_gradient(m, -2200, 100)
    lerp = build_lerp(m, g, -1600, 400)
    strength = scalar_param(m, "EmissiveStrength", 6.0, -1300, 560)
    emis = expr(m, unreal.MaterialExpressionMultiply, -1000, 400)
    link(lerp, "", emis, "A")
    link(strength, "", emis, "B")
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)

    # M_Voxel_Glass: Translucent, 渐变色 BaseColor + 弱 Emissive(1.5), Opacity 0.35
    m = new_material("M_Voxel_Glass")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = build_gradient(m, -2200, 100)
    lerp = build_lerp(m, g, -1600, 400)
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    strength = scalar_param(m, "EmissiveStrength", 1.5, -1300, 560)
    emis = expr(m, unreal.MaterialExpressionMultiply, -1000, 400)
    link(lerp, "", emis, "A")
    link(strength, "", emis, "B")
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(const1(m, 0.35, -400, 300), "",
                                  unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(const1(m, 0.1, -400, 450), "",
                                  unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)

    unreal.log("[voxel_mats] done: M_Voxel_Concrete / Marble / Glow / Glass")


main()
