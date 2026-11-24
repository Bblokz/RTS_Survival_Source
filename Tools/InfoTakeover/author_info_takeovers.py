"""Builds the vehicle info takeover assets (2026-11-11). Needs the C++ with UW_VehicleInfoTakeover.

Run headless:  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
               -script="D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/author_info_takeovers.py"

- Imports the picked Soviet portraits (PORTRAIT_DIR/<name>.png) as UI textures in UnitIcons/Rus.
- WBP_VehicleInfoTakeover: Blueprint child of UW_VehicleInfoTakeover whose M_Style.Skin is copied from
  BP_W_UnitVariantSelectionMenu and whose checkbox uses the escape menu's settings checkbox skin.
- DT_VehicleInfoTakeovers: the prefab library (FVehicleInfoTakeoverEntry rows). Existing rows are kept; the rows in
  VEHICLES are written over, so re-running after a text change is safe.
"""
import json
import os

import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover"
WIDGET_NAME = "WBP_VehicleInfoTakeover"
LIBRARY_NAME = "DT_VehicleInfoTakeovers"
ICON_FOLDER = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Rus"
PORTRAIT_DIR = "D:/UE5Projects/RTS_Survival/Saved/UnitPortraits/RusInfoTakeover_picked"
VARIANT_MENU = "/Game/RTS_Survival/FactionSelection/UI/BP_W_UnitVariantSelectionMenu"
CHECKBOX_STYLE_FILE = "D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/settings_checkbox_style.txt"
# A German placard portrait whose import settings the Soviet portraits copy.
REFERENCE_ICON = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/LightTanks/T_Ger_LightTank_Hetzer"
VEHICLE_ROOT = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle"

