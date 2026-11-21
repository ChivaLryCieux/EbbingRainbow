# -*- coding: utf-8 -*-
"""升级 Lvl_Legion 地形材质与氛围光照:
 - MSM_DEFAULT_LIT PBR 岩板地形: 接受太阳阴影与士兵投射阴影;
 - 基于 fwidth() 屏幕导数的等像素宽度等高线 (消除平坡大白块, 保持 1~1.6px 建筑级细线);
 - 细微世界空间岩纹粗糙度变化 + 25m 战略参考网格 + 克制主线微光;
 - 同步精修 Lvl_Legion 的低角度主光、雾效层次与克制后处理。

Run headless:
  "D:\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" D:\\EbbingRainbow\\EbbingRainbow.uproject ^
      -ExecutePythonScript="D:\\EbbingRainbow\\Scripts\\legion_make_terrain_material.py" -unattended -nosplash -nullrhi -stdout
"""
import unreal

PKG = "/Game/Battle/Materials"
NAME = "M_LegionTopo"
LEVEL_PATH = "/Game/Battle/Maps/Lvl_Legion"
mel = unreal.MaterialEditingLibrary


def get_or_create(name):
    path = f"{PKG}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, PKG, unreal.Material, unreal.MaterialFactoryNew())


def node(cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def scalar(name, default, x, y):
    n = node(unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    return n


def color(name, r, g, b, x, y):
    n = node(unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(r, g, b, 1.0))
    return n


def const(v, x, y):
    n = node(unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", v)
    return n


def link(a, b, bin="", aout=""):
    mel.connect_material_expressions(a, aout, b, bin)


def make_custom_input(name):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", name)
    return ci


def fwidth_contour_mask(val_expr, px_width_expr, x, y):
    """基于 fwidth() 的屏幕等像素宽抗锯齿等值线遮罩 (1 = 线上, 0 = 线外)。"""
    c = node(unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    c.set_editor_property("description", "AA_Contour")
    c.set_editor_property("inputs", [make_custom_input("Val"), make_custom_input("PxWidth")])
    c.set_editor_property("code", (
        "float fw = max(fwidth(Val), 1e-4);\n"
        "float dist = abs(frac(Val - 0.5) - 0.5) / fw;\n"
        "return 1.0 - saturate(dist / max(PxWidth, 0.25));"
    ))
    link(val_expr, c, "Val")
    link(px_width_expr, c, "PxWidth")
    return c


def fwidth_grid_mask(xy_expr, spacing_expr, px_width_expr, x, y):
    """基于 fwidth() 的屏幕等像素宽 2D 正交网格遮罩。"""
    c = node(unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    c.set_editor_property("description", "AA_Grid")
    c.set_editor_property("inputs", [
        make_custom_input("XY"),
        make_custom_input("Spacing"),
        make_custom_input("PxWidth"),
    ])
    c.set_editor_property("code", (
        "float2 g = XY / max(Spacing, 1.0);\n"
        "float2 fw = max(fwidth(g), float2(1e-4, 1e-4));\n"
        "float2 d = abs(frac(g - 0.5) - 0.5) / fw;\n"
        "float m = min(d.x, d.y);\n"
        "return 1.0 - saturate(m / max(PxWidth, 0.25));"
    ))
    link(xy_expr, c, "XY")
    link(spacing_expr, c, "Spacing")
    link(px_width_expr, c, "PxWidth")
    return c


mat = get_or_create(NAME)
# 关键升级: 切为 PBR Lit 受光材质, 接收主光立体雕塑感与士兵实时投影
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

# ---------- 输入 ----------
wpos = node(unreal.MaterialExpressionWorldPosition, -1700, 0)
zmask = node(unreal.MaterialExpressionComponentMask, -1520, -20)
zmask.set_editor_property("r", False); zmask.set_editor_property("g", False); zmask.set_editor_property("b", True)
link(wpos, zmask)

xymask = node(unreal.MaterialExpressionComponentMask, -1520, 760)
xymask.set_editor_property("r", True); xymask.set_editor_property("g", True); xymask.set_editor_property("b", False)
link(wpos, xymask)

vnorm = node(unreal.MaterialExpressionVertexNormalWS, -1700, 320)

# ---------- 参数 ----------
spacing = scalar("ContourSpacing", 450.0, -1700, 140)
major_every = scalar("MajorEvery", 5.0, -1700, 220)
minor_px = scalar("MinorLinePixels", 1.15, -1320, 180)
major_px = scalar("MajorLinePixels", 1.65, -1000, 320)
minor_str = scalar("MinorStrength", 0.34, -1700, 560)
major_str = scalar("MajorStrength", 0.62, -1700, 640)
hmin = scalar("HeightMin", -11000.0, -1700, -140)
hmax = scalar("HeightMax", 11000.0, -1700, -80)
slope_pow = scalar("SlopePower", 1.6, -1500, 400)
steep_shade = scalar("SteepShade", 0.52, -1500, 480)
grid_spacing = scalar("GridSpacing", 2500.0, -1700, 840)
grid_px = scalar("GridPixels", 0.95, -1700, 920)
grid_str = scalar("GridStrength", 0.14, -1700, 1000)
major_glow = scalar("MajorGlow", 0.08, 380, 420)

# 静谧玄武岩 × 建筑沙盘配色
base_lo = color("BaseLow", 0.032, 0.038, 0.046, -1500, -260)
base_hi = color("BaseHigh", 0.19, 0.20, 0.22, -1500, -180)
line_col = color("LineColor", 0.48, 0.54, 0.60, 120, -240)
major_col = color("MajorColor", 0.78, 0.74, 0.64, 120, -160)
grid_col = color("GridColor", 0.32, 0.38, 0.44, 120, -80)

# ---------- 屏幕导数抗锯齿等高线 ----------
h = node(unreal.MaterialExpressionDivide, -1350, 40)
link(zmask, h, "A"); link(spacing, h, "B")
minor = fwidth_contour_mask(h, minor_px, -1120, 40)

hm = node(unreal.MaterialExpressionDivide, -1120, 220)
link(h, hm, "A"); link(major_every, hm, "B")
major = fwidth_contour_mask(hm, major_px, -820, 220)

# ---------- 微妙分层明暗 (阶梯沙盘层次) ----------
flr = node(unreal.MaterialExpressionFloor, -760, 380); link(hm, flr)
fh = node(unreal.MaterialExpressionDivide, -600, 380)
fhalf = const(2.0, -760, 460)
link(flr, fh, "A"); link(fhalf, fh, "B")
ff = node(unreal.MaterialExpressionFrac, -460, 380); link(fh, ff)
ft = node(unreal.MaterialExpressionMultiply, -320, 380)
ftwo = const(2.0, -460, 460)
link(ff, ft, "A"); link(ftwo, ft, "B")
band_f = scalar("BandDarken", 0.84, -320, 460)
bone = const(1.0, -320, 520)
bshade = node(unreal.MaterialExpressionLinearInterpolate, -140, 380)
link(bone, bshade, "A"); link(band_f, bshade, "B"); link(ft, bshade, "Alpha")

# ---------- 岩石微肌理 (破除纯色塑料感) ----------
noise = node(unreal.MaterialExpressionNoise, -1350, -400)
noise.set_editor_property("scale", 0.0022)
noise.set_editor_property("levels", 3)
link(wpos, noise, "Position")
n_lo = const(0.88, -1350, -300)
n_hi = const(1.12, -1350, -250)
grain = node(unreal.MaterialExpressionLinearInterpolate, -1140, -380)
link(n_lo, grain, "A"); link(n_hi, grain, "B"); link(noise, grain, "Alpha")

# ---------- 基底色: 高度渐变 × 坡度岩面压暗 × 层带 × 肌理 ----------
hsub = node(unreal.MaterialExpressionSubtract, -1240, -90)
link(zmask, hsub, "A"); link(hmin, hsub, "B")
hrange = node(unreal.MaterialExpressionSubtract, -1240, -170)
link(hmax, hrange, "A"); link(hmin, hrange, "B")
hn = node(unreal.MaterialExpressionDivide, -1080, -110)
link(hsub, hn, "A"); link(hrange, hn, "B")
hsat = node(unreal.MaterialExpressionSaturate, -920, -110); link(hn, hsat)
grad = node(unreal.MaterialExpressionLinearInterpolate, -720, -140)
link(base_lo, grad, "A"); link(base_hi, grad, "B"); link(hsat, grad, "Alpha")

nzmask = node(unreal.MaterialExpressionComponentMask, -1480, 320)
nzmask.set_editor_property("r", False); nzmask.set_editor_property("g", False); nzmask.set_editor_property("b", True)
link(vnorm, nzmask)
nsat = node(unreal.MaterialExpressionSaturate, -1320, 320); link(nzmask, nsat)
sp = node(unreal.MaterialExpressionPower, -1160, 340)
link(nsat, sp, "Base"); link(slope_pow, sp, "Exp")
one = const(1.0, -1160, 440)
shade = node(unreal.MaterialExpressionLinearInterpolate, -960, 360)
link(steep_shade, shade, "A"); link(one, shade, "B"); link(sp, shade, "Alpha")

basec = node(unreal.MaterialExpressionMultiply, -520, -100)
link(grad, basec, "A"); link(shade, basec, "B")
basec2 = node(unreal.MaterialExpressionMultiply, -340, -40)
link(basec, basec2, "A"); link(bshade, basec2, "B")
basec3 = node(unreal.MaterialExpressionMultiply, -180, -100)
link(basec2, basec3, "A"); link(grain, basec3, "B")

# ---------- 25m 战略参考网格 ----------
grid_m = fwidth_grid_mask(xymask, grid_spacing, grid_px, -900, 780)

# ---------- 合成 BaseColor ----------
c0 = node(unreal.MaterialExpressionLinearInterpolate, -10, 140)
link(basec3, c0, "A"); link(grid_col, c0, "B")
gs = node(unreal.MaterialExpressionMultiply, -180, 240)
link(grid_m, gs, "A"); link(grid_str, gs, "B"); link(gs, c0, "Alpha")

c1 = node(unreal.MaterialExpressionLinearInterpolate, 160, 80)
link(c0, c1, "A"); link(line_col, c1, "B")
ms = node(unreal.MaterialExpressionMultiply, -10, 260)
link(minor, ms, "A"); link(minor_str, ms, "B"); link(ms, c1, "Alpha")

c2 = node(unreal.MaterialExpressionLinearInterpolate, 340, 20)
link(c1, c2, "A"); link(major_col, c2, "B")
ms2 = node(unreal.MaterialExpressionMultiply, 160, 240)
link(major, ms2, "A"); link(major_str, ms2, "B"); link(ms2, c2, "Alpha")

mel.connect_material_property(c2, "", unreal.MaterialProperty.MP_BASE_COLOR)

# 粗糙度: 哑光玄武岩 (0.86), 线处略微光滑 (0.50)
r_rock = const(0.88, 120, 480)
r_line = const(0.50, 120, 540)
rough = node(unreal.MaterialExpressionLinearInterpolate, 340, 480)
link(r_rock, rough, "A"); link(r_line, rough, "B"); link(major, rough, "Alpha")
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

spec = const(0.32, 340, 580)
mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

# 克制主线微光 (暗影区仍保持清晰可读)
em = node(unreal.MaterialExpressionMultiply, 360, 300)
link(major, em, "A"); link(major_col, em, "B")
em2 = node(unreal.MaterialExpressionMultiply, 540, 320)
link(em, em2, "A"); link(major_glow, em2, "B")
mel.connect_material_property(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(f"{PKG}/{NAME}")
print("[LegionTopo] Lit PBR material saved:", mat.get_path_name())

# ---------- 挂到关卡 Landscape 并精修主光/雾效/后处理 ----------
unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
assigned = 0
sun = None
sky = None
fog = None
ppv = None

for a in unreal.EditorLevelLibrary.get_all_level_actors():
    cls = a.get_class().get_name()
    lbl = a.get_actor_label()
    if cls == "Landscape":
        a.set_editor_property("landscape_material", mat)
        assigned += 1
    elif cls == "DirectionalLight":
        sun = a
    elif cls == "SkyLight":
        sky = a
    elif cls == "ExponentialHeightFog":
        fog = a
    elif cls == "PostProcessVolume":
        ppv = a

if sun:
    # 低角度斜射冷暖主光, 雕琢山脊明暗与兵团长影
    sun.set_actor_rotation(unreal.Rotator(-24.0, -34.0, 0.0), False)
    lc = sun.light_component
    lc.set_editor_property("intensity", 5.8)
    lc.set_editor_property("light_color", unreal.Color(255, 236, 206, 255))
    lc.set_editor_property("cast_shadows", True)

if sky:
    sc = sky.light_component
    sc.set_editor_property("intensity", 0.85)
    sc.set_editor_property("light_color", unreal.Color(178, 202, 232, 255))

if fog:
    fc = fog.component
    fc.set_editor_property("fog_density", 0.008)
    fc.set_editor_property("fog_height_falloff", 0.24)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.20, 0.25, 0.32, 1.0))

if ppv is None:
    ppv = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator())
    if ppv:
        ppv.set_actor_label("LegionPostProcess")
if ppv:
    ppv.set_editor_property("unbound", True)
    settings = ppv.get_editor_property("settings")
    settings.set_editor_property("override_bloom_intensity", True)
    settings.set_editor_property("bloom_intensity", 0.28)
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.38)
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 0.75)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 1.8)
    ppv.set_editor_property("settings", settings)

unreal.EditorLevelLibrary.save_current_level()
print(f"[LegionTopo] done, assigned={assigned}, lighting/fog/ppv tuned.")
