"""Fills DA_UnitVariants with the finished variant Blueprints of the German units and sets each Blueprint's
RTS component UnitVariant (Docs/Units/UnitVariants.md, sections 3.2 and 7).

Variants are per subtype, so a unit both factions train gets the same variants in both. The order inside an entry is
the saved variant index: append new variants, never reorder, or saved choices change meaning.

Run as a commandlet: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file>. Safe to run again.
"""
import os

import unreal

ROOT = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
DATA_ASSET = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_UnitVariants"
# The first test variant (G turret on the H hull); the real Pz IV G variant replaces it.
OBSOLETE_TEST_VARIANT = ROOT + "Ger_Medium/PanzerIV/Variants/Ger_Vehicle_PanzerIV_G_VariantA"
BASE_DESCRIPTION = "The standard configuration."
# Selection screen pictures: 256x256 crops of the unit portrait tool (Tools/UnitPortraits, front-right idle view).
# A missing crop keeps the texture already imported, so the script runs without the Saved folder too.
ICON_SOURCE = "D:/UE5Projects/RTS_Survival/Saved/VariantPortraits_Cropped"
ICON_FOLDER = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Variants"
BASE_BLUEPRINTS = {
    "TANK_SDKFZ250": "Ger_ArmoredCar/Sdkfz-250/Ger_ArmoredCar_Sdkfz250",
    "TANK_SDKFZ250_37MM": "Ger_ArmoredCar/sdkfz-250_37mm/Ger_ArmoredCar_Sdkfz250_37",
    "TANK_PZ38T": "Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T",
    "TANK_PZ38T_RAIL_GUN": "Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL",
    "TANK_PZ_JAGER": "Ger_Light/PanzerJager1/Ger_Vehicle_PanzerJager1",
    "TANK_PZ_I_15CM": "Ger_Light/PZ1_15cm/Ger_Vehicle_Pz1_15CM",
    "TANK_PZ_IV_F1": "Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1",
    "TANK_PZ_IV_G": "Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G",
    "TANK_PZ_IV_70": "Ger_Medium/JagdPanzerIV70/Ger_Vehicle_JagdPanzerIV70",
    "TANK_MARDER": "Ger_Light/Marder1/Ger_Vehicle_Marder",
}

# Subtype -> [(Blueprint path under ROOT, display name, description)], VariantA first.
TANK_VARIANTS = {
    "TANK_SDKFZ250": [
        ("Ger_ArmoredCar/Sdkfz-250/Ger_ArmoredCar_Sdkfz250_ArmouredAmbulance", "Armoured Ambulance",
         "Ambulance body with red cross markings. The MG 34 is removed."),
        ("Ger_ArmoredCar/Sdkfz-250/Ger_ArmoredCar_Sdkfz250_AmmoCarrier", "Ammo Carrier",
         "Ammunition carrier body on the Sd.Kfz. 250 chassis."),
    ],
    "TANK_SDKFZ250_37MM": [
        ("Ger_ArmoredCar/sdkfz-250_37mm/Ger_ArmoredCar_Sdkfz250_37_LongBarrel", "Long Barrel",
         "A longer 3.7 cm barrel."),
        ("Ger_ArmoredCar/sdkfz-250_37mm/Ger_ArmoredCar_Sdkfz250_37_Howitzer250", "Howitzer 250",
         "A short howitzer replaces the 3.7 cm gun."),
    ],
    "TANK_PZ38T": [
        ("Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_ATAmmoStock", "AT Ammo Stock",
         "Anti-tank ammunition stowed on a reworked hull and turret."),
        ("Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_Rocketeer", "Rocketeer",
         "An 11 cm rocket rack on the turret, with a long reload between salvos."),
    ],
    "TANK_PZ38T_RAIL_GUN": [
        ("Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL_Overcharge", "Overcharge",
         "A broad, heavy capacitor bank replaces the rear deck."),
        ("Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL_Sniper", "Sniper",
         "A long, narrow rail barrel with a telescopic sight on the turret."),
    ],
    "TANK_PZ_JAGER": [
        ("Ger_Light/PanzerJager1/Ger_Vehicle_PanzerJager1_MobileBattery", "Mobile Battery",
         "A larger ammunition rack on the rear deck."),
        ("Ger_Light/PanzerJager1/Ger_Vehicle_PanzerJager1_AmbushShield", "Ambush Shield",
         "An angled gun shield with camouflage netting over the hull."),
    ],
    "TANK_PZ_I_15CM": [
        ("Ger_Light/PZ1_15cm/Ger_Vehicle_Pz1_15CM_IndirectFire", "Indirect Fire",
         "The 15 cm gun set up for indirect fire on a reworked superstructure."),
        ("Ger_Light/PZ1_15cm/Ger_Vehicle_Pz1_15CM_DirectFire", "Direct Fire",
         "A short high-velocity gun behind a flat shield replaces the howitzer."),
    ],
    "TANK_PZ_IV_F1": [
        ("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1_Spotter", "Spotter",
         "Observation fittings on a reworked hull and turret."),
        ("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1_Assault", "Assault",
         "The turret is removed for an open assault compartment."),
    ],
    "TANK_PZ_IV_G": [
        ("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G_DoubleTurret", "Double Turret",
         "A second turret at the rear of a reworked hull."),
    ],
    "TANK_PZ_IV_70": [
        ("Ger_Medium/JagdPanzerIV70/Ger_Vehicle_JagdPanzerIV70_Ambush", "Ambush",
         "Camouflage netting and a low gun shield on a reworked hull."),
        ("Ger_Medium/JagdPanzerIV70/Ger_Vehicle_JagdPanzerIV70_Siege", "Siege",
         "A siege gun in the casemate."),
    ],
    "TANK_MARDER": [
        ("Ger_Light/Marder1/Ger_Vehicle_Marder_DualBarrel", "Dual Barrel",
         "Twin anti-tank barrels on the gun mount."),
    ],
}
SLOT_VARIANTS = [unreal.UnitVariant.VARIANT_A, unreal.UnitVariant.VARIANT_B]

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
log = []


