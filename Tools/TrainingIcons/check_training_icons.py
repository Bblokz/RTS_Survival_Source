"""Read-only check that the training UI resolves each new squad icon, the way the C++ does it:
row name = UTrainingOptionLibrary::GetTrainingOptionName(UNType_Squad, subtype) ("Train_" + enum name), row of
UTrainingLayoutDataAsset::TrainingOptionIconTable (DT_TrainingOptions), its ImageBrush SlateBrushAsset, and that brush's
resource object must be a Texture2D imported from its composed PNG (not an empty brush, so no fallback picture).

Headless (reads what is on disk):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -DisablePlugins=BpGeneratorUltimate
Prints TRAINING_ICONS_CHECK lines and a PASS/FAIL summary.
"""
import json

import unreal

TABLE = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions"
SUBTYPES = ["Squad_Ger_EliteScavengers", "Squad_Ger_RedSpine", "Squad_Ger_RailTroopers", "Squad_Ger_FelsKrieger",
            "Squad_Ger_Scavengers"]
OPTIONAL = {"Squad_Ger_FelsKrieger"}  # composed but not registered (lead, 16:24)
LOG = "TRAINING_ICONS_CHECK "


def member(enum_type, name):
    wanted = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.replace("_", "").lower() == wanted:
            return getattr(enum_type, attr)
    raise RuntimeError("no member " + name)


def main():
    fails = []
    table = unreal.load_asset(TABLE)
    rows = {r["Name"]: r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
    unreal.log_warning(LOG + "table rows %d" % len(rows))
    squad_type = member(unreal.AllUnitType, "UNType_Squad")
    for name in SUBTYPES:
        value = int(member(unreal.SquadSubtype, name).value)
        row_name = unreal.TrainingOptionLibrary.get_training_option_name(squad_type, value)
        row = rows.get(row_name)
        if row is None:
            line = "%s value %d row %s MISSING" % (name, value, row_name)
            (unreal.log_warning if name in OPTIONAL else unreal.log_error)(LOG + line)
            if name not in OPTIONAL:
                fails.append(line)
            continue
        brush_path = row["ImageBrush"].split("'")[1]
        brush_asset = unreal.load_asset(brush_path)
        texture = brush_asset.get_editor_property("brush").get_editor_property("resource_object") if brush_asset else None
        ok = isinstance(texture, unreal.Texture2D)
        size = (texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) if ok else None
        src = texture.get_editor_property("asset_import_data").get_first_filename() if ok else None
        line = "%s value %d row %s brush %s texture %s size %s group %s source %s" % (
            name, value, row_name, brush_path.split(".")[-1], texture.get_name() if texture else None, size,
            texture.get_editor_property("lod_group") if ok else None, src)
        unreal.log_warning(LOG + line)
        is_new = name != "Squad_Ger_Scavengers"  # the live Scavengers row is the control
        if not ok or (is_new and not str(src).endswith(texture.get_name() + ".png")):
            fails.append(line)
    unreal.log_warning(LOG + ("PASS" if not fails else "FAIL %s" % fails))


main()
