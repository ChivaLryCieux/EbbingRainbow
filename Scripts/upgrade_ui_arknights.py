# -*- coding: utf-8 -*-
"""
《虹不我与》明日方舟机能风 × 《控制：共振》巨细排印对比 全场景 UI 资产重构脚本。

用法 (确保已编译最新 C++ 且 UnrealEditor 已运行):
    $env:PYTHONPATH = "D:/SteerAgents/client/src"
    & "D:/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/upgrade_ui_arknights.py

执行内容:
  1. 导入本机 Linux Biolinum Bold (LinBiolinum_RB.ttf) -> /Game/UI/Fonts/FF_LinBiolinum_Bold
  2. 重建复合字体 /Game/UI/Fonts/F_ERUI_Composite:
       DefaultTypeface (Regular & Bold) = FF_LinBiolinum_Bold (英文/数字统一)
       FallbackTypeface Regular = FF_NotoSC_Regular, Bold = FF_NotoSC_Bold (中文双字重)
  3. 更新 /Game/UI/DA_ERUITheme 为「黑-白-点睛彩」+ 《控制:共振》巨细字阶
  4. 全量升级并落盘 Hall / Personnel / Route / Battle 全部 WBP 蓝图
"""

import json
import os
import shutil

from steeragents.console import exec_py
from steeragents.toolsets import call, connect

UMG = "UMGToolSet.UMGToolSet"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

FONT = "/Game/UI/Fonts/F_ERUI_Composite"
WHITE = "/Game/UI/Textures/T_UI_White"
SCRIM = "/Game/UI/Textures/T_UI_Scrim"
RING = "/Game/UI/Textures/T_UI_Ring"
PORTRAIT_PLACEHOLDER = "/Game/UI/Textures/T_UI_HeroPortrait_Placeholder"
CHAR_TEX = "/Game/UI/Textures/T_Character"
MAT_HP = "/Game/UI/Materials/M_UI_PortraitHP"
THEME = "/Game/UI/DA_ERUITheme"

LIN_BIOLINUM_SYS = r"C:\Users\LRY\AppData\Local\Microsoft\Windows\Fonts\LinBiolinum_RB.ttf"
LIN_BIOLINUM_REPO = r"D:\EbbingRainbow\Content\UI\Fonts\Source\LinBiolinum_RB.ttf"


def asset_exists(m, pkg_path: str) -> bool:
    return json.loads(call(m, ASSET, "exists", {"path": pkg_path}))["returnValue"]


def ensure_folder(m, folder: str):
    if not asset_exists(m, folder):
        try:
            call(m, ASSET, "create_folder", {"path": folder})
        except Exception:
            pass


def py(m, code: str):
    preamble = """import unreal
EAL = unreal.EditorAssetLibrary
def mk_font(fnt, sz, bold=False, spacing=0):
    fi = unreal.SlateFontInfo()
    fi.font_object = fnt
    fi.size = sz
    fi.typeface_font_name = 'Bold' if bold else 'Regular'
    fi.letter_spacing = spacing
    return fi
def mk_sz(w, h):
    dv = unreal.DeprecateSlateVector2D()
    dv.import_text(f'(X={w},Y={h})')
    return dv
def fill_size(v=1.0):
    s = unreal.SlateChildSize()
    s.size_rule = unreal.SlateSizeRule.FILL
    s.value = v
    return s
def obj(p): return unreal.find_object(None, p)
def rect_brush(fill, outline_w=0.0, outline=None):
    b = unreal.SlateBrush()
    b.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX
    b.tint_color = unreal.SlateColor(fill)
    b.outline_settings.rounding_type = unreal.SlateBrushRoundingType.FIXED_RADIUS
    b.outline_settings.corner_radii = unreal.Vector4(0.0, 0.0, 0.0, 0.0)
    b.outline_settings.width = outline_w
    if outline is not None:
        b.outline_settings.color = unreal.SlateColor(outline)
    return b
def transparent_btn_style():
    st = unreal.ButtonStyle()
    empty = unreal.SlateBrush()
    empty.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
    st.normal = empty; st.hovered = empty; st.pressed = empty; st.disabled = empty
    st.normal_padding = unreal.Margin(0, 0, 0, 0)
    st.pressed_padding = unreal.Margin(0, 0, 0, 0)
    return st
"""
    out = exec_py(m, preamble + "\n" + code, timeout=180)
    if out.strip():
        print(out.rstrip())
    if "Traceback (most recent call last)" in out:
        raise RuntimeError(f"Editor Python failed:\n{out}")


def recreate_wbp(m, folder: str, name: str, parent_cls_path: str) -> str:
    pkg = f"{folder}/{name}"
    wbp_ref = f"{pkg}.{name}"
    in_mem = exec_py(m, f"import unreal; print('YES' if unreal.find_object(None, '{wbp_ref}') else 'NO')").strip().endswith("YES")
    if asset_exists(m, pkg) or in_mem:
        ws = json.loads(call(m, UMG, "GetWidgets", {
            "widgetBlueprint": {"refPath": wbp_ref}
        }))["returnValue"]["widgets"]
        for w in ws:
            if w["parent"] == "None":
                try:
                    call(m, UMG, "RemoveWidget", {
                        "widgetBlueprint": {"refPath": wbp_ref},
                        "widget": {"refPath": w["widget"]["refPath"]},
                    })
                except Exception:
                    pass
        return wbp_ref
    res = json.loads(call(m, UMG, "CreateWidgetBlueprint", {
        "folderPath": folder,
        "assetName": name,
        "parentClass": {"refPath": parent_cls_path},
    }))
    return res["returnValue"]["refPath"]


def node(m, wbp: str, cls: str, name: str, parent: str = None, var: bool = True):
    args = {
        "widgetBlueprint": {"refPath": wbp},
        "widgetClass": {"refPath": cls},
        "widgetDisplayName": name,
    }
    if parent:
        args["parentWidget"] = {"refPath": parent}
    info = json.loads(call(m, UMG, "AddWidget", args))["returnValue"]
    w_ref = info["widget"]["refPath"]
    if var:
        call(m, UMG, "ToggleWidgetAsVariable", {
            "widgetBlueprint": {"refPath": wbp},
            "widget": {"refPath": w_ref},
            "bIsVariable": True,
        })
    slot = info.get("slot")
    if isinstance(slot, dict):
        slot_ref = slot.get("refPath")
    elif isinstance(slot, str) and slot not in ("", "None"):
        slot_ref = slot
    else:
        slot_ref = None
    return w_ref, slot_ref


def compile_and_save(m, wbp_ref: str):
    ok = json.loads(call(m, UMG, "CompileWidgetBlueprint", {
        "widgetBlueprint": {"refPath": wbp_ref}
    }))["returnValue"]
    print(f"  [Compile] {wbp_ref} -> {ok}")
    if not ok:
        raise RuntimeError(f"Failed to compile {wbp_ref}")
    pkg = wbp_ref.split(".")[0]
    py(m, f"EAL.save_asset('{pkg}', False)")


def step_fonts_and_theme(m):
    print("=== [1] Importing Linux Biolinum & Upgrading Composite Font + DA_ERUITheme ===")
    if os.path.isfile(LIN_BIOLINUM_SYS) and not os.path.isfile(LIN_BIOLINUM_REPO):
        os.makedirs(os.path.dirname(LIN_BIOLINUM_REPO), exist_ok=True)
        shutil.copy2(LIN_BIOLINUM_SYS, LIN_BIOLINUM_REPO)
        print(f"  Copied {LIN_BIOLINUM_SYS} -> {LIN_BIOLINUM_REPO}")

    src_ttf = LIN_BIOLINUM_REPO if os.path.isfile(LIN_BIOLINUM_REPO) else LIN_BIOLINUM_SYS
    if not os.path.isfile(src_ttf):
        raise RuntimeError(f"Linux Biolinum TTF not found at {src_ttf}")

    py(m, f"""
tools = unreal.AssetToolsHelpers.get_asset_tools()
src_ttf = r'{src_ttf}'

task = unreal.AssetImportTask()
task.set_editor_property('filename', src_ttf)
task.set_editor_property('destination_path', '/Game/UI/Fonts')
task.set_editor_property('destination_name', 'FF_LinBiolinum_Bold')
task.set_editor_property('automated', True)
task.set_editor_property('replace_existing', True)
task.set_editor_property('save', True)
tools.import_asset_tasks([task])

ff_bio = EAL.load_asset('/Game/UI/Fonts/FF_LinBiolinum_Bold')
ff_noto_reg = EAL.load_asset('/Game/UI/Fonts/FF_NotoSC_Regular')
ff_noto_bold = EAL.load_asset('/Game/UI/Fonts/FF_NotoSC_Bold')
comp = EAL.load_asset('{FONT}')
if not (ff_bio and ff_noto_reg and ff_noto_bold and comp):
    raise RuntimeError(f'Missing font assets: bio={{ff_bio}} reg={{ff_noto_reg}} bold={{ff_noto_bold}} comp={{comp}}')

cf = comp.get_editor_property('composite_font')
cf_text = (
    '(DefaultTypeface=(Fonts=('
    '(Name="Regular",Font=(FontFaceAsset="/Script/Engine.FontFace\\'/Game/UI/Fonts/FF_LinBiolinum_Bold.FF_LinBiolinum_Bold\\'")),'
    '(Name="Bold",Font=(FontFaceAsset="/Script/Engine.FontFace\\'/Game/UI/Fonts/FF_LinBiolinum_Bold.FF_LinBiolinum_Bold\\'"))'
    ')),FallbackTypeface=(Typeface=(Fonts=('
    '(Name="Regular",Font=(FontFaceAsset="/Script/Engine.FontFace\\'/Game/UI/Fonts/FF_NotoSC_Regular.FF_NotoSC_Regular\\'")),'
    '(Name="Bold",Font=(FontFaceAsset="/Script/Engine.FontFace\\'/Game/UI/Fonts/FF_NotoSC_Bold.FF_NotoSC_Bold\\'"))'
    ')),ScalingFactor=1.000000),SubTypefaces=,bEnableAscentDescentOverride=True)'
)
cf.import_text(cf_text)
comp.set_editor_property('composite_font', cf)
print('  F_ERUI_Composite updated ->', comp.get_editor_property('composite_font').export_text())
EAL.save_asset('/Game/UI/Fonts/FF_LinBiolinum_Bold', False)
EAL.save_asset('{FONT}', False)

da = EAL.load_asset('{THEME}')
if not da:
    raise RuntimeError('DA_ERUITheme not found!')

# 黑-白-点睛彩 线性空间校准调色板
da.set_editor_property('ink_panel',         unreal.LinearColor(0.008, 0.009, 0.012, 0.92))
da.set_editor_property('obsidian_elevated', unreal.LinearColor(0.018, 0.020, 0.026, 0.94))
da.set_editor_property('pure_ink',          unreal.LinearColor(0.006, 0.007, 0.009, 1.00))
da.set_editor_property('ink_scrim',         unreal.LinearColor(0.004, 0.005, 0.007, 1.00))
da.set_editor_property('bone',              unreal.LinearColor(0.92,  0.92,  0.90,  1.00))
da.set_editor_property('bone_secondary',    unreal.LinearColor(0.92,  0.92,  0.90,  0.62))
da.set_editor_property('bone_tertiary',     unreal.LinearColor(0.92,  0.92,  0.90,  0.36))
da.set_editor_property('bone_ghost',        unreal.LinearColor(0.92,  0.92,  0.90,  0.16))
da.set_editor_property('accent',            unreal.LinearColor(0.96,  0.56,  0.04,  1.00))
da.set_editor_property('ally_mark',         unreal.LinearColor(0.05,  0.56,  0.94,  1.00))
da.set_editor_property('enemy_mark',        unreal.LinearColor(0.84,  0.05,  0.03,  1.00))
da.set_editor_property('danger_color',      unreal.LinearColor(0.84,  0.05,  0.03,  1.00))
da.set_editor_property('prism_start',       unreal.LinearColor(0.04,  0.62,  0.96,  1.00))
da.set_editor_property('prism_mid',         unreal.LinearColor(0.96,  0.56,  0.04,  1.00))
da.set_editor_property('prism_end',         unreal.LinearColor(0.86,  0.06,  0.04,  1.00))
da.set_editor_property('dead_tint',         unreal.LinearColor(0.60,  0.60,  0.62,  0.28))

da.set_editor_property('font_latin',           ff_bio)
da.set_editor_property('font_body',            ff_noto_reg)
da.set_editor_property('font_body_bold',       ff_noto_bold)
da.set_editor_property('composite_font_asset', comp)
da.set_editor_property('micro_size',     9)
da.set_editor_property('caption_size',   11)
da.set_editor_property('body_size',      13)
da.set_editor_property('title_size',     24)
da.set_editor_property('display_size',   54)
da.set_editor_property('hero_size',      96)
da.set_editor_property('micro_tracking', 280)

EAL.save_asset('{THEME}', False)
print('  DA_ERUITheme saved.')
""")


