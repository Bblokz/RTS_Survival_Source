"""Remove every in-mission technology research slot from the building expansions (2026-11-26).

Technologies moved to the between-mission campaign tech tree, so expansions must not sell them again. The research
system (UResearchTechnologyAbilityComp) stays: it will host mission-only "field modification kits". This script only
deletes the research components listed in ALLOWLIST from the BXP Blueprints (an empty component would log errors at
BeginPlay and show a blank button), and drops the research phrases from the matching DT_Items texts.

Before deleting it dumps every component's full flattened chain per faction (tech, required techs, costs, preferred
slot) to OUT_JSON. Components not on the allowlist are reported and left alone (a kit component may be added later).
Also reports Blueprints derived from these BXPs and global abilities that require a technology.

Headless one-shot (no UECP needed):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject -nullrhi -unattended -ExecutePythonScript=<this file> -ABSLOG=<log>
Pass --dry (via env BXP_RESEARCH_DRY=1) to only dump.
"""
import csv
import io
import json
import os

import unreal

EAL = unreal.EditorAssetLibrary
DRY = os.environ.get("BXP_RESEARCH_DRY") == "1"
OUT_JSON = os.environ.get("BXP_RESEARCH_OUT", "D:/UE5Projects/RTS_Survival/Saved/ClaudeExports/bxp_research_removal.json")
GERBXPS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/"
DT_ITEMS = "/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionItems/DT_Items"

# Blueprint -> research component variable names that may be deleted (from bxp_qa_audit.json + tech_probe.json).
ALLOWLIST = {
    GERBXPS + "HQ/HarvesterHub/Ger_Bxp_HQHarvesterHub": None,  # None = every research comp (names read at run time)
    GERBXPS + "Barracks/FuelCell/Ger_Bxp_BarracksFuelExpansion": ["Flak88GroundRoleTech"],
    GERBXPS + "Barracks/RadixReactor/Ger_Bxp_BarracksReactorExpansion": [
        "InfantryKitTech", "GhillieSuitsTech", "EntrenchingToolsTech"],
    GERBXPS + "Forge/RadixFuel/Ger_Bxp_LFactoryFuelExpansion": [
        "FlameRangeOrRailReload", "FlameRadixiteDmg", "ArmoredTroopBayTech"],
    GERBXPS + "Forge/RadixReactor/Ger_Bxp_LFactoryRadixiteReactor": [
        "SpallLinersTech", "AutoCannonTech", "AutoLoader", "RangefinderTungstenTech", "ShellHoistTech"],
}

# DT_Items row -> {column: [(old phrase, new phrase)]}; a phrase that is not found is reported, never forced.
TEXT_EDITS = {
    "BTX_GerHQHarvester": {
        "Tooltip": [("Economy research", "HQ platform")],
        "Description": [(
            "A research platform on the HQ. Unlocks <Text_Accent>economy research</>: harvester capacity and speed, "
            "scavenging speed and reward, stronger repairs and Barricades.",
            "A platform on the HQ. Economy upgrades (harvester capacity and speed, scavenging, repairs, Barricades) "
            "are researched on the <Text_Accent>campaign tech tree</> now.")],
    },
    "BTX_GerBarracksRadixReactor": {
        "Tooltip": [("Infantry research", "Barracks energy")],
        "Description": [(" and infantry kit, ghillie suit and entrenching research.", ".")],
    },
    "BTX_GerBarrackFuelCell": {
        "Tooltip": [("Support research", "Sniper Team unlock")],
        "Description": [(" and the Flak 88 Ground Role research.", ".")],
    },
    "BTX_GerLFactoryRadixiteReactor": {
        "Tooltip": [("Vehicle research", "Forge energy")],
        "Description": [(
            " Unlocks vehicle research: autocannon damage, auto loaders, spall liners, rangefinders, "
            "tungsten 8.8 cm shells, shell hoists.", "")],
    },
    "BTX_GerLFactoryFuelStorage": {
        "Tooltip": [("Flame and half-track research", "Forge energy")],
        "Description": [(
            " Unlocks flame weapon research and Armoured Troop Bay: half-tracks get +20% hull armour and their "
            "passengers bail out.", "")],
    },
}

SUBOBJECTS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
REPORT = {"dry": DRY, "bxps": {}, "skipped_components": [], "derived_blueprints": [], "ga_tech_requirements": [],
          "dt_items": {}, "errors": []}


def log(message):
    unreal.log("BXPRESEARCH " + message)


def dump_chain(data_obj, depth=0):
    if data_obj is None:
        return []
    entry = {
        "depth": depth,
        "technology": str(data_obj.get_editor_property("technology")),
        "required": [str(t) for t in data_obj.get_editor_property("required_technologies")],
        "costs": str(data_obj.get_editor_property("costs")),
    }
    chain = [entry]
    for follow_up in data_obj.get_editor_property("follow_up_technologies"):
        chain.extend(dump_chain(follow_up, depth + 1))
    return chain


def dump_component(component):
    result = {}
    settings_map = component.get_editor_property("tech_ability_settings_per_faction")
    for faction, faction_settings in settings_map.items():
        result[str(faction)] = {
            "preferred_index": faction_settings.get_editor_property("preferred_ability_index"),
            "chain": dump_chain(faction_settings.get_editor_property("technology_data")),
        }
    return result


def variable_name(handle):
    data = SDL.get_data(handle)
    try:
        return str(SDL.get_variable_name(data))
    except Exception:  # noqa: BLE001 - fall back to the template name
        obj = SDL.get_object(data)
        return obj.get_name().replace("_GEN_VARIABLE", "") if obj else "?"


