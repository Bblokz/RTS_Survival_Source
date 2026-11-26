"""Imports the standalone placard badges (tactical_icon_catalog.py output) and fills DA_TacticalIconCatalog
(UTacticalIconCatalog), which the selected unit description reads to show the portrait's tactical icons.

Run in an editor with the C++ that has UTacticalIconCatalog:
  exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/TacticalIconBackgrounds/fill_tactical_icon_catalog.py").read())
Logs TACTICAL_ICONS lines; portraits whose texture does not exist are skipped and listed.
"""
import json

import unreal

BADGE_DIR = "D:/UE_Assets/Icons/TrainingUI/Placard/Badges"
CATALOG_JSON = "D:/UE5Projects/RTS_Survival/Source/Tools/TacticalIconBackgrounds/tactical_icon_catalog.json"
BADGE_FOLDER = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Tactical/Placard"
CATALOG_PATH = BADGE_FOLDER + "/DA_TacticalIconCatalog"


def log(text):
    unreal.log_warning("TACTICAL_ICONS " + text)


def import_badge(name):
    task = unreal.AssetImportTask()
    task.filename = BADGE_DIR + "/" + name + ".png"
    task.destination_path = BADGE_FOLDER
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(BADGE_FOLDER + "/" + name)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


def load_or_create_catalog():
    if unreal.EditorAssetLibrary.does_asset_exist(CATALOG_PATH):
        return unreal.load_asset(CATALOG_PATH)
    folder, name = CATALOG_PATH.rsplit("/", 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.TacticalIconCatalog)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.TacticalIconCatalog, factory)


def main():
    with open(CATALOG_JSON, encoding="utf-8") as handle:
        catalog_data = json.load(handle)

    icons = unreal.Map(unreal.Name, unreal.TacticalIconInfo)
    for spec, entry in catalog_data["icons"].items():
        info = unreal.TacticalIconInfo()
        info.set_editor_property("badge_texture", import_badge(entry["texture"]))
        info.set_editor_property("description", unreal.Text(entry["description"]))
        icons[unreal.Name(spec)] = info

    portraits = unreal.Map(unreal.Name, unreal.TacticalPortraitIcons)
    missing = []
    for texture_path, (left, right) in catalog_data["portraits"].items():
        if not unreal.EditorAssetLibrary.does_asset_exist(texture_path):
            missing.append(texture_path)
            continue
        pair = unreal.TacticalPortraitIcons()
        pair.set_editor_property("left_icon", unreal.Name(left) if left else unreal.Name("None"))
        pair.set_editor_property("right_icon", unreal.Name(right) if right else unreal.Name("None"))
        portraits[unreal.Name(texture_path)] = pair

    catalog = load_or_create_catalog()
    catalog.set_editor_property("icons", icons)
    catalog.set_editor_property("portraits", portraits)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(catalog, False)
    for path in missing:
        log("missing portrait " + path)
    log("done icons=%d portraits=%d missing=%d saved=%s" % (len(icons), len(portraits), len(missing), saved))


main()