def step_hall(m):
    print("=== [2] Upgrading Hall Blueprints (WBP_HallButton + WBP_HallHUD) ===")
    wbp_btn = recreate_wbp(m, "/Game/UI/Hall", "WBP_HallButton", "/Script/EbbingRainbow.ERHallButton")
    hit, _ = node(m, wbp_btn, "/Script/UMG.Button", "HitButton")
    sz, _ = node(m, wbp_btn, "/Script/UMG.SizeBox", "ButtonSizeBox", hit)
    ov, _ = node(m, wbp_btn, "/Script/UMG.Overlay", "RootOverlay", sz, var=False)
    frame, frame_s = node(m, wbp_btn, "/Script/EbbingRainbow.ERTechFrame", "TechFrame", ov)
    row, row_s = node(m, wbp_btn, "/Script/UMG.HorizontalBox", "ContentRow", ov, var=False)
    idx, idx_s = node(m, wbp_btn, "/Script/UMG.TextBlock", "IndexText", row)
    tcol, tcol_s = node(m, wbp_btn, "/Script/UMG.VerticalBox", "TextCol", row, var=False)
    lbl, _ = node(m, wbp_btn, "/Script/UMG.TextBlock", "LabelText", tcol)
    cap, cap_s = node(m, wbp_btn, "/Script/UMG.TextBlock", "CaptionText", tcol)
    arr, arr_s = node(m, wbp_btn, "/Script/UMG.TextBlock", "ArrowText", row)

    py(m, f"""
fnt = EAL.load_asset('{FONT}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
btn = obj('{hit}')
btn.set_editor_property('widget_style', transparent_btn_style())
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)

sbox = obj('{sz}')
sbox.set_width_override(312.0)
sbox.set_height_override(74.0)
obj('{ov}').set_editor_property('visibility', SELF)

fr = obj('{frame}')
fr.set_editor_property('chamfer', unreal.Vector4(0.0, 14.0, 0.0, 14.0))
fr.set_editor_property('border_thickness', 1.0)
fr.set_editor_property('draw_corner_brackets', True)
fr.set_editor_property('bracket_length', 10.0)
fr.set_editor_property('bracket_thickness', 1.5)
fr.set_editor_property('accent_edge', unreal.ERAccentEdge.LEFT)
fr.set_editor_property('accent_thickness', 4.0)
fr.set_editor_property('visibility', SELF)
fs = obj('{frame_s}')
fs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
fs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

obj('{row}').set_editor_property('visibility', SELF)
rs = obj('{row_s}')
rs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
rs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
rs.set_editor_property('padding', unreal.Margin(22.0, 10.0, 20.0, 10.0))

ix = obj('{idx}')
ix.set_editor_property('text', '02')
ix.set_editor_property('font', mk_font(fnt, 24, True, 40))
ix.set_editor_property('visibility', SELF)
ixs = obj('{idx_s}')
ixs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ixs.set_editor_property('padding', unreal.Margin(0.0, 0.0, 16.0, 0.0))

obj('{tcol}').set_editor_property('visibility', SELF)
ts = obj('{tcol_s}')
ts.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ts.set_editor_property('size', fill_size())

lb = obj('{lbl}')
lb.set_editor_property('text', '航线管理')
lb.set_editor_property('font', mk_font(fnt, 20, True, 60))
lb.set_editor_property('visibility', SELF)

cp = obj('{cap}')
cp.set_editor_property('text', 'EXPEDITION ROUTES')
cp.set_editor_property('font', mk_font(fnt, 8, False, 240))
cp.set_editor_property('visibility', SELF)
obj('{cap_s}').set_editor_property('padding', unreal.Margin(0.0, 3.0, 0.0, 0.0))

ar = obj('{arr}')
ar.set_editor_property('text', '→')
ar.set_editor_property('font', mk_font(fnt, 18, True, 0))
ar.set_editor_property('visibility', SELF)
ars = obj('{arr_s}')
ars.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ars.set_editor_property('padding', unreal.Margin(10.0, 0.0, 0.0, 0.0))
""")
    compile_and_save(m, wbp_btn)

    wbp_hud = recreate_wbp(m, "/Game/UI/Hall", "WBP_HallHUD", "/Script/EbbingRainbow.ERHallHUD")
    root, _ = node(m, wbp_hud, "/Script/UMG.CanvasPanel", "RootCanvas")
    dash, dash_s = node(m, wbp_hud, "/Script/UMG.CanvasPanel", "DashboardContainer", root)
    ret, ret_s = node(m, wbp_hud, "/Script/EbbingRainbow.ERCornerReticle", "CornerReticle", dash)

    mast, mast_s = node(m, wbp_hud, "/Script/UMG.Overlay", "MastheadContainer", dash)
    mfr, mfr_s = node(m, wbp_hud, "/Script/EbbingRainbow.ERTechFrame", "MastheadFrame", mast)
    mcol, mcol_s = node(m, wbp_hud, "/Script/UMG.VerticalBox", "MastheadCol", mast, var=False)
    mtag, _ = node(m, wbp_hud, "/Script/UMG.TextBlock", "MastheadTagText", mcol)
    mttl, mttl_s = node(m, wbp_hud, "/Script/UMG.TextBlock", "MastheadTitleText", mcol)
    mdiv, mdiv_s = node(m, wbp_hud, "/Script/EbbingRainbow.ERTechDivider", "MastheadDivider", mcol)
    mmta, _ = node(m, wbp_hud, "/Script/UMG.TextBlock", "MastheadMetaText", mcol)

    dock, dock_s = node(m, wbp_hud, "/Script/UMG.HorizontalBox", "DockBox", dash)
    pbtn, _ = node(m, wbp_hud, wbp_btn + "_C", "PersonnelButton", dock)
    sp, _ = node(m, wbp_hud, "/Script/UMG.Spacer", "DockSpacer", dock, var=False)
    rbtn, _ = node(m, wbp_hud, wbp_btn + "_C", "RoutesButton", dock)

    zen, zen_s = node(m, wbp_hud, "/Script/UMG.Border", "ZenHintBorder", root)
    zent, _ = node(m, wbp_hud, "/Script/UMG.TextBlock", "ZenHintText", zen)

    py(m, f"""
fnt = EAL.load_asset('{FONT}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE2 = unreal.LinearColor(0.92, 0.92, 0.90, 0.62)
BONE3 = unreal.LinearColor(0.92, 0.92, 0.90, 0.38)
GHOST = unreal.LinearColor(0.92, 0.92, 0.90, 0.16)
ACCENT = unreal.LinearColor(0.96, 0.56, 0.04, 1.0)
ALLY = unreal.LinearColor(0.05, 0.56, 0.94, 1.0)

obj('{root}').set_editor_property('visibility', unreal.SlateVisibility.VISIBLE)
obj('{dash}').set_editor_property('visibility', SELF)
ds = obj('{dash_s}')
ds.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1)))
ds.set_offsets(unreal.Margin(0, 0, 0, 0))

rt = obj('{ret}')
rt.set_editor_property('top_right_code', 'ER // 58.01')
rt.set_editor_property('bottom_right_code', 'ORBITAL.SANCTUM')
rt.set_editor_property('reticle_color', unreal.LinearColor(0.95, 0.95, 0.92, 0.48))
rt.set_editor_property('shadow_color', unreal.LinearColor(0.0, 0.0, 0.0, 0.68))
rt.set_editor_property('visibility', SELF)
rs = obj('{ret_s}')
rs.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1)))
rs.set_offsets(unreal.Margin(0, 0, 0, 0))

obj('{mast}').set_editor_property('visibility', SELF)
ms = obj('{mast_s}')
ms.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(0, 0)))
ms.set_offsets(unreal.Margin(46.0, 44.0, 0.0, 0.0))
ms.set_auto_size(True)

mf = obj('{mfr}')
mf.set_editor_property('chamfer', unreal.Vector4(0.0, 0.0, 18.0, 0.0))
mf.set_editor_property('fill_color', unreal.LinearColor(0.006, 0.007, 0.010, 0.88))
mf.set_editor_property('border_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.16))
mf.set_editor_property('draw_corner_brackets', True)
mf.set_editor_property('bracket_length', 10.0)
mf.set_editor_property('bracket_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.70))
mf.set_editor_property('accent_edge', unreal.ERAccentEdge.LEFT)
mf.set_editor_property('use_prism_accent', True)
mf.set_editor_property('accent_thickness', 4.0)
mf.set_editor_property('visibility', SELF)
mfs = obj('{mfr_s}')
mfs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
mfs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

obj('{mcol}').set_editor_property('visibility', SELF)
obj('{mcol_s}').set_editor_property('padding', unreal.Margin(24.0, 16.0, 34.0, 16.0))

tg = obj('{mtag}')
tg.set_editor_property('text', 'EBBING RAINBOW // SANCTUM TERMINAL')
tg.set_editor_property('font', mk_font(fnt, 9, False, 280))
tg.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2))
tg.set_editor_property('visibility', SELF)

tt = obj('{mttl}')
tt.set_editor_property('text', '虹不我与')
tt.set_editor_property('font', mk_font(fnt, 48, True, 80))
tt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE))
tt.set_editor_property('visibility', SELF)
obj('{mttl_s}').set_editor_property('padding', unreal.Margin(0.0, 2.0, 0.0, 6.0))

dv = obj('{mdiv}')
dv.set_editor_property('anchor_length', 28.0)
dv.set_editor_property('anchor_color', ACCENT)
dv.set_editor_property('line_color', GHOST)
dv.set_editor_property('visibility', SELF)
obj('{mdiv_s}').set_editor_property('padding', unreal.Margin(0.0, 0.0, 0.0, 7.0))

mt = obj('{mmta}')
mt.set_editor_property('text', 'SECTOR 00  /  ORBITAL SANCTUARY  /  STANDBY')
mt.set_editor_property('font', mk_font(fnt, 8, False, 220))
mt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3))
mt.set_editor_property('visibility', SELF)

obj('{dock}').set_editor_property('visibility', SELF)
ks = obj('{dock_s}')
ks.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 1.0), unreal.Vector2D(0.5, 1.0)))
ks.set_alignment(unreal.Vector2D(0.5, 1.0))
ks.set_offsets(unreal.Margin(0.0, -52.0, 0.0, 0.0))
ks.set_auto_size(True)

pb = obj('{pbtn}')
pb.set_editor_property('index_code', '01')
pb.set_editor_property('label', '人员管理')
pb.set_editor_property('caption', 'PERSONNEL ROSTER')
pb.set_editor_property('accent_color', ALLY)
pb.set_editor_property('primary', False)

obj('{sp}').set_editor_property('size', unreal.Vector2D(20.0, 1.0))

rb = obj('{rbtn}')
rb.set_editor_property('index_code', '02')
rb.set_editor_property('label', '航线管理')
rb.set_editor_property('caption', 'EXPEDITION ROUTES')
rb.set_editor_property('accent_color', ACCENT)
rb.set_editor_property('primary', True)

zb = obj('{zen}')
zb.set_editor_property('background', rect_brush(unreal.LinearColor(0.008, 0.009, 0.012, 0.88), 1.0, GHOST))
zb.set_editor_property('padding', unreal.Margin(24.0, 10.0, 24.0, 10.0))
zb.set_editor_property('visibility', unreal.SlateVisibility.HIDDEN)
zs = obj('{zen_s}')
zs.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 1.0), unreal.Vector2D(0.5, 1.0)))
zs.set_alignment(unreal.Vector2D(0.5, 1.0))
zs.set_offsets(unreal.Margin(0.0, -32.0, 0.0, 0.0))
zs.set_auto_size(True)

zt = obj('{zent}')
zt.set_editor_property('text', 'ZEN MODE // 观景模式 · 点击任意位置或按 ESC 返回')
zt.set_editor_property('font', mk_font(fnt, 9, False, 200))
zt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2))
zt.set_editor_property('visibility', SELF)
""")
    compile_and_save(m, wbp_hud)


def step_personnel(m):
    print("=== [3] Upgrading Personnel Dossier Blueprints ===")
    # StatCell
    wbp_cell = recreate_wbp(m, "/Game/UI/Hall", "WBP_PersonnelStatCell", "/Script/EbbingRainbow.ERStatCell")
    col, _ = node(m, wbp_cell, "/Script/UMG.VerticalBox", "CellColumn")
    lbl, _ = node(m, wbp_cell, "/Script/UMG.TextBlock", "LabelText", col)
    val, val_s = node(m, wbp_cell, "/Script/UMG.TextBlock", "ValueText", col)
    tick, tick_s = node(m, wbp_cell, "/Script/UMG.Image", "FootTick", col, var=False)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
