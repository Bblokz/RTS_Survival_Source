"""Points the command, ammo resupply and range auras at their radius types and rising icons (Docs/UI/AuraRadii.md).
Needs a DLL with ERTSRadiusType::FullCircle_AmmoResupplyAura and ERTSVerticalAnimatedIcon::AmmoResupply.
Run through the UECP bridge:
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/AuraRadii/setup_aura_feedback.py").read())
Imports T_Beh_BeingResupplied, adds it to DA_VeticalAnim_Icons, then switches the command, ammo and range aura components
from animated text to animated icons. The heal aura re-applies every second, so it keeps no feedback. Idempotent."""
import unreal

TOOLS = "D:/UE5Projects/RTS_Survival/Source/Tools/AuraRadii/"
ICON_FOLDER = "/Game/RTS_Survival/Textures/GameUI/Behaviours/AmmoResupply"
ICON_NAME = "T_Beh_BeingResupplied"
CATALOG = "/Game/RTS_Survival/Blueprints/GameUI/PooledUI/Data/DA_VeticalAnim_Icons"
VEHICLES = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
AMMO_CARRIERS = [
    VEHICLES + "Ger_ArmoredCar/Sdkfz251_Transport/Variants/AmmoCarrier/Ger_ArmoredCar_251_AmmoCarrier",
    VEHICLES + "Ger_ArmoredCar/Sdkfz-250/Ger_ArmoredCar_Sdkfz250_AmmoCarrier",
]
COMMANDERS = [
    VEHICLES + "Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J_Commander",
    VEHICLES + "Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_Commander",
]
RANGE_AURAS = ["/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/HQ/HQRadar/Ger_Bxp_HQRadar"]


def import_icon():
    task = unreal.AssetImportTask()
    task.filename = TOOLS + ICON_NAME + ".png"
    task.destination_path = ICON_FOLDER
    task.destination_name = ICON_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(ICON_FOLDER + "/" + ICON_NAME)
    # Settings from Docs/UI/AnimatedIcons.md: smooth alpha edges, no mips for a small screen-space icon.
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_no_alpha", False)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    print("icon saved", unreal.EditorAssetLibrary.save_loaded_asset(texture, False))
    return texture


def add_to_catalog(texture):
    catalog = unreal.load_asset(CATALOG)
    definitions = catalog.get_editor_property("icon_definitions")
    template = definitions[unreal.RTSVerticalAnimatedIcon.COMMANDER_BOOST]
    definition = unreal.RTSVerticalAnimatedIconDefinition()
    definition.set_editor_property("texture", texture)
    definition.set_editor_property("display_size", template.get_editor_property("display_size"))
    definition.set_editor_property("tint", template.get_editor_property("tint"))
    definition.set_editor_property("default_animation_settings", template.get_editor_property("default_animation_settings"))
    definitions[unreal.RTSVerticalAnimatedIcon.AMMO_RESUPPLY] = definition
    catalog.set_editor_property("icon_definitions", definitions)
    print("catalog saved", unreal.EditorAssetLibrary.save_loaded_asset(catalog, False))


def find_aura_component(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = library.get_data(handle)
        component = library.get_object_for_blueprint(data, blueprint)
        if component and isinstance(component, unreal.RadiusAOEBehaviourComponent):
            return component
    raise RuntimeError("No radius aura on " + blueprint.get_path_name())


def use_icons(path, icon, radius_type, keep_text_timing):
    blueprint = unreal.load_asset(path)
    component = find_aura_component(blueprint)
    settings = component.get_editor_property("AOEBehaviourSettings")
    text = settings.get_editor_property("text_settings")
    icons = settings.get_editor_property("icon_settings")
    icons.set_editor_property("use_icons", True)
    icons.set_editor_property("icon_type", icon)
    icons.set_editor_property("local_offset", text.get_editor_property("text_offset"))
    # The commander text lingered for 2 s and rose 125 cm; the icon keeps that so it is not a blink.
    icons.set_editor_property("override_animation_settings", keep_text_timing)
    if keep_text_timing:
        timing = text.get_editor_property("settings")
        animation = unreal.RTSVerticalAnimIconSettings()
        animation.set_editor_property("visible_duration", timing.get_editor_property("visible_duration"))
        animation.set_editor_property("fade_out_duration", timing.get_editor_property("fade_out_duration"))
        animation.set_editor_property("delta_z", timing.get_editor_property("delta_z"))
        icons.set_editor_property("animation_settings", animation)
    text.set_editor_property("use_text", False)
    settings.set_editor_property("icon_settings", icons)
    settings.set_editor_property("text_settings", text)
    component.set_editor_property("AOEBehaviourSettings", settings)

    radius = component.get_editor_property("RadiusSettings")
    radius.set_editor_property("radius_type", radius_type)
    component.set_editor_property("RadiusSettings", radius)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    print(path.split("/")[-1], "saved", unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False))


texture = import_icon()
add_to_catalog(texture)
for carrier in AMMO_CARRIERS:
    use_icons(carrier, unreal.RTSVerticalAnimatedIcon.AMMO_RESUPPLY, unreal.RTSRadiusType.FULL_CIRCLE_AMMO_RESUPPLY_AURA, False)
for commander in COMMANDERS:
    use_icons(commander, unreal.RTSVerticalAnimatedIcon.COMMANDER_BOOST, unreal.RTSRadiusType.FULL_CIRCLE_COMMAND_AURA, True)
for range_aura in RANGE_AURAS:
    use_icons(range_aura, unreal.RTSVerticalAnimatedIcon.RANGE_BOOST, unreal.RTSRadiusType.FULL_CIRCLE_IMPROVED_RANGE_AREA, False)
