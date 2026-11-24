"""Create /Game/Run/Materials/M_Route_Glow (additive unlit soft-mask material for the route scene).

Params: Color, Intensity, Power, ScaleU, ScaleV, EndFade.
Mask = pow(saturate(1 - |(uv-0.5)*2 * (ScaleU,ScaleV)|), Power) * lerp(1, 1-|v-0.5|*2, EndFade)
Run inside the editor: python -m steeragents py Scripts/route_make_materials.py
"""
import unreal

PKG = "/Game/Run/Materials"
NAME = "M_Route_Glow"
mel = unreal.MaterialEditingLibrary

def get_or_create(name):
    path = f"{PKG}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, PKG, unreal.Material, unreal.MaterialFactoryNew())


mat = get_or_create(NAME)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("used_with_instanced_static_meshes", True)


def node(cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def scalar(name, default, x, y):
    n = node(unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n


def const(v, x, y):
    n = node(unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", v)
    return n


def link(a, b, bin="", aout=""):
    mel.connect_material_expressions(a, aout, b, bin)


col = node(unreal.MaterialExpressionVectorParameter, -900, -300)
col.set_editor_property("parameter_name", "Color")
col.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
inten = scalar("Intensity", 1.0, -900, -150)
power = scalar("Power", 2.0, -900, 300)
su = scalar("ScaleU", 1.0, -900, 400)
sv = scalar("ScaleV", 1.0, -900, 480)
endfade = scalar("EndFade", 0.0, -900, 700)

uv = node(unreal.MaterialExpressionTextureCoordinate, -1200, 300)
half = const(0.5, -1200, 400)
two = const(2.0, -1200, 480)

# BasicShapes/Plane 的 UV: u 沿网格局部 X, v 沿局部 Y (已用 ProceduralMeshLibrary 验证)
centered = node(unreal.MaterialExpressionSubtract, -1000, 300)
link(uv, centered, "A"); link(half, centered, "B")
scaled2 = node(unreal.MaterialExpressionMultiply, -800, 300)
link(centered, scaled2, "A"); link(two, scaled2, "B")

suv = node(unreal.MaterialExpressionAppendVector, -800, 440)
link(su, suv, "A"); link(sv, suv, "B")
p = node(unreal.MaterialExpressionMultiply, -600, 340)
link(scaled2, p, "A"); link(suv, p, "B")
dot = node(unreal.MaterialExpressionDotProduct, -450, 340)
link(p, dot, "A"); link(p, dot, "B")
ln = node(unreal.MaterialExpressionSquareRoot, -320, 340)
link(dot, ln)
om = node(unreal.MaterialExpressionOneMinus, -200, 340)
link(ln, om)
sat = node(unreal.MaterialExpressionSaturate, -80, 340)
link(om, sat)
pw = node(unreal.MaterialExpressionPower, 40, 340)
link(sat, pw, "Base"); link(power, pw, "Exp")

vmask = node(unreal.MaterialExpressionComponentMask, -1000, 600)
vmask.set_editor_property("r", False); vmask.set_editor_property("g", True)
link(uv, vmask)
vc = node(unreal.MaterialExpressionSubtract, -860, 600)
link(vmask, vc, "A"); link(half, vc, "B")
vabs = node(unreal.MaterialExpressionAbs, -740, 600)
link(vc, vabs)
vm2 = node(unreal.MaterialExpressionMultiply, -620, 600)
link(vabs, vm2, "A"); link(two, vm2, "B")
vom = node(unreal.MaterialExpressionOneMinus, -500, 600)
link(vm2, vom)
vsat = node(unreal.MaterialExpressionSaturate, -380, 600)
link(vom, vsat)
one = const(1.0, -380, 700)
lerp = node(unreal.MaterialExpressionLinearInterpolate, -240, 650)
link(one, lerp, "A"); link(vsat, lerp, "B"); link(endfade, lerp, "Alpha")

mask = node(unreal.MaterialExpressionMultiply, 200, 450)
link(pw, mask, "A"); link(lerp, mask, "B")

ci = node(unreal.MaterialExpressionMultiply, -600, -220)
link(col, ci, "A"); link(inten, ci, "B")
emis = node(unreal.MaterialExpressionMultiply, 360, 0)
link(ci, emis, "A"); link(mask, emis, "B")

mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(f"{PKG}/{NAME}")
print("created", mat.get_path_name())
print(mel.get_scalar_parameter_names(mat), mel.get_vector_parameter_names(mat))
