"""Imports the composed squad training icons (compose_training_icons.py) as UI textures + SlateBrushAssets in
Textures/GameUI/UnitIcons/Ger/Squads and adds/updates ONLY their four DT_TrainingOptions rows.

Run in the shared editor (UECP execute_python: exec(open(<this file>).read())): DT_TrainingOptions, textures and brushes
use no new C++ type, so saving them there is safe. The table is saved only if every other row survives the JSON round
trip unchanged. Texture settings and the brush are copied from the live T_Ger_Squad_Scavengers icon.
"""
import json

import unreal

ICON_DIR = "D:/UE5Projects/RTS_Survival/Saved/TrainingIcons/icons"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Squads"
REFERENCE_TEXTURE = FOLDER + "/T_Ger_Squad_Scavengers"
REFERENCE_BRUSH = REFERENCE_TEXTURE + "_Brush"
TABLE = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions"
TEXT_NAMESPACE = "TrainingOptions"
LOG = "TRAINING_ICONS "

# row name: (icon stem, title, description)
ROWS = {
    "Train_Squad_Ger_EliteScavengers": (
        "T_Ger_Squad_EliteScavengers", "Elite Scavengers",
        "Veteran scavengers in reinforced kit: three carry the <Text_Armor>STG 44</> and two the "
        "<Text_Armor>Panzerfaust</> with a Mauser as backup. They still <Text_Scavenging>Scavenge</> wrecks and "
        "<Text_Armor>repair vehicles & structures</>, and can knock out light and medium tanks."),
    "Train_Squad_Ger_RedSpine": (
        "T_Ger_Squad_RedSpine", "Red Spine",
        "Three light <Text_Armor>armored walkers</> with <Text_Armor>MP-46 SMGs</>. Their plating shrugs off rifle "
        "fire, so they can push into infantry positions; <Text_Bad>anti-tank weapons</> still hurt them."),
    "Train_Squad_Ger_FelsKrieger": (
        "T_Ger_Squad_FelsKrieger", "Fels Krieger",
        "Medium <Text_Armor>armored walkers</>, a head taller than the Red Spine, carrying the <Text_Armor>Ripper "
        "Gun</>. Very durable against infantry fire, but <Text_Bad>costly</>."),
    "Train_Squad_Ger_RailTroopers": (
        "T_Ger_Squad_RailTroopers", "Rail Troopers",
        "Heavily armored infantry with <Text_Armor>M920 rail rifles</>. Their slugs have <Text_Armor>high armor "
        "penetration</> at long range, making them a strong <Text_Armor>anti-tank</> squad."),
}
# 2026-11-05 16:24 lead: Fels Krieger was stopped by the user; its icon is composed but not registered until the lead
# says so. Add "Train_Squad_Ger_FelsKrieger" here to register it too.
SKIP_ROWS = globals().get("TRAINING_ICONS_SKIP", [])
ROWS = {name: row for name, row in ROWS.items() if name not in SKIP_ROWS}
eal = unreal.EditorAssetLibrary


def import_texture(stem):
    task = unreal.AssetImportTask()
    task.filename = ICON_DIR + "/" + stem + ".png"
    task.destination_path = FOLDER
    task.destination_name = stem
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + stem)
    reference = unreal.load_asset(REFERENCE_TEXTURE)
    for setting in ("compression_settings", "lod_group", "mip_gen_settings", "srgb", "never_stream", "filter",
                    "max_texture_size"):
        texture.set_editor_property(setting, reference.get_editor_property(setting))
    eal.save_asset(FOLDER + "/" + stem, only_if_is_dirty=False)
    return texture


def make_brush(stem, texture):
    path = FOLDER + "/" + stem + "_Brush"
    if eal.does_asset_exist(path):
        brush_asset = unreal.load_asset(path)
    else:
        brush_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            stem + "_Brush", FOLDER, unreal.SlateBrushAsset, unreal.SlateBrushAssetFactory())
    brush = unreal.load_asset(REFERENCE_BRUSH).get_editor_property("brush")
    brush.set_editor_property("resource_object", texture)
    brush_asset.set_editor_property("brush", brush)
    eal.save_asset(path, only_if_is_dirty=False)
    return path


def loc_text(row_name, field, text):
    return 'NSLOCTEXT("%s", "%s_%s", "%s")' % (TEXT_NAMESPACE, row_name, field, text.replace('"', '\\"'))


def update_rows(brush_paths):
    table = unreal.load_asset(TABLE)
    before = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    others = [row for row in before if row["Name"] not in ROWS]
    new_rows = []
    for row_name, (stem, title, description) in ROWS.items():
        brush_path = brush_paths[stem]
        brush_name = brush_path.split("/")[-1]
        new_rows.append({
            "Name": row_name,
            "ImageBrush": "/Script/Engine.SlateBrushAsset'%s.%s'" % (brush_path, brush_name),
            "Description": loc_text(row_name, "Description", description),
            "Tooltip": loc_text(row_name, "Tooltip", title),
            "Title": loc_text(row_name, "Title", "<Text_Title>%s</>" % title),
        })
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(others + new_rows)):
        raise RuntimeError(LOG + "fill_data_table_from_json_string failed")
    after = {row["Name"]: row for row in
             json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
    changed = [row["Name"] for row in others if after.get(row["Name"]) != row]
    missing = [row["Name"] for row in new_rows if row["Name"] not in after]
    if changed or missing or len(after) != len(others) + len(new_rows):
        raise RuntimeError(LOG + "round trip changed %s missing %s (before %d after %d); NOT saved"
                           % (changed, missing, len(before), len(after)))
    print(LOG + "save", eal.save_loaded_asset(table, only_if_is_dirty=False), "rows before", len(before),
          "after", len(after), "others unchanged", len(others))
    for row in new_rows:
        print(LOG + "row", row["Name"], after[row["Name"]]["ImageBrush"])


def run():
    dirty = [str(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    if any("DT_TrainingOptions" in p for p in dirty):
        raise RuntimeError(LOG + "DT_TrainingOptions has unsaved changes from someone else; not touching it")
    brush_paths = {}
    for stem, _title, _description in ROWS.values():
        brush_paths[stem] = make_brush(stem, import_texture(stem))
        print(LOG + "imported", stem)
    update_rows(brush_paths)


run()