obj('{col}').set_editor_property('visibility', SELF)
l = obj('{lbl}')
l.set_editor_property('text', '完整度')
l.set_editor_property('font', mk_font(fnt, 8, False, 220))
l.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 0.38)))
l.set_editor_property('visibility', SELF)
v = obj('{val}')
v.set_editor_property('text', '60')
v.set_editor_property('font', mk_font(fnt, 26, True, 10))
v.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 1.0)))
v.set_editor_property('visibility', SELF)
obj('{val_s}').set_editor_property('padding', unreal.Margin(0.0, 2.0, 0.0, 4.0))
tk = obj('{tick}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(16.0, 1.5)
tk.set_editor_property('brush', b)
tk.set_editor_property('color_and_opacity', unreal.LinearColor(0.92, 0.92, 0.90, 0.16))
tk.set_editor_property('visibility', SELF)
obj('{tick_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_LEFT)
""")
    compile_and_save(m, wbp_cell)

    # MemberRow
    wbp_row = recreate_wbp(m, "/Game/UI/Hall", "WBP_PersonnelMemberRow", "/Script/EbbingRainbow.ERPersonnelMemberRow")
    hit, _ = node(m, wbp_row, "/Script/UMG.Button", "HitButton")
    rsz, _ = node(m, wbp_row, "/Script/UMG.SizeBox", "RowSize", hit, var=False)
    ov, _ = node(m, wbp_row, "/Script/UMG.Overlay", "RowOverlay", rsz, var=False)
    rfr, rfr_s = node(m, wbp_row, "/Script/EbbingRainbow.ERTechFrame", "RowFrame", ov)
    row, row_s = node(m, wbp_row, "/Script/UMG.HorizontalBox", "RowContent", ov, var=False)
    tsz, tsz_s = node(m, wbp_row, "/Script/UMG.SizeBox", "ThumbSize", row, var=False)
    tfr, _ = node(m, wbp_row, "/Script/UMG.Border", "ThumbFrame", tsz)
    tim, _ = node(m, wbp_row, "/Script/UMG.Image", "PortraitThumb", tfr)
    tcol, tcol_s = node(m, wbp_row, "/Script/UMG.VerticalBox", "TextCol", row, var=False)
    nrow, _ = node(m, wbp_row, "/Script/UMG.HorizontalBox", "NameRow", tcol, var=False)
    midx, midx_s = node(m, wbp_row, "/Script/UMG.TextBlock", "MemberIndexText", nrow)
    mname, mname_s = node(m, wbp_row, "/Script/UMG.TextBlock", "MemberName", nrow)
    srow, srow_s = node(m, wbp_row, "/Script/UMG.HorizontalBox", "SchoolRow", tcol, var=False)
    dsz, dsz_s = node(m, wbp_row, "/Script/UMG.SizeBox", "SchoolDotSize", srow, var=False)
    dot, _ = node(m, wbp_row, "/Script/UMG.Border", "SchoolDot", dsz)
    msch, msch_s = node(m, wbp_row, "/Script/UMG.TextBlock", "MemberSchool", srow)
    mint, mint_s = node(m, wbp_row, "/Script/UMG.TextBlock", "MemberIntegrity", row)

    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE2 = unreal.LinearColor(0.92, 0.92, 0.90, 0.62)
BONE3 = unreal.LinearColor(0.92, 0.92, 0.90, 0.38)

btn = obj('{hit}')
btn.set_editor_property('widget_style', transparent_btn_style())
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)
obj('{rsz}').set_height_override(84.0)
obj('{ov}').set_editor_property('visibility', SELF)

rf = obj('{rfr}')
rf.set_editor_property('chamfer', unreal.Vector4(0.0, 10.0, 0.0, 0.0))
rf.set_editor_property('border_thickness', 1.0)
rf.set_editor_property('draw_corner_brackets', False)
rf.set_editor_property('accent_edge', unreal.ERAccentEdge.LEFT)
rf.set_editor_property('accent_thickness', 4.0)
rf.set_editor_property('visibility', SELF)
rfs = obj('{rfr_s}')
rfs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
rfs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

obj('{row}').set_editor_property('visibility', SELF)
rs = obj('{row_s}')
rs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
rs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
rs.set_editor_property('padding', unreal.Margin(14.0, 8.0, 14.0, 8.0))

obj('{tsz}').set_width_override(50.0)
obj('{tsz}').set_height_override(64.0)
obj('{tsz_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

tf = obj('{tfr}')
tf.set_editor_property('padding', unreal.Margin(1.0, 1.0, 1.0, 1.0))
tf.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
tf.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
tf.set_editor_property('background', rect_brush(unreal.LinearColor(0.008, 0.009, 0.012, 0.95), 1.0, unreal.LinearColor(0.92, 0.92, 0.90, 0.2)))

ti = obj('{tim}')
tb = unreal.SlateBrush(); tb.set_editor_property('resource_object', white); tb.image_size = mk_sz(50, 64)
ti.set_editor_property('brush', tb)
ti.set_editor_property('visibility', SELF)

obj('{tcol}').set_editor_property('visibility', SELF)
tcs = obj('{tcol_s}')
tcs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
tcs.set_editor_property('padding', unreal.Margin(12.0, 0.0, 6.0, 0.0))
tcs.set_editor_property('size', fill_size())
obj('{nrow}').set_editor_property('visibility', SELF)

ix = obj('{midx}')
ix.set_editor_property('text', '01')
ix.set_editor_property('font', mk_font(fnt, 9, True, 140))
ix.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3))
ix.set_editor_property('visibility', SELF)
ixs = obj('{midx_s}')
ixs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM)
ixs.set_editor_property('padding', unreal.Margin(0.0, 0.0, 7.0, 2.0))

nm = obj('{mname}')
nm.set_editor_property('text', '磐')
nm.set_editor_property('font', mk_font(fnt, 20, True, 20))
nm.set_editor_property('color_and_opacity', unreal.SlateColor(BONE))
nm.set_editor_property('visibility', SELF)
obj('{mname_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM)

obj('{srow}').set_editor_property('visibility', SELF)
obj('{srow_s}').set_editor_property('padding', unreal.Margin(0.0, 4.0, 0.0, 0.0))
obj('{dsz}').set_width_override(5.0)
obj('{dsz}').set_height_override(5.0)
ds = obj('{dsz_s}')
ds.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ds.set_editor_property('padding', unreal.Margin(0.0, 0.0, 6.0, 0.0))
obj('{dot}').set_editor_property('padding', unreal.Margin(0, 0, 0, 0))
obj('{dot}').set_editor_property('background', rect_brush(unreal.LinearColor(0.05, 0.56, 0.94, 1.0)))

sc = obj('{msch}')
sc.set_editor_property('text', '力学 · 先锋')
sc.set_editor_property('font', mk_font(fnt, 9, False, 100))
sc.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3))
sc.set_editor_property('visibility', SELF)
obj('{msch_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

it = obj('{mint}')
it.set_editor_property('text', '60')
it.set_editor_property('font', mk_font(fnt, 17, True, 0))
it.set_editor_property('justification', unreal.TextJustify.RIGHT)
it.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2))
it.set_editor_property('visibility', SELF)
obj('{mint_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
    compile_and_save(m, wbp_row)

    # PersonnelPanel
    wbp_panel = recreate_wbp(m, "/Game/UI/Hall", "WBP_PersonnelPanel", "/Script/EbbingRainbow.ERPersonnelPanel")
    root, _ = node(m, wbp_panel, "/Script/UMG.CanvasPanel", "RootCanvas")
    scrim, scrim_s = node(m, wbp_panel, "/Script/UMG.Image", "ScrimImage", root)
    pborder, pborder_s = node(m, wbp_panel, "/Script/UMG.Border", "PanelBorder", root)
    pov, _ = node(m, wbp_panel, "/Script/UMG.Overlay", "PanelRootOverlay", pborder, var=False)
    pfr, pfr_s = node(m, wbp_panel, "/Script/EbbingRainbow.ERTechFrame", "PanelFrame", pov)
    col, col_s = node(m, wbp_panel, "/Script/UMG.VerticalBox", "PanelColumn", pov, var=False)

    hdr, hdr_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "HeaderRow", col, var=False)
    hprism, hprism_s = node(m, wbp_panel, "/Script/EbbingRainbow.ERPrismBar", "HeaderPrismBar", hdr)
    tcol, tcol_s = node(m, wbp_panel, "/Script/UMG.VerticalBox", "TitleCol", hdr, var=False)
    hcap, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "HeaderCaption", tcol)
    httl, httl_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "HeaderTitle", tcol)
    hsp, hsp_s = node(m, wbp_panel, "/Script/UMG.Spacer", "HeaderSpacer", hdr, var=False)
    scol, scol_s = node(m, wbp_panel, "/Script/UMG.VerticalBox", "StatusCol", hdr, var=False)
    ssum, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "SquadSummaryText", scol)
    ssta, ssta_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "SquadStateText", scol)
    cbtn, cbtn_s = node(m, wbp_panel, "/Script/UMG.Button", "CloseButton", hdr)
    csz, _ = node(m, wbp_panel, "/Script/UMG.SizeBox", "CloseSize", cbtn, var=False)
    cring, _ = node(m, wbp_panel, "/Script/UMG.Border", "CloseRing", csz, var=False)
    cgly, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "CloseGlyphText", cring)

    hdiv, hdiv_s = node(m, wbp_panel, "/Script/EbbingRainbow.ERTechDivider", "HeaderDivider", col)

    body, body_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "BodyRow", col, var=False)
    railsz, railsz_s = node(m, wbp_panel, "/Script/UMG.SizeBox", "RailSize", body, var=False)
    rail, _ = node(m, wbp_panel, "/Script/UMG.VerticalBox", "MemberRail", railsz, var=False)
    rttl, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "RailTitleText", rail)
    mlist, mlist_s = node(m, wbp_panel, "/Script/UMG.VerticalBox", "MemberListBox", rail)
    rsp, rsp_s = node(m, wbp_panel, "/Script/UMG.Spacer", "RailSpacer", rail, var=False)
    rhint, rhint_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "RailHintText", rail)

    rrule, rrule_s = node(m, wbp_panel, "/Script/UMG.Image", "RailRule", body)

    det, det_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "DetailContainer", body)
    pcol, pcol_s = node(m, wbp_panel, "/Script/UMG.SizeBox", "PortraitCol", det, var=False)
    pcolumn, _ = node(m, wbp_panel, "/Script/UMG.VerticalBox", "PortraitColumn", pcol, var=False)
    pfsz, _ = node(m, wbp_panel, "/Script/UMG.SizeBox", "PortraitFrameSize", pcolumn, var=False)
    pframe, _ = node(m, wbp_panel, "/Script/UMG.Border", "PortraitFrame", pfsz)
    poverlay, _ = node(m, wbp_panel, "/Script/UMG.Overlay", "PortraitOverlay", pframe, var=False)
    pimg, _ = node(m, wbp_panel, "/Script/UMG.Image", "PortraitImage", poverlay)
    pscrim, _ = node(m, wbp_panel, "/Script/UMG.Image", "PortraitScrim", poverlay)
    pitag, pitag_s = node(m, wbp_panel, "/Script/UMG.Border", "PortraitIndexTag", poverlay, var=False)
    pidx, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "PortraitIndexText", pitag)
    pcap, pcap_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "PortraitCaptionText", pcolumn)

    info, info_s = node(m, wbp_panel, "/Script/UMG.VerticalBox", "InfoCol", det, var=False)
    nm, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "NameText", info)
    cs, cs_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "CallsignText", info)
    schrow, schrow_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "SchoolRow", info, var=False)
    chip, chip_s = node(m, wbp_panel, "/Script/UMG.Border", "SchoolChipBorder", schrow)
    chipt, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "SchoolChipText", chip)
    role, role_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "RoleText", schrow)
    tagl, tagl_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "TaglineText", info)
    idiv, idiv_s = node(m, wbp_panel, "/Script/EbbingRainbow.ERTechDivider", "InfoDivider", info)
    sttl, sttl_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "StatsTitleText", info)
    pstats, pstats_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "PrimaryStatsBox", info)
    sstats, sstats_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "SecondaryStatsBox", info)
    rttl2, rttl2_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "ResistTitleText", info)
    rbox, rbox_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "ResistBox", info)
    kttl, kttl_s = node(m, wbp_panel, "/Script/UMG.TextBlock", "SkillsTitleText", info)
    kbox, kbox_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "SkillsBox", info)

    ftr, ftr_s = node(m, wbp_panel, "/Script/UMG.HorizontalBox", "FooterRow", col, var=False)
    fhint, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "FooterHintText", ftr)
    fsp, fsp_s = node(m, wbp_panel, "/Script/UMG.Spacer", "FooterSpacer", ftr, var=False)
    fmark, _ = node(m, wbp_panel, "/Script/UMG.TextBlock", "FooterMark", ftr, var=False)

    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
scrim_tex = EAL.load_asset('{SCRIM}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE2 = unreal.LinearColor(0.92, 0.92, 0.90, 0.62)
BONE3 = unreal.LinearColor(0.92, 0.92, 0.90, 0.36)
GHOST = unreal.LinearColor(0.92, 0.92, 0.90, 0.16)
INK = unreal.LinearColor(0.006, 0.007, 0.009, 1.0)
ACCENT = unreal.LinearColor(0.96, 0.56, 0.04, 1.0)

def txt(ref, text, size, color, bold=False, spacing=0, justify=None, wrap=False):
    w = obj(ref)
    w.set_editor_property('text', text)
    w.set_editor_property('font', mk_font(fnt, size, bold, spacing))
    w.set_editor_property('color_and_opacity', unreal.SlateColor(color))
    w.set_editor_property('visibility', SELF)
    if justify is not None: w.set_editor_property('justification', justify)
    if wrap: w.set_editor_property('auto_wrap_text', True)
def pad(slot_ref, top=0.0, bottom=0.0, left=0.0, right=0.0):
    obj(slot_ref).set_editor_property('padding', unreal.Margin(left, top, right, bottom))

obj('{root}').set_editor_property('visibility', unreal.SlateVisibility.VISIBLE)
ss = obj('{scrim_s}')
ss.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1)))
ss.set_offsets(unreal.Margin(0, 0, 0, 0))
sc = obj('{scrim}')
sb = unreal.SlateBrush(); sb.set_editor_property('resource_object', white); sb.image_size = mk_sz(4, 4)
sc.set_editor_property('brush', sb)
sc.set_editor_property('color_and_opacity', unreal.LinearColor(0.004, 0.005, 0.007, 0.72))
sc.set_editor_property('visibility', SELF)

pb = obj('{pborder}')
eb = unreal.SlateBrush(); eb.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
pb.set_editor_property('background', eb)
pb.set_editor_property('padding', unreal.Margin(0, 0, 0, 0))
pb.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
pb.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
pbs = obj('{pborder_s}')
pbs.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 0.5), unreal.Vector2D(0.5, 0.5)))
pbs.set_alignment(unreal.Vector2D(0.5, 0.5))
pbs.set_offsets(unreal.Margin(0.0, 0.0, 1360.0, 820.0))
pbs.set_auto_size(False)

obj('{pov}').set_editor_property('visibility', SELF)
pf = obj('{pfr}')
pf.set_editor_property('chamfer', unreal.Vector4(20.0, 0.0, 20.0, 0.0))
pf.set_editor_property('fill_color', unreal.LinearColor(0.006, 0.007, 0.010, 0.96))
pf.set_editor_property('border_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.18))
pf.set_editor_property('draw_corner_brackets', True)
pf.set_editor_property('bracket_length', 16.0)
pf.set_editor_property('bracket_thickness', 1.6)
pf.set_editor_property('bracket_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.80))
pf.set_editor_property('accent_edge', unreal.ERAccentEdge.TOP)
pf.set_editor_property('use_prism_accent', True)
pf.set_editor_property('accent_thickness', 3.0)
pf.set_editor_property('visibility', SELF)
for sref in ['{pfr_s}', '{col_s}']:
    sl = obj(sref)
    sl.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
    sl.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
obj('{col}').set_editor_property('visibility', SELF)

pad('{hdr_s}', top=26.0, left=40.0, right=34.0)
hp = obj('{hprism}')
hp.set_editor_property('vertical', True)
hp.set_editor_property('bar_size', unreal.Vector2D(4.0, 42.0))
hp.set_editor_property('visibility', SELF)
obj('{hprism_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

pad('{tcol_s}', left=16.0)
obj('{tcol_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
txt('{hcap}', 'OPERATOR DOSSIER // SQUAD MANIFEST', 8, BONE3, False, 280)
txt('{httl}', '人员档案', 30, BONE, True, 80)
pad('{httl_s}', top=1.0)

obj('{hsp_s}').set_editor_property('size', fill_size())
pad('{scol_s}', right=22.0)
obj('{scol_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
txt('{ssum}', '编队 4 / 4', 18, BONE, True, 80, unreal.TextJustify.RIGHT)
txt('{ssta}', '待命中 · 尚未启程', 8, BONE3, False, 180, unreal.TextJustify.RIGHT)
pad('{ssta_s}', top=3.0)

cb = obj('{cbtn}')
cb.set_editor_property('widget_style', transparent_btn_style())
cb.set_editor_property('cursor', unreal.MouseCursor.HAND)
obj('{cbtn_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
obj('{csz}').set_width_override(38.0)
obj('{csz}').set_height_override(38.0)
cr = obj('{cring}')
cr.set_editor_property('padding', unreal.Margin(0, 0, 0, 0))
cr.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
cr.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
cr.set_editor_property('background', rect_brush(unreal.LinearColor(0.02, 0.022, 0.028, 0.85), 1.0, unreal.LinearColor(0.92, 0.92, 0.90, 0.28)))
txt('{cgly}', '×', 20, BONE, True, 0, unreal.TextJustify.CENTER)

hd = obj('{hdiv}')
hd.set_editor_property('anchor_length', 42.0)
hd.set_editor_property('anchor_color', BONE)
hd.set_editor_property('line_color', GHOST)
hd.set_editor_property('visibility', SELF)
pad('{hdiv_s}', top=16.0, left=40.0, right=34.0)

pad('{body_s}', top=20.0, left=40.0, right=34.0)
obj('{body_s}').set_editor_property('size', fill_size())

obj('{railsz}').set_width_override(272.0)
obj('{railsz_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
txt('{rttl}', '特勤编队 // SQUAD ROSTER', 8, BONE3, False, 200)
pad('{mlist_s}', top=12.0)
obj('{rsp_s}').set_editor_property('size', fill_size())
txt('{rhint}', 'SELECT OPERATOR // 点击切换档案', 8, BONE3, False, 120)
pad('{rhint_s}', top=12.0)

rr = obj('{rrule}')
rb = unreal.SlateBrush(); rb.set_editor_property('resource_object', white); rb.image_size = mk_sz(1, 1)
rr.set_editor_property('brush', rb)
rr.set_editor_property('color_and_opacity', GHOST)
rrs = obj('{rrule_s}')
rrs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
rrs.set_editor_property('padding', unreal.Margin(26.0, 2.0, 28.0, 2.0))

obj('{det}').set_editor_property('visibility', SELF)
obj('{det_s}').set_editor_property('size', fill_size())

obj('{pcol}').set_width_override(384.0)
obj('{pcol_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
obj('{pfsz}').set_width_override(384.0)
obj('{pfsz}').set_height_override(516.0)
pfr_w = obj('{pframe}')
pfr_w.set_editor_property('padding', unreal.Margin(0, 0, 0, 0))
pfr_w.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
pfr_w.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
pfr_w.set_editor_property('background', rect_brush(unreal.LinearColor(0.008, 0.009, 0.012, 0.96), 1.0, unreal.LinearColor(0.92, 0.92, 0.90, 0.24)))

pi = obj('{pimg}')
pib = unreal.SlateBrush(); pib.set_editor_property('resource_object', white); pib.image_size = mk_sz(384, 516)
pi.set_editor_property('brush', pib)
pi.set_editor_property('color_and_opacity', unreal.LinearColor(0.05, 0.06, 0.08, 1.0))
pi.set_editor_property('visibility', SELF)

ps = obj('{pscrim}')
psb = unreal.SlateBrush(); psb.set_editor_property('resource_object', scrim_tex); psb.image_size = mk_sz(4, 64)
ps.set_editor_property('brush', psb)
ps.set_editor_property('color_and_opacity', unreal.LinearColor(0.004, 0.005, 0.007, 0.78))
ps.set_editor_property('visibility', SELF)
ps.set_editor_property('render_transform_pivot', unreal.Vector2D(0.5, 0.5))
wt = unreal.WidgetTransform(); wt.scale = unreal.Vector2D(1.0, -1.0)
ps.set_editor_property('render_transform', wt)

ptg = obj('{pitag}')
ptg.set_editor_property('background', rect_brush(BONE))
ptg.set_editor_property('padding', unreal.Margin(10.0, 3.0, 10.0, 3.0))
ptgs = obj('{pitag_s}')
ptgs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_LEFT)
ptgs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM)
ptgs.set_editor_property('padding', unreal.Margin(12.0, 0.0, 0.0, 12.0))
txt('{pidx}', '01 / 04', 11, INK, True, 160)
txt('{pcap}', 'ARCHIVE // VISUAL RECORD', 8, BONE3, False, 220)
pad('{pcap_s}', top=10.0, left=2.0)

islt = obj('{info_s}')
islt.set_editor_property('size', fill_size())
islt.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
islt.set_editor_property('padding', unreal.Margin(36.0, 0.0, 0.0, 0.0))

txt('{nm}', '磐', 72, BONE, True, 20)
txt('{cs}', 'BASALT', 15, ACCENT, True, 340)
pad('{cs_s}', left=2.0)

pad('{schrow_s}', top=12.0)
ch = obj('{chip}')
ch.set_editor_property('padding', unreal.Margin(10.0, 3.0, 10.0, 3.0))
ch.set_editor_property('background', rect_brush(BONE))
obj('{chip_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
txt('{chipt}', '学派 · 力学', 10, INK, True, 120)
obj('{role_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
pad('{role_s}', left=14.0)
txt('{role}', '先锋 · VANGUARD', 10, BONE2, False, 180)

txt('{tagl}', '以身为墙，替全队吃掉第一轮伤害。', 12, BONE2, False, 40, wrap=True)
pad('{tagl_s}', top=12.0)

idv = obj('{idiv}')
idv.set_editor_property('anchor_length', 30.0)
idv.set_editor_property('anchor_color', ACCENT)
idv.set_editor_property('line_color', GHOST)
idv.set_editor_property('visibility', SELF)
pad('{idiv_s}', top=14.0)

txt('{sttl}', '基本属性 // ATTRIBUTES', 8, BONE3, False, 220)
pad('{sttl_s}', top=14.0)
pad('{pstats_s}', top=10.0)
pad('{sstats_s}', top=10.0)

txt('{rttl2}', '四学派抗性 // RESISTANCE', 8, BONE3, False, 220)
pad('{rttl2_s}', top=14.0)
pad('{rbox_s}', top=10.0)

txt('{kttl}', '战术技能 // LOADOUT', 8, BONE3, False, 220)
pad('{kttl_s}', top=14.0)
pad('{kbox_s}', top=10.0)

pad('{ftr_s}', top=10.0, bottom=20.0, left=40.0, right=34.0)
txt('{fhint}', 'CLICK OPERATOR TO INSPECT  //  ESC 关闭', 8, BONE3, False, 140)
obj('{fsp_s}').set_editor_property('size', fill_size())
txt('{fmark}', 'EBBING RAINBOW // PERSONNEL ARCHIVE', 8, BONE3, False, 240)
""")
    compile_and_save(m, wbp_panel)

    # Wire Personnel CDO
    py(m, f"""
bp = EAL.load_asset('/Game/UI/Hall/WBP_PersonnelPanel')
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('default_portrait', EAL.load_asset('{PORTRAIT_PLACEHOLDER}'))
cdo.set_editor_property('member_row_class', EAL.load_asset('/Game/UI/Hall/WBP_PersonnelMemberRow').generated_class())
cdo.set_editor_property('stat_cell_class', EAL.load_asset('/Game/UI/Hall/WBP_PersonnelStatCell').generated_class())
EAL.save_asset('/Game/UI/Hall/WBP_PersonnelPanel', False)
print('  WBP_PersonnelPanel CDO wired & saved.')
""")


def step_route(m):
    print("=== [4] Upgrading Route Map Blueprints ===")
    wbp_leg = recreate_wbp(m, "/Game/UI/Route", "WBP_RouteLegendItem", "/Script/EbbingRainbow.ERRouteLegendItem")
    lrow, _ = node(m, wbp_leg, "/Script/UMG.HorizontalBox", "LegendRow", var=False)
    lic, lic_s = node(m, wbp_leg, "/Script/EbbingRainbow.ERRouteTypeIcon", "TypeIcon", lrow)
    limg, limg_s = node(m, wbp_leg, "/Script/UMG.Image", "CustomIconImage", lrow)
    ltxt, ltxt_s = node(m, wbp_leg, "/Script/UMG.TextBlock", "LabelText", lrow)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
obj('{lrow}').set_editor_property('visibility', SELF)
obj('{lic}').set_editor_property('visibility', SELF)
s = obj('{lic_s}'); s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); s.set_editor_property('padding', unreal.Margin(0, 0, 7.0, 0))
im = obj('{limg}'); b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(18, 18)
im.set_editor_property('brush', b); im.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
s2 = obj('{limg_s}'); s2.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); s2.set_editor_property('padding', unreal.Margin(0, 0, 7.0, 0))
t = obj('{ltxt}')
t.set_editor_property('text', '安全节点')
t.set_editor_property('font', mk_font(fnt, 9, False, 160))
t.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 0.68)))
t.set_editor_property('visibility', SELF)
obj('{ltxt_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
    compile_and_save(m, wbp_leg)

    wbp_node = recreate_wbp(m, "/Game/UI/Route", "WBP_RouteNode", "/Script/EbbingRainbow.ERRouteNode")
    nbtn, _ = node(m, wbp_node, "/Script/UMG.Button", "HitButton")
    nsz, _ = node(m, wbp_node, "/Script/UMG.SizeBox", "NodeSizeBox", nbtn)
    ncol, _ = node(m, wbp_node, "/Script/UMG.VerticalBox", "NodeCol", nsz, var=False)
    bsz, bsz_s = node(m, wbp_node, "/Script/UMG.SizeBox", "BadgeSizeBox", ncol)
    bov, _ = node(m, wbp_node, "/Script/UMG.Overlay", "BadgeOverlay", bsz, var=False)
    sig, sig_s = node(m, wbp_node, "/Script/EbbingRainbow.ERRouteNodeSigil", "Sigil", bov)
    nimg, nimg_s = node(m, wbp_node, "/Script/UMG.Image", "CustomIconImage", bov)
    ngly, ngly_s = node(m, wbp_node, "/Script/UMG.TextBlock", "GlyphText", bov)
    nlbl, nlbl_s = node(m, wbp_node, "/Script/UMG.TextBlock", "LabelText", ncol)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
btn = obj('{nbtn}')
btn.set_editor_property('widget_style', transparent_btn_style())
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)
obj('{nsz}').set_width_override(120.0)
obj('{nsz}').set_height_override(150.0)
obj('{bsz}').set_width_override(88.0)
obj('{bsz}').set_height_override(88.0)
bs = obj('{bsz_s}')
bs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
bs.set_editor_property('padding', unreal.Margin(0.0, 31.0, 0.0, 0.0))
obj('{sig}').set_editor_property('visibility', SELF)
ss = obj('{sig_s}')
ss.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
ss.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
im = obj('{nimg}'); ib = unreal.SlateBrush(); ib.set_editor_property('resource_object', white); ib.image_size = mk_sz(36, 36)
im.set_editor_property('brush', ib); im.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
ims = obj('{nimg_s}')
ims.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
ims.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

