# -*- coding: utf-8 -*-
"""
大厅「人员管理」面板资产生成脚本 (WBP + 占位立绘).

用法 (确保 UnrealEditor 已开启, 且已编译包含 UERPersonnelPanel 的 C++):
    $env:PYTHONPATH = "D:/SteerAgents/client/src"
    & "D:/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/setup_personnel_panel.py

生成/同步:
  /Game/UI/Textures/T_UI_HeroPortrait_Placeholder    <- 根目录 IMG.PNG (占位立绘)
  /Game/UI/Hall/WBP_PersonnelStatCell                <- UERStatCell
  /Game/UI/Hall/WBP_PersonnelMemberRow               <- UERPersonnelMemberRow
  /Game/UI/Hall/WBP_PersonnelPanel                   <- UERPersonnelPanel
并把面板 CDO 的 DefaultPortrait / MemberRowClass / StatCellClass 指到上述资产。
"""

import json

from steeragents.console import exec_py
from steeragents.toolsets import call, connect

UMG = "UMGToolSet.UMGToolSet"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

FONT = "/Game/UI/Fonts/F_ERUI_Composite"
WHITE = "/Game/UI/Textures/T_UI_White"
SCRIM = "/Game/UI/Textures/T_UI_Scrim"
PORTRAIT = "/Game/UI/Textures/T_UI_HeroPortrait_Placeholder"
THEME = "/Game/UI/DA_ERUITheme"
PANEL_DIR = "/Game/UI/Hall"


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


def add_w(m, wbp_ref: str, cls_ref: str, name: str, parent_ref: str = None, is_var: bool = True) -> dict:
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


def node(m, wbp, cls, name, parent=None, var=True):
    """返回 (widget_ref, slot_ref)"""
    info = add_w(m, wbp, cls, name, parent, var)
    slot = info.get("slot")
    if isinstance(slot, dict):
        slot_ref = slot.get("refPath")
    elif isinstance(slot, str) and slot not in ("", "None"):
        slot_ref = slot
    else:
        slot_ref = None
    return info["widget"]["refPath"], slot_ref


def compile_wbp(m, wbp_ref: str):
    ok = json.loads(call(m, UMG, "CompileWidgetBlueprint", {
        "widgetBlueprint": {"refPath": wbp_ref}
    }))["returnValue"]
    print(f"  [Compile] {wbp_ref} -> {ok}")
    if not ok:
        raise RuntimeError(f"Failed to compile {wbp_ref}")
    # UMG ToolSet 改树/改属性不会把包置脏, save_assets 的 only_if_is_dirty 会静默跳过;
    # 这里对每个 WBP 强制落盘, 否则重启编辑器后 Brush/字体/尺寸全部回到默认值。
    pkg = wbp_ref.split(".")[0]
    py(m, f"""
import unreal
unreal.EditorAssetLibrary.save_asset('{pkg}', False)
print('  saved {pkg}')
""")


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
def fill_size(v=1.0):
    s = unreal.SlateChildSize()
    s.size_rule = unreal.SlateSizeRule.FILL
    s.value = v
    return s
def obj(p): return unreal.find_object(None, p)
def rounded(radius, fill, outline_w=0.0, outline=None):
    b = unreal.SlateBrush()
    b.draw_as = unreal.SlateBrushDrawType.ROUNDED_BOX
    b.tint_color = unreal.SlateColor(fill)
    b.outline_settings.rounding_type = unreal.SlateBrushRoundingType.FIXED_RADIUS
    b.outline_settings.corner_radii = unreal.Vector4(radius, radius, radius, radius)
    b.outline_settings.width = outline_w
    if outline is not None:
        b.outline_settings.color = unreal.SlateColor(outline)
    return b
"""
    out = exec_py(m, preamble + "\n" + code, timeout=180)
    if out.strip():
        print(out.rstrip())
    if "Traceback (most recent call last)" in out:
        raise RuntimeError(f"Editor Python failed:\n{out}")


def import_portrait(m):
    print("=== [1] Importing portrait placeholder (IMG.PNG) ===")
    py(m, f"""
