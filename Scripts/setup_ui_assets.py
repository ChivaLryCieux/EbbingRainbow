# -*- coding: utf-8 -*-
"""
全项目 UI 蓝图与主题资产一键构建/升级脚本。
用法 (确保 UnrealEditor 已开启):
    $env:PYTHONPATH = "D:/SteerAgents/client/src"
    & "D:/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/setup_ui_assets.py

构建/同步的资产清单:
  /Game/UI/DA_ERUITheme
  /Game/UI/Textures/{T_UI_White, T_UI_Scrim, T_UI_Ring, T_Character}
  /Game/UI/Hall/{WBP_HallButton, WBP_HallHUD}
  /Game/UI/Route/{WBP_RouteLegendItem, WBP_RouteNode, WBP_RouteHUD}
  /Game/UI/Battle/{WBP_PipBar, WBP_ActionButton, WBP_UnitToken, WBP_InitiativeToken,
                   WBP_SkillCard, WBP_SkillTooltip, WBP_HeroPlate,
                   WBP_BattleResultRow, WBP_BattleResult, WBP_BattleHUD,
                   WBP_FloatingText, BP_BattleFloatingText}
  并将全局入口类写入 Project Settings -> Game -> ER UI (Config/DefaultGame.ini)。
"""

import json
import sys

from steeragents.console import exec_py
from steeragents.toolsets import call, connect

UMG = "UMGToolSet.UMGToolSet"
ASSET = "editor_toolset.toolsets.asset.AssetTools"


def asset_exists(m, pkg_path: str) -> bool:
    return json.loads(call(m, ASSET, "exists", {"path": pkg_path}))["returnValue"]


def ensure_folder(m, folder: str):
    if not json.loads(call(m, ASSET, "exists", {"path": folder}))["returnValue"]:
        try:
            call(m, ASSET, "create_folder", {"path": folder})
        except Exception:
            pass


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


def ensure_wbp(m, folder: str, name: str, parent_cls_path: str) -> tuple[str, bool]:
    pkg = f"{folder}/{name}"
    if asset_exists(m, pkg):
        return f"{pkg}.{name}", False
    res = json.loads(call(m, UMG, "CreateWidgetBlueprint", {
        "folderPath": folder,
        "assetName": name,
        "parentClass": {"refPath": parent_cls_path},
    }))
    return res["returnValue"]["refPath"], True


def add_w(m, wbp_ref: str, cls_ref: str, name: str, parent_ref: str = None, is_var: bool = False) -> dict:
    args = {
        "widgetBlueprint": {"refPath": wbp_ref},
        "widgetClass": {"refPath": cls_ref},
        "widgetDisplayName": name,
    }
    if parent_ref:
        args["parentWidget"] = {"refPath": parent_ref}
    info = json.loads(call(m, UMG, "AddWidget", args))["returnValue"]
    w_ref = info["widget"]["refPath"]
    if is_var:
        call(m, UMG, "ToggleWidgetAsVariable", {
            "widgetBlueprint": {"refPath": wbp_ref},
            "widget": {"refPath": w_ref},
            "bIsVariable": True,
        })
    return info


def compile_wbp(m, wbp_ref: str):
    ok = json.loads(call(m, UMG, "CompileWidgetBlueprint", {
        "widgetBlueprint": {"refPath": wbp_ref}
    }))["returnValue"]
    print(f"  [Compile] {wbp_ref} -> {ok}")
    if not ok:
        raise RuntimeError(f"Failed to compile {wbp_ref}")


def py(m, code: str):
    preamble = """import unreal
def mk_font(fnt, sz, spacing=0):
    fi = unreal.SlateFontInfo()
    fi.font_object = fnt
    fi.size = sz
    fi.typeface_font_name = 'Regular'
    fi.letter_spacing = spacing
    return fi
def mk_sz(w, h):
    dv = unreal.DeprecateSlateVector2D()
    dv.import_text(f'(X={w},Y={h})')
    return dv
"""
    out = exec_py(m, preamble + "\n" + code, timeout=120)
    if out.strip():
        print(out.rstrip())
    if "Traceback (most recent call last)" in out:
        raise RuntimeError(f"Editor Python failed:\n{out}")


def build_all():
    m = connect()
    try:
        # ------------------------------------------------------------
        # 0. 目录与基础纹理 / 主题资产同步
        # ------------------------------------------------------------
        print("=== [0] Syncing Theme & Texture Assets ===")
        py(m, """
import os, struct, zlib, unreal
EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

for folder in ['/Game/UI/Hall', '/Game/UI/Route', '/Game/UI/Battle', '/Game/UI/Textures', '/Game/UI/Fonts']:
    if not EAL.does_directory_exist(folder):
        EAL.make_directory(folder)

# 确保 T_UI_Ring 存在
if not EAL.does_asset_exist('/Game/UI/Textures/T_UI_Ring'):
    src_dir = 'D:/EbbingRainbow/Content/UI/Textures/Source'
    os.makedirs(src_dir, exist_ok=True)
    png_path = src_dir + '/T_UI_Ring.png'
    w, h = 9, 9
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            b = (x == 0 or y == 0 or x == w - 1 or y == h - 1)
            raw.extend((255, 255, 255, 255 if b else 0))
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    png = b'\\x89PNG\\r\\n\\x1a\\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(bytes(raw))) + chunk(b'IEND', b'')
    with open(png_path, 'wb') as f:
        f.write(png)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', png_path)
    task.set_editor_property('destination_path', '/Game/UI/Textures')
    task.set_editor_property('destination_name', 'T_UI_Ring')
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    tools.import_asset_tasks([task])

ring = EAL.load_asset('/Game/UI/Textures/T_UI_Ring')
if ring:
    ring.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    ring.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    ring.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    ring.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
    EAL.save_asset('/Game/UI/Textures/T_UI_Ring')

da = EAL.load_asset('/Game/UI/DA_ERUITheme')
if da:
    da.set_editor_property('composite_font_asset', EAL.load_asset('/Game/UI/Fonts/F_ERUI_Composite'))
    da.set_editor_property('white_texture_asset', EAL.load_asset('/Game/UI/Textures/T_UI_White'))
    da.set_editor_property('ring_texture_asset', ring)
    da.set_editor_property('scrim_texture_asset', EAL.load_asset('/Game/UI/Textures/T_UI_Scrim'))
    da.set_editor_property('default_portrait_asset', EAL.load_asset('/Game/UI/Textures/T_Character'))
    EAL.save_asset('/Game/UI/DA_ERUITheme')
    print('  DA_ERUITheme updated.')
""")

        # ------------------------------------------------------------
        # 1. Hall UI: WBP_HallButton + WBP_HallHUD
        # ------------------------------------------------------------
        print("=== [1] Building Hall UI Blueprints ===")
        wbp_hall_btn = recreate_wbp(m, "/Game/UI/Hall", "WBP_HallButton", "/Script/EbbingRainbow.ERHallButton")
        hb_btn = add_w(m, wbp_hall_btn, "/Script/UMG.Button", "HitButton", is_var=True)["widget"]["refPath"]
        hb_sz = add_w(m, wbp_hall_btn, "/Script/UMG.SizeBox", "ButtonSizeBox", hb_btn, is_var=True)["widget"]["refPath"]
        hb_glow = add_w(m, wbp_hall_btn, "/Script/UMG.Border", "GlowBorder", hb_sz, is_var=True)["widget"]["refPath"]
        hb_body = add_w(m, wbp_hall_btn, "/Script/UMG.Border", "BodyBorder", hb_glow, is_var=True)["widget"]["refPath"]
        hb_row = add_w(m, wbp_hall_btn, "/Script/UMG.HorizontalBox", "ContentRow", hb_body)["widget"]["refPath"]
        hb_lsp = add_w(m, wbp_hall_btn, "/Script/UMG.Spacer", "LeftSpacer", hb_row)
        hb_led_box = add_w(m, wbp_hall_btn, "/Script/UMG.SizeBox", "LedSizeBox", hb_row)
        hb_led = add_w(m, wbp_hall_btn, "/Script/UMG.Border", "LedBorder", hb_led_box["widget"]["refPath"], is_var=True)["widget"]["refPath"]
        hb_tcol = add_w(m, wbp_hall_btn, "/Script/UMG.VerticalBox", "TextCol", hb_row)
        hb_lbl = add_w(m, wbp_hall_btn, "/Script/UMG.TextBlock", "LabelText", hb_tcol["widget"]["refPath"], is_var=True)["widget"]["refPath"]
        hb_cap = add_w(m, wbp_hall_btn, "/Script/UMG.TextBlock", "CaptionText", hb_tcol["widget"]["refPath"], is_var=True)
        hb_rsp = add_w(m, wbp_hall_btn, "/Script/UMG.Spacer", "RightSpacer", hb_row)

        py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
