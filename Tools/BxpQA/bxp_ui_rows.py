"""German BXP QA: new placard icons + names / tooltips / descriptions into DT_Items (headless).

Inputs: D:/UE_Assets/Icons/TrainingUI/Expansions/Ger/manifest.json (icon PNGs from the portrait + placard pipeline)
and Source/Tools/BxpQA/bxp_descriptions.json (name / tooltip / description per row). Per type: imports the placard
icon as T_Ger_Bxp_<Short> (UI group, no mips) into Textures/GameUI/UnitIcons/Ger/BuildingExpansions, makes the slate
brush T_Ger_Bxp_<Short>_Brush from the 37mm flak option brush, and writes the row through CSV so the struct's own
Name field is set as well (JSON fills cannot reach it: its key collides with the row name).
Rows not in the inputs are kept as they are. Pass --dry to only print.
"""
import csv
import io
import json
import sys
import unreal

EAL = unreal.EditorAssetLibrary
DT = "/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionItems/DT_Items"
ICON_HOME = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/BuildingExpansions"
TEMPLATE_BRUSH = "/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionOptions/SB_OptionImage_37mmFLak"
MANIFEST = "D:/UE_Assets/Icons/TrainingUI/Expansions/Ger/manifest.json"
TEXTS = "D:/UE5Projects/RTS_Survival/Source/Tools/BxpQA/bxp_descriptions.json"
DRY = "--dry" in sys.argv
ONLY = sys.argv[sys.argv.index("--only") + 1].split(",") if "--only" in sys.argv else None
# Asset names are case-insensitive: these shorts would overwrite the January T_GER_BXP_* icons.
NAME_OVERRIDES = {"HQRadar": "HQRadarPlacard"}


def log(msg):
    unreal.log("BXPUI " + msg)


def import_icon(png, name):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = ICON_HOME
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(ICON_HOME + "/" + name)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("srgb", True)
    EAL.save_loaded_asset(texture, False)
    return texture


def make_brush(texture, name):
    path = ICON_HOME + "/" + name + "_Brush"
    if not EAL.does_asset_exist(path):
        EAL.duplicate_asset(TEMPLATE_BRUSH, path)
    brush_asset = unreal.load_asset(path)
    brush = brush_asset.get_editor_property("brush")
    brush.set_editor_property("resource_object", texture)
    brush_asset.set_editor_property("brush", brush)
    EAL.save_loaded_asset(brush_asset, False)
    return brush_asset


def text_cell(value):
    # Plain strings import as new FText; keep it simple and readable in the table.
    return value


def main():
    manifest = json.load(open(MANIFEST, encoding="utf-8"))
    texts = json.load(open(TEXTS, encoding="utf-8"))
    dt = unreal.load_asset(DT)
    raw = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(dt)
    reader = csv.reader(io.StringIO(raw))
    header = next(reader)
    rows = [r for r in reader if r]
    index = {h: i for i, h in enumerate(header)}
    by_name = {r[0]: r for r in rows}
    template = by_name["BTX_37mmFlak"]
    import re
    header_text = open("D:/UE5Projects/RTS_Survival/Source/RTS_Survival/Buildings/BuildingExpansion/"
                       "BuildingExpansionEnums.h", encoding="utf-8").read()
    enum_names = set(re.findall(r"^\s*(B[TX][TX]_\w+)", header_text, re.M))
    changed = []
    for row_name in sorted(set(manifest) | set(texts)):
        if ONLY and row_name not in ONLY:
            continue
        if row_name not in enum_names:
            # Modelled expansions without their own type yet: icon assets only, no data table row.
            icon = manifest.get(row_name)
            if icon and icon.get("icon") and not DRY:
                name = "T_Ger_Bxp_" + icon["short"]
                make_brush(import_icon(icon["icon"], name), name)
            log("%s icon only (no enum value yet)" % row_name)
            continue
        row = by_name.get(row_name)
        if row is None:
            row = list(template)
            row[0] = row_name
            rows.append(row)
            by_name[row_name] = row
        icon = manifest.get(row_name)
        if icon and icon.get("icon"):
            short = icon["short"]
            name = "T_Ger_Bxp_" + NAME_OVERRIDES.get(short, short)
            if not DRY:
                brush = make_brush(import_icon(icon["icon"], name), name)
                row[index["Built_ImageBrush"]] = "/Script/Engine.SlateBrushAsset'%s.%s'" % (
                    brush.get_path_name().split(".")[0], brush.get_name())
        info = texts.get(row_name)
        if info:
            row[index["Name"]] = text_cell(info["name"])
            row[index["Tooltip"]] = text_cell(info["tooltip"])
            row[index["Description"]] = text_cell(info["description"])
        changed.append(row_name)
        log("%s icon=%s name=%s" % (row_name, bool(icon and icon.get("icon")), info["name"] if info else "-"))
    out = io.StringIO()
    writer = csv.writer(out, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    if DRY:
        log("dry run, %d rows would change" % len(changed))
        return
    dt.modify()
    problems = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(dt, out.getvalue())
    saved = EAL.save_loaded_asset(dt, False)
    log("fill problems=%s saved=%s rows=%d changed=%d" % (problems, saved, len(rows), len(changed)))


main()