import os, unreal
from PIL import Image
EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
src = 'D:/EbbingRainbow/IMG.PNG'
if not os.path.isfile(src):
    raise RuntimeError('IMG.PNG not found at ' + src)
# 源图 2695x3579 太大: 立绘最大显示宽度 ~400px, 下采样到 1024 宽即可, 资产从 7.8MB 降到 ~1MB
staged_dir = 'D:/EbbingRainbow/Saved/PortraitPlaceholder'
os.makedirs(staged_dir, exist_ok=True)
staged = staged_dir + '/T_UI_HeroPortrait_Placeholder.png'
img = Image.open(src).convert('RGB')
if img.width > 1024:
    img = img.resize((1024, int(round(img.height * 1024.0 / img.width))), Image.LANCZOS)
img.save(staged, 'PNG')
print('  staged', img.size)
if not EAL.does_directory_exist('/Game/UI/Textures'):
    EAL.make_directory('/Game/UI/Textures')
task = unreal.AssetImportTask()
task.set_editor_property('filename', staged)
task.set_editor_property('destination_path', '/Game/UI/Textures')
task.set_editor_property('destination_name', 'T_UI_HeroPortrait_Placeholder')
task.set_editor_property('automated', True)
task.set_editor_property('replace_existing', True)
task.set_editor_property('save', True)
tools.import_asset_tasks([task])
tex = EAL.load_asset('{PORTRAIT}')
if not tex:
    raise RuntimeError('portrait import failed')
tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
tex.set_editor_property('srgb', True)
tex.set_editor_property('never_stream', True)
try:
    tex.set_editor_property('max_texture_size', 1024)
except Exception as e:
    print('  max_texture_size skipped:', e)
EAL.save_asset('{PORTRAIT}', False)
print('  imported:', tex.get_path_name())
""")


def build_stat_cell(m):
    print("=== [2] WBP_PersonnelStatCell ===")
    wbp = recreate_wbp(m, PANEL_DIR, "WBP_PersonnelStatCell", "/Script/EbbingRainbow.ERStatCell")
    col, _ = node(m, wbp, "/Script/UMG.VerticalBox", "CellColumn")
    label, _ = node(m, wbp, "/Script/UMG.TextBlock", "LabelText", col)
    value, _ = node(m, wbp, "/Script/UMG.TextBlock", "ValueText", col)
    py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('{FONT}')
theme = unreal.EditorAssetLibrary.load_asset('{THEME}')
def obj(p): return unreal.find_object(None, p)

obj('{col}').set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

lbl = obj('{label}')
lbl.set_editor_property('text', '完整度')
lbl.set_editor_property('font', mk_font(fnt, 9, 120))
lbl.set_editor_property('color_and_opacity', unreal.SlateColor(theme.get_editor_property('bone_tertiary')))
lbl.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

val = obj('{value}')
val.set_editor_property('text', '60')
val.set_editor_property('font', mk_font(fnt, 21, 0))
val.set_editor_property('color_and_opacity', unreal.SlateColor(theme.get_editor_property('bone')))
val.set_editor_property('visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
""")
    compile_wbp(m, wbp)