def process_blueprint(path, allowed_names):
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        REPORT["errors"].append("missing " + path)
        return
    handles = SUBOBJECTS.k2_gather_subobject_data_for_blueprint(blueprint)
    info = {"components": [], "removed": {}, "remaining_research_after": None}
    to_delete = []
    for handle in handles:
        obj = SDL.get_object(SDL.get_data(handle))
        if obj is None:
            continue
        name = variable_name(handle)
        info["components"].append(obj.get_class().get_name() + ":" + name)
        if obj.get_class().get_name() != "ResearchTechnologyAbilityComp":
            continue
        dumped = dump_component(obj)
        if allowed_names is not None and name not in allowed_names:
            REPORT["skipped_components"].append({"bp": path, "component": name, "settings": dumped})
            continue
        info["removed"][name] = dumped
        to_delete.append(handle)
    REPORT["bxps"][path] = info
    if DRY or not to_delete:
        return
    removed_count = SUBOBJECTS.delete_subobjects(handles[0], to_delete, blueprint)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    remaining = []
    for handle in SUBOBJECTS.k2_gather_subobject_data_for_blueprint(blueprint):
        obj = SDL.get_object(SDL.get_data(handle))
        if obj is not None and obj.get_class().get_name() == "ResearchTechnologyAbilityComp":
            remaining.append(variable_name(handle))
    info["deleted_count"] = removed_count
    info["remaining_research_after"] = remaining
    info["saved"] = EAL.save_loaded_asset(blueprint, False)
    log("%s deleted=%s remaining=%s saved=%s" % (path, removed_count, remaining, info["saved"]))


def report_derived_and_ga_requirements():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    bxp_classes = [p.rsplit("/", 1)[1] + "_C" for p in ALLOWLIST]
    blueprints = registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "Blueprint"), True)
    for asset_data in blueprints:
        parent = str(asset_data.get_tag_value("ParentClass") or "")
        native_parent = str(asset_data.get_tag_value("NativeParentClass") or "")
        if any(cls in parent for cls in bxp_classes):
            REPORT["derived_blueprints"].append(str(asset_data.package_name) + " <- " + parent)
        if "GlobalAbility" not in native_parent:
            continue
        try:
            cls = unreal.load_object(None, str(asset_data.package_name) + "." + str(asset_data.asset_name) + "_C")
            cdo = unreal.get_default_object(cls)
            requirements = cdo.get_editor_property("M_AbilityRequirements")
            tech = str(requirements.get_editor_property("RequiredTechnology"))
        except Exception as error:  # noqa: BLE001 - report, keep going
            tech = "?" + str(error)[:80]
        if "NONE" not in tech:
            REPORT["ga_tech_requirements"].append(str(asset_data.package_name) + " -> " + tech)


def edit_dt_items():
    table = unreal.load_asset(DT_ITEMS)
    raw = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(table)
    reader = csv.reader(io.StringIO(raw))
    header = next(reader)
    rows = [r for r in reader if r]
    index = {h: i for i, h in enumerate(header)}
    by_name = {r[0]: r for r in rows}
    changed = 0
    for row_name, columns in TEXT_EDITS.items():
        row = by_name.get(row_name)
        if row is None:
            REPORT["dt_items"][row_name] = "row missing"
            continue
        row_report = {}
        for column, edits in columns.items():
            old_text = row[index[column]]
            new_text = old_text
            missing = [old for old, _ in edits if old not in new_text]
            for old, new in edits:
                new_text = new_text.replace(old, new)
            row_report[column] = {"old": old_text, "new": new_text, "phrases_not_found": missing}
            if new_text != old_text:
                row[index[column]] = new_text
                changed += 1
        REPORT["dt_items"][row_name] = row_report
    if DRY or changed == 0:
        return
    out = io.StringIO()
    writer = csv.writer(out, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    table.modify()
    filled = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, out.getvalue())
    # Round-trip check: only the intended text cells may differ from the original export, nothing else.
    before = {r[0]: r for r in csv.reader(io.StringIO(raw)) if r}
    after = {r[0]: r for r in csv.reader(io.StringIO(
        unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(table))) if r}
    diffs = sorted((name, header[i]) for name in set(before) | set(after)
                   for i in range(len(header))
                   if (before.get(name) or [None] * len(header))[i] != (after.get(name) or [None] * len(header))[i])
    intended = sorted((name, column) for name, columns in TEXT_EDITS.items() for column in columns)
    safe = bool(filled) and diffs == intended
    saved = EAL.save_loaded_asset(table, False) if safe else False
    REPORT["dt_items_write"] = {"filled": bool(filled), "saved": saved, "cells_changed": changed,
                                "round_trip_ok": safe, "diffs": diffs}
    log("DT_Items cells=%d filled=%s round_trip_ok=%s saved=%s" % (changed, filled, safe, saved))


def main():
    for path, names in ALLOWLIST.items():
        try:
            process_blueprint(path, names)
        except Exception as error:  # noqa: BLE001 - keep the other assets going
            REPORT["errors"].append(path + ": " + repr(error))
    try:
        edit_dt_items()
    except Exception as error:  # noqa: BLE001
        REPORT["errors"].append("DT_Items: " + repr(error))
    try:
        report_derived_and_ga_requirements()
    except Exception as error:  # noqa: BLE001
        REPORT["errors"].append("registry: " + repr(error))
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w", encoding="utf-8") as handle:
        json.dump(REPORT, handle, indent=1)
    log("written " + OUT_JSON)


main()
unreal.SystemLibrary.quit_editor()