gl = obj('{ngly}')
gl.set_editor_property('text', '战')
gl.set_editor_property('font', mk_font(fnt, 24, True, 0))
gl.set_editor_property('justification', unreal.TextJustify.CENTER)
gl.set_editor_property('shadow_offset', unreal.Vector2D(0, 1))
gl.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.72))
gl.set_editor_property('visibility', SELF)
gls = obj('{ngly_s}')
gls.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
gls.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

lb = obj('{nlbl}')
lb.set_editor_property('text', '战斗节点')
lb.set_editor_property('font', mk_font(fnt, 9, False, 140))
lb.set_editor_property('justification', unreal.TextJustify.CENTER)
lb.set_editor_property('shadow_offset', unreal.Vector2D(0, 1))
lb.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.75))
lb.set_editor_property('visibility', SELF)
lbs = obj('{nlbl_s}')
lbs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
lbs.set_editor_property('padding', unreal.Margin(0.0, 5.0, 0.0, 0.0))
""")
    compile_and_save(m, wbp_node)

    wbp_hud = recreate_wbp(m, "/Game/UI/Route", "WBP_RouteHUD", "/Script/EbbingRainbow.ERRouteHUD")
    root, _ = node(m, wbp_hud, "/Script/UMG.CanvasPanel", "RootCanvas")
    topsc, topsc_s = node(m, wbp_hud, "/Script/UMG.Image", "TopScrim", root)
    botsc, botsc_s = node(m, wbp_hud, "/Script/UMG.Image", "BottomScrim", root)
    ret, ret_s = node(m, wbp_hud, "/Script/EbbingRainbow.ERCornerReticle", "CornerReticle", root)
    mscale, mscale_s = node(m, wbp_hud, "/Script/UMG.ScaleBox", "MapScaleBox", root)
    msz, _ = node(m, wbp_hud, "/Script/UMG.SizeBox", "MapSizeBox", mscale)
    mov, _ = node(m, wbp_hud, "/Script/UMG.Overlay", "MapOverlay", msz)
    rul, rul_s = node(m, wbp_hud, "/Script/EbbingRainbow.ERRouteLayerRuler", "Ruler", mov)
    edg, edg_s = node(m, wbp_hud, "/Script/EbbingRainbow.ERRouteEdgeLayer", "EdgeLayer", mov)
    ncv, ncv_s = node(m, wbp_hud, "/Script/UMG.CanvasPanel", "NodeCanvas", mov)

    hrow, hrow_s = node(m, wbp_hud, "/Script/UMG.HorizontalBox", "HeaderRow", root, var=False)
    hprism, hprism_s = node(m, wbp_hud, "/Script/EbbingRainbow.ERPrismBar", "TitlePrismBar", hrow)
    hcol, _ = node(m, wbp_hud, "/Script/UMG.VerticalBox", "HeaderCol", hrow, var=False)
    sub, _ = node(m, wbp_hud, "/Script/UMG.TextBlock", "SubtitleText", hcol)
    ttl, _ = node(m, wbp_hud, "/Script/UMG.TextBlock", "TitleText", hcol)

    scol, scol_s = node(m, wbp_hud, "/Script/UMG.VerticalBox", "StatusCol", root, var=False)
    prog, prog_s = node(m, wbp_hud, "/Script/UMG.TextBlock", "ProgressText", scol)
    stat, stat_s = node(m, wbp_hud, "/Script/UMG.TextBlock", "StatText", scol)

    leg, leg_s = node(m, wbp_hud, "/Script/UMG.HorizontalBox", "LegendBox", root)
    hint, hint_s = node(m, wbp_hud, "/Script/UMG.TextBlock", "HintText", root)
    nbd, nbd_s = node(m, wbp_hud, "/Script/UMG.Border", "NotificationBorder", root)
    ntx, _ = node(m, wbp_hud, "/Script/UMG.TextBlock", "NotificationText", nbd)

    py(m, f"""
