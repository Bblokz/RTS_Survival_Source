"""Points BP_GliderTankDelivery_Mammut at the game model and gives GA_GerE100Drop its icon (DebugGame-Cmd).

UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
Run after import_mammut_model.py and compose_mammut_icon.py.
"""
import unreal

SETUP = 'D:/UE5Projects/RTS_Survival/Source/Tools/GliderDelivery/setup_mammut_delivery.py'
ICON_PNG = 'D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/out/T_GA_GerMammutE100Drop.png'
ICON_FOLDER = '/Game/RTS_Survival/Textures/GameUI/GlobalAbilities'
ICON_NAME = 'T_GlobalAbility_GerMammutE100Drop'
MAX_ICON_SIZE = 256

MAMMUT_SKIP_MAIN = True
exec(open(SETUP, encoding='utf8').read())
MESH_FOLDER = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut/Meshes'


def import_icon():
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', ICON_PNG)
    task.set_editor_property('destination_path', ICON_FOLDER)
    task.set_editor_property('destination_name', ICON_NAME)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(ICON_FOLDER + '/' + ICON_NAME)
    # Same settings as the other ability cards (GlobalAbilityIcons/import_ability_icons.py).
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('max_texture_size', MAX_ICON_SIZE)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    log('icon imported')
    return texture


def set_ability_icon(texture):
    da = load(DA_PATH)
    templates = da.get_editor_property('m_ability_templates_by_type')
    ability_enum = type(next(iter(templates.keys())))
    ability = templates[find_enum(ability_enum, 'GA_GerE100Drop')]
    ui = fresh_copy(ability.get_editor_property('m_ui_settings'))
    ui.set_editor_property('ability_icon', texture)
    ability.set_editor_property('m_ui_settings', ui)
    log('data asset saved=%s' % unreal.EditorAssetLibrary.save_loaded_asset(da, False))


make_blueprint()
set_ability_icon(import_icon())
log('FINALIZE DONE')