def obj(p): return unreal.find_object(None, p)

btn = obj('{hb_btn}')
st = unreal.ButtonStyle()
empty = unreal.SlateBrush()
empty.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
st.normal = empty; st.hovered = empty; st.pressed = empty; st.disabled = empty
st.normal_padding = unreal.Margin(0,0,0,0); st.pressed_padding = unreal.Margin(0,0,0,0)
btn.set_editor_property('widget_style', st)
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)

sz = obj('{hb_sz}')
sz.set_width_override(264.0)

glow = obj('{hb_glow}')
glow.set_editor_property('padding', unreal.Margin(11.0, 10.0, 11.0, 10.0))
glow.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
glow.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

body = obj('{hb_body}')
body.set_editor_property('padding', unreal.Margin(20.0, 12.0, 20.0, 12.0))
body.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
body.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

fill_sz = unreal.SlateChildSize()
fill_sz.size_rule = unreal.SlateSizeRule.FILL
fill_sz.value = 1.0
obj('{hb_lsp["slot"]["refPath"]}').set_editor_property('size', fill_sz)
obj('{hb_rsp["slot"]["refPath"]}').set_editor_property('size', fill_sz)

lbox = obj('{hb_led_box["widget"]["refPath"]}')
lbox.set_width_override(9.0)
lbox.set_height_override(9.0)
obj('{hb_led_box["slot"]["refPath"]}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

obj('{hb_led}').set_editor_property('padding', unreal.Margin(0,0,0,0))

tc_slot = obj('{hb_tcol["slot"]["refPath"]}')
tc_slot.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
tc_slot.set_editor_property('padding', unreal.Margin(13.0, 0.0, 0.0, 0.0))

lbl = obj('{hb_lbl}')
lbl.set_editor_property('text', '航线管理')
lbl.set_editor_property('font', mk_font(fnt, 15))
lbl.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.96, 0.98, 1.0, 0.96)))
lbl.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

cap = obj('{hb_cap["widget"]["refPath"]}')
cap.set_editor_property('text', 'R O U T E S')
cf = mk_font(fnt, 8)
cf.letter_spacing = 120
cap.set_editor_property('font', cf)
cap.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.78, 0.85, 0.95, 0.58)))
cap.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{hb_cap["slot"]["refPath"]}').set_editor_property('padding', unreal.Margin(0.0, 2.0, 0.0, 0.0))
""")
        compile_wbp(m, wbp_hall_btn)

        wbp_hall_hud = recreate_wbp(m, "/Game/UI/Hall", "WBP_HallHUD", "/Script/EbbingRainbow.ERHallHUD")
        hh_root = add_w(m, wbp_hall_hud, "/Script/UMG.CanvasPanel", "RootCanvas", is_var=True)["widget"]["refPath"]
        hh_dash = add_w(m, wbp_hall_hud, "/Script/UMG.CanvasPanel", "DashboardContainer", hh_root, is_var=True)
        hh_dock = add_w(m, wbp_hall_hud, "/Script/UMG.HorizontalBox", "DockBox", hh_dash["widget"]["refPath"], is_var=True)
        hh_pbtn = add_w(m, wbp_hall_hud, wbp_hall_btn + "_C", "PersonnelButton", hh_dock["widget"]["refPath"], is_var=True)["widget"]["refPath"]
        hh_sp = add_w(m, wbp_hall_hud, "/Script/UMG.Spacer", "DockSpacer", hh_dock["widget"]["refPath"])["widget"]["refPath"]
        hh_rbtn = add_w(m, wbp_hall_hud, wbp_hall_btn + "_C", "RoutesButton", hh_dock["widget"]["refPath"], is_var=True)["widget"]["refPath"]
        hh_zen = add_w(m, wbp_hall_hud, "/Script/UMG.Border", "ZenHintBorder", hh_root, is_var=True)
        hh_zent = add_w(m, wbp_hall_hud, "/Script/UMG.TextBlock", "ZenHintText", hh_zen["widget"]["refPath"], is_var=True)["widget"]["refPath"]

        py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
def obj(p): return unreal.find_object(None, p)

obj('{hh_root}').set_editor_property('visibility', unreal.SlateVisibility.VISIBLE)

dash = obj('{hh_dash["widget"]["refPath"]}')
dash.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
ds = obj('{hh_dash["slot"]["refPath"]}')
ds.set_anchors(unreal.Anchors(unreal.Vector2D(0,0), unreal.Vector2D(1,1)))
ds.set_offsets(unreal.Margin(0,0,0,0))

ks = obj('{hh_dock["slot"]["refPath"]}')
ks.set_anchors(unreal.Anchors(unreal.Vector2D(0.5,1.0), unreal.Vector2D(0.5,1.0)))
ks.set_alignment(unreal.Vector2D(0.5, 1.0))
ks.set_offsets(unreal.Margin(0.0, -56.0, 0.0, 0.0))
ks.set_auto_size(True)

pbtn = obj('{hh_pbtn}')
pbtn.set_editor_property('label', '人员管理')
pbtn.set_editor_property('caption', 'P E R S O N N E L')
pbtn.set_editor_property('accent_color', unreal.LinearColor(0.68, 0.78, 0.94, 1.0))
pbtn.set_editor_property('primary', False)

obj('{hh_sp}').set_editor_property('size', unreal.Vector2D(24.0, 1.0))

rbtn = obj('{hh_rbtn}')
rbtn.set_editor_property('label', '航线管理')
rbtn.set_editor_property('caption', 'R O U T E S')
rbtn.set_editor_property('accent_color', unreal.LinearColor(0.32, 0.76, 1.0, 1.0))
rbtn.set_editor_property('primary', True)

zb = obj('{hh_zen["widget"]["refPath"]}')
pill = unreal.SlateBrush()
pill.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX
pill.tint_color = unreal.SlateColor(unreal.LinearColor(0.04, 0.06, 0.09, 0.58))
pill.outline_settings.rounding_type = unreal.SlateBrushRoundingType.FIXED_RADIUS
pill.outline_settings.corner_radii = unreal.Vector4(18, 18, 18, 18)
pill.outline_settings.width = 1.0
pill.outline_settings.color = unreal.SlateColor(unreal.LinearColor(0.90, 0.95, 1.0, 0.16))
zb.set_editor_property('background', pill)
zb.set_editor_property('padding', unreal.Margin(22.0, 9.0, 22.0, 9.0))
zb.set_editor_property('visibility', unreal.SlateVisibility.HIDDEN)

zs = obj('{hh_zen["slot"]["refPath"]}')
zs.set_anchors(unreal.Anchors(unreal.Vector2D(0.5,1.0), unreal.Vector2D(0.5,1.0)))
zs.set_alignment(unreal.Vector2D(0.5, 1.0))
zs.set_offsets(unreal.Margin(0.0, -28.0, 0.0, 0.0))
zs.set_auto_size(True)

zt = obj('{hh_zent}')
zt.set_editor_property('text', '观景模式 · 点击任意位置或按 Esc 返回界面')
zf = mk_font(fnt, 11); zf.letter_spacing = 40
zt.set_editor_property('font', zf)
zt.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.88, 0.93, 1.0, 0.85)))
zt.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
""")
        compile_wbp(m, wbp_hall_hud)

        # ------------------------------------------------------------
        # 2. Route Map UI: WBP_RouteLegendItem + WBP_RouteNode + WBP_RouteHUD
        # ------------------------------------------------------------
        print("=== [2] Building Route Map UI Blueprints ===")
        wbp_route_leg = recreate_wbp(m, "/Game/UI/Route", "WBP_RouteLegendItem", "/Script/EbbingRainbow.ERRouteLegendItem")
        rl_row = add_w(m, wbp_route_leg, "/Script/UMG.HorizontalBox", "LegendRow")["widget"]["refPath"]
        rl_ic = add_w(m, wbp_route_leg, "/Script/EbbingRainbow.ERRouteTypeIcon", "TypeIcon", rl_row, is_var=True)
        rl_img = add_w(m, wbp_route_leg, "/Script/UMG.Image", "CustomIconImage", rl_row, is_var=True)
        rl_txt = add_w(m, wbp_route_leg, "/Script/UMG.TextBlock", "LabelText", rl_row, is_var=True)

        py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
