"""Imports engraved training icons (engrave_icons.py) as UI textures + SlateBrushAssets and adds their
DT_TrainingOptions rows. Rows that already exist are left alone; existing rows are checked to survive unchanged.

Run as a commandlet with the editor closed:
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>"
"""
import json
import os

import unreal

ICON_DIR = "D:/UE5Projects/RTS_Survival/Saved/UnitPortraits/gaps_icons"
ICON_ROOT = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger"
FOLDER_BY_PREFIX = {
    "T_Ger_ArmoredCar_": "ArmoredCar",
    "T_Ger_LightTank_": "LightTanks",
    "T_Ger_MediumTank_": "MediumTanks",
    "T_Ger_HeavyTank_": "HeavyTanks",
    "T_Ger_TeamWeapon_": "Squads",
}
# Settings and brush copied from an existing hand-made icon so the new ones load and draw the same way.
REFERENCE_TEXTURE = ICON_ROOT + "/LightTanks/T_Ger_LightTank_JagdPz"
REFERENCE_BRUSH = REFERENCE_TEXTURE + "_Brush"
TABLE = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions"
TEXT_NAMESPACE = "TrainingOptions"
CHECK_ROW = "Train_Tank_PzIV_G"

# row name: (icon stem, title, description draft)
ROWS = {
    "Train_Tank_Pz38t_RailGun": ("T_Ger_LightTank_Pz38_Rail", "Pz 38(t) Railgun",
        "A Panzer 38(t) chassis carrying an experimental <Text_Armor>railgun</>. Its slugs have "
        "<Text_Armor>very high armor penetration</> at long range, but the light hull <Text_Bad>cannot take many hits</>."),
    "Train_Tank_PZIV_Rockets": ("T_Ger_MediumTank_PZIV_Rockets", "Panzer IV Rockets",
        "A Panzer IV fitted with a <Text_Armor>rocket launcher</> for bombarding infantry and emplacements from range. "
        "The rockets take long to reload, so keep it behind the main line."),
    "Train_Tank_PzJagerLaser": ("T_Ger_LightTank_PzJager_Laser", "Panzerjager Laser",
        "The Panzerjager chassis with an experimental <Text_Armor>laser cannon</> that burns through armor. Like the gun "
        "version it has <Text_Bad>no rear armor</> and is vulnerable to flanking infantry."),
    "Train_Tank_PzIV_H": ("T_Ger_MediumTank_PZIV_H", "Panzer IV H",
        "The final Panzer IV, with <Text_Armor>80mm frontal armor</>, side skirts and the long <Text_Armor>75mm KwK 40</>. "
        "A dependable all-round medium tank."),
    "Train_Tank_TigerRail": ("T_Ger_HeavyTank_Tiger1_Rail", "Tiger Rail",
        "A Tiger I rearmed with a <Text_Armor>railgun</>. Combines the Tiger's <Text_Armor>heavy armor</> with extreme "
        "penetration at long range, but is <Text_Bad>slow to reposition</>."),
    "Train_Tank_Sdkfz251_Mortar": ("T_Ger_ArmoredCar_Sdkfz251_Mortar", "Sdkfz. 251 Mortar",
        "A half-track carrying an <Text_Armor>81mm mortar</> that fires over cover at infantry and emplacements. "
        "Its armor is thin; <Text_Bad>keep it away from the front</>."),
    "Train_Tank_Pz38t_R": ("T_Ger_LightTank_Panzer38t_R", "Pz 38(t) Rockets",
        "A Panzer 38(t) with a rack of <Text_Armor>rockets</> for a heavy opening salvo against infantry and light "
        "armor. <Text_Bad>Light armor</> and a long reload."),
    "Train_Tank_Sdkfz_7_Flakvierling": ("T_Ger_ArmoredCar_Sdkfz7_Flakvierling", "Sdkfz 7 Flakvierling",
        "A half-track with the quadruple <Text_Armor>20mm Flakvierling</>. Its high rate of fire shreds aircraft and "
        "<Text_Armor>infantry</>, but the gun crew is <Text_Bad>exposed</>."),
    "Train_Tank_TigerH1": ("T_Ger_HeavyTank_TigerH1", "Tiger H1",
        "The early Tiger I with thick <Text_Armor>100mm frontal armor</> and the <Text_Armor>88mm KwK 36</>, deadly to "
        "most tanks at range. <Text_Bad>Slow and costly</>."),
    "Train_Tank_PzII_Luchs": ("T_Ger_LightTank_PanzerII_Luchs", "Pz II Luchs",
        "A fast reconnaissance tank with a <Text_Armor>20mm autocannon</>. Spots and harasses light targets but "
        "<Text_Bad>cannot fight heavier armor</>."),
    "Train_Squad_Ger_PaK38": ("T_Ger_TeamWeapon_Pak38", "Pak 38",
        "A towed <Text_Armor>50mm anti-tank gun</>. The crew deploys it to fire; strong against light and medium armor."),
    "Train_Squad_Ger_PaK40": ("T_Ger_TeamWeapon_Pak40", "Pak 40",
        "A towed <Text_Armor>75mm anti-tank gun</> that can penetrate most tanks. It must deploy before it fires and "
        "<Text_Bad>turns slowly</>."),
    "Train_Squad_Ger_88mmFlak": ("T_Ger_TeamWeapon_Flak88", "88mm Flak",
        "The towed <Text_Armor>88mm Flak</>: hits aircraft and <Text_Armor>destroys heavy armor at long range</>. "
        "<Text_Bad>Large and slow to deploy</>."),
    "Train_Squad_Ger_LefH18": ("T_Ger_TeamWeapon_LeFH18", "leFH 18",
        "A towed <Text_Armor>105mm howitzer</> that shells infantry and emplacements from long range. "
        "<Text_Bad>Defenceless up close</>."),
    "Train_Squad_Ger_210MM_Morser": ("T_Ger_TeamWeapon_Morser210", "21cm Mörser",
        "A <Text_Armor>210mm siege howitzer</> with enormous <Text_Armor>HE filler</> for breaking fortifications. "
        "<Text_Bad>Very slow to move and deploy</>."),
}


