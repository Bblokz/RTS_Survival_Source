"""German BXP QA: the Breakthrough concrete bunkers as HQ field defenses (headless).

1. DT_TrainingOptions rows Train_<type> (image = the BXP placard brush, title / tooltip / description from
   bxp_descriptions.json); the rail Pak bunker's row is the template, so the row keys match.
2. DA_TrainingLayout, Breakthrough HQ: a sub section shows nine slots, so Field Defenses keeps the nine shared pits and
   Barriers gets the field trench, the rail Pak bunker (it was a tenth, never shown Field Defenses slot) and the bunkers.

Run: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file> [--only Type,Type]"
"""
import json
import sys

import unreal

TEXTS = "D:/UE5Projects/RTS_Survival/Source/Tools/BxpQA/bxp_descriptions.json"
MANIFEST = "D:/UE_Assets/Icons/TrainingUI/Expansions/Ger/manifest.json"
ICON_HOME = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/BuildingExpansions"
TRAINING_TABLE = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions"
LAYOUT = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_TrainingLayout.DA_TrainingLayout"
TEMPLATE_ROW = "Train_BTX_GerRailPakBunker"
BUNKERS = ["BTX_GerFlak88Bunker", "BTX_GerFlakvierlingBunker", "BTX_GerPZ38TBunker"]
ONLY = sys.argv[sys.argv.index("--only") + 1].split(",") if "--only" in sys.argv else BUNKERS
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("BXPFD " + msg)


def localized(key, value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return 'NSLOCTEXT("FieldDefenses", "%s", "%s")' % (key, escaped)


def write_training_rows():
    texts = json.load(open(TEXTS, encoding="utf-8"))
    manifest = json.load(open(MANIFEST, encoding="utf-8"))
    table = unreal.load_asset(TRAINING_TABLE)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    template = next(r for r in rows if r["Name"] == TEMPLATE_ROW)
    written = []
    for bxp in ONLY:
        icon = manifest.get(bxp)
        if icon is None:
            log("%s no icon in the manifest yet, row skipped" % bxp)
            continue
        brush = "%s/T_Ger_Bxp_%s_Brush" % (ICON_HOME, icon["short"])
        if not EAL.does_asset_exist(brush):
            log("%s brush missing %s, row skipped" % (bxp, brush))
            continue
        name = "Train_" + bxp
        text = texts[bxp]
        row = dict(template)
        row["Name"] = name
        row["ImageBrush"] = "/Script/Engine.SlateBrushAsset'%s.%s'" % (brush, brush.split("/")[-1])
        row["Description"] = localized(name + "_Description", text["description"])
        row["Tooltip"] = localized(name + "_Tooltip", text["name"])
        row["Title"] = localized(name + "_Title", "<Text_Title>%s</>" % text["name"])
        rows = [r for r in rows if r["Name"] != name] + [row]
        written.append(name)
    before = {r["Name"]: r for r in rows if r["Name"] not in written}
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows))
    after = {r["Name"]: r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
    changed = [n for n, r in before.items() if after.get(n) != r]
    missing = [n for n in written if n not in after]
    if changed or missing:
        raise RuntimeError("DT_TrainingOptions round trip changed %s missing %s" % (changed[:5], missing))
    log("training rows %s saved %s" % (written, EAL.save_loaded_asset(table, False)))


def slot(field_defense):
    s = unreal.TrainingLayoutSlot()
    s.set_editor_property("unit_type", unreal.AllUnitType.UN_TYPE_BUILDING_EXPANSION)
    s.set_editor_property("field_defense_type", field_defense)
    return s


def enum_value(name):
    wanted = name.replace("_", "").upper()
    for attr in dir(unreal.BuildingExpansionType):
        if attr.replace("_", "").upper() == wanted:
            return getattr(unreal.BuildingExpansionType, attr)
    raise KeyError(name)


def write_layout():
    layout = unreal.load_asset(LAYOUT)
    factions = layout.get_editor_property("faction_layouts")
    faction_key = next(k for k in factions.keys() if "BREAKTHROUGH" in str(k).upper())
    faction = factions[faction_key]
    buildings = faction.get_editor_property("buildings")
    hq_key = next(k for k in buildings.keys() if "NOMADIC_GER_HQ:" in str(k).upper())
    hq = buildings[hq_key]
    rail_pak = enum_value("BTX_GerRailPakBunker")
    doctrine = [rail_pak] + [enum_value(b) for b in BUNKERS]
    sections = list(hq.get_editor_property("sub_sections"))
    for section in sections:
        name = str(section.get_editor_property("display_name"))
        slots = list(section.get_editor_property("slots"))
        if name == "Field Defenses":
            slots = [s for s in slots if s.get_editor_property("field_defense_type") not in doctrine]
        elif name == "Barriers":
            used = [s for s in slots if s.get_editor_property("field_defense_type") != enum_value("BXT_Invalid")
                    and s.get_editor_property("field_defense_type") not in doctrine]
            slots = used + [slot(t) for t in doctrine]
        else:
            continue
        section.set_editor_property("slots", slots)
        log("%s -> %s" % (name, [str(s.get_editor_property("field_defense_type")).split(".")[-1][:24] for s in slots]))
    hq.set_editor_property("sub_sections", sections)
    buildings[hq_key] = hq
    faction.set_editor_property("buildings", buildings)
    factions[faction_key] = faction
    layout.modify()
    layout.set_editor_property("faction_layouts", factions)
    log("layout saved %s" % EAL.save_loaded_asset(layout, False))


write_training_rows()
if "--rows-only" not in sys.argv:
    write_layout()
