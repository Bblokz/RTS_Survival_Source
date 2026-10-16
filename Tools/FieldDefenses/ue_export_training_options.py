# Exports DT_TrainingOptions to JSON so the field defense rows can be modelled on existing rows.
# Run: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file> <out json>"
import sys
import unreal

TABLE_PATH = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions"

out_path = sys.argv[-1]
table = unreal.load_asset(TABLE_PATH)
json_text = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
with open(out_path, "w", encoding="utf-8") as out_file:
    out_file.write(json_text)
unreal.log("FIELD_DEFENSE_DT exported {} chars to {}".format(len(json_text), out_path))
