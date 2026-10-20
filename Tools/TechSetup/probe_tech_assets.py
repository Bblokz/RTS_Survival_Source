"""Read-only probe of the assets the new technologies are wired into; writes Saved/ClaudeExports/tech_probe.json.

Headless: UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>"
"""
import json
import re

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/ClaudeExports/tech_probe.json"
BUTTONS = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
CONTROLLER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_PlayerController"
ICON_STYLE_SEARCH = "BehaviourIconStyleDataAsset"
BXPS = [
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Barracks/FuelCell/Ger_Bxp_BarracksFuelExpansion",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Barracks/RadixReactor/Ger_Bxp_BarracksReactorExpansion",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Forge/RadixFuel/Ger_Bxp_LFactoryFuelExpansion",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Forge/RadixReactor/Ger_Bxp_LFactoryRadixiteReactor",
]

result = {}

buttons = unreal.load_asset(BUTTONS)
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(buttons))
result["button_rows"] = {}
for row in rows:
    if row["Name"] in ("IdActivateMode", "IdDisableMode", "IdResearchTechnology"):
        mapping = {}
        for key, value in (row.get("CustomTypeMapping") or {}).items():
            title = re.search(r'CustomTitle=NSLOCTEXT\("[^"]*", "[^"]*", "(.*?)"\)', value)
            icon = re.search(r"CustomIcon=\"[^']*'([^']*)'", value)
            mapping[key] = [title.group(1) if title else value[:80], icon.group(1) if icon else None]
        result["button_rows"][row["Name"]] = mapping

controller_bp = unreal.load_asset(CONTROLLER)
controller_class = unreal.load_object(None, CONTROLLER + ".RTS_BP_PlayerController_C")
controller_cdo = unreal.get_default_object(controller_class)
result["controller_map_candidates"] = {}
for variable in unreal.BlueprintEditorLibrary.get_blueprint_variable_names(controller_bp) \
        if hasattr(unreal.BlueprintEditorLibrary, "get_blueprint_variable_names") else []:
    result["controller_map_candidates"][str(variable)] = "?"
for name in ["Technologies", "TechnologyEffects", "TechnologiesMap", "TechMap", "Tech Map", "TechnologyMap"]:
    try:
        value = controller_cdo.get_editor_property(name)
        result["controller_map_candidates"][name] = {str(k): str(v) for k, v in value.items()}
    except Exception as error:  # noqa: BLE001 - probing names
        pass

registry = unreal.AssetRegistryHelpers.get_asset_registry()
styles = registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/RTS_Survival", ICON_STYLE_SEARCH))
result["icon_style_assets"] = [str(a.package_name) for a in styles]
for asset_data in styles:
    asset = asset_data.get_asset()
    style_map = asset.get_editor_property("BehaviourIconStyles")
    result["icon_style_" + str(asset_data.asset_name)] = {
        str(k): str(v.get_editor_property("IconTexture").get_path_name() if v.get_editor_property("IconTexture") else None)
        for k, v in style_map.items() if "TECH" in str(k)}

subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
result["bxps"] = {}
for path in BXPS:
    blueprint = unreal.load_asset(path)
    info = {"components": [], "research": []}
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None:
            continue
        info["components"].append(obj.get_class().get_name() + ":" + obj.get_name())
        if obj.get_class().get_name() == "ResearchTechnologyAbilityComp":
            settings = obj.get_editor_property("TechAbilitySettingsPerFaction")
            for faction, faction_settings in settings.items():
                data_obj = faction_settings.get_editor_property("TechnologyData")
                info["research"].append([obj.get_name(), str(faction), faction_settings.get_editor_property(
                    "PreferredAbilityIndex"), str(data_obj.get_editor_property("technology")) if data_obj else None])
    result["bxps"][path.rsplit("/", 1)[1]] = info

result["factions"] = [n for n in dir(unreal.RTSFaction) if n.isupper()]
with open(OUT, "w", encoding="utf-8") as handle:
    json.dump(result, handle, indent=1)
unreal.log("TECH_PROBE written " + OUT)