def content_folder(stem):
    for prefix, folder in FOLDER_BY_PREFIX.items():
        if stem.startswith(prefix):
            return ICON_ROOT + "/" + folder
    raise RuntimeError("TRAINING_ICONS no folder for " + stem)


def import_texture(stem):
    folder = content_folder(stem)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ICON_DIR, stem + ".png")
    task.destination_path = folder
    task.destination_name = stem
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(folder + "/" + stem)
    reference = unreal.load_asset(REFERENCE_TEXTURE)
    for setting in ("compression_settings", "lod_group", "mip_gen_settings", "srgb", "never_stream", "filter",
                    "max_texture_size"):
        texture.set_editor_property(setting, reference.get_editor_property(setting))
    unreal.EditorAssetLibrary.save_asset(folder + "/" + stem, only_if_is_dirty=False)
    return texture


def make_brush(stem, texture):
    folder = content_folder(stem)
    path = folder + "/" + stem + "_Brush"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        brush_asset = unreal.load_asset(path)
    else:
        brush_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            stem + "_Brush", folder, unreal.SlateBrushAsset, unreal.SlateBrushAssetFactory())
    brush = unreal.load_asset(REFERENCE_BRUSH).get_editor_property("brush")
    brush.set_editor_property("resource_object", texture)
    brush_asset.set_editor_property("brush", brush)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return path


def loc_text(row_name, field, text):
    return 'NSLOCTEXT("%s", "%s_%s", "%s")' % (TEXT_NAMESPACE, row_name, field, text.replace('"', '\\"'))


def add_rows(brush_paths):
    table = unreal.load_asset(TABLE)
    before = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    names = {row["Name"] for row in before}
    rows = list(before)
    added = []
    for row_name, (stem, title, description) in ROWS.items():
        if row_name in names:
            continue
        brush_path = brush_paths[stem]
        brush_name = brush_path.split("/")[-1]
        rows.append({
            "Name": row_name,
            "ImageBrush": "/Script/Engine.SlateBrushAsset'%s.%s'" % (brush_path, brush_name),
            "Description": loc_text(row_name, "Description", description),
            "Tooltip": loc_text(row_name, "Tooltip", title),
            "Title": loc_text(row_name, "Title", "<Text_Title>%s</>" % title),
        })
        added.append(row_name)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        raise RuntimeError("TRAINING_ICONS fill_data_table_from_json_string failed")
    after = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    after_by_name = {row["Name"]: row for row in after}
    changed = [row["Name"] for row in before if after_by_name.get(row["Name"]) != row]
    if changed or len(after) != len(before) + len(added):
        raise RuntimeError("TRAINING_ICONS round trip changed rows %s (before %d, added %d, after %d); not saved"
                           % (changed, len(before), len(added), len(after)))
    unreal.EditorAssetLibrary.save_asset(TABLE, only_if_is_dirty=False)
    unreal.log_warning("TRAINING_ICONS rows before=%d added=%d after=%d unchanged=%d %s=%s" % (
        len(before), len(added), len(after), len(before), CHECK_ROW, after_by_name[CHECK_ROW]["ImageBrush"]))
    for row_name in added:
        unreal.log_warning("TRAINING_ICONS row %s brush=%s" % (row_name, after_by_name[row_name]["ImageBrush"]))


def main():
    brush_paths = {}
    for stem, _title, _description in ROWS.values():
        brush_paths[stem] = make_brush(stem, import_texture(stem))
        unreal.log_warning("TRAINING_ICONS imported %s" % stem)
    add_rows(brush_paths)
    unreal.log_warning("TRAINING_ICONS done")


main()
