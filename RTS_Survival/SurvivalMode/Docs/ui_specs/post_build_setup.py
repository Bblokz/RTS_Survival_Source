# Survival mission UI - post-build editor setup.
# Run in the live editor AFTER the C++ in SurvivalMode/UI is compiled and loaded:
#   UECP execute_python(code=<this file>)   (os/subprocess imports are blocked there; only unreal.* is used)
# What it does (idempotent):
#   1. Creates/updates /Game/RTS_Survival/SurvivalMode/UI/DA_SurvivalUiStyle (USurvivalUiStyleAsset) by copying the
#      Frontstahl brushes, fonts and colours from BP_W_UnitVariantSelectionMenu and BP_W_FactionUnitBrowser class
#      defaults plus a few extra slice instances.
#   2. Reparents WBP_SurvivalArmySelection / WBP_SurvivalStartLocationPicker / WBP_SurvivalHud (built from the
#      ui_specs JSON with a plain UserWidget parent) to their C++ classes and sets M_StyleAsset.
#   3. Compiles and saves everything and checks the files exist on disk.
import unreal

UI = "/Game/RTS_Survival/SurvivalMode/UI"
STYLE_NAME = "DA_SurvivalUiStyle"
FS = "/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/Instances/"
VARIANT_MENU = "/Game/RTS_Survival/FactionSelection/UI/BP_W_UnitVariantSelectionMenu"
UNIT_BROWSER = "/Game/RTS_Survival/FactionSelection/UI/BP_W_FactionUnitBrowser"
WIDGETS = {
    "WBP_SurvivalArmySelection": unreal.W_SurvivalArmySelection,
    "WBP_SurvivalStartLocationPicker": unreal.W_SurvivalStartLocationPicker,
    "WBP_SurvivalHud": unreal.W_SurvivalHud,
}


def cdo_style(bp_path):
    cls = unreal.load_object(None, bp_path + "." + bp_path.split("/")[-1] + "_C")
    return unreal.get_default_object(cls).get_editor_property("m_style")


def brush(asset_name, width, height):
    b = unreal.SlateBrush()
    b.set_editor_property("resource_object", unreal.load_asset(FS + asset_name))
    # ImageSize is optional (every brush sits in a size box or fills a border) and is FDeprecateSlateVector2D in 5.5.
    try:
        b.set_editor_property("image_size", unreal.Vector2D(width, height))
    except Exception as error:
        print("image_size skipped for", asset_name, error)
    return b


def button_style(normal, hovered, pressed, disabled, width, height):
    s = unreal.ButtonStyle()
    s.set_editor_property("normal", brush(normal, width, height))
    s.set_editor_property("hovered", brush(hovered, width, height))
    s.set_editor_property("pressed", brush(pressed, width, height))
    s.set_editor_property("disabled", brush(disabled, width, height))
    return s


def load_or_create_style_asset():
    path = UI + "/" + STYLE_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SurvivalUiStyleAsset)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(STYLE_NAME, UI, unreal.SurvivalUiStyleAsset, factory)