white = unreal.EditorAssetLibrary.load_asset('/Game/UI/Textures/T_UI_White')
def obj(p): return unreal.find_object(None, p)

obj('{rl_row}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

obj('{rl_ic["widget"]["refPath"]}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
s_ic = obj('{rl_ic["slot"]["refPath"]}')
s_ic.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
s_ic.set_editor_property('padding', unreal.Margin(0, 0, 6.0, 0))

img = obj('{rl_img["widget"]["refPath"]}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(18, 18)
img.set_editor_property('brush', b)
img.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
s_im = obj('{rl_img["slot"]["refPath"]}')
s_im.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
s_im.set_editor_property('padding', unreal.Margin(0, 0, 6.0, 0))

txt = obj('{rl_txt["widget"]["refPath"]}')
txt.set_editor_property('text', '安全节点')
f = mk_font(fnt, 10); f.letter_spacing = 40
txt.set_editor_property('font', f)
txt.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.74, 0.82, 0.94, 0.70)))
txt.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{rl_txt["slot"]["refPath"]}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
        compile_wbp(m, wbp_route_leg)

        wbp_route_node = recreate_wbp(m, "/Game/UI/Route", "WBP_RouteNode", "/Script/EbbingRainbow.ERRouteNode")
        rn_btn = add_w(m, wbp_route_node, "/Script/UMG.Button", "HitButton", is_var=True)["widget"]["refPath"]
        rn_sz = add_w(m, wbp_route_node, "/Script/UMG.SizeBox", "NodeSizeBox", rn_btn, is_var=True)["widget"]["refPath"]
        rn_col = add_w(m, wbp_route_node, "/Script/UMG.VerticalBox", "NodeCol", rn_sz)["widget"]["refPath"]
        rn_bsz = add_w(m, wbp_route_node, "/Script/UMG.SizeBox", "BadgeSizeBox", rn_col, is_var=True)
        rn_ov = add_w(m, wbp_route_node, "/Script/UMG.Overlay", "BadgeOverlay", rn_bsz["widget"]["refPath"])["widget"]["refPath"]
        rn_sig = add_w(m, wbp_route_node, "/Script/EbbingRainbow.ERRouteNodeSigil", "Sigil", rn_ov, is_var=True)
        rn_img = add_w(m, wbp_route_node, "/Script/UMG.Image", "CustomIconImage", rn_ov, is_var=True)
        rn_gly = add_w(m, wbp_route_node, "/Script/UMG.TextBlock", "GlyphText", rn_ov, is_var=True)
        rn_lbl = add_w(m, wbp_route_node, "/Script/UMG.TextBlock", "LabelText", rn_col, is_var=True)

        py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
white = unreal.EditorAssetLibrary.load_asset('/Game/UI/Textures/T_UI_White')
def obj(p): return unreal.find_object(None, p)

btn = obj('{rn_btn}')
st = unreal.ButtonStyle()
empty = unreal.SlateBrush(); empty.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
st.normal = empty; st.hovered = empty; st.pressed = empty; st.disabled = empty
st.normal_padding = unreal.Margin(0,0,0,0); st.pressed_padding = unreal.Margin(0,0,0,0)
btn.set_editor_property('widget_style', st)
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)

sz = obj('{rn_sz}')
sz.set_width_override(120.0)
sz.set_height_override(150.0)

bsz = obj('{rn_bsz["widget"]["refPath"]}')
bsz.set_width_override(88.0)
bsz.set_height_override(88.0)
bs_slot = obj('{rn_bsz["slot"]["refPath"]}')
bs_slot.set_editor_property('horizontal_alignment', unreal.VerticalAlignment.V_ALIGN_FILL if False else unreal.HorizontalAlignment.H_ALIGN_CENTER)
bs_slot.set_editor_property('padding', unreal.Margin(0, 31.0, 0, 0))

obj('{rn_sig["widget"]["refPath"]}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
sg_s = obj('{rn_sig["slot"]["refPath"]}')
sg_s.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
sg_s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

im = obj('{rn_img["widget"]["refPath"]}')
ib = unreal.SlateBrush(); ib.set_editor_property('resource_object', white); ib.image_size = mk_sz(36, 36)
im.set_editor_property('brush', ib)
im.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
im_s = obj('{rn_img["slot"]["refPath"]}')
im_s.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
im_s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

gl = obj('{rn_gly["widget"]["refPath"]}')
gl.set_editor_property('text', '战')
gl.set_editor_property('font', mk_font(fnt, 23))
gl.set_editor_property('justification', unreal.TextJustify.CENTER)
gl.set_editor_property('shadow_offset', unreal.Vector2D(0, 1))
gl.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.65))
gl.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
gl_s = obj('{rn_gly["slot"]["refPath"]}')
gl_s.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
gl_s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