fnt = EAL.load_asset('{FONT}')
scrim = EAL.load_asset('{SCRIM}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE2 = unreal.LinearColor(0.92, 0.92, 0.90, 0.64)
BONE3 = unreal.LinearColor(0.92, 0.92, 0.90, 0.38)

obj('{root}').set_editor_property('visibility', SELF)
sb = unreal.SlateBrush(); sb.set_editor_property('resource_object', scrim)

ts = obj('{topsc}'); ts.set_editor_property('brush', sb); ts.set_editor_property('color_and_opacity', unreal.LinearColor(0.0, 0.002, 0.004, 0.62)); ts.set_editor_property('visibility', SELF)
tss = obj('{topsc_s}'); tss.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 0))); tss.set_offsets(unreal.Margin(0, 0, 0, 165.0))

bs = obj('{botsc}'); bs.set_editor_property('brush', sb); bs.set_editor_property('color_and_opacity', unreal.LinearColor(0.0, 0.002, 0.004, 0.62))
bs.set_editor_property('render_transform_pivot', unreal.Vector2D(0.5, 0.5))
wt = unreal.WidgetTransform(); wt.scale = unreal.Vector2D(1.0, -1.0)
bs.set_editor_property('render_transform', wt); bs.set_editor_property('visibility', SELF)
bss = obj('{botsc_s}'); bss.set_anchors(unreal.Anchors(unreal.Vector2D(0, 1), unreal.Vector2D(1, 1))); bss.set_alignment(unreal.Vector2D(0, 1)); bss.set_offsets(unreal.Margin(0, -145.0, 0, 0))

rt = obj('{ret}')
rt.set_editor_property('top_right_code', '')
rt.set_editor_property('bottom_right_code', 'DAG // NAV.CHART')
rt.set_editor_property('reticle_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.26))
rt.set_editor_property('draw_center_ticks', False)
rt.set_editor_property('visibility', SELF)
rts = obj('{ret_s}'); rts.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1))); rts.set_offsets(unreal.Margin(0, 0, 0, 0))

sc = obj('{mscale}'); sc.set_editor_property('stretch', unreal.Stretch.SCALE_TO_FIT); sc.set_editor_property('stretch_direction', unreal.StretchDirection.BOTH); sc.set_editor_property('visibility', SELF)
scs = obj('{mscale_s}'); scs.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1))); scs.set_offsets(unreal.Margin(0.0, 88.0, 0.0, 58.0))
obj('{msz}').set_width_override(2040.0); obj('{msz}').set_height_override(980.0); obj('{msz}').set_editor_property('visibility', SELF)
obj('{mov}').set_editor_property('visibility', SELF)
for wref, sref in [('{rul}', '{rul_s}'), ('{edg}', '{edg_s}'), ('{ncv}', '{ncv_s}')]:
    obj(wref).set_editor_property('visibility', SELF)
    sl = obj(sref)
    sl.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
    sl.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

hrs = obj('{hrow_s}'); hrs.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(0, 0))); hrs.set_offsets(unreal.Margin(38.0, 24.0, 0, 0)); hrs.set_auto_size(True)
hp = obj('{hprism}'); hp.set_editor_property('vertical', True); hp.set_editor_property('bar_size', unreal.Vector2D(4.0, 54.0)); hp.set_editor_property('visibility', SELF)
hps = obj('{hprism_s}'); hps.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); hps.set_editor_property('padding', unreal.Margin(0, 2.0, 16.0, 2.0))

st = obj('{sub}'); st.set_editor_property('text', 'EBBING RAINBOW // EXPEDITION GRAPH'); st.set_editor_property('font', mk_font(fnt, 9, False, 280)); st.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3)); st.set_editor_property('visibility', SELF)
tt = obj('{ttl}'); tt.set_editor_property('text', '航线图'); tt.set_editor_property('font', mk_font(fnt, 42, True, 80)); tt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); tt.set_editor_property('shadow_offset', unreal.Vector2D(0, 2)); tt.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.72)); tt.set_editor_property('visibility', SELF)

scs2 = obj('{scol_s}'); scs2.set_anchors(unreal.Anchors(unreal.Vector2D(1, 0), unreal.Vector2D(1, 0))); scs2.set_alignment(unreal.Vector2D(1, 0)); scs2.set_offsets(unreal.Margin(-38.0, 22.0, 0, 0)); scs2.set_auto_size(True)
pg = obj('{prog}'); pg.set_editor_property('text', '01 / 10'); pg.set_editor_property('font', mk_font(fnt, 38, True, 40)); pg.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); pg.set_editor_property('shadow_offset', unreal.Vector2D(0, 2)); pg.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.7)); pg.set_editor_property('visibility', SELF)
obj('{prog_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_RIGHT)
stt = obj('{stat}'); stt.set_editor_property('text', 'LAYER  //  战斗 0 场 · SEED 424242'); stt.set_editor_property('font', mk_font(fnt, 9, False, 200)); stt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2)); stt.set_editor_property('visibility', SELF)
stts = obj('{stat_s}'); stts.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_RIGHT); stts.set_editor_property('padding', unreal.Margin(0, 2.0, 0, 0))

lgs = obj('{leg_s}'); lgs.set_anchors(unreal.Anchors(unreal.Vector2D(0, 1), unreal.Vector2D(0, 1))); lgs.set_alignment(unreal.Vector2D(0, 1)); lgs.set_offsets(unreal.Margin(38.0, -24.0, 0, 0)); lgs.set_auto_size(True)
ht = obj('{hint}'); ht.set_editor_property('text', 'SELECT REACHABLE NODE  //  点击可选节点推进航线'); ht.set_editor_property('font', mk_font(fnt, 9, False, 200)); ht.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3)); ht.set_editor_property('visibility', SELF)
hts = obj('{hint_s}'); hts.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 1), unreal.Vector2D(0.5, 1))); hts.set_alignment(unreal.Vector2D(0.5, 1)); hts.set_offsets(unreal.Margin(0, -24.0, 0, 0)); hts.set_auto_size(True)

nb = obj('{nbd}'); nb.set_editor_property('background', rect_brush(unreal.LinearColor(0.008, 0.009, 0.012, 0.94), 1.0, unreal.LinearColor(0.92, 0.92, 0.90, 0.34))); nb.set_editor_property('padding', unreal.Margin(26.0, 10.0, 26.0, 10.0)); nb.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
nbs = obj('{nbd_s}'); nbs.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 0), unreal.Vector2D(0.5, 0))); nbs.set_alignment(unreal.Vector2D(0.5, 0)); nbs.set_offsets(unreal.Margin(0, 30.0, 0, 0)); nbs.set_auto_size(True)
nt = obj('{ntx}'); nt.set_editor_property('text', '安全节点 · 无事发生'); nt.set_editor_property('font', mk_font(fnt, 13, True, 80)); nt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); nt.set_editor_property('visibility', SELF)
""")
    compile_and_save(m, wbp_hud)
    py(m, """
wbp = EAL.load_asset('/Game/UI/Route/WBP_RouteHUD')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('node_widget_class', EAL.load_asset('/Game/UI/Route/WBP_RouteNode').generated_class())
cdo.set_editor_property('legend_item_class', EAL.load_asset('/Game/UI/Route/WBP_RouteLegendItem').generated_class())
EAL.save_asset('/Game/UI/Route/WBP_RouteHUD', False)
""")


def step_battle(m):
    print("=== [5] Upgrading Battle Blueprints ===")
    # 5a. WBP_PipBar
    wbp_pip = recreate_wbp(m, "/Game/UI/Battle", "WBP_PipBar", "/Script/EbbingRainbow.ERPipBar")
    pbox, _ = node(m, wbp_pip, "/Script/UMG.HorizontalBox", "PipBox")
    py(m, f"obj('{pbox}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)")
    compile_and_save(m, wbp_pip)

    # 5b. WBP_ActionButton (黑曜石切角 / 紧急反白)
    wbp_act = recreate_wbp(m, "/Game/UI/Battle", "WBP_ActionButton", "/Script/EbbingRainbow.ERActionButton")
    ahit, _ = node(m, wbp_act, "/Script/UMG.Button", "HitButton")
    aov, _ = node(m, wbp_act, "/Script/UMG.Overlay", "ButtonOverlay", ahit, var=False)
    afr, afr_s = node(m, wbp_act, "/Script/EbbingRainbow.ERTechFrame", "ButtonFrame", aov)
    arow, arow_s = node(m, wbp_act, "/Script/UMG.HorizontalBox", "Row", aov, var=False)
    albl, albl_s = node(m, wbp_act, "/Script/UMG.TextBlock", "LabelText", arow)
    aarr, aarr_s = node(m, wbp_act, "/Script/UMG.TextBlock", "ArrowText", arow)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
btn = obj('{ahit}'); btn.set_editor_property('widget_style', transparent_btn_style()); btn.set_editor_property('cursor', unreal.MouseCursor.HAND)
obj('{aov}').set_editor_property('visibility', SELF)
af = obj('{afr}')
af.set_editor_property('chamfer', unreal.Vector4(0.0, 10.0, 0.0, 10.0))
af.set_editor_property('fill_color', unreal.LinearColor(0.008, 0.009, 0.012, 0.90))
af.set_editor_property('border_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.28))
af.set_editor_property('draw_corner_brackets', True)
af.set_editor_property('bracket_length', 7.0)
af.set_editor_property('accent_edge', unreal.ERAccentEdge.LEFT)
af.set_editor_property('accent_thickness', 3.0)
af.set_editor_property('visibility', SELF)
afs = obj('{afr_s}'); afs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); afs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
obj('{arow}').set_editor_property('visibility', SELF)
ars = obj('{arow_s}'); ars.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); ars.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); ars.set_editor_property('padding', unreal.Margin(22.0, 12.0, 22.0, 12.0))
al = obj('{albl}'); al.set_editor_property('text', '结束回合'); al.set_editor_property('font', mk_font(fnt, 14, True, 120)); al.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); al.set_editor_property('visibility', SELF)
obj('{albl_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
aa = obj('{aarr}'); aa.set_editor_property('text', '→'); aa.set_editor_property('font', mk_font(fnt, 15, True, 0)); aa.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); aa.set_editor_property('visibility', SELF)
aas = obj('{aarr_s}'); aas.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); aas.set_editor_property('padding', unreal.Margin(10.0, 0.0, 0.0, 0.0))
""")
    compile_and_save(m, wbp_act)

    # 5c. WBP_InitiativeToken & WBP_UnitToken (黑曜石框 + 阵营顶条 + 血线 + 当前金条)
    def build_token(name: str, chip_px: float, show_name: bool):
        wbp_t = recreate_wbp(m, "/Game/UI/Battle", name, "/Script/EbbingRainbow.ERUnitToken")
        thit, _ = node(m, wbp_t, "/Script/UMG.Button", "HitButton")
        tcol, _ = node(m, wbp_t, "/Script/UMG.VerticalBox", "Col", thit, var=False)
        ttick, ttick_s = node(m, wbp_t, "/Script/UMG.Image", "TeamTick", tcol)
        tbox, _ = node(m, wbp_t, "/Script/UMG.SizeBox", "ChipBox", tcol)
        tov, _ = node(m, wbp_t, "/Script/UMG.Overlay", "Ov", tbox, var=False)
        tfr, tfr_s = node(m, wbp_t, "/Script/EbbingRainbow.ERTechFrame", "TokenFrame", tov)
        tpor, tpor_s = node(m, wbp_t, "/Script/UMG.Image", "PortraitImage", tov)
        tgly, tgly_s = node(m, wbp_t, "/Script/UMG.TextBlock", "GlyphText", tov)
        tact, tact_s = node(m, wbp_t, "/Script/UMG.Image", "ActiveRule", tcol)
        tnam, tnam_s = node(m, wbp_t, "/Script/UMG.TextBlock", "NameText", tcol)
        py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