def fill_style(asset):
    vs = cdo_style(VARIANT_MENU)
    bs = cdo_style(UNIT_BROWSER)
    st = asset.get_editor_property("style")
    copy = {
        # survival field: (source struct, source field)
        "header_plate": (vs, "header_plate"),
        "roster_panel": (vs, "variant_panel"),
        "roster_entry": (vs, "unit_entry"),
        "roster_icon_well": (vs, "unit_icon_well"),
        "badge": (vs, "selected_marker"),
        "confirm_button": (vs, "confirm_button"),
        "back_button": (vs, "back_button"),
        "heading_font": (vs, "heading_font"),
        "body_font": (vs, "body_font"),
        "text_color": (vs, "text_color"),
        "muted_text_color": (vs, "muted_text_color"),
        "accent_color": (vs, "accent_color"),
        "warning_color": (vs, "removed_color"),
        "accept_color": (vs, "better_color"),
        "tab": (bs, "tab"),
        "tab_selected": (bs, "tab_selected"),
        "tab_text_color": (bs, "tab_text_color"),
        "slot_empty": (bs, "tile"),
        "slot_filled": (bs, "tile_selected"),
        "slot_icon_well": (bs, "icon_well"),
        "slot_wear": (bs, "tile_wear"),
    }
    for field, (source, source_field) in copy.items():
        st.set_editor_property(field, source.get_editor_property(source_field))
    # The locked HQ slot: the selected tile's pressed look.
    st.set_editor_property("slot_locked", bs.get_editor_property("tile_selected").get_editor_property("pressed"))
    st.set_editor_property("army_panel", brush("MI9_Custom_ThinPanel_1078x679_u_n", 1078, 679))
    st.set_editor_property("dialog_panel", brush("MI9_Custom_DialogPanel_578x242_u_n", 578, 242))
    st.set_editor_property("hud_plate", brush("MI9_Custom_ResourceCounter_510x130_u_n", 510, 130))
    st.set_editor_property("banner_plate", brush("MI9_Theme01_T_Panel_Tip03_511x150_f_n", 511, 150))
    st.set_editor_property("result_panel", brush("MI9_Theme01_T_Panel06_Dark_750x450_f_n", 750, 450))
    st.set_editor_property("arrow_button", button_style(
        "MI9_Theme01_T_Btn_Theme01Next_p__62x69_f_n", "MI9_Theme02_T_Btn_Theme2Next_n_62x69_f_h",
        "MI9_Theme03_T_Btn_Theme2Next_n_62x69_f_p", "MI9_Theme01_T_Btn_Theme01Next_n__62x69_f_d", 62, 69))
    bar = unreal.ProgressBarStyle()
    bar.set_editor_property("background_image", brush("MI9_Theme02_T_LinearProgress03_Bg_987x42_u_n", 987, 42))
    bar.set_editor_property("fill_image", brush("MI9_Theme02_T_LinearProgress03_987x42_u_n", 987, 42))
    st.set_editor_property("supply_bar", bar)
    scroll = unreal.ScrollBarStyle()
    scroll.set_editor_property("vertical_background_image", brush("MI9_Custom_SettingsScrollTrackV_8x32_u_n", 8, 32))
    scroll.set_editor_property("normal_thumb_image", brush("MI9_Custom_SettingsScrollThumbV_n_8x32_u_n", 8, 32))
    scroll.set_editor_property("hovered_thumb_image", brush("MI9_Custom_SettingsScrollThumbV_h_8x32_u_h", 8, 32))
    scroll.set_editor_property("dragged_thumb_image", brush("MI9_Custom_SettingsScrollThumbV_h_8x32_u_p", 8, 32))
    st.set_editor_property("scroll_bar", scroll)
    # screen_shade stays empty on purpose: the Blueprints' own black 55% Border colour is used.
    asset.set_editor_property("style", st)


def reparent_and_style(style_asset):
    for name, cls in WIDGETS.items():
        bp = unreal.load_asset(UI + "/" + name)
        if bp is None:
            print("MISSING", name)
            continue
        # Idempotent; reparenting to the class it already has is a no-op.
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, cls)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(unreal.load_object(None, UI + "/" + name + "." + name + "_C"))
        cdo.set_editor_property("m_style_asset", style_asset)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)


def save_and_verify(names):
    root = unreal.Paths.project_content_dir()
    for name in names:
        ok = unreal.EditorAssetLibrary.save_asset(UI + "/" + name, only_if_is_dirty=False)
        on_disk = unreal.Paths.file_exists(root + "RTS_Survival/SurvivalMode/UI/" + name + ".uasset")
        print(name, "saved" if ok else "SAVE FAILED", "on disk" if on_disk else "NOT ON DISK")


style_asset = load_or_create_style_asset()
try:
    fill_style(style_asset)
except Exception as error:
    # Cosmetic only; the reparent below is what makes the widgets work.
    print("STYLE FILL FAILED:", error)
reparent_and_style(style_asset)
save_and_verify([STYLE_NAME] + list(WIDGETS.keys()))