lb = obj('{rn_lbl["widget"]["refPath"]}')
lb.set_editor_property('text', '战斗节点')
lf = mk_font(fnt, 9); lf.letter_spacing = 40
lb.set_editor_property('font', lf)
lb.set_editor_property('justification', unreal.TextJustify.CENTER)
lb.set_editor_property('shadow_offset', unreal.Vector2D(0, 1))
lb.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.7))
lb.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
lb_s = obj('{rn_lbl["slot"]["refPath"]}')
lb_s.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
lb_s.set_editor_property('padding', unreal.Margin(0, 5.0, 0, 0))
""")
        compile_wbp(m, wbp_route_node)

        wbp_route_hud = recreate_wbp(m, "/Game/UI/Route", "WBP_RouteHUD", "/Script/EbbingRainbow.ERRouteHUD")
        rh_root = add_w(m, wbp_route_hud, "/Script/UMG.CanvasPanel", "RootCanvas", is_var=True)["widget"]["refPath"]
        rh_top = add_w(m, wbp_route_hud, "/Script/UMG.Image", "TopScrim", rh_root, is_var=True)
        rh_bot = add_w(m, wbp_route_hud, "/Script/UMG.Image", "BottomScrim", rh_root, is_var=True)
        rh_scale = add_w(m, wbp_route_hud, "/Script/UMG.ScaleBox", "MapScaleBox", rh_root, is_var=True)
        rh_msz = add_w(m, wbp_route_hud, "/Script/UMG.SizeBox", "MapSizeBox", rh_scale["widget"]["refPath"], is_var=True)["widget"]["refPath"]
        rh_mov = add_w(m, wbp_route_hud, "/Script/UMG.Overlay", "MapOverlay", rh_msz, is_var=True)["widget"]["refPath"]
        rh_rul = add_w(m, wbp_route_hud, "/Script/EbbingRainbow.ERRouteLayerRuler", "Ruler", rh_mov, is_var=True)
        rh_edg = add_w(m, wbp_route_hud, "/Script/EbbingRainbow.ERRouteEdgeLayer", "EdgeLayer", rh_mov, is_var=True)
        rh_ncv = add_w(m, wbp_route_hud, "/Script/UMG.CanvasPanel", "NodeCanvas", rh_mov, is_var=True)

        rh_hrow = add_w(m, wbp_route_hud, "/Script/UMG.HorizontalBox", "HeaderRow", rh_root)
        rh_hrule = add_w(m, wbp_route_hud, "/Script/UMG.Image", "TitleAccentRule", rh_hrow["widget"]["refPath"], is_var=True)
        rh_hcol = add_w(m, wbp_route_hud, "/Script/UMG.VerticalBox", "HeaderCol", rh_hrow["widget"]["refPath"])["widget"]["refPath"]
        rh_title = add_w(m, wbp_route_hud, "/Script/UMG.TextBlock", "TitleText", rh_hcol, is_var=True)["widget"]["refPath"]
        rh_sub = add_w(m, wbp_route_hud, "/Script/UMG.TextBlock", "SubtitleText", rh_hcol, is_var=True)

        rh_scol = add_w(m, wbp_route_hud, "/Script/UMG.VerticalBox", "StatusCol", rh_root)
        rh_prog = add_w(m, wbp_route_hud, "/Script/UMG.TextBlock", "ProgressText", rh_scol["widget"]["refPath"], is_var=True)
        rh_stat = add_w(m, wbp_route_hud, "/Script/UMG.TextBlock", "StatText", rh_scol["widget"]["refPath"], is_var=True)

        rh_leg = add_w(m, wbp_route_hud, "/Script/UMG.HorizontalBox", "LegendBox", rh_root, is_var=True)
        rh_hint = add_w(m, wbp_route_hud, "/Script/UMG.TextBlock", "HintText", rh_root, is_var=True)
        rh_nbd = add_w(m, wbp_route_hud, "/Script/UMG.Border", "NotificationBorder", rh_root, is_var=True)
        rh_ntx = add_w(m, wbp_route_hud, "/Script/UMG.TextBlock", "NotificationText", rh_nbd["widget"]["refPath"], is_var=True)["widget"]["refPath"]

        py(m, f"""
import unreal
EAL = unreal.EditorAssetLibrary
fnt = EAL.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
white = EAL.load_asset('/Game/UI/Textures/T_UI_White')
scrim = EAL.load_asset('/Game/UI/Textures/T_UI_Scrim')
def obj(p): return unreal.find_object(None, p)

obj('{rh_root}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

sb_top = unreal.SlateBrush(); sb_top.set_editor_property('resource_object', scrim)
ts = obj('{rh_top["widget"]["refPath"]}')
ts.set_editor_property('brush', sb_top)
ts.set_editor_property('color_and_opacity', unreal.LinearColor(0.0, 0.004, 0.012, 0.55))
ts.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
ts_s = obj('{rh_top["slot"]["refPath"]}')
ts_s.set_anchors(unreal.Anchors(unreal.Vector2D(0,0), unreal.Vector2D(1,0)))
ts_s.set_offsets(unreal.Margin(0, 0, 0, 160.0))

bs = obj('{rh_bot["widget"]["refPath"]}')
bs.set_editor_property('brush', sb_top)
bs.set_editor_property('color_and_opacity', unreal.LinearColor(0.0, 0.004, 0.012, 0.55))
bs.set_editor_property('render_transform_pivot', unreal.Vector2D(0.5, 0.5))
rt = unreal.WidgetTransform(); rt.scale = unreal.Vector2D(1.0, -1.0)
bs.set_editor_property('render_transform', rt)
bs.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
bs_s = obj('{rh_bot["slot"]["refPath"]}')
bs_s.set_anchors(unreal.Anchors(unreal.Vector2D(0,1), unreal.Vector2D(1,1)))
bs_s.set_alignment(unreal.Vector2D(0, 1))
bs_s.set_offsets(unreal.Margin(0, -140.0, 0, 0))

sc = obj('{rh_scale["widget"]["refPath"]}')
sc.set_editor_property('stretch', unreal.Stretch.SCALE_TO_FIT)
sc.set_editor_property('stretch_direction', unreal.StretchDirection.BOTH)
sc.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
sc_s = obj('{rh_scale["slot"]["refPath"]}')
sc_s.set_anchors(unreal.Anchors(unreal.Vector2D(0,0), unreal.Vector2D(1,1)))
sc_s.set_offsets(unreal.Margin(0.0, 88.0, 0.0, 58.0))

msz = obj('{rh_msz}')
msz.set_width_override(2040.0)
msz.set_height_override(980.0)
msz.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{rh_mov}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

for info in [{rh_rul}, {rh_edg}, {rh_ncv}]:
    obj(info['widget']['refPath']).set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    sl = obj(info['slot']['refPath'])
    sl.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
    sl.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

hr_s = obj('{rh_hrow["slot"]["refPath"]}')
hr_s.set_anchors(unreal.Anchors(unreal.Vector2D(0,0), unreal.Vector2D(0,0)))
hr_s.set_offsets(unreal.Margin(34.0, 26.0, 0, 0))
hr_s.set_auto_size(True)

hrl = obj('{rh_hrule["widget"]["refPath"]}')
rb = unreal.SlateBrush(); rb.set_editor_property('resource_object', white); rb.image_size = mk_sz(2.0, 36.0)
hrl.set_editor_property('brush', rb)
hrl.set_editor_property('color_and_opacity', unreal.LinearColor(1.0, 0.80, 0.34, 0.85))
hrl.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
hrl_s = obj('{rh_hrule["slot"]["refPath"]}')
hrl_s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
hrl_s.set_editor_property('padding', unreal.Margin(0, 2.0, 12.0, 2.0))

tt = obj('{rh_title}')
tt.set_editor_property('text', '航线图')
tt.set_editor_property('font', mk_font(fnt, 24))
tt.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.93, 0.96, 1.0, 0.96)))
tt.set_editor_property('shadow_offset', unreal.Vector2D(0, 1))
tt.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.6))
tt.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

st = obj('{rh_sub["widget"]["refPath"]}')
st.set_editor_property('text', '虹 不 我 与   ·   E B B I N G   R A I N B O W')
sf = mk_font(fnt, 9); sf.letter_spacing = 80
st.set_editor_property('font', sf)
st.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.50, 0.72, 0.98, 0.62)))
st.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{rh_sub["slot"]["refPath"]}').set_editor_property('padding', unreal.Margin(1.0, 2.0, 0, 0))