mat_hp = EAL.load_asset('{MAT_HP}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
btn = obj('{thit}'); btn.set_editor_property('widget_style', transparent_btn_style()); btn.set_editor_property('cursor', unreal.MouseCursor.HAND)
tk = obj('{ttick}'); tb = unreal.SlateBrush(); tb.set_editor_property('resource_object', white); tb.image_size = mk_sz({chip_px}, 3.0)
tk.set_editor_property('brush', tb); tk.set_editor_property('color_and_opacity', unreal.LinearColor(0.05, 0.56, 0.94, 1.0)); tk.set_editor_property('visibility', SELF)
obj('{ttick_s}').set_editor_property('padding', unreal.Margin(0, 0, 0, 2.0))
obj('{tbox}').set_width_override({chip_px}); obj('{tbox}').set_height_override({chip_px})
tf = obj('{tfr}')
tf.set_editor_property('chamfer', unreal.Vector4(0.0, 6.0, 0.0, 0.0))
tf.set_editor_property('fill_color', unreal.LinearColor(0.008, 0.009, 0.012, 0.90))
tf.set_editor_property('border_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.25))
tf.set_editor_property('draw_corner_brackets', True); tf.set_editor_property('bracket_length', 5.0); tf.set_editor_property('visibility', SELF)
tfs = obj('{tfr_s}'); tfs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); tfs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
pr = obj('{tpor}'); pb = unreal.SlateBrush(); pb.set_editor_property('resource_object', mat_hp); pb.image_size = mk_sz({chip_px - 4.0}, {chip_px - 4.0})
pr.set_editor_property('brush', pb); pr.set_editor_property('visibility', SELF)
prs = obj('{tpor_s}'); prs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); prs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL); prs.set_editor_property('padding', unreal.Margin(2.0, 2.0, 2.0, 2.0))
gl = obj('{tgly}'); gl.set_editor_property('text', '锋'); gl.set_editor_property('font', mk_font(fnt, 14, True, 0)); gl.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 1.0))); gl.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
gls = obj('{tgly_s}'); gls.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); gls.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ac = obj('{tact}'); ab = unreal.SlateBrush(); ab.set_editor_property('resource_object', white); ab.image_size = mk_sz({chip_px}, 3.0)
ac.set_editor_property('brush', ab); ac.set_editor_property('color_and_opacity', unreal.LinearColor(0.96, 0.56, 0.04, 1.0)); ac.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
obj('{tact_s}').set_editor_property('padding', unreal.Margin(0, 3.0, 0, 0))
nm = obj('{tnam}'); nm.set_editor_property('text', '磐'); nm.set_editor_property('font', mk_font(fnt, 9, True, 80)); nm.set_editor_property('justification', unreal.TextJustify.CENTER); nm.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 0.90)))
nm.set_editor_property('visibility', SELF if {show_name} else unreal.SlateVisibility.COLLAPSED)
nms = obj('{tnam_s}'); nms.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); nms.set_editor_property('padding', unreal.Margin(0, 3.0, 0, 0))
""")
        compile_and_save(m, wbp_t)
        py(m, f"""
wbp = EAL.load_asset('/Game/UI/Battle/{name}')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('default_portrait', EAL.load_asset('{CHAR_TEX}'))
EAL.save_asset('/Game/UI/Battle/{name}', False)
""")

    build_token("WBP_InitiativeToken", 44.0, False)
    build_token("WBP_UnitToken", 52.0, True)

    # 5d. WBP_SkillCard (98x132 切角黑曜石战术卡)
    wbp_card = recreate_wbp(m, "/Game/UI/Battle", "WBP_SkillCard", "/Script/EbbingRainbow.ERSkillCard")
    chit, _ = node(m, wbp_card, "/Script/UMG.Button", "HitButton")
    cbox, _ = node(m, wbp_card, "/Script/UMG.SizeBox", "CardBox", chit, var=False)
    cov, _ = node(m, wbp_card, "/Script/UMG.Overlay", "Ov", cbox, var=False)
    cfr, cfr_s = node(m, wbp_card, "/Script/EbbingRainbow.ERTechFrame", "CardFrame", cov)
    ccol, ccol_s = node(m, wbp_card, "/Script/UMG.VerticalBox", "Col", cov, var=False)
    ctop, _ = node(m, wbp_card, "/Script/UMG.HorizontalBox", "Top", ccol, var=False)
    cbadge, cbadge_s = node(m, wbp_card, "/Script/UMG.Border", "CostBadge", ctop)
    ccost, _ = node(m, wbp_card, "/Script/UMG.TextBlock", "CostText", cbadge)
    csp, csp_s = node(m, wbp_card, "/Script/UMG.Spacer", "Sp", ctop, var=False)
    chk, chk_s = node(m, wbp_card, "/Script/UMG.TextBlock", "HotkeyText", ctop)
    cmid, cmid_s = node(m, wbp_card, "/Script/UMG.Overlay", "Mid", ccol, var=False)
    ibox, ibox_s = node(m, wbp_card, "/Script/UMG.SizeBox", "IconBox", cmid, var=False)
    iimg, _ = node(m, wbp_card, "/Script/UMG.Image", "IconImage", ibox)
    cgly, cgly_s = node(m, wbp_card, "/Script/UMG.TextBlock", "GlyphText", cmid)
    cname, cname_s = node(m, wbp_card, "/Script/UMG.TextBlock", "NameText", ccol)
    cdbg, cdbg_s = node(m, wbp_card, "/Script/UMG.Image", "CooldownBg", cov)
    cdtx, cdtx_s = node(m, wbp_card, "/Script/UMG.TextBlock", "CooldownText", cov)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
sword = EAL.load_asset('/Game/UI/SkillIcons/T_Gold_Sword')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE3 = unreal.LinearColor(0.92, 0.92, 0.90, 0.42)
INK = unreal.LinearColor(0.006, 0.007, 0.009, 1.0)
DANGER = unreal.LinearColor(0.84, 0.05, 0.03, 1.0)

btn = obj('{chit}'); btn.set_editor_property('widget_style', transparent_btn_style()); btn.set_editor_property('cursor', unreal.MouseCursor.HAND)
obj('{cbox}').set_width_override(98.0); obj('{cbox}').set_height_override(132.0)
obj('{cov}').set_editor_property('visibility', SELF)
cf = obj('{cfr}')
cf.set_editor_property('chamfer', unreal.Vector4(0.0, 12.0, 0.0, 0.0))
cf.set_editor_property('fill_color', unreal.LinearColor(0.008, 0.009, 0.012, 0.92))
cf.set_editor_property('border_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.22))
cf.set_editor_property('draw_corner_brackets', True); cf.set_editor_property('bracket_length', 7.0)
cf.set_editor_property('accent_edge', unreal.ERAccentEdge.BOTTOM); cf.set_editor_property('accent_thickness', 3.0)
cf.set_editor_property('visibility', SELF)
cfs = obj('{cfr_s}'); cfs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); cfs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
obj('{ccol}').set_editor_property('visibility', SELF)
ccs = obj('{ccol_s}'); ccs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); ccs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL); ccs.set_editor_property('padding', unreal.Margin(8.0, 7.0, 8.0, 8.0))

cb = obj('{cbadge}'); cb.set_editor_property('background', rect_brush(BONE)); cb.set_editor_property('padding', unreal.Margin(6.0, 1.0, 6.0, 1.0))
obj('{cbadge_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
cc = obj('{ccost}'); cc.set_editor_property('text', '2'); cc.set_editor_property('font', mk_font(fnt, 13, True, 0)); cc.set_editor_property('color_and_opacity', unreal.SlateColor(INK)); cc.set_editor_property('visibility', SELF)
obj('{csp_s}').set_editor_property('size', fill_size())
hk = obj('{chk}'); hk.set_editor_property('text', '[1]'); hk.set_editor_property('font', mk_font(fnt, 9, True, 100)); hk.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3)); hk.set_editor_property('visibility', SELF)
obj('{chk_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

ms = obj('{cmid_s}'); ms.set_editor_property('size', fill_size()); ms.set_editor_property('padding', unreal.Margin(0.0, 4.0, 0.0, 4.0))
obj('{ibox}').set_width_override(48.0); obj('{ibox}').set_height_override(48.0)
ibs = obj('{ibox_s}'); ibs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); ibs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
im = obj('{iimg}'); ib = unreal.SlateBrush(); ib.set_editor_property('resource_object', sword); ib.image_size = mk_sz(48.0, 48.0)
im.set_editor_property('brush', ib); im.set_editor_property('visibility', SELF)
gl = obj('{cgly}'); gl.set_editor_property('text', '斩'); gl.set_editor_property('font', mk_font(fnt, 24, True, 0)); gl.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); gl.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
gls = obj('{cgly_s}'); gls.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); gls.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

nm = obj('{cname}'); nm.set_editor_property('text', '相位斩击'); nm.set_editor_property('font', mk_font(fnt, 10, True, 40)); nm.set_editor_property('justification', unreal.TextJustify.CENTER); nm.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); nm.set_editor_property('visibility', SELF)
nms = obj('{cname_s}'); nms.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); nms.set_editor_property('padding', unreal.Margin(0, 0, 0, 2.0))

cd = obj('{cdbg}'); cdb = unreal.SlateBrush(); cdb.set_editor_property('resource_object', white); cdb.image_size = mk_sz(20.0, 18.0)
cd.set_editor_property('brush', cdb); cd.set_editor_property('color_and_opacity', DANGER); cd.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
cds = obj('{cdbg_s}'); cds.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_RIGHT); cds.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM); cds.set_editor_property('padding', unreal.Margin(0, 0, 4.0, 6.0))
ct = obj('{cdtx}'); ct.set_editor_property('text', '2'); ct.set_editor_property('font', mk_font(fnt, 10, True, 0)); ct.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); ct.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
cts = obj('{cdtx_s}'); cts.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_RIGHT); cts.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM); cts.set_editor_property('padding', unreal.Margin(0, 0, 10.0, 8.0))
""")
    compile_and_save(m, wbp_card)
    py(m, """
wbp = EAL.load_asset('/Game/UI/Battle/WBP_SkillCard')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('preview_icon', EAL.load_asset('/Game/UI/SkillIcons/T_Gold_Sword'))
cdo.set_editor_property('hover_lift_y', -14.0)
cdo.set_editor_property('hover_scale', 1.06)
EAL.save_asset('/Game/UI/Battle/WBP_SkillCard', False)
""")

    # 5e. WBP_SkillTooltip
    wbp_tip = recreate_wbp(m, "/Game/UI/Battle", "WBP_SkillTooltip", "/Script/EbbingRainbow.ERSkillTooltip")
    tov, _ = node(m, wbp_tip, "/Script/UMG.Overlay", "TooltipOverlay", var=False)
    tfr, tfr_s = node(m, wbp_tip, "/Script/EbbingRainbow.ERTechFrame", "TooltipFrame", tov)
    tcol, tcol_s = node(m, wbp_tip, "/Script/UMG.VerticalBox", "TipCol", tov, var=False)
    tttl, _ = node(m, wbp_tip, "/Script/UMG.TextBlock", "TitleText", tcol)
    tdsc, tdsc_s = node(m, wbp_tip, "/Script/UMG.TextBlock", "DescriptionText", tcol)
    tdiv, tdiv_s = node(m, wbp_tip, "/Script/EbbingRainbow.ERTechDivider", "TooltipDivider", tcol)
    tmta, _ = node(m, wbp_tip, "/Script/UMG.TextBlock", "MetaText", tcol)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE2 = unreal.LinearColor(0.92, 0.92, 0.90, 0.65)
