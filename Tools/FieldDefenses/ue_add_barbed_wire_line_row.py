# Adds the HQ barbed wire line's training option row (icon, title, tooltip, description) to DT_TrainingOptions, and
# the slate brush its icon uses (the scavengers' barbed wire field construction icon, framed like the trench's).
# The training menu finds the row by "Train_" + the expansion type name (BTX_GerBarbedWireLine).
# Idempotent: an existing brush or row is left alone. The table is only saved when every existing row survives the
# JSON round trip unchanged.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -NullRHI
import json
import unreal

TABLE_PATH = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions"
TRENCH_BRUSH_PATH = "/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/UI/SB_OptionImage_GerFieldTrench"
ICON_TEXTURE_PATH = "/Game/RTS_Survival/Textures/GameUI/ActionUI/FieldConstruction/T_FieldConstruction_Barbedwire"
BRUSH_FOLDER = "/Game/RTS_Survival/Blueprints/Environment/BarbedWireLine/UI"
BRUSH_NAME = "SB_OptionImage_GerBarbedWireLine"
TEXT_NAMESPACE = "FieldDefenses"
ROW = {
    "Row": "Train_BTX_GerBarbedWireLine",
    "Name": "Barbed Wire",
    "Description": "Barbed wire that stops infantry; vehicles drive through and crush it. Click where it starts, "
                   "move the mouse to draw it and click again to end it. Both ends snap to open ends of your other "
                   "wire, also to the gap a destroyed piece left. Every 4 m piece stands on its own: shells destroy "
                   "it and a vehicle crushes it without taking the rest of the line. The cost shown is per metre: "
                   "each piece is paid for as it is strung, and stringing waits while resources run short.",
}


def log(message):
    unreal.log("BARBED_WIRE_ROW " + message)


def ensure_brush():
    brush_path = "{}/{}".format(BRUSH_FOLDER, BRUSH_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(brush_path):
        log("brush exists " + brush_path)
        return brush_path
    texture = unreal.load_asset(ICON_TEXTURE_PATH)
    trench_brush_asset = unreal.load_asset(TRENCH_BRUSH_PATH)
    if texture is None or trench_brush_asset is None:
        raise RuntimeError("Missing icon texture or trench brush")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    brush_asset = asset_tools.create_asset(BRUSH_NAME, BRUSH_FOLDER, unreal.SlateBrushAsset, None)
    # The trench brush's settings (draw as image, white tint), with the wire's icon.
    brush = trench_brush_asset.get_editor_property("brush")
    brush.set_editor_property("resource_object", texture)
    brush_asset.set_editor_property("brush", brush)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(brush_asset, only_if_is_dirty=False)
    log("brush created {} saved={}".format(brush_path, saved))
    return brush_path


def make_text(key, value):
    escaped = value.replace("\\", "\\\\").replace("\"", "\\\"")
    return "NSLOCTEXT(\"{}\", \"{}\", \"{}\")".format(TEXT_NAMESPACE, key, escaped)


def make_row(brush_path):
    row_key = ROW["Row"]
    object_path = "{}.{}".format(brush_path, BRUSH_NAME)
    return {
        "Name": row_key,
        "ImageBrush": "/Script/Engine.SlateBrushAsset'{}'".format(object_path),
        "Description": make_text(row_key + "_Description", ROW["Description"]),
        "Tooltip": make_text(row_key + "_Tooltip", ROW["Name"]),
        "Title": make_text(row_key + "_Title", "<Text_Title>{}</>".format(ROW["Name"])),
    }


def main():
    brush_path = ensure_brush()
    table = unreal.load_asset(TABLE_PATH)
    original_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    if any(row["Name"] == ROW["Row"] for row in original_rows):
        log("row exists " + ROW["Row"])
        return
    new_row = make_row(brush_path)
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(original_rows + [new_row]))
    round_trip_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    round_trip_by_name = {row["Name"]: row for row in round_trip_rows}
    changed = [row["Name"] for row in original_rows if round_trip_by_name.get(row["Name"]) != row]
    if changed or ROW["Row"] not in round_trip_by_name:
        unreal.log_error("BARBED_WIRE_ROW not saved; changed rows: {}".format(changed))
        return
    log("added {} -> {}".format(ROW["Row"], round_trip_by_name[ROW["Row"]]))
    saved = unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)
    log("table saved={} rows={}".format(saved, len(round_trip_rows)))


main()