sc_col = obj('{rh_scol["slot"]["refPath"]}')
sc_col.set_anchors(unreal.Anchors(unreal.Vector2D(1,0), unreal.Vector2D(1,0)))
sc_col.set_alignment(unreal.Vector2D(1, 0))
sc_col.set_offsets(unreal.Margin(-34.0, 26.0, 0, 0))
sc_col.set_auto_size(True)

pg = obj('{rh_prog["widget"]["refPath"]}')
pg.set_editor_property('text', '第 1 / 10 层')
pg.set_editor_property('font', mk_font(fnt, 17))
pg.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(1.0, 0.86, 0.46, 0.98)))
pg.set_editor_property('shadow_offset', unreal.Vector2D(0, 1))
pg.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.6))
pg.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{rh_prog["slot"]["refPath"]}').set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_RIGHT)

stt = obj('{rh_stat["widget"]["refPath"]}')
stt.set_editor_property('text', '战斗 0 场   ·   种子 1024')
stf = mk_font(fnt, 10); stf.letter_spacing = 40
stt.set_editor_property('font', stf)
stt.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.74, 0.82, 0.94, 0.70)))
stt.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
stt_s = obj('{rh_stat["slot"]["refPath"]}')
stt_s.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_RIGHT)
stt_s.set_editor_property('padding', unreal.Margin(0, 3.0, 0, 0))

lg_s = obj('{rh_leg["slot"]["refPath"]}')
lg_s.set_anchors(unreal.Anchors(unreal.Vector2D(0,1), unreal.Vector2D(0,1)))
lg_s.set_alignment(unreal.Vector2D(0, 1))
lg_s.set_offsets(unreal.Margin(34.0, -22.0, 0, 0))
lg_s.set_auto_size(True)

ht = obj('{rh_hint["widget"]["refPath"]}')
ht.set_editor_property('text', '点击发光节点前进   ·   战斗节点将进入竞技场')
hf = mk_font(fnt, 10); hf.letter_spacing = 60
ht.set_editor_property('font', hf)
ht.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.62, 0.72, 0.88, 0.46)))
ht.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
ht_s = obj('{rh_hint["slot"]["refPath"]}')
ht_s.set_anchors(unreal.Anchors(unreal.Vector2D(0.5,1), unreal.Vector2D(0.5,1)))
ht_s.set_alignment(unreal.Vector2D(0.5, 1))
ht_s.set_offsets(unreal.Margin(0, -22.0, 0, 0))
ht_s.set_auto_size(True)

nb = obj('{rh_nbd["widget"]["refPath"]}')
npill = unreal.SlateBrush()
npill.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX
npill.tint_color = unreal.SlateColor(unreal.LinearColor(0.020, 0.034, 0.062, 0.86))
npill.outline_settings.rounding_type = unreal.SlateBrushRoundingType.FIXED_RADIUS
npill.outline_settings.corner_radii = unreal.Vector4(16, 16, 16, 16)
npill.outline_settings.width = 1.0
npill.outline_settings.color = unreal.SlateColor(unreal.LinearColor(0.80, 0.90, 1.00, 0.24))
nb.set_editor_property('background', npill)
nb.set_editor_property('padding', unreal.Margin(22.0, 9.0, 22.0, 9.0))
nb.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
nb_s = obj('{rh_nbd["slot"]["refPath"]}')
nb_s.set_anchors(unreal.Anchors(unreal.Vector2D(0.5,0), unreal.Vector2D(0.5,0)))
nb_s.set_alignment(unreal.Vector2D(0.5, 0))
nb_s.set_offsets(unreal.Margin(0, 30.0, 0, 0))
nb_s.set_auto_size(True)

nt = obj('{rh_ntx}')
nt.set_editor_property('text', '安全节点 · 无事发生')
nt.set_editor_property('font', mk_font(fnt, 13))
nt.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.93, 0.96, 1.0, 0.96)))
nt.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
""")
        compile_wbp(m, wbp_route_hud)
        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
wbp = EAL.load_asset('/Game/UI/Route/WBP_RouteHUD')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('node_widget_class', EAL.load_asset('/Game/UI/Route/WBP_RouteNode').generated_class())
cdo.set_editor_property('legend_item_class', EAL.load_asset('/Game/UI/Route/WBP_RouteLegendItem').generated_class())
""")
        compile_wbp(m, wbp_route_hud)

        # ------------------------------------------------------------
        # 3. Battle UI New Sub-Widgets: WBP_PipBar, WBP_BattleResultRow,
        #    WBP_SkillTooltip, WBP_InitiativeToken, WBP_FloatingText, BP_BattleFloatingText
        # ------------------------------------------------------------
        print("=== [3] Building & Upgrading Battle UI Blueprints ===")
        wbp_pip = recreate_wbp(m, "/Game/UI/Battle", "WBP_PipBar", "/Script/EbbingRainbow.ERPipBar")
        pip_box = add_w(m, wbp_pip, "/Script/UMG.HorizontalBox", "PipBox", is_var=True)["widget"]["refPath"]
        py(m, f"""
import unreal
unreal.find_object(None, '{pip_box}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
""")
        compile_wbp(m, wbp_pip)

        wbp_res_row = recreate_wbp(m, "/Game/UI/Battle", "WBP_BattleResultRow", "/Script/EbbingRainbow.ERBattleResultRow")
        rr_box = add_w(m, wbp_res_row, "/Script/UMG.HorizontalBox", "RowBox")["widget"]["refPath"]
        rr_lbl = add_w(m, wbp_res_row, "/Script/UMG.TextBlock", "LabelText", rr_box, is_var=True)
        rr_val = add_w(m, wbp_res_row, "/Script/UMG.TextBlock", "ValueText", rr_box, is_var=True)
        py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
def obj(p): return unreal.find_object(None, p)
obj('{rr_box}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

l = obj('{rr_lbl["widget"]["refPath"]}')
l.set_editor_property('text', '存活')
lf = mk_font(fnt, 10); lf.letter_spacing = 240
l.set_editor_property('font', lf)
l.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.90, 0.87, 0.34)))
l.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
ls = obj('{rr_lbl["slot"]["refPath"]}')
ls.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ls.set_editor_property('padding', unreal.Margin(0, 0, 14.0, 0))