GHOST = unreal.LinearColor(0.92, 0.92, 0.90, 0.16)
ACCENT = unreal.LinearColor(0.96, 0.56, 0.04, 1.0)
obj('{tov}').set_editor_property('visibility', SELF)
tf = obj('{tfr}')
tf.set_editor_property('chamfer', unreal.Vector4(0.0, 12.0, 0.0, 0.0))
tf.set_editor_property('fill_color', unreal.LinearColor(0.008, 0.009, 0.012, 0.94))
tf.set_editor_property('border_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.24))
tf.set_editor_property('draw_corner_brackets', True); tf.set_editor_property('bracket_length', 8.0)
tf.set_editor_property('accent_edge', unreal.ERAccentEdge.LEFT); tf.set_editor_property('accent_thickness', 3.0); tf.set_editor_property('visibility', SELF)
tfs = obj('{tfr_s}'); tfs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); tfs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
obj('{tcol_s}').set_editor_property('padding', unreal.Margin(18.0, 12.0, 20.0, 12.0))
tt = obj('{tttl}'); tt.set_editor_property('text', '相位斩击'); tt.set_editor_property('font', mk_font(fnt, 18, True, 40)); tt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); tt.set_editor_property('visibility', SELF)
td = obj('{tdsc}'); td.set_editor_property('text', '突进劈砍单体目标'); td.set_editor_property('font', mk_font(fnt, 12, False, 20)); td.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2)); td.set_editor_property('visibility', SELF)
obj('{tdsc_s}').set_editor_property('padding', unreal.Margin(0, 5.0, 0, 0))
dv = obj('{tdiv}'); dv.set_editor_property('anchor_length', 20.0); dv.set_editor_property('anchor_color', ACCENT); dv.set_editor_property('line_color', GHOST); dv.set_editor_property('visibility', SELF)
obj('{tdiv_s}').set_editor_property('padding', unreal.Margin(0, 8.0, 0, 6.0))
tm = obj('{tmta}'); tm.set_editor_property('text', '射 300 · 伤 35 · 2 AP'); tm.set_editor_property('font', mk_font(fnt, 10, True, 160)); tm.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); tm.set_editor_property('visibility', SELF)
""")
    compile_and_save(m, wbp_tip)

    # 5f. WBP_HeroPlate (黑曜石切角战术铭牌 + 52pt 巨型 HP + 双层弹簧血条 + 机能 AP 格)
    wbp_hp = recreate_wbp(m, "/Game/UI/Battle", "WBP_HeroPlate", "/Script/EbbingRainbow.ERHeroPlate")
    hov, _ = node(m, wbp_hp, "/Script/UMG.Overlay", "PlateRoot", var=False)
    hfr, hfr_s = node(m, wbp_hp, "/Script/EbbingRainbow.ERTechFrame", "PlateFrame", hov)
    hcol, hcol_s = node(m, wbp_hp, "/Script/UMG.VerticalBox", "Col", hov, var=False)
    nrow, nrow_s = node(m, wbp_hp, "/Script/UMG.HorizontalBox", "NameRow", hcol, var=False)
    htick, htick_s = node(m, wbp_hp, "/Script/UMG.Image", "TeamTick", nrow)
    hnam, hnam_s = node(m, wbp_hp, "/Script/UMG.TextBlock", "NameText", nrow)
    nsp, nsp_s = node(m, wbp_hp, "/Script/UMG.Spacer", "TagSpacer", nrow, var=False)
    htag, htag_s = node(m, wbp_hp, "/Script/UMG.TextBlock", "UnitTagText", nrow)

    hrow, _ = node(m, wbp_hp, "/Script/UMG.HorizontalBox", "Row", hcol, var=False)
    hnum, hnum_s = node(m, wbp_hp, "/Script/UMG.TextBlock", "HpNumber", hrow)
    hmax, hmax_s = node(m, wbp_hp, "/Script/UMG.TextBlock", "HpMaxText", hrow)
    hbar, hbar_s = node(m, wbp_hp, "/Script/UMG.TextBlock", "BarrierText", hrow)

    lbox, lbox_s = node(m, wbp_hp, "/Script/UMG.SizeBox", "LineBox", hcol, var=False)
    bov, _ = node(m, wbp_hp, "/Script/UMG.Overlay", "HpBarOverlay", lbox, var=False)
    htrail, htrail_s = node(m, wbp_hp, "/Script/UMG.ProgressBar", "HpTrailLine", bov)
    hline, hline_s = node(m, wbp_hp, "/Script/UMG.ProgressBar", "HpLine", bov)

    aprow, _ = node(m, wbp_hp, "/Script/UMG.HorizontalBox", "ApRow", hcol, var=False)
    appips, appips_s = node(m, wbp_hp, wbp_pip + "_C", "ApPips", aprow)
    apsp, apsp_s = node(m, wbp_hp, "/Script/UMG.Spacer", "ApSpacer", aprow, var=False)
    aptxt, aptxt_s = node(m, wbp_hp, "/Script/UMG.TextBlock", "ApText", aprow)
    hsta, hsta_s = node(m, wbp_hp, "/Script/UMG.TextBlock", "StatusText", hcol)

    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE2 = unreal.LinearColor(0.92, 0.92, 0.90, 0.65)
BONE3 = unreal.LinearColor(0.92, 0.92, 0.90, 0.38)
ALLY = unreal.LinearColor(0.05, 0.56, 0.94, 1.0)
DANGER = unreal.LinearColor(0.84, 0.05, 0.03, 1.0)
ACCENT = unreal.LinearColor(0.96, 0.56, 0.04, 1.0)

obj('{hov}').set_editor_property('visibility', SELF)
hf = obj('{hfr}')
hf.set_editor_property('chamfer', unreal.Vector4(0.0, 14.0, 0.0, 0.0))
hf.set_editor_property('fill_color', unreal.LinearColor(0.008, 0.009, 0.012, 0.90))
hf.set_editor_property('border_color', unreal.LinearColor(0.92, 0.92, 0.90, 0.22))
hf.set_editor_property('draw_corner_brackets', True); hf.set_editor_property('bracket_length', 9.0)
hf.set_editor_property('accent_edge', unreal.ERAccentEdge.LEFT); hf.set_editor_property('accent_thickness', 3.5); hf.set_editor_property('accent_color', ALLY)
hf.set_editor_property('visibility', SELF)
hfs = obj('{hfr_s}'); hfs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); hfs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
obj('{hcol}').set_editor_property('visibility', SELF)
obj('{hcol_s}').set_editor_property('padding', unreal.Margin(18.0, 12.0, 22.0, 14.0))

obj('{nrow_s}').set_editor_property('padding', unreal.Margin(0, 0, 0, 2.0))
tk = obj('{htick}'); tb = unreal.SlateBrush(); tb.set_editor_property('resource_object', white); tb.image_size = mk_sz(4.0, 12.0)
tk.set_editor_property('brush', tb); tk.set_editor_property('color_and_opacity', ALLY); tk.set_editor_property('visibility', SELF)
tks = obj('{htick_s}'); tks.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); tks.set_editor_property('padding', unreal.Margin(0, 0, 8.0, 0))
nm = obj('{hnam}'); nm.set_editor_property('text', '磐'); nm.set_editor_property('font', mk_font(fnt, 14, True, 60)); nm.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); nm.set_editor_property('visibility', SELF)
obj('{hnam_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
obj('{nsp_s}').set_editor_property('size', fill_size())
tg = obj('{htag}'); tg.set_editor_property('text', 'ACTIVE // ALLY'); tg.set_editor_property('font', mk_font(fnt, 8, False, 220)); tg.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3)); tg.set_editor_property('visibility', SELF)
obj('{htag_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

hn = obj('{hnum}'); hn.set_editor_property('text', '60'); hn.set_editor_property('font', mk_font(fnt, 52, True, -10)); hn.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); hn.set_editor_property('visibility', SELF)
obj('{hnum_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM)
hm = obj('{hmax}'); hm.set_editor_property('text', '/ 60'); hm.set_editor_property('font', mk_font(fnt, 13, False, 60)); hm.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3)); hm.set_editor_property('visibility', SELF)
hms = obj('{hmax_s}'); hms.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM); hms.set_editor_property('padding', unreal.Margin(8.0, 0, 0, 10.0))
hb = obj('{hbar}'); hb.set_editor_property('text', '+15'); hb.set_editor_property('font', mk_font(fnt, 13, True, 60)); hb.set_editor_property('color_and_opacity', unreal.SlateColor(ALLY)); hb.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
hbs = obj('{hbar_s}'); hbs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM); hbs.set_editor_property('padding', unreal.Margin(10.0, 0, 0, 10.0))

obj('{lbox}').set_width_override(236.0); obj('{lbox}').set_height_override(3.0)
obj('{lbox_s}').set_editor_property('padding', unreal.Margin(0, 4.0, 0, 11.0))

wb = unreal.SlateBrush(); wb.set_editor_property('resource_object', white)
eb = unreal.SlateBrush(); eb.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
bg = unreal.SlateBrush(); bg.set_editor_property('resource_object', white); bg.tint_color = unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 0.14))

ts_bar = unreal.ProgressBarStyle(); ts_bar.background_image = bg; ts_bar.fill_image = wb
htr = obj('{htrail}'); htr.set_editor_property('widget_style', ts_bar); htr.set_editor_property('fill_color_and_opacity', DANGER); htr.set_editor_property('percent', 1.0); htr.set_editor_property('visibility', SELF)
obj('{htrail_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); obj('{htrail_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

ms_bar = unreal.ProgressBarStyle(); ms_bar.background_image = eb; ms_bar.fill_image = wb
hl = obj('{hline}'); hl.set_editor_property('widget_style', ms_bar); hl.set_editor_property('fill_color_and_opacity', BONE); hl.set_editor_property('percent', 1.0); hl.set_editor_property('visibility', SELF)
obj('{hline_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL); obj('{hline_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

obj('{appips_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
obj('{apsp_s}').set_editor_property('size', fill_size())
at = obj('{aptxt}'); at.set_editor_property('text', '3/3 AP'); at.set_editor_property('font', mk_font(fnt, 11, True, 140)); at.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2)); at.set_editor_property('visibility', SELF)
ats = obj('{aptxt_s}'); ats.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); ats.set_editor_property('padding', unreal.Margin(10.0, 0, 0, 0))
st = obj('{hsta}'); st.set_editor_property('text', ''); st.set_editor_property('font', mk_font(fnt, 9, False, 140)); st.set_editor_property('color_and_opacity', unreal.SlateColor(ACCENT)); st.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
obj('{hsta_s}').set_editor_property('padding', unreal.Margin(0, 7.0, 0, 0))
""")
    compile_and_save(m, wbp_hp)

    # 5g. WBP_BattleResultRow & WBP_BattleResult
    wbp_rr = recreate_wbp(m, "/Game/UI/Battle", "WBP_BattleResultRow", "/Script/EbbingRainbow.ERBattleResultRow")
    rrbox, _ = node(m, wbp_rr, "/Script/UMG.HorizontalBox", "RowBox", var=False)
    rrl, rrl_s = node(m, wbp_rr, "/Script/UMG.TextBlock", "LabelText", rrbox)
    rrv, rrv_s = node(m, wbp_rr, "/Script/UMG.TextBlock", "ValueText", rrbox)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
obj('{rrbox}').set_editor_property('visibility', SELF)
l = obj('{rrl}'); l.set_editor_property('text', '回合'); l.set_editor_property('font', mk_font(fnt, 9, False, 260)); l.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 0.42))); l.set_editor_property('visibility', SELF)
ls = obj('{rrl_s}'); ls.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER); ls.set_editor_property('padding', unreal.Margin(0, 0, 18.0, 0))
v = obj('{rrv}'); v.set_editor_property('text', '04'); v.set_editor_property('font', mk_font(fnt, 16, True, 80)); v.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.92, 0.90, 1.0))); v.set_editor_property('visibility', SELF)
obj('{rrv_s}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
    compile_and_save(m, wbp_rr)

    wbp_res = recreate_wbp(m, "/Game/UI/Battle", "WBP_BattleResult", "/Script/EbbingRainbow.ERBattleResult")
    rroot, _ = node(m, wbp_res, "/Script/UMG.CanvasPanel", "RootPanel", var=False)
    rdim, rdim_s = node(m, wbp_res, "/Script/UMG.Image", "DimImage", rroot)
    rtbar, rtbar_s = node(m, wbp_res, "/Script/UMG.Image", "TopBar", rroot)
    rbbar, rbbar_s = node(m, wbp_res, "/Script/UMG.Image", "BottomBar", rroot)
    rcol, rcol_s = node(m, wbp_res, "/Script/UMG.VerticalBox", "Col", rroot, var=False)
    rstag, rstag_s = node(m, wbp_res, "/Script/UMG.TextBlock", "StatusTagText", rcol)
    rttl, rttl_s = node(m, wbp_res, "/Script/UMG.TextBlock", "TitleText", rcol)
    rprism, rprism_s = node(m, wbp_res, "/Script/EbbingRainbow.ERPrismBar", "TitlePrismBar", rcol)
    rsub, rsub_s = node(m, wbp_res, "/Script/UMG.TextBlock", "SubtitleText", rcol)
    rrows, rrows_s = node(m, wbp_res, "/Script/UMG.VerticalBox", "RowsBox", rcol)
    rpbtn, rpbtn_s = node(m, wbp_res, wbp_act + "_C", "PrimaryButton", rcol)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
BONE2 = unreal.LinearColor(0.92, 0.92, 0.90, 0.62)
wb = unreal.SlateBrush(); wb.set_editor_property('resource_object', white)

dm = obj('{rdim}'); dm.set_editor_property('brush', wb); dm.set_editor_property('color_and_opacity', unreal.LinearColor(0.004, 0.005, 0.007, 0.72))
dms = obj('{rdim_s}'); dms.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1))); dms.set_offsets(unreal.Margin(0, 0, 0, 0))
tb = obj('{rtbar}'); tb.set_editor_property('brush', wb); tb.set_editor_property('color_and_opacity', unreal.LinearColor(0.004, 0.005, 0.007, 1.0))
tbs = obj('{rtbar_s}'); tbs.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 0))); tbs.set_offsets(unreal.Margin(0, 0, 0, 112.0))
bb = obj('{rbbar}'); bb.set_editor_property('brush', wb); bb.set_editor_property('color_and_opacity', unreal.LinearColor(0.004, 0.005, 0.007, 1.0))
bbs = obj('{rbbar_s}'); bbs.set_anchors(unreal.Anchors(unreal.Vector2D(0, 1), unreal.Vector2D(1, 1))); bbs.set_alignment(unreal.Vector2D(0, 1)); bbs.set_offsets(unreal.Margin(0, -112.0, 0, 0))

