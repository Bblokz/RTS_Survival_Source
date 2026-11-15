"""Builds the squad info takeover assets (2026-11-15). Needs the C++ with FSquadInfoTakeoverEntry.

Run headless:  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
               -script="D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/author_squad_info_takeovers.py"

- Imports the picked infantry portraits (PORTRAIT_DIR/<name>.png) as UI textures in UnitIcons/Rus. Pictures come from
  Tools/UnitPortraits (rus_squad_info_takeover.txt + its picks file). Rows with "Texture" instead of "Portrait" use a
  texture that is already in the project (the German squads use their training card art).
- DT_SquadInfoTakeovers: the prefab library (FSquadInfoTakeoverEntry rows). Existing rows are kept; the rows in SQUADS
  are written over, so re-running after a text change is safe.
- Squads open in WBP_VehicleInfoTakeover (one style for every unit takeover), so no widget is made here.
"""
import json
import os

import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover"
LIBRARY_NAME = "DT_SquadInfoTakeovers"
ICON_FOLDER = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Rus"
PORTRAIT_DIR = "D:/UE5Projects/RTS_Survival/Saved/UnitPortraits/RusSquadInfoTakeover_picked"
# The Soviet vehicle portrait whose import settings the infantry portraits copy.
REFERENCE_ICON = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Rus/T_Rus_LightTank_T26"
SQUAD_ROOT = "/Game/RTS_Survival/Blueprints/Squads"
GER_SQUAD_ICONS = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Squads"

SQUADS = [
    {
        "Name": "MosinBazooka",
        "DisplayName": "Bazooka Rifleman",
        "ServiceYear": "1942",
        "Portrait": "T_Rus_Squad_MosinBazooka",
        "Class": SQUAD_ROOT + "/SoloInfantry/Rus/BP_Solo_Rus_Bazooka.BP_Solo_Rus_Bazooka_C",
        "Description": "A Soviet rifleman issued a Lend-Lease bazooka. His rocket is a real threat to light vehicles "
                       "and to the flanks and rear of medium tanks, but he has to stop and aim before every shot. "
                       "Alone he falls quickly to machine-gun fire: keep infantry or an MG in front of your tanks "
                       "to catch him before he gets close.",
    },
    {
        "Name": "HazmatEngineers",
        "DisplayName": "Hazmat Engineers",
        "ServiceYear": "????",
        "Portrait": "T_Rus_Squad_HazmatEngineers",
        "Class": SQUAD_ROOT + "/Rus/Rus_Squad_HazmatEngineers.Rus_Squad_HazmatEngineers_C",
        "Description": "Soviet engineers in sealed suits, armed with Mosin and SVT rifles. They do not lead the "
                       "attack: they block it. They lay anti-tank mine fields and put up hedgehogs and tank traps "
                       "on the roads your armour needs, and they repair their own vehicles. Kill them "
                       "before they dig in, and sweep any road they worked on before you drive down it.",
    },
    {
        "Name": "Vultures",
        "DisplayName": "Hazmat Vultures",
        "ServiceYear": "????",
        "Texture": GER_SQUAD_ICONS + "/T_Ger_Squad_VultureHazmats.T_Ger_Squad_VultureHazmats",
        "Class": SQUAD_ROOT + "/Ger/ArmoryUnlocked/Squad_Ger_HazmatVultures.Squad_Ger_HazmatVultures_C",
        "Description": "Elite assassins in hazmat suits, armed with FG 42 rifles. They are deadly against infantry "
                       "at any range and can switch between rapid fire and aimed sniper fire. A volley of stick "
                       "grenades lets them hurt armour too, but they are few and costly: keep them out of reach of "
                       "tanks and machine guns.",
    },
    {
        "Name": "Scavengers",
        "DisplayName": "Scavengers",
        "ServiceYear": "????",
        "Texture": GER_SQUAD_ICONS + "/T_Ger_Squad_Scavengers.T_Ger_Squad_Scavengers",
        "Class": SQUAD_ROOT + "/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers.Squad_Ger_Scavengers_C",
        "Description": "Your core infantry. Scavengers strip the wrecks they find for resources and repair "
                       "vehicles and structures. They can fight, but they are not soldiers first: keep them behind "
                       "your armour, and send them to the wrecks once the fighting has moved on.",
    },
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log("SQUAD_INFO_TAKEOVER_AUTHOR " + message)


def import_portraits():
    reference = unreal.load_asset(REFERENCE_ICON)
    imported = {}
    for squad in SQUADS:
        if "Portrait" not in squad:
            continue
        source = os.path.join(PORTRAIT_DIR, squad["Portrait"] + ".png")
        if not os.path.exists(source):
            log("no portrait picked for %s (%s)" % (squad["Name"], source))
            continue
        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = ICON_FOLDER
        task.destination_name = squad["Portrait"]
        task.replace_existing = True
        task.automated = True
        task.save = False
        asset_tools.import_asset_tasks([task])
        texture = unreal.load_asset(ICON_FOLDER + "/" + squad["Portrait"])
        if texture is None:
            log("import failed for %s" % source)
            continue
        for prop in ("lod_group", "mip_gen_settings", "never_stream", "compression_settings", "srgb"):
            if reference is not None:
                texture.set_editor_property(prop, reference.get_editor_property(prop))
        editor_assets.save_loaded_asset(texture, False)
        imported[squad["Name"]] = texture.get_path_name()
        log("portrait %s -> %s" % (squad["Name"], texture.get_path_name()))
    return imported


def ensure_library(portraits):
    path = FOLDER + "/" + LIBRARY_NAME
    library = unreal.load_asset(path) if editor_assets.does_asset_exist(path) else None
    if library is None:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", unreal.load_object(None, "/Script/RTS_Survival.SquadInfoTakeoverEntry"))
        library = asset_tools.create_asset(LIBRARY_NAME, FOLDER, unreal.DataTable, factory)
        log("created %s" % path)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(library) or "[]")
    written = {squad["Name"] for squad in SQUADS}
    rows = [row for row in rows if row.get("Name") not in written]
    for squad in SQUADS:
        portrait = portraits.get(squad["Name"]) or squad.get("Texture")
        if portrait is None and editor_assets.does_asset_exist(ICON_FOLDER + "/" + squad["Portrait"]):
            portrait = ICON_FOLDER + "/" + squad["Portrait"] + "." + squad["Portrait"]
        rows.append({
            "Name": squad["Name"],
            "DisplayName": squad["DisplayName"],
            "Description": squad["Description"],
            "ServiceYear": squad["ServiceYear"],
            "Image": portrait or "None",
            "ReferenceSquadClass": squad["Class"],
        })
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(library, json.dumps(rows))
    log("library filled=%s rows=%s" % (ok, [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(library)]))
    log("row json %s" % unreal.DataTableFunctionLibrary.export_data_table_to_json_string(library))
    editor_assets.save_loaded_asset(library, False)


def main():
    portraits = import_portraits()
    ensure_library(portraits)
    log("done")


main()