v = obj('{rr_val["widget"]["refPath"]}')
v.set_editor_property('text', '3 / 3')
vf = mk_font(fnt, 12); vf.letter_spacing = 120
v.set_editor_property('font', vf)
v.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.90, 0.87, 1.0)))
v.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{rr_val["slot"]["refPath"]}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
        compile_wbp(m, wbp_res_row)

        wbp_tip = recreate_wbp(m, "/Game/UI/Battle", "WBP_SkillTooltip", "/Script/EbbingRainbow.ERSkillTooltip")
        tp_ov = add_w(m, wbp_tip, "/Script/UMG.Overlay", "TooltipOverlay")["widget"]["refPath"]
        tp_bg = add_w(m, wbp_tip, "/Script/UMG.Image", "TooltipBg", tp_ov, is_var=True)
        tp_col = add_w(m, wbp_tip, "/Script/UMG.VerticalBox", "TipCol", tp_ov)
        tp_rule = add_w(m, wbp_tip, "/Script/UMG.Image", "AccentRule", tp_col["widget"]["refPath"], is_var=True)
        tp_ttl = add_w(m, wbp_tip, "/Script/UMG.TextBlock", "TitleText", tp_col["widget"]["refPath"], is_var=True)["widget"]["refPath"]
        tp_dsc = add_w(m, wbp_tip, "/Script/UMG.TextBlock", "DescriptionText", tp_col["widget"]["refPath"], is_var=True)
        tp_mta = add_w(m, wbp_tip, "/Script/UMG.TextBlock", "MetaText", tp_col["widget"]["refPath"], is_var=True)
        py(m, f"""
import unreal
EAL = unreal.EditorAssetLibrary
fnt = EAL.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
white = EAL.load_asset('/Game/UI/Textures/T_UI_White')
def obj(p): return unreal.find_object(None, p)

obj('{tp_ov}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
bg = obj('{tp_bg["widget"]["refPath"]}')
bb = unreal.SlateBrush(); bb.set_editor_property('resource_object', white)
bg.set_editor_property('brush', bb)
bg.set_editor_property('color_and_opacity', unreal.LinearColor(0.030, 0.035, 0.043, 0.88))
bg.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
bgs = obj('{tp_bg["slot"]["refPath"]}')
bgs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
bgs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

obj('{tp_col["slot"]["refPath"]}').set_editor_property('padding', unreal.Margin(16.0, 10.0, 16.0, 10.0))

rl = obj('{tp_rule["widget"]["refPath"]}')
rb = unreal.SlateBrush(); rb.set_editor_property('resource_object', white); rb.image_size = mk_sz(28.0, 2.0)
rl.set_editor_property('brush', rb)
rl.set_editor_property('color_and_opacity', unreal.LinearColor(0.94, 0.72, 0.30, 1.0))
rl.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
rls = obj('{tp_rule["slot"]["refPath"]}')
rls.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_LEFT)
rls.set_editor_property('padding', unreal.Margin(0, 0, 0, 6.0))

ttl = obj('{tp_ttl}')
ttl.set_editor_property('text', '相位斩击')
ttl.set_editor_property('font', mk_font(fnt, 14))
ttl.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.90, 0.87, 1.0)))
ttl.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

dsc = obj('{tp_dsc["widget"]["refPath"]}')
dsc.set_editor_property('text', '突进劈砍单体目标')
dsc.set_editor_property('font', mk_font(fnt, 12))
dsc.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.90, 0.87, 0.65)))
dsc.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{tp_dsc["slot"]["refPath"]}').set_editor_property('padding', unreal.Margin(0, 4.0, 0, 0))

mta = obj('{tp_mta["widget"]["refPath"]}')
mta.set_editor_property('text', '射 300 · 伤 35 · 2 AP')
mf = mk_font(fnt, 10); mf.letter_spacing = 120
mta.set_editor_property('font', mf)
mta.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.90, 0.87, 0.40)))
mta.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{tp_mta["slot"]["refPath"]}').set_editor_property('padding', unreal.Margin(0, 6.0, 0, 0))
""")
        compile_wbp(m, wbp_tip)

        # WBP_InitiativeToken (独立的先攻条小标记蓝图, 与左侧编队 WBP_UnitToken 解耦供独立调尺寸/装饰)
        wbp_init_tok = recreate_wbp(m, "/Game/UI/Battle", "WBP_InitiativeToken", "/Script/EbbingRainbow.ERUnitToken")
        it_btn = add_w(m, wbp_init_tok, "/Script/UMG.Button", "HitButton", is_var=True)["widget"]["refPath"]
        it_col = add_w(m, wbp_init_tok, "/Script/UMG.VerticalBox", "Col", it_btn)["widget"]["refPath"]
        it_tick = add_w(m, wbp_init_tok, "/Script/UMG.Image", "TeamTick", it_col, is_var=True)
        it_box = add_w(m, wbp_init_tok, "/Script/UMG.SizeBox", "ChipBox", it_col, is_var=True)["widget"]["refPath"]
        it_ov = add_w(m, wbp_init_tok, "/Script/UMG.Overlay", "Ov", it_box)["widget"]["refPath"]
        it_gly = add_w(m, wbp_init_tok, "/Script/UMG.TextBlock", "GlyphText", it_ov, is_var=True)
        it_por = add_w(m, wbp_init_tok, "/Script/UMG.Image", "PortraitImage", it_ov, is_var=True)
        it_act = add_w(m, wbp_init_tok, "/Script/UMG.Image", "ActiveRule", it_col, is_var=True)
        py(m, f"""
import unreal
EAL = unreal.EditorAssetLibrary
fnt = EAL.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
white = EAL.load_asset('/Game/UI/Textures/T_UI_White')
mat_hp = EAL.load_asset('/Game/UI/Materials/M_UI_PortraitHP')
t_char = EAL.load_asset('/Game/UI/Textures/T_Character')
def obj(p): return unreal.find_object(None, p)

btn = obj('{it_btn}')
st = unreal.ButtonStyle()
empty = unreal.SlateBrush(); empty.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
st.normal = empty; st.hovered = empty; st.pressed = empty; st.disabled = empty
st.normal_padding = unreal.Margin(0,0,0,0); st.pressed_padding = unreal.Margin(0,0,0,0)
btn.set_editor_property('widget_style', st)
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)

tk = obj('{it_tick["widget"]["refPath"]}')
tb = unreal.SlateBrush(); tb.set_editor_property('resource_object', white); tb.image_size = mk_sz(38.0, 2.0)
tk.set_editor_property('brush', tb)
tk.set_editor_property('color_and_opacity', unreal.LinearColor(0.42, 0.70, 0.95, 1.0))
tk.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
obj('{it_tick["slot"]["refPath"]}').set_editor_property('padding', unreal.Margin(0, 0, 0, 3.0))

bx = obj('{it_box}')
bx.set_width_override(38.0)
bx.set_height_override(38.0)

gl = obj('{it_gly["widget"]["refPath"]}')
gl.set_editor_property('text', '锋')
gl.set_editor_property('font', mk_font(fnt, 14))
gl.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.92, 0.90, 0.87, 1.0)))
gl.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
gs = obj('{it_gly["slot"]["refPath"]}')
gs.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
gs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

pr = obj('{it_por["widget"]["refPath"]}')
pb = unreal.SlateBrush(); pb.set_editor_property('resource_object', mat_hp); pb.image_size = mk_sz(38.0, 38.0)
pr.set_editor_property('brush', pb)
pr.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
ps = obj('{it_por["slot"]["refPath"]}')
ps.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
ps.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)

ac = obj('{it_act["widget"]["refPath"]}')
ab = unreal.SlateBrush(); ab.set_editor_property('resource_object', white); ab.image_size = mk_sz(38.0, 2.0)
ac.set_editor_property('brush', ab)
ac.set_editor_property('color_and_opacity', unreal.LinearColor(0.94, 0.72, 0.30, 1.0))
ac.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
obj('{it_act["slot"]["refPath"]}').set_editor_property('padding', unreal.Margin(0, 4.0, 0, 0))
""")
        compile_wbp(m, wbp_init_tok)
        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
wbp = EAL.load_asset('/Game/UI/Battle/WBP_InitiativeToken')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('default_portrait', EAL.load_asset('/Game/UI/Textures/T_Character'))
""")
        compile_wbp(m, wbp_init_tok)

        # WBP_FloatingText + BP_BattleFloatingText
        wbp_ft = recreate_wbp(m, "/Game/UI/Battle", "WBP_FloatingText", "/Script/EbbingRainbow.ERFloatingText")
        ft_ov = add_w(m, wbp_ft, "/Script/UMG.Overlay", "RootOverlay")["widget"]["refPath"]
        ft_txt = add_w(m, wbp_ft, "/Script/UMG.TextBlock", "FloatingTextBlock", ft_ov, is_var=True)
        py(m, f"""