cs = obj('{rcol_s}'); cs.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 0.5), unreal.Vector2D(0.5, 0.5))); cs.set_alignment(unreal.Vector2D(0.5, 0.5)); cs.set_auto_size(True)
st = obj('{rstag}'); st.set_editor_property('text', 'OPERATION CLEARED // TACTICAL SUMMARY'); st.set_editor_property('font', mk_font(fnt, 10, False, 360)); st.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2)); st.set_editor_property('visibility', SELF)
obj('{rstag_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
tt = obj('{rttl}'); tt.set_editor_property('text', '胜  利'); tt.set_editor_property('font', mk_font(fnt, 96, True, 120)); tt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); tt.set_editor_property('visibility', SELF)
obj('{rttl_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
pr = obj('{rprism}'); pr.set_editor_property('vertical', False); pr.set_editor_property('bar_size', unreal.Vector2D(260.0, 3.0)); pr.set_editor_property('visibility', SELF)
prs = obj('{rprism_s}'); prs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); prs.set_editor_property('padding', unreal.Margin(0, 16.0, 0, 16.0))
sb = obj('{rsub}'); sb.set_editor_property('text', '敌方全灭 · 推演完成'); sb.set_editor_property('font', mk_font(fnt, 12, False, 160)); sb.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2)); sb.set_editor_property('visibility', SELF)
obj('{rsub_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
obj('{rrows_s}').set_editor_property('padding', unreal.Margin(0, 22.0, 0, 0))
obj('{rpbtn}').set_editor_property('label', '返回航线图')
pbs = obj('{rpbtn_s}'); pbs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); pbs.set_editor_property('padding', unreal.Margin(0, 34.0, 0, 0))
""")
    compile_and_save(m, wbp_res)
    py(m, """
wbp = EAL.load_asset('/Game/UI/Battle/WBP_BattleResult')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('row_widget_class', EAL.load_asset('/Game/UI/Battle/WBP_BattleResultRow').generated_class())
EAL.save_asset('/Game/UI/Battle/WBP_BattleResult', False)
""")

    # 5h. WBP_FloatingText
    wbp_ft = recreate_wbp(m, "/Game/UI/Battle", "WBP_FloatingText", "/Script/EbbingRainbow.ERFloatingText")
    ftov, _ = node(m, wbp_ft, "/Script/UMG.Overlay", "RootOverlay", var=False)
    fttx, fttx_s = node(m, wbp_ft, "/Script/UMG.TextBlock", "FloatingTextBlock", ftov)
    py(m, f"""
fnt = EAL.load_asset('{FONT}')
obj('{ftov}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
tb = obj('{fttx}')
tb.set_editor_property('text', '-48')
fi = mk_font(fnt, 28, True, 20)
fi.outline_settings.outline_size = 2
fi.outline_settings.outline_color = unreal.LinearColor(0.005, 0.006, 0.008, 0.92)
tb.set_editor_property('font', fi)
tb.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.84, 0.05, 0.03, 1.0)))
tb.set_editor_property('shadow_offset', unreal.Vector2D(1.5, 2.0))
tb.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.85))
tb.set_editor_property('justification', unreal.TextJustify.CENTER)
tb.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
ts = obj('{fttx_s}'); ts.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); ts.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
    compile_and_save(m, wbp_ft)

    # 5i. WBP_BattleHUD (全套机能重构主 HUD)
    wbp_bhud = recreate_wbp(m, "/Game/UI/Battle", "WBP_BattleHUD", "/Script/EbbingRainbow.ERBattleHUD")
    root, _ = node(m, wbp_bhud, "/Script/UMG.CanvasPanel", "RootCanvas")
    topsc, topsc_s = node(m, wbp_bhud, "/Script/UMG.Image", "TopScrim", root)
    botsc, botsc_s = node(m, wbp_bhud, "/Script/UMG.Image", "BottomScrim", root)
    ret, ret_s = node(m, wbp_bhud, "/Script/EbbingRainbow.ERCornerReticle", "CornerReticle", root)

    istrip, istrip_s = node(m, wbp_bhud, "/Script/UMG.VerticalBox", "InitiativeStrip", root, var=False)
    irule, irule_s = node(m, wbp_bhud, "/Script/UMG.Image", "InitiativeRule", istrip)
    ibox, ibox_s = node(m, wbp_bhud, "/Script/UMG.HorizontalBox", "InitiativeBox", istrip)

    nban, nban_s = node(m, wbp_bhud, "/Script/UMG.Border", "NotifyBanner", root)
    ntxt, _ = node(m, wbp_bhud, "/Script/UMG.TextBlock", "NotifyText", nban)

    sqbox, sqbox_s = node(m, wbp_bhud, "/Script/UMG.VerticalBox", "SquadBox", root)
    hpl, hpl_s = node(m, wbp_bhud, wbp_hp + "_C", "HeroPlate", root)
    hand, hand_s = node(m, wbp_bhud, "/Script/UMG.HorizontalBox", "HandBox", root)
    stip, stip_s = node(m, wbp_bhud, wbp_tip + "_C", "SkillTooltip", root)
    etbn, etbn_s = node(m, wbp_bhud, wbp_act + "_C", "EndTurnButton", root)
    rpan, rpan_s = node(m, wbp_bhud, wbp_res + "_C", "ResultPanel", root)

    py(m, f"""
fnt = EAL.load_asset('{FONT}')
white = EAL.load_asset('{WHITE}')
scrim = EAL.load_asset('{SCRIM}')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE
BONE = unreal.LinearColor(0.92, 0.92, 0.90, 1.0)
GHOST = unreal.LinearColor(0.92, 0.92, 0.90, 0.22)

obj('{root}').set_editor_property('visibility', SELF)
sb = unreal.SlateBrush(); sb.set_editor_property('resource_object', scrim)

ts = obj('{topsc}'); ts.set_editor_property('brush', sb); ts.set_editor_property('color_and_opacity', unreal.LinearColor(0.004, 0.005, 0.007, 0.72)); ts.set_editor_property('visibility', SELF)
tss = obj('{topsc_s}'); tss.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 0))); tss.set_offsets(unreal.Margin(0, 0, 0, 128.0))

bs = obj('{botsc}'); bs.set_editor_property('brush', sb); bs.set_editor_property('color_and_opacity', unreal.LinearColor(0.004, 0.005, 0.007, 0.78))
bs.set_editor_property('render_transform_pivot', unreal.Vector2D(0.5, 0.5))
wt = unreal.WidgetTransform(); wt.scale = unreal.Vector2D(1.0, -1.0)
bs.set_editor_property('render_transform', wt); bs.set_editor_property('visibility', SELF)
bss = obj('{botsc_s}'); bss.set_anchors(unreal.Anchors(unreal.Vector2D(0, 1), unreal.Vector2D(1, 1))); bss.set_alignment(unreal.Vector2D(0, 1)); bss.set_offsets(unreal.Margin(0, -180.0, 0, 0))

rt = obj('{ret}')
rt.set_editor_property('top_right_code', '')
rt.set_editor_property('bottom_right_code', 'TACTICAL // GRID')
rt.set_editor_property('reticle_color', unreal.LinearColor(0.95, 0.95, 0.92, 0.38))
rt.set_editor_property('shadow_color', unreal.LinearColor(0.0, 0.0, 0.0, 0.65))
rt.set_editor_property('draw_center_ticks', False); rt.set_editor_property('visibility', SELF)
rts = obj('{ret_s}'); rts.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1))); rts.set_offsets(unreal.Margin(0, 0, 0, 0))

obj('{istrip}').set_editor_property('visibility', SELF)
iss = obj('{istrip_s}'); iss.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 0), unreal.Vector2D(0.5, 0))); iss.set_alignment(unreal.Vector2D(0.5, 0)); iss.set_offsets(unreal.Margin(0, 26.0, 0, 0)); iss.set_auto_size(True)
ir = obj('{irule}'); irb = unreal.SlateBrush(); irb.set_editor_property('resource_object', white); irb.image_size = mk_sz(440.0, 1.0)
ir.set_editor_property('brush', irb); ir.set_editor_property('color_and_opacity', GHOST); ir.set_editor_property('visibility', SELF)
irs = obj('{irule_s}'); irs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER); irs.set_editor_property('padding', unreal.Margin(0, 0, 0, 8.0))
obj('{ibox}').set_editor_property('visibility', SELF); obj('{ibox_s}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)

nb = obj('{nban}'); nb.set_editor_property('background', rect_brush(unreal.LinearColor(0.008, 0.009, 0.012, 0.92), 1.0, unreal.LinearColor(0.92, 0.92, 0.90, 0.28))); nb.set_editor_property('padding', unreal.Margin(22.0, 7.0, 22.0, 7.0)); nb.set_editor_property('visibility', unreal.SlateVisibility.HIDDEN)
nbs = obj('{nban_s}'); nbs.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 0), unreal.Vector2D(0.5, 0))); nbs.set_alignment(unreal.Vector2D(0.5, 0)); nbs.set_offsets(unreal.Margin(0, 108.0, 0, 0)); nbs.set_auto_size(True)
nt = obj('{ntxt}'); nt.set_editor_property('text', '观测者 施放 相位斩击 · 命中 48'); nt.set_editor_property('font', mk_font(fnt, 12, True, 80)); nt.set_editor_property('color_and_opacity', unreal.SlateColor(BONE)); nt.set_editor_property('visibility', SELF)

obj('{sqbox}').set_editor_property('visibility', SELF)
sqs = obj('{sqbox_s}'); sqs.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0.5), unreal.Vector2D(0, 0.5))); sqs.set_alignment(unreal.Vector2D(0, 0.5)); sqs.set_offsets(unreal.Margin(44.0, 0.0, 0, 0)); sqs.set_auto_size(True)

obj('{hpl}').set_editor_property('visibility', SELF)
hps = obj('{hpl_s}'); hps.set_anchors(unreal.Anchors(unreal.Vector2D(0, 1), unreal.Vector2D(0, 1))); hps.set_alignment(unreal.Vector2D(0, 1)); hps.set_offsets(unreal.Margin(44.0, -40.0, 0, 0)); hps.set_auto_size(True)

obj('{hand}').set_editor_property('visibility', SELF)
hns = obj('{hand_s}'); hns.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 1), unreal.Vector2D(0.5, 1))); hns.set_alignment(unreal.Vector2D(0.5, 1)); hns.set_offsets(unreal.Margin(0.0, -36.0, 0, 0)); hns.set_auto_size(True)

obj('{stip}').set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
sts = obj('{stip_s}'); sts.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 1), unreal.Vector2D(0.5, 1))); sts.set_alignment(unreal.Vector2D(0.5, 1)); sts.set_offsets(unreal.Margin(0.0, -188.0, 0, 0)); sts.set_auto_size(True)

obj('{etbn}').set_editor_property('label', '结束回合')
ets = obj('{etbn_s}'); ets.set_anchors(unreal.Anchors(unreal.Vector2D(1, 1), unreal.Vector2D(1, 1))); ets.set_alignment(unreal.Vector2D(1, 1)); ets.set_offsets(unreal.Margin(-44.0, -48.0, 0, 0)); ets.set_auto_size(True)

obj('{rpan}').set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
rps = obj('{rpan_s}'); rps.set_anchors(unreal.Anchors(unreal.Vector2D(0, 0), unreal.Vector2D(1, 1))); rps.set_offsets(unreal.Margin(0, 0, 0, 0))
""")
    compile_and_save(m, wbp_bhud)
    py(m, """
wbp = EAL.load_asset('/Game/UI/Battle/WBP_BattleHUD')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('initiative_token_class', EAL.load_asset('/Game/UI/Battle/WBP_InitiativeToken').generated_class())
cdo.set_editor_property('squad_token_class', EAL.load_asset('/Game/UI/Battle/WBP_UnitToken').generated_class())
cdo.set_editor_property('skill_card_class', EAL.load_asset('/Game/UI/Battle/WBP_SkillCard').generated_class())
cdo.set_editor_property('hand_card_gap', 7.0)
cdo.set_editor_property('move_card_icon', EAL.load_asset('/Game/UI/SkillIcons/T_Blue_Sparkle'))
cdo.set_editor_property('preview_skill_icons', [
    EAL.load_asset('/Game/UI/SkillIcons/T_Gold_Sword'),
    EAL.load_asset('/Game/UI/SkillIcons/T_Blue_Shield'),
    EAL.load_asset('/Game/UI/SkillIcons/T_Purple_Bolt'),
])
EAL.save_asset('/Game/UI/Battle/WBP_BattleHUD', False)
""")


def main():
    m = connect()
    try:
        for folder in ["/Game/UI/Fonts", "/Game/UI/Hall", "/Game/UI/Route", "/Game/UI/Battle"]:
            ensure_folder(m, folder)
        step_fonts_and_theme(m)
        step_hall(m)
        step_personnel(m)
        step_route(m)
        step_battle(m)
        print("=== ARKNIGHTS x CONTROL UI OVERHAUL ASSETS COMPLETE ===")
    finally:
        m.close()


if __name__ == "__main__":
    main()
