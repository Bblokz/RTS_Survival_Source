"""Imports the composed global ability icons as UI textures and sets them on the DA_GlobalAbilitySystem templates.
Run in the open editor: exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/import_ability_icons.py").read())
Re-running re-imports over the same textures. Check the asset mtimes afterwards: a save that another process blocks
still returns True (see the uecp-editor-bridge notes)."""

import unreal

ICON_DIR = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/out"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities"
DATA_ASSET = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
# The buttons are 96 slate units; 256 keeps them sharp at 4K UI scale and the 512 source stays in the asset.
MAX_SIZE = 256

# EGlobalAbility value name -> composed icon (the enum shares its python name with the UGlobalAbility class)
ICONS = {
    "GA_BARRAGE": "Barrage",
    "GA_HEAVY_CARPET_JU390B_250_GR": "Ju390Carpet250",
    "GA_3X_JU87_STRAFING": "3xJu87Strafing",
    "GA_HEAVY_CARPET_PE8_500_GR": "PE8Carpet500",
    "GA_HEAVY_CARPET_PE8_1000_GR": "PE8Carpet1000",
    "GA_HEAVY_CARPET_PE8_3000_GR": "PE8Carpet3000",
    "GA_SOVIET_T34_DROP": "SovietT34Drop",
    "GA_SOVIET_T34_DROP_VARIATION": "SovietT34DropVariation",
    "GA_SOVIET_PARATROOPERS": "SovietParatroopers",
    "GA_GER_PARATROOPERS": "GerParatroopers",
    "GA_2X_HORTENTRAFING": "2xHo229Strafing",
    "GA_GER_PZ_III_FLAK41_DROP": "GerPzIIIFlak41Drop",
    "GA_GER_SDKFZ7_FLAK_DROP": "GerSdkfz7FlakDrop",
    "GA_GER_RADIXITE_PZ_II_MARDER_DROP": "GerRadixitePzIIMarderDrop",
    "GA_GER_FLAK_LUCHS_STUG_DROP": "GerFlakLuchsStugDrop",
    "GA_GER_SPOTTER_PZ_III_DROP": "GerSpotterPzIIIDrop",
    "GA_GER_RECON_FLIGHT": "GerReconFlight",
    "GA_SOVIET_RECON_FLIGHT": "SovietReconFlight",
    "GA_GER_FLARE_CURTAIN": "GerFlareCurtain",
    "GA_GER_AIR_MINEFIELD": "GerAirMinefield",
    "GA_SOVIET_AIR_MINEFIELD": "SovietAirMinefield",
    "GA_GER_RADIXITE_NUKE": "GerRadixiteNuke",
    "GA_SOVIET_RADIXITE_NUKE": "SovietRadixiteNuke",
}


def import_texture(name):
    asset_name = "T_GlobalAbility_" + name
    task = unreal.AssetImportTask()
    task.filename = "%s/T_GA_%s.png" % (ICON_DIR, name)
    task.destination_path = FOLDER
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + asset_name)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    # The UI group has no mips by default; the card is drawn at a quarter of its size and would shimmer without them.
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("max_texture_size", MAX_SIZE)
    unreal.EditorAssetLibrary.save_asset(FOLDER + "/" + asset_name, only_if_is_dirty=False)
    return texture


def assign_icons():
    data_asset = unreal.load_asset(DATA_ASSET)
    templates = {key.name: value for key, value in
                 data_asset.get_editor_property("M_AbilityTemplatesByType").items()}
    for ability, name in ICONS.items():
        template = templates[ability]
        # A fresh struct per template: edited copies of get_editor_property have leaked between templates before.
        old = template.get_editor_property("M_UISettings")
        settings = type(old)()
        settings.import_text(old.export_text())
        settings.set_editor_property("AbilityIcon", import_texture(name))
        template.modify()
        template.set_editor_property("M_UISettings", settings)
    data_asset.modify()
    saved = unreal.EditorAssetLibrary.save_loaded_asset(data_asset, False)
    print("SAVED_DA", saved)
    for ability in ICONS:
        icon = templates[ability].get_editor_property("M_UISettings").get_editor_property("AbilityIcon")
        print("ICON", ability, icon.get_name() if icon else None,
              templates[ability].get_editor_property("M_UISettings").get_editor_property("Title"))


assign_icons()
