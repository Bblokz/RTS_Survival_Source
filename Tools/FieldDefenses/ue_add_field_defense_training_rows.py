# Adds the training option rows (icon, title, tooltip, description) of the HQ's field defenses to DT_TrainingOptions.
# The training menu finds a row by "Train_" + the expansion type name, see UTrainingOptionLibrary::GetTrainingOptionName.
# Idempotent: rows that exist are left alone. The table is only saved when every existing row survives the JSON
# round trip unchanged.
# Run: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -NullRHI
import json
import unreal

TABLE_PATH = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions"
BRUSH_ROOT = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/UI/"
TEXT_NAMESPACE = "FieldDefenses"

FIELD_DEFENSES = [
    {
        "Row": "Train_BTX_GerPak40Pit",
        "Brush": "SB_OptionImage_GerPit_Pak40",
        "Name": "Pak 40 Pit",
        "Description": "A 7.5 cm Pak 40 anti-tank gun behind a low sandbag ring. The gun and its crew turn a full "
                       "circle. Place it on the map; the HQ builds it there once it is its turn in the queue.",
    },
    {
        "Row": "Train_BTX_GerFlak37Pit",
        "Brush": "SB_OptionImage_GerPit_Flak37",
        "Name": "Flak 37 Pit",
        "Description": "A 3.7 cm Flak 37 in a sandbagged pit that fires at aircraft and ground targets alike. "
                       "Place it on the map; the HQ builds it there once it is its turn in the queue.",
    },
    {
        "Row": "Train_BTX_GerLeFH18Pit",
        "Brush": "SB_OptionImage_GerPit_LeFH18",
        "Name": "leFH 18 Pit",
        "Description": "A 10.5 cm leFH 18 howitzer in a high sandbag pit under camouflage canvas. "
                       "Place it on the map; the HQ builds it there once it is its turn in the queue.",
    },
    {
        "Row": "Train_BTX_GerSFH18Pit",
        "Brush": "SB_OptionImage_GerPit_SFH18",
        "Name": "sFH 18 Pit",
        "Description": "A 15 cm sFH 18 heavy howitzer in a high sandbag pit: slow to reload, devastating at long "
                       "range. Place it on the map; the HQ builds it there once it is its turn in the queue.",
    },
]


def make_text(key, value):
    escaped = value.replace("\\", "\\\\").replace("\"", "\\\"")
    return "NSLOCTEXT(\"{}\", \"{}\", \"{}\")".format(TEXT_NAMESPACE, key, escaped)


def make_row(entry):
    row_key = entry["Row"]
    brush_name = entry["Brush"]
    brush_path = "{}{}.{}".format(BRUSH_ROOT, brush_name, brush_name)
    if not unreal.EditorAssetLibrary.does_asset_exist(brush_path):
        raise RuntimeError("Missing brush " + brush_path)
    return {
        "Name": row_key,
        "ImageBrush": "/Script/Engine.SlateBrushAsset'{}'".format(brush_path),
        "Description": make_text(row_key + "_Description", entry["Description"]),
        "Tooltip": make_text(row_key + "_Tooltip", entry["Name"]),
        "Title": make_text(row_key + "_Title", "<Text_Title>{}</>".format(entry["Name"])),
    }


def main():
    table = unreal.load_asset(TABLE_PATH)
    original_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    existing_names = {row["Name"] for row in original_rows}
    new_rows = [make_row(entry) for entry in FIELD_DEFENSES if entry["Row"] not in existing_names]
    if not new_rows:
        unreal.log("FIELD_DEFENSE_DT nothing to add; all rows exist")
        return

    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(original_rows + new_rows))
    round_trip_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    round_trip_by_name = {row["Name"]: row for row in round_trip_rows}

    changed = [row["Name"] for row in original_rows if round_trip_by_name.get(row["Name"]) != row]
    missing = [row["Name"] for row in new_rows if row["Name"] not in round_trip_by_name]
    if changed or missing:
        unreal.log_error("FIELD_DEFENSE_DT not saved; changed rows: {} missing rows: {}".format(changed, missing))
        return
    for row in new_rows:
        unreal.log("FIELD_DEFENSE_DT added {} -> {}".format(row["Name"], round_trip_by_name[row["Name"]]))
    saved = unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)
    unreal.log("FIELD_DEFENSE_DT saved={} rows={}".format(saved, len(round_trip_rows)))


main()