import unreal
EAL = unreal.EditorAssetLibrary
fnt = EAL.load_asset('/Game/UI/Fonts/F_ERUI_Composite')
def obj(p): return unreal.find_object(None, p)

obj('{ft_ov}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
tb = obj('{ft_txt["widget"]["refPath"]}')
tb.set_editor_property('text', '-48 暴击')
fi = mk_font(fnt, 22)
fi.outline_settings.outline_size = 1
fi.outline_settings.outline_color = unreal.LinearColor(0, 0, 0, 0.78)
tb.set_editor_property('font', fi)
tb.set_editor_property('color_and_opacity', unreal.SlateColor(unreal.LinearColor(0.95, 0.35, 0.25, 1.0)))
tb.set_editor_property('shadow_offset', unreal.Vector2D(1.5, 1.5))
tb.set_editor_property('shadow_color_and_opacity', unreal.LinearColor(0, 0, 0, 0.85))
tb.set_editor_property('justification', unreal.TextJustify.CENTER)
tb.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
ts = obj('{ft_txt["slot"]["refPath"]}')
ts.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
ts.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
        compile_wbp(m, wbp_ft)

        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
bp_path = '/Game/UI/Battle/BP_BattleFloatingText'
if EAL.does_asset_exist(bp_path):
    bp = EAL.load_asset(bp_path)
else:
    fac = unreal.BlueprintFactory()
    fac.set_editor_property('parent_class', unreal.load_class(None, '/Script/EbbingRainbow.BattleFloatingText'))
    bp = tools.create_asset('BP_BattleFloatingText', '/Game/UI/Battle', unreal.Blueprint, fac)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('floating_widget_class', EAL.load_asset('/Game/UI/Battle/WBP_FloatingText').generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
EAL.save_asset(bp_path)
print('  BP_BattleFloatingText compiled & saved.')
""")

        # ------------------------------------------------------------
        # 4. Polish Existing 6 Battle WBPs (Preserving User Customizations!)
        # ------------------------------------------------------------
        print("=== [4] Polishing Existing Battle WBPs ===")

        # 4a. WBP_ActionButton: 修复 ArrowText 乱码 + 给 HitButton 挂上可视发丝环画刷与完整四周 Padding
        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
ring = EAL.load_asset('/Game/UI/Textures/T_UI_Ring')
def obj(p): return unreal.find_object(None, p)

btn = obj('/Game/UI/Battle/WBP_ActionButton.WBP_ActionButton:WidgetTree.HitButton')
if btn and ring:
    def box_b(col):
        b = unreal.SlateBrush()
        b.set_editor_property('resource_object', ring)
        b.draw_as = unreal.SlateBrushDrawType.BOX
        b.margin = unreal.Margin(2.0/9.0, 2.0/9.0, 2.0/9.0, 2.0/9.0)
        b.tint_color = unreal.SlateColor(col)
        return b
    st = unreal.ButtonStyle()
    st.normal = box_b(unreal.LinearColor(0.92, 0.90, 0.87, 0.24))
    st.hovered = box_b(unreal.LinearColor(0.92, 0.90, 0.87, 1.0))
    st.pressed = box_b(unreal.LinearColor(0.94, 0.72, 0.30, 1.0))
    st.disabled = box_b(unreal.LinearColor(0.92, 0.90, 0.87, 0.16))
    pad = unreal.Margin(18.0, 10.0, 18.0, 10.0)
    st.normal_padding = pad; st.pressed_padding = pad
    btn.set_editor_property('widget_style', st)
    btn.set_editor_property('cursor', unreal.MouseCursor.HAND)

lbl = obj('/Game/UI/Battle/WBP_ActionButton.WBP_ActionButton:WidgetTree.LabelText')
if lbl: lbl.set_editor_property('text', '结束回合')
arr = obj('/Game/UI/Battle/WBP_ActionButton.WBP_ActionButton:WidgetTree.ArrowText')
if arr: arr.set_editor_property('text', '→')
""")
        compile_wbp(m, "/Game/UI/Battle/WBP_ActionButton.WBP_ActionButton")

        # 4b. WBP_UnitToken: 完整保留用户调好的 46x46 PortraitImage, 补齐默认预览贴图与首字符
        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
gl = unreal.find_object(None, '/Game/UI/Battle/WBP_UnitToken.WBP_UnitToken:WidgetTree.GlyphText')
if gl: gl.set_editor_property('text', '锋')
wbp = EAL.load_asset('/Game/UI/Battle/WBP_UnitToken')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('default_portrait', EAL.load_asset('/Game/UI/Textures/T_Character'))
""")
        compile_wbp(m, "/Game/UI/Battle/WBP_UnitToken.WBP_UnitToken")

        # 4c. WBP_SkillCard: 完整保留用户调好的 92x130 CardBox 与 90x90 IconBox, 挂上默认预览剑图标与文案
        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
sword = EAL.load_asset('/Game/UI/SkillIcons/T_Gold_Sword')
gl = unreal.find_object(None, '/Game/UI/Battle/WBP_SkillCard.WBP_SkillCard:WidgetTree.GlyphText')
if gl: gl.set_editor_property('text', '斩')
cd = unreal.find_object(None, '/Game/UI/Battle/WBP_SkillCard.WBP_SkillCard:WidgetTree.CooldownText')
if cd: cd.set_editor_property('text', '2')
ic = unreal.find_object(None, '/Game/UI/Battle/WBP_SkillCard.WBP_SkillCard:WidgetTree.IconImage')
if ic and sword:
    b = ic.get_editor_property('brush')
    b.set_editor_property('resource_object', sword)
    ic.set_editor_property('brush', b)
    ic.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
wbp = EAL.load_asset('/Game/UI/Battle/WBP_SkillCard')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('preview_icon', sword)
""")
        compile_wbp(m, "/Game/UI/Battle/WBP_SkillCard.WBP_SkillCard")

        # 4d. WBP_HeroPlate: 把裸 C++ ERPipBar 替换成 WBP_PipBar 蓝图实例, 并把所有 "Text Block" 换成直观预览文本
        hp_widgets = json.loads(call(m, UMG, "GetWidgets", {
            "widgetBlueprint": {"refPath": "/Game/UI/Battle/WBP_HeroPlate.WBP_HeroPlate"}
        }))["returnValue"]["widgets"]
        for w in hp_widgets:
            if w["widgetName"] == "ApPips" and w["widgetClassPath"]["refPath"] == "/Script/EbbingRainbow.ERPipBar":
                call(m, UMG, "ReplaceWidgetWithTemplate", {
                    "widgetBlueprint": {"refPath": "/Game/UI/Battle/WBP_HeroPlate.WBP_HeroPlate"},
                    "widgetToReplace": {"refPath": w["widget"]["refPath"]},
                    "templateClass": {"refPath": "/Game/UI/Battle/WBP_PipBar.WBP_PipBar_C"},
                })
                call(m, UMG, "ToggleWidgetAsVariable", {
                    "widgetBlueprint": {"refPath": "/Game/UI/Battle/WBP_HeroPlate.WBP_HeroPlate"},
                    "widget": {"refPath": "/Game/UI/Battle/WBP_HeroPlate.WBP_HeroPlate:WidgetTree.ApPips"},
                    "bIsVariable": True,
                })
        py(m, """
import unreal
def set_txt(name, val):
    w = unreal.find_object(None, f'/Game/UI/Battle/WBP_HeroPlate.WBP_HeroPlate:WidgetTree.{name}')
    if w: w.set_editor_property('text', val)
set_txt('NameText', '观测者 · 01')
set_txt('HpNumber', '84')
set_txt('HpMaxText', '/ 100')
set_txt('BarrierText', '+15')
set_txt('ApText', '3 / 5')
set_txt('StatusText', '稳态 · 掩体')
""")
        compile_wbp(m, "/Game/UI/Battle/WBP_HeroPlate.WBP_HeroPlate")

        # 4e. WBP_BattleResult: 绑定 RowWidgetClass = WBP_BattleResultRow_C, 修复默认预览标题与副标题
        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