VEHICLES = [
    {
        "Name": "T26",
        "DisplayName": "T-26",
        "ProductionYear": "1931",
        "Portrait": "T_Rus_LightTank_T26",
        "Class": VEHICLE_ROOT + "/Rus_Light/T26/Rus_Vehicle_T26.Rus_Vehicle_T26_C",
        "Description": "A slow light tank with a 45mm gun that can pose a danger to most light vehicles. "
                       "Its armor is very minimal and only reliably protects against small arms.",
    },
    {
        "Name": "BT7",
        "DisplayName": "BT-7",
        "ProductionYear": "1935",
        "Portrait": "T_Rus_LightTank_BT7",
        "Class": VEHICLE_ROOT + "/Rus_Light/BT-7/Rus_Vehicle_BT-7.Rus_Vehicle_BT-7_C",
        "Description": "A very fast light tank with the same 45mm gun as the T-26. It strikes light vehicles from "
                       "the flanks and is gone before you can answer, but its thin armor gives way to almost any "
                       "anti-tank gun that catches it.",
    },
    {
        "Name": "BT7_4",
        "DisplayName": "BT-7-4",
        "ProductionYear": "1937",
        "Portrait": "T_Rus_LightTank_BT7_4",
        "Class": VEHICLE_ROOT + "/Rus_Light/BT-7/Rus_Vehicle_BT-7-4.Rus_Vehicle_BT-7-4_C",
        "Description": "A fast BT-7 refitted with a short 76mm gun that fires heavy high-explosive shells. It is a "
                       "threat to infantry, guns and light vehicles, but it still wears the BT-7's thin armor.",
    },
    {
        "Name": "T34_76",
        "DisplayName": "T-34",
        "ProductionYear": "1940",
        "Portrait": "T_Rus_MediumTank_T34_76",
        "Class": VEHICLE_ROOT + "/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C",
        "Description": "The backbone of the Soviet tank force. Its 76mm gun penetrates most light and medium "
                       "armor, and its sloped front deflects many shots. Engage it from the sides, where its "
                       "armor is thinner.",
    },
    {
        "Name": "T34E",
        "DisplayName": "T-34E",
        "ProductionYear": "1942",
        "Portrait": "T_Rus_MediumTank_T34E",
        "Class": VEHICLE_ROOT + "/Rus_Medium/T34/Rus_Vehicle_T34E.Rus_Vehicle_T34E_C",
        "Description": "A T-34 up-armored with bolted-on plates and riveted side skirts. The skirts soak up the "
                       "first hits on its flanks and can be shot off; once they are gone, the hull behind them is "
                       "barely thicker than a regular T-34's.",
    },
    {
        "Name": "KV1_L11",
        "DisplayName": "KV-1 L-11",
        "ProductionYear": "1940",
        "Portrait": "T_Rus_HeavyTank_KV1_L11",
        "Class": VEHICLE_ROOT + "/Rus_Heavy/KV1/Rus_Vehicle_KV-1_L11.Rus_Vehicle_KV-1_L11_C",
        "Description": "A slow heavy tank with thick armor on every side and a 76mm L-11 gun. Light and medium "
                       "guns bounce off its front; bring your heaviest anti-tank weapons or work around to its "
                       "rear.",
    },
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log("INFO_TAKEOVER_AUTHOR " + message)


def import_portraits():
    reference = unreal.load_asset(REFERENCE_ICON)
    imported = {}
    for vehicle in VEHICLES:
        source = os.path.join(PORTRAIT_DIR, vehicle["Portrait"] + ".png")
        if not os.path.exists(source):
            log("no portrait picked for %s (%s)" % (vehicle["Name"], source))
            continue
        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = ICON_FOLDER
        task.destination_name = vehicle["Portrait"]
        task.replace_existing = True
        task.automated = True
        task.save = False
        asset_tools.import_asset_tasks([task])
        texture = unreal.load_asset(ICON_FOLDER + "/" + vehicle["Portrait"])
        if texture is None:
            log("import failed for %s" % source)
            continue
        for prop in ("lod_group", "mip_gen_settings", "never_stream", "compression_settings", "srgb"):
            if reference is not None:
                texture.set_editor_property(prop, reference.get_editor_property(prop))
        editor_assets.save_loaded_asset(texture, False)
        imported[vehicle["Name"]] = texture.get_path_name()
        log("portrait %s -> %s" % (vehicle["Name"], texture.get_path_name()))
    return imported


def ensure_widget_blueprint():
    path = FOLDER + "/" + WIDGET_NAME
    widget_blueprint = unreal.load_asset(path) if editor_assets.does_asset_exist(path) else None
    if widget_blueprint is None:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/RTS_Survival.W_VehicleInfoTakeover"))
        widget_blueprint = asset_tools.create_asset(WIDGET_NAME, FOLDER, unreal.WidgetBlueprint, factory)
        log("created %s" % path)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_blueprint)
    widget_cdo = unreal.get_default_object(widget_blueprint.generated_class())
    menu = unreal.load_asset(VARIANT_MENU)
    skin_text = unreal.get_default_object(menu.generated_class()).get_editor_property("m_style").export_text()
    with open(CHECKBOX_STYLE_FILE, encoding="utf-8") as handle:
        checkbox_text = handle.read().strip()
    style = widget_cdo.get_editor_property("m_style")
    style.import_text("(Skin=%s,NeverShowAgainCheckBox=%s)" % (skin_text, checkbox_text))
    widget_cdo.set_editor_property("m_style", style)
    check = widget_cdo.get_editor_property("m_style").export_text()
    log("style copied: skin font %s, checkbox skin %s" % (
        "Roboto" in check or "FontObject" in check, "SettingsCheck" in check))
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_blueprint)
    editor_assets.save_loaded_asset(widget_blueprint, False)
    return widget_blueprint


def ensure_library(portraits):
    path = FOLDER + "/" + LIBRARY_NAME
    library = unreal.load_asset(path) if editor_assets.does_asset_exist(path) else None
    if library is None:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", unreal.load_object(None, "/Script/RTS_Survival.VehicleInfoTakeoverEntry"))
        library = asset_tools.create_asset(LIBRARY_NAME, FOLDER, unreal.DataTable, factory)
        log("created %s" % path)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(library) or "[]")
    written = {vehicle["Name"] for vehicle in VEHICLES}
    rows = [row for row in rows if row.get("Name") not in written]
    for vehicle in VEHICLES:
        portrait = portraits.get(vehicle["Name"])
        if portrait is None and editor_assets.does_asset_exist(ICON_FOLDER + "/" + vehicle["Portrait"]):
            portrait = ICON_FOLDER + "/" + vehicle["Portrait"] + "." + vehicle["Portrait"]
        rows.append({
            "Name": vehicle["Name"],
            "DisplayName": vehicle["DisplayName"],
            "Description": vehicle["Description"],
            "ProductionYear": vehicle["ProductionYear"],
            "Image": portrait or "None",
            "ReferenceVehicleClass": vehicle["Class"],
        })
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(library, json.dumps(rows))
    log("library filled=%s rows=%s" % (ok, [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(library)]))
    editor_assets.save_loaded_asset(library, False)


def main():
    portraits = import_portraits()
    ensure_widget_blueprint()
    ensure_library(portraits)
    log("done")


main()