def full_path(relative_path):
    return ROOT + relative_path


def set_blueprint_unit_variant(blueprint, unit_variant):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        if str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data)) != "RTS":
            continue
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)
        if template is None:
            break
        if template.get_editor_property("unit_variant") != unit_variant:
            template.modify()
            template.set_editor_property("unit_variant", unit_variant)
        return True
    log.append("NO RTS component on %s" % blueprint.get_name())
    return False


def import_icon(relative_path):
    blueprint_name = relative_path.rsplit("/", 1)[-1]
    texture_path = "%s/T_Variant_%s" % (ICON_FOLDER, blueprint_name)
    source = "%s/%s/%s_FrontRight_idle.png" % (ICON_SOURCE, blueprint_name, blueprint_name)
    if os.path.exists(source):
        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = ICON_FOLDER
        task.destination_name = "T_Variant_" + blueprint_name
        task.replace_existing = True
        task.automated = True
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(texture_path)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        texture.set_editor_property("never_stream", True)
        unreal.EditorAssetLibrary.save_asset(texture_path, only_if_is_dirty=False)
    if not unreal.EditorAssetLibrary.does_asset_exist(texture_path):
        log.append("NO ICON for %s" % blueprint_name)
        return None
    return unreal.load_asset(texture_path)


def make_definition(display_name, description, unit_class=None, icon=None):
    definition = unreal.UnitVariantDefinition()
    definition.set_editor_property("display_name", display_name)
    definition.set_editor_property("description", description)
    if unit_class is not None:
        definition.set_editor_property("unit_class", unit_class)
    if icon is not None:
        definition.set_editor_property("icon", icon)
    return definition


def build_tank_entries():
    entries = {}
    for subtype_name, variants in TANK_VARIANTS.items():
        definitions = [make_definition("", BASE_DESCRIPTION, icon=import_icon(BASE_BLUEPRINTS[subtype_name]))]
        for slot_index, (relative_path, display_name, description) in enumerate(variants):
            blueprint = unreal.load_asset(full_path(relative_path))
            if blueprint is None:
                log.append("MISSING %s" % relative_path)
                continue
            set_blueprint_unit_variant(blueprint, SLOT_VARIANTS[slot_index])
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
            definitions.append(make_definition(display_name, description, blueprint.generated_class(),
                                               import_icon(relative_path)))
        entry = unreal.UnitVariantEntry()
        entry.set_editor_property("variants", definitions)
        entries[getattr(unreal.TankSubtype, subtype_name)] = entry
        log.append("ENTRY %s: %s" % (subtype_name, [d.get_editor_property("display_name") for d in definitions[1:]]))
    return entries


def run():
    data_asset = unreal.load_asset(DATA_ASSET)
    data_asset.modify()
    data_asset.set_editor_property("m_tank_variants", build_tank_entries())
    data_asset.set_editor_property("m_debug_forced_variants", [])
    unreal.EditorAssetLibrary.save_loaded_asset(data_asset, False)

    if unreal.EditorAssetLibrary.does_asset_exist(OBSOLETE_TEST_VARIANT):
        log.append("deleted test variant: %s" % unreal.EditorAssetLibrary.delete_asset(OBSOLETE_TEST_VARIANT))

    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    settings = unreal.ValidateAssetsSettings()
    settings.set_editor_property("show_if_no_failures", True)
    _, results = validator.validate_assets_with_settings(
        [unreal.EditorAssetLibrary.find_asset_data(DATA_ASSET)], settings)
    log.append("validation invalid=%d warnings=%d valid=%d" % (
        results.get_editor_property("num_invalid"), results.get_editor_property("num_warnings"),
        results.get_editor_property("num_valid")))
    for line in log:
        print("FILL_UNIT_VARIANTS " + line)


run()