def set_txt(name, val):
    w = unreal.find_object(None, f'/Game/UI/Battle/WBP_BattleResult.WBP_BattleResult:WidgetTree.{name}')
    if w: w.set_editor_property('text', val)
set_txt('TitleText', '胜  利')
set_txt('SubtitleText', '敌方全灭 · 推演完成')
btn = unreal.find_object(None, '/Game/UI/Battle/WBP_BattleResult.WBP_BattleResult:WidgetTree.PrimaryButton')
if btn: btn.set_editor_property('label', '返回航线图')
wbp = EAL.load_asset('/Game/UI/Battle/WBP_BattleResult')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('row_widget_class', EAL.load_asset('/Game/UI/Battle/WBP_BattleResultRow').generated_class())
""")
        compile_wbp(m, "/Game/UI/Battle/WBP_BattleResult.WBP_BattleResult")

        # 4f. WBP_BattleHUD: 挂载 WBP_SkillTooltip + 配置先攻/编队/手牌子蓝图与移动卡图标
        hud_widgets = json.loads(call(m, UMG, "GetWidgets", {
            "widgetBlueprint": {"refPath": "/Game/UI/Battle/WBP_BattleHUD.WBP_BattleHUD"}
        }))["returnValue"]["widgets"]
        has_skill_tip = any(w["widgetName"] == "SkillTooltip" for w in hud_widgets)
        for w in hud_widgets:
            if w["widgetName"] == "TooltipRoot" and w["parent"] != "None":
                call(m, UMG, "RemoveWidget", {
                    "widgetBlueprint": {"refPath": "/Game/UI/Battle/WBP_BattleHUD.WBP_BattleHUD"},
                    "widget": {"refPath": w["widget"]["refPath"]},
                })
        if not has_skill_tip:
            st_info = add_w(m,
                            "/Game/UI/Battle/WBP_BattleHUD.WBP_BattleHUD",
                            "/Game/UI/Battle/WBP_SkillTooltip.WBP_SkillTooltip_C",
                            "SkillTooltip",
                            "/Game/UI/Battle/WBP_BattleHUD.WBP_BattleHUD:WidgetTree.RootCanvas",
                            is_var=True)
            py(m, f"""
import unreal
w = unreal.find_object(None, '{st_info["widget"]["refPath"]}')
w.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
s = unreal.find_object(None, '{st_info["slot"]["refPath"]}')
s.set_anchors(unreal.Anchors(unreal.Vector2D(0.5, 1.0), unreal.Vector2D(0.5, 1.0)))
s.set_alignment(unreal.Vector2D(0.5, 1.0))
s.set_offsets(unreal.Margin(0.0, -184.0, 0.0, 0.0))
s.set_auto_size(True)
""")

        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
nt = unreal.find_object(None, '/Game/UI/Battle/WBP_BattleHUD.WBP_BattleHUD:WidgetTree.NotifyText')
if nt: nt.set_editor_property('text', '观测者 施放 相位斩击 · 命中 48')
et = unreal.find_object(None, '/Game/UI/Battle/WBP_BattleHUD.WBP_BattleHUD:WidgetTree.EndTurnButton')
if et: et.set_editor_property('label', '结束回合')

wbp = EAL.load_asset('/Game/UI/Battle/WBP_BattleHUD')
cdo = unreal.get_default_object(wbp.generated_class())
cdo.set_editor_property('initiative_token_class', EAL.load_asset('/Game/UI/Battle/WBP_InitiativeToken').generated_class())
cdo.set_editor_property('squad_token_class', EAL.load_asset('/Game/UI/Battle/WBP_UnitToken').generated_class())
cdo.set_editor_property('skill_card_class', EAL.load_asset('/Game/UI/Battle/WBP_SkillCard').generated_class())
cdo.set_editor_property('move_card_icon', EAL.load_asset('/Game/UI/SkillIcons/T_Blue_Sparkle'))
cdo.set_editor_property('preview_skill_icons', [
    EAL.load_asset('/Game/UI/SkillIcons/T_Gold_Sword'),
    EAL.load_asset('/Game/UI/SkillIcons/T_Blue_Shield'),
    EAL.load_asset('/Game/UI/SkillIcons/T_Purple_Bolt'),
])
""")
        compile_wbp(m, "/Game/UI/Battle/WBP_BattleHUD.WBP_BattleHUD")

        # ------------------------------------------------------------
        # 5. Save All Assets & Write ERUISettings -> DefaultGame.ini
        # ------------------------------------------------------------
        print("=== [5] Saving All Assets & Updating Project Settings ===")
        all_assets = [
            "/Game/UI/DA_ERUITheme",
            "/Game/UI/Textures/T_UI_Ring",
            "/Game/UI/Hall/WBP_HallButton",
            "/Game/UI/Hall/WBP_HallHUD",
            "/Game/UI/Route/WBP_RouteLegendItem",
            "/Game/UI/Route/WBP_RouteNode",
            "/Game/UI/Route/WBP_RouteHUD",
            "/Game/UI/Battle/WBP_PipBar",
            "/Game/UI/Battle/WBP_BattleResultRow",
            "/Game/UI/Battle/WBP_SkillTooltip",
            "/Game/UI/Battle/WBP_InitiativeToken",
            "/Game/UI/Battle/WBP_FloatingText",
            "/Game/UI/Battle/BP_BattleFloatingText",
            "/Game/UI/Battle/WBP_ActionButton",
            "/Game/UI/Battle/WBP_UnitToken",
            "/Game/UI/Battle/WBP_SkillCard",
            "/Game/UI/Battle/WBP_HeroPlate",
            "/Game/UI/Battle/WBP_BattleResult",
            "/Game/UI/Battle/WBP_BattleHUD",
        ]
        call(m, ASSET, "save_assets", {"asset_paths": all_assets})

        py(m, """
import unreal
EAL = unreal.EditorAssetLibrary
settings_cls = unreal.load_class(None, '/Script/EbbingRainbow.ERUISettings')
cdo = unreal.get_default_object(settings_cls)
cdo.set_editor_property('theme', EAL.load_asset('/Game/UI/DA_ERUITheme'))
cdo.set_editor_property('hall_hud_widget_class', EAL.load_asset('/Game/UI/Hall/WBP_HallHUD').generated_class())
cdo.set_editor_property('route_hud_widget_class', EAL.load_asset('/Game/UI/Route/WBP_RouteHUD').generated_class())
cdo.set_editor_property('battle_hud_widget_class', EAL.load_asset('/Game/UI/Battle/WBP_BattleHUD').generated_class())
cdo.set_editor_property('battle_floating_text_class', EAL.load_asset('/Game/UI/Battle/BP_BattleFloatingText').generated_class())
print('  ERUISettings CDO updated in memory.')
""")
        print("=== ALL UI BLUEPRINTS BUILT & SAVED SUCCESSFULLY ===")
    finally:
        m.close()


if __name__ == "__main__":
    build_all()