def build_member_row(m):
    print("=== [3] WBP_PersonnelMemberRow ===")
    wbp = recreate_wbp(m, PANEL_DIR, "WBP_PersonnelMemberRow", "/Script/EbbingRainbow.ERPersonnelMemberRow")
    hit, _ = node(m, wbp, "/Script/UMG.Button", "HitButton")
    size, _ = node(m, wbp, "/Script/UMG.SizeBox", "RowSize", hit, var=False)
    border, _ = node(m, wbp, "/Script/UMG.Border", "RowBorder", size)
    row, _ = node(m, wbp, "/Script/UMG.HorizontalBox", "RowContent", border, var=False)
    rule, rule_slot = node(m, wbp, "/Script/UMG.Image", "SelectRule", row)
    thumb_size, thumb_size_slot = node(m, wbp, "/Script/UMG.SizeBox", "ThumbSize", row, var=False)
    thumb_frame, _ = node(m, wbp, "/Script/UMG.Border", "ThumbFrame", thumb_size)
    thumb, _ = node(m, wbp, "/Script/UMG.Image", "PortraitThumb", thumb_frame)
    text_col, text_col_slot = node(m, wbp, "/Script/UMG.VerticalBox", "TextCol", row, var=False)
    name, _ = node(m, wbp, "/Script/UMG.TextBlock", "MemberName", text_col)
    school_row, school_row_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "SchoolRow", text_col, var=False)
    dot_size, dot_size_slot = node(m, wbp, "/Script/UMG.SizeBox", "SchoolDotSize", school_row, var=False)
    dot, _ = node(m, wbp, "/Script/UMG.Border", "SchoolDot", dot_size)
    school, school_slot = node(m, wbp, "/Script/UMG.TextBlock", "MemberSchool", school_row)
    integrity, integrity_slot = node(m, wbp, "/Script/UMG.TextBlock", "MemberIntegrity", row)

    py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('{FONT}')
white = unreal.EditorAssetLibrary.load_asset('{WHITE}')
theme = unreal.EditorAssetLibrary.load_asset('{THEME}')
def obj(p): return unreal.find_object(None, p)
BONE = theme.get_editor_property('bone')
BONE2 = theme.get_editor_property('bone_secondary')
BONE3 = theme.get_editor_property('bone_tertiary')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE

btn = obj('{hit}')
st = unreal.ButtonStyle()
empty = unreal.SlateBrush(); empty.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
st.normal = empty; st.hovered = empty; st.pressed = empty; st.disabled = empty
st.normal_padding = unreal.Margin(0,0,0,0); st.pressed_padding = unreal.Margin(0,0,0,0)
btn.set_editor_property('widget_style', st)
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)
btn.set_editor_property('visibility', unreal.SlateVisibility.VISIBLE)

obj('{size}').set_height_override(86.0)

rb = obj('{border}')
rb.set_editor_property('padding', unreal.Margin(10.0, 8.0, 10.0, 8.0))
rb.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
rb.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
rb.set_editor_property('background', rounded(10.0, unreal.LinearColor(0.055, 0.075, 0.105, 0.30), 1.0, unreal.LinearColor(0.90, 0.95, 1.0, 0.08)))

obj('{row}').set_editor_property('visibility', SELF)

rule_w = obj('{rule}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(2, 40)
rule_w.set_editor_property('brush', b)
rule_w.set_editor_property('color_and_opacity', theme.get_editor_property('accent'))
s = obj('{rule_slot}')
s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
s.set_editor_property('padding', unreal.Margin(0.0, 0.0, 9.0, 0.0))

obj('{thumb_size}').set_width_override(52.0)
obj('{thumb_size}').set_height_override(64.0)
obj('{thumb_size_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

tf = obj('{thumb_frame}')
tf.set_editor_property('padding', unreal.Margin(0,0,0,0))
tf.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
tf.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
tf.set_editor_property('background', rounded(2.0, unreal.LinearColor(0.02, 0.03, 0.04, 0.85), 1.0, unreal.LinearColor(1.0, 1.0, 1.0, 0.10)))

ti = obj('{thumb}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(52, 64)
ti.set_editor_property('brush', b)
ti.set_editor_property('color_and_opacity', unreal.LinearColor(0.10, 0.12, 0.15, 1.0))
ti.set_editor_property('visibility', SELF)

obj('{text_col}').set_editor_property('visibility', SELF)
s = obj('{text_col_slot}')
s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
s.set_editor_property('padding', unreal.Margin(12.0, 0.0, 6.0, 0.0))
s.set_editor_property('size', fill_size())

nm = obj('{name}')
nm.set_editor_property('text', '磐')
nm.set_editor_property('font', mk_font(fnt, 16, 0))
nm.set_editor_property('color_and_opacity', unreal.SlateColor(BONE))
nm.set_editor_property('visibility', SELF)

obj('{school_row}').set_editor_property('visibility', SELF)
obj('{school_row_slot}').set_editor_property('padding', unreal.Margin(0.0, 4.0, 0.0, 0.0))

obj('{dot_size}').set_width_override(6.0)
obj('{dot_size}').set_height_override(6.0)
s = obj('{dot_size_slot}')
s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
s.set_editor_property('padding', unreal.Margin(0.0, 0.0, 6.0, 0.0))

dt = obj('{dot}')
dt.set_editor_property('padding', unreal.Margin(0,0,0,0))
dt.set_editor_property('background', rounded(3.0, unreal.LinearColor(0.42, 0.70, 0.95, 1.0)))

sc = obj('{school}')
sc.set_editor_property('text', '力学 · 先锋')
sc.set_editor_property('font', mk_font(fnt, 9, 80))
sc.set_editor_property('color_and_opacity', unreal.SlateColor(BONE3))
sc.set_editor_property('visibility', SELF)
obj('{school_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)

it = obj('{integrity}')
it.set_editor_property('text', '60')
it.set_editor_property('font', mk_font(fnt, 11, 0))
it.set_editor_property('justification', unreal.TextJustify.RIGHT)
it.set_editor_property('color_and_opacity', unreal.SlateColor(BONE2))
it.set_editor_property('visibility', SELF)
obj('{integrity_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
""")
    compile_wbp(m, wbp)


def build_panel(m):
    print("=== [4] WBP_PersonnelPanel ===")
    wbp = recreate_wbp(m, PANEL_DIR, "WBP_PersonnelPanel", "/Script/EbbingRainbow.ERPersonnelPanel")

    root, root_slot = node(m, wbp, "/Script/UMG.CanvasPanel", "RootCanvas")
    scrim, scrim_slot = node(m, wbp, "/Script/UMG.Image", "ScrimImage", root)
    panel, panel_slot = node(m, wbp, "/Script/UMG.Border", "PanelBorder", root)

    col, _ = node(m, wbp, "/Script/UMG.VerticalBox", "PanelColumn", panel, var=False)
    header, header_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "HeaderRow", col, var=False)
    tick_size, tick_slot = node(m, wbp, "/Script/UMG.SizeBox", "AccentTickSize", header, var=False)
    tick, _ = node(m, wbp, "/Script/UMG.Border", "AccentTick", tick_size, var=False)
    title_col, title_col_slot = node(m, wbp, "/Script/UMG.VerticalBox", "TitleCol", header, var=False)
    header_title, _ = node(m, wbp, "/Script/UMG.TextBlock", "HeaderTitle", title_col)
    header_caption, _ = node(m, wbp, "/Script/UMG.TextBlock", "HeaderCaption", title_col)
    header_spacer, header_spacer_slot = node(m, wbp, "/Script/UMG.Spacer", "HeaderSpacer", header, var=False)
    status_col, status_col_slot = node(m, wbp, "/Script/UMG.VerticalBox", "StatusCol", header, var=False)
    summary, _ = node(m, wbp, "/Script/UMG.TextBlock", "SquadSummaryText", status_col)
    state, state_slot = node(m, wbp, "/Script/UMG.TextBlock", "SquadStateText", status_col)
    close_btn, close_btn_slot = node(m, wbp, "/Script/UMG.Button", "CloseButton", header)
    close_size, _ = node(m, wbp, "/Script/UMG.SizeBox", "CloseSize", close_btn, var=False)
    close_ring, _ = node(m, wbp, "/Script/UMG.Border", "CloseRing", close_size, var=False)
    close_glyph, _ = node(m, wbp, "/Script/UMG.TextBlock", "CloseGlyphText", close_ring)

    header_rule, header_rule_slot = node(m, wbp, "/Script/UMG.Image", "HeaderRule", col)

    body, body_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "BodyRow", col, var=False)
    rail_size, rail_size_slot = node(m, wbp, "/Script/UMG.SizeBox", "RailSize", body, var=False)
    rail, _ = node(m, wbp, "/Script/UMG.VerticalBox", "MemberRail", rail_size, var=False)
    rail_title, _ = node(m, wbp, "/Script/UMG.TextBlock", "RailTitleText", rail)
    member_list, member_list_slot = node(m, wbp, "/Script/UMG.VerticalBox", "MemberListBox", rail)
    rail_spacer, rail_spacer_slot = node(m, wbp, "/Script/UMG.Spacer", "RailSpacer", rail, var=False)
    rail_hint, rail_hint_slot = node(m, wbp, "/Script/UMG.TextBlock", "RailHintText", rail)

    rail_rule, rail_rule_slot = node(m, wbp, "/Script/UMG.Image", "RailRule", body)

    detail, detail_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "DetailRow", body, var=False)
    portrait_col, portrait_col_slot = node(m, wbp, "/Script/UMG.SizeBox", "PortraitCol", detail, var=False)
    portrait_column, _ = node(m, wbp, "/Script/UMG.VerticalBox", "PortraitColumn", portrait_col, var=False)
    frame_size, _ = node(m, wbp, "/Script/UMG.SizeBox", "PortraitFrameSize", portrait_column, var=False)
    frame, _ = node(m, wbp, "/Script/UMG.Border", "PortraitFrame", frame_size)
    overlay, _ = node(m, wbp, "/Script/UMG.Overlay", "PortraitOverlay", frame, var=False)
    portrait_image, _ = node(m, wbp, "/Script/UMG.Image", "PortraitImage", overlay)
    portrait_scrim, _ = node(m, wbp, "/Script/UMG.Image", "PortraitScrim", overlay)
    portrait_index, portrait_index_slot = node(m, wbp, "/Script/UMG.TextBlock", "PortraitIndexText", overlay)
    portrait_caption, portrait_caption_slot = node(m, wbp, "/Script/UMG.TextBlock", "PortraitCaptionText", portrait_column)

    info, info_slot = node(m, wbp, "/Script/UMG.VerticalBox", "InfoCol", detail, var=False)
    name, _ = node(m, wbp, "/Script/UMG.TextBlock", "NameText", info)
    callsign, callsign_slot = node(m, wbp, "/Script/UMG.TextBlock", "CallsignText", info)
    school_row, school_row_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "SchoolRow", info, var=False)
    chip, chip_slot = node(m, wbp, "/Script/UMG.Border", "SchoolChipBorder", school_row)
    chip_text, _ = node(m, wbp, "/Script/UMG.TextBlock", "SchoolChipText", chip)
    role, role_slot = node(m, wbp, "/Script/UMG.TextBlock", "RoleText", school_row)
    tagline, tagline_slot = node(m, wbp, "/Script/UMG.TextBlock", "TaglineText", info)
    info_rule, info_rule_slot = node(m, wbp, "/Script/UMG.Image", "InfoRule", info)
    stats_title, stats_title_slot = node(m, wbp, "/Script/UMG.TextBlock", "StatsTitleText", info)
    primary_stats, primary_stats_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "PrimaryStatsBox", info)
    secondary_stats, secondary_stats_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "SecondaryStatsBox", info)
    resist_title, resist_title_slot = node(m, wbp, "/Script/UMG.TextBlock", "ResistTitleText", info)
    resist_box, resist_box_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "ResistBox", info)
    skills_title, skills_title_slot = node(m, wbp, "/Script/UMG.TextBlock", "SkillsTitleText", info)
    skills_box, skills_box_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "SkillsBox", info)

    footer, footer_slot = node(m, wbp, "/Script/UMG.HorizontalBox", "FooterRow", col, var=False)
    footer_hint, _ = node(m, wbp, "/Script/UMG.TextBlock", "FooterHintText", footer)
    footer_spacer, footer_spacer_slot = node(m, wbp, "/Script/UMG.Spacer", "FooterSpacer", footer, var=False)
    footer_mark, _ = node(m, wbp, "/Script/UMG.TextBlock", "FooterMark", footer, var=False)

    py(m, f"""
import unreal
fnt = unreal.EditorAssetLibrary.load_asset('{FONT}')
white = unreal.EditorAssetLibrary.load_asset('{WHITE}')
scrim_tex = unreal.EditorAssetLibrary.load_asset('{SCRIM}')
theme = unreal.EditorAssetLibrary.load_asset('{THEME}')
def obj(p): return unreal.find_object(None, p)
BONE = theme.get_editor_property('bone')
BONE2 = theme.get_editor_property('bone_secondary')
BONE3 = theme.get_editor_property('bone_tertiary')
GHOST = theme.get_editor_property('bone_ghost')
INK = theme.get_editor_property('ink_panel')
INKSCRIM = theme.get_editor_property('ink_scrim')
ACCENT = theme.get_editor_property('accent')
SELF = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE

def txt(ref, text, size, color, spacing=0, justify=None, wrap=False):
    w = obj(ref)
    w.set_editor_property('text', text)
    w.set_editor_property('font', mk_font(fnt, size, spacing))
    w.set_editor_property('color_and_opacity', unreal.SlateColor(color))
    w.set_editor_property('visibility', SELF)
    if justify is not None:
        w.set_editor_property('justification', justify)
    if wrap:
        w.set_editor_property('auto_wrap_text', True)

def pad(slot_ref, top=0.0, bottom=0.0, left=0.0, right=0.0):
    obj(slot_ref).set_editor_property('padding', unreal.Margin(left, top, right, bottom))

# ---------------- 根 / 压暗 / 面板 ----------------
obj('{root}').set_editor_property('visibility', unreal.SlateVisibility.VISIBLE)
s = obj('{scrim_slot}')
s.set_anchors(unreal.Anchors(unreal.Vector2D(0,0), unreal.Vector2D(1,1)))
s.set_offsets(unreal.Margin(0,0,0,0))

sc = obj('{scrim}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(4,4)
sc.set_editor_property('brush', b)
c = INKSCRIM; c.a = 0.62
sc.set_editor_property('color_and_opacity', c)
sc.set_editor_property('visibility', SELF)

pb = obj('{panel}')
pb.set_editor_property('padding', unreal.Margin(0,0,0,0))
pb.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
pb.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
pb.set_editor_property('background', rounded(26.0, INK, 1.0, GHOST))
s = obj('{panel_slot}')
s.set_anchors(unreal.Anchors(unreal.Vector2D(0.5,0.5), unreal.Vector2D(0.5,0.5)))
s.set_alignment(unreal.Vector2D(0.5, 0.5))
s.set_offsets(unreal.Margin(0.0, 0.0, 1320.0, 800.0))
s.set_auto_size(False)

# ---------------- 标题 ----------------
pad('{header_slot}', top=26.0, left=36.0, right=30.0)
obj('{tick_size}').set_width_override(2.0)
obj('{tick_size}').set_height_override(26.0)
obj('{tick_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
tk = obj('{tick}')
tk.set_editor_property('padding', unreal.Margin(0,0,0,0))
tk.set_editor_property('background', rounded(1.0, ACCENT))

pad('{title_col_slot}', left=14.0)
obj('{title_col_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
txt('{header_title}', '人员管理', 21, BONE, 60)
txt('{header_caption}', 'P E R S O N N E L   R O S T E R', 8, BONE3, 200)

obj('{header_spacer_slot}').set_editor_property('size', fill_size())
pad('{status_col_slot}', right=18.0)
obj('{status_col_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
txt('{summary}', '编队 4 / 4', 11, BONE2, 80, unreal.TextJustify.RIGHT)
txt('{state}', '待命中 · 尚未启程', 9, BONE3, 60, unreal.TextJustify.RIGHT)
pad('{state_slot}', top=3.0)

btn = obj('{close_btn}')
st = unreal.ButtonStyle()
empty = unreal.SlateBrush(); empty.draw_as = unreal.SlateBrushDrawType.NO_DRAW_TYPE
st.normal = empty; st.hovered = empty; st.pressed = empty; st.disabled = empty
st.normal_padding = unreal.Margin(0,0,0,0); st.pressed_padding = unreal.Margin(0,0,0,0)
btn.set_editor_property('widget_style', st)
btn.set_editor_property('cursor', unreal.MouseCursor.HAND)
obj('{close_btn_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
obj('{close_size}').set_width_override(34.0)
obj('{close_size}').set_height_override(34.0)
ring = obj('{close_ring}')
ring.set_editor_property('padding', unreal.Margin(0,0,0,0))
ring.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
ring.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ring.set_editor_property('background', rounded(17.0, unreal.LinearColor(0.06, 0.08, 0.11, 0.35), 1.0, GHOST))
txt('{close_glyph}', '×', 17, BONE2, 0, unreal.TextJustify.CENTER)

b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(1,1)
hr = obj('{header_rule}')
hr.set_editor_property('brush', b)
hr.set_editor_property('color_and_opacity', GHOST)
pad('{header_rule_slot}', top=18.0, left=36.0, right=30.0)

# ---------------- 主体 ----------------
pad('{body_slot}', top=22.0, left=36.0, right=30.0)
obj('{body_slot}').set_editor_property('size', fill_size())

obj('{rail_size}').set_width_override(260.0)
obj('{rail_size_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
txt('{rail_title}', '特勤编队 · SQUAD', 9, BONE3, 140)
pad('{member_list_slot}', top=12.0)
obj('{rail_spacer_slot}').set_editor_property('size', fill_size())
txt('{rail_hint}', '点击成员查看档案', 9, unreal.LinearColor(BONE3.r, BONE3.g, BONE3.b, BONE3.a * 0.8), 40)
pad('{rail_hint_slot}', top=12.0)

rr = obj('{rail_rule}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(1,1)
rr.set_editor_property('brush', b)
rr.set_editor_property('color_and_opacity', unreal.LinearColor(GHOST.r, GHOST.g, GHOST.b, GHOST.a * 0.9))
rs = obj('{rail_rule_slot}')
rs.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
rs.set_editor_property('padding', unreal.Margin(28.0, 2.0, 28.0, 2.0))

# 立绘列
obj('{portrait_col}').set_width_override(384.0)
obj('{portrait_col_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
obj('{frame_size}').set_width_override(384.0)
obj('{frame_size}').set_height_override(510.0)
fr = obj('{frame}')
fr.set_editor_property('padding', unreal.Margin(0,0,0,0))
fr.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_FILL)
fr.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
fr.set_editor_property('background', rounded(4.0, unreal.LinearColor(0.02, 0.03, 0.045, 0.72), 1.0, GHOST))

pi = obj('{portrait_image}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(384, 510)
pi.set_editor_property('brush', b)
pi.set_editor_property('color_and_opacity', unreal.LinearColor(0.08, 0.10, 0.13, 1.0))
pi.set_editor_property('visibility', SELF)

ps = obj('{portrait_scrim}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', scrim_tex); b.image_size = mk_sz(4, 64)
ps.set_editor_property('brush', b)
c = INKSCRIM; c.a = 0.72
ps.set_editor_property('color_and_opacity', c)
ps.set_editor_property('visibility', SELF)
try:
    ps.set_editor_property('render_transform_pivot', unreal.Vector2D(0.5, 0.5))
    wt = unreal.WidgetTransform(); wt.scale = unreal.Vector2D(1.0, -1.0)
    ps.set_editor_property('render_transform', wt)
except Exception as e:
    print('  portrait scrim flip skipped:', e)

txt('{portrait_index}', '01 / 04', 10, unreal.LinearColor(BONE.r, BONE.g, BONE.b, 0.78), 120)
s = obj('{portrait_index_slot}')
s.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_LEFT)
s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_BOTTOM)
s.set_editor_property('padding', unreal.Margin(14.0, 0.0, 0.0, 12.0))

txt('{portrait_caption}', '档案影像 · ARCHIVE', 9, BONE3, 80)
pad('{portrait_caption_slot}', top=10.0, left=2.0)

# 信息列
s = obj('{info_slot}')
s.set_editor_property('size', fill_size())
s.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_FILL)
s.set_editor_property('padding', unreal.Margin(34.0, 0.0, 0.0, 0.0))

txt('{name}', '磐', 40, BONE, 30)
txt('{callsign}', 'BASALT', 12, BONE2, 260)
pad('{callsign_slot}', top=2.0)

pad('{school_row_slot}', top=12.0)
ch = obj('{chip}')
ch.set_editor_property('padding', unreal.Margin(10.0, 4.0, 10.0, 4.0))
ch.set_editor_property('horizontal_alignment', unreal.HorizontalAlignment.H_ALIGN_CENTER)
ch.set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
ch.set_editor_property('background', rounded(4.0, unreal.LinearColor(0.10, 0.12, 0.16, 0.55), 1.0, GHOST))
obj('{chip_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
txt('{chip_text}', '学派 · 力学', 10, BONE2, 40)
obj('{role_slot}').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
pad('{role_slot}', left=14.0)
txt('{role}', '先锋 · VANGUARD', 10, BONE2, 100)

txt('{tagline}', '以身为墙，替全队吃掉第一轮伤害。', 12, BONE2, 30, wrap=True)
pad('{tagline_slot}', top=14.0)

ir = obj('{info_rule}')
b = unreal.SlateBrush(); b.set_editor_property('resource_object', white); b.image_size = mk_sz(1,1)
ir.set_editor_property('brush', b)
ir.set_editor_property('color_and_opacity', GHOST)
pad('{info_rule_slot}', top=16.0)

txt('{stats_title}', '基本属性 · ATTRIBUTES', 9, BONE3, 140)
pad('{stats_title_slot}', top=16.0)
pad('{primary_stats_slot}', top=10.0)
pad('{secondary_stats_slot}', top=12.0)

txt('{resist_title}', '四学派抗性 · RESISTANCE', 9, BONE3, 140)
pad('{resist_title_slot}', top=16.0)
pad('{resist_box_slot}', top=10.0)

txt('{skills_title}', '战术技能 · LOADOUT', 9, BONE3, 140)
pad('{skills_title_slot}', top=16.0)
pad('{skills_box_slot}', top=10.0)

# 页脚
pad('{footer_slot}', top=12.0, bottom=20.0, left=36.0, right=30.0)
txt('{footer_hint}', '点击左侧成员切换档案 · Esc 关闭', 9, BONE3, 40)
obj('{footer_spacer_slot}').set_editor_property('size', fill_size())
txt('{footer_mark}', 'E R · P E R S O N N E L', 8, unreal.LinearColor(BONE3.r, BONE3.g, BONE3.b, BONE3.a * 0.7), 200)
""")
    compile_wbp(m, wbp)


def wire_cdo(m):
    print("=== [5] Wiring panel CDO ===")
    py(m, f"""
import unreal
EAL = unreal.EditorAssetLibrary
tex = EAL.load_asset('{PORTRAIT}')
row_cls = EAL.load_asset('/Game/UI/Hall/WBP_PersonnelMemberRow').generated_class()
cell_cls = EAL.load_asset('/Game/UI/Hall/WBP_PersonnelStatCell').generated_class()
bp = EAL.load_asset('/Game/UI/Hall/WBP_PersonnelPanel')
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('default_portrait', tex)
cdo.set_editor_property('member_row_class', row_cls)
cdo.set_editor_property('stat_cell_class', cell_cls)
# 注意: save_asset 默认 only_if_is_dirty=True; 改 CDO 不会自动置脏, 必须显式 False 才会落盘
EAL.save_asset('/Game/UI/Hall/WBP_PersonnelPanel', False)
print('  CDO wired: portrait / row class / stat cell class')
""")


def build_all():
    m = connect()
    try:
        ensure_folder(m, "/Game/UI/Textures")
        ensure_folder(m, PANEL_DIR)
        import_portrait(m)
        build_stat_cell(m)
        build_member_row(m)
        build_panel(m)
        wire_cdo(m)
        call(m, ASSET, "save_assets", {"asset_paths": [
            PORTRAIT,
            "/Game/UI/Hall/WBP_PersonnelStatCell",
            "/Game/UI/Hall/WBP_PersonnelMemberRow",
            "/Game/UI/Hall/WBP_PersonnelPanel",
        ]})
        print("=== PERSONNEL PANEL ASSETS BUILT ===")
    finally:
        m.close()


if __name__ == "__main__":
    build_all()
