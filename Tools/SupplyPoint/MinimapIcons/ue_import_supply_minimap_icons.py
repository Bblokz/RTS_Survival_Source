"""Imports the final supply minimap icons and adds their rows to DA_MinimapIcon.
Run in a commandlet on a build that knows the EMinimapIconType supply values, with the editor closed (the data
asset must not be locked):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script=D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/MinimapIcons/ue_import_supply_minimap_icons.py
Prints SUPPLY_ICONS lines; the last one is SUPPLY_ICONS RESULT OK|FAIL.
"""
import unreal

SRC = 'D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/MinimapIcons/Final/'
DEST = '/Game/RTS_Survival/Blueprints/GameUI/MiniMap/MiniMapIcons/Supply'
DATA_ASSET = '/Game/RTS_Survival/Blueprints/GameUI/MiniMap/MiniMapIcons/DA_MinimapIcon'
# enum value name -> draw size in Slate pixels. The Italian faction has no flag asset yet, so no row.
ICONS = {
    'SupplyTruck': 28.0,
    'SupplyFlag_DirectorOfShield': 32.0,
    'SupplyFlag_DirectorOfForge': 32.0,
    'SupplyFlag_DirectorOfHarvest': 32.0,
    'SupplyFlag_GerBreakthrough': 32.0,
    'SupplyFlag_GerStrikeDivision': 32.0,
}


def log(message):
    unreal.log('SUPPLY_ICONS ' + message)


def find_enum(value_name):
    wanted = value_name.replace('_', '').lower()
    for attr in dir(unreal.MinimapIconType):
        if attr.replace('_', '').lower() == wanted:
            return getattr(unreal.MinimapIconType, attr)
    return None


def import_texture(value_name):
    asset_name = 'T_MinimapIcon_' + value_name
    task = unreal.AssetImportTask()
    task.filename = SRC + asset_name + '.png'
    task.destination_path = DEST
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(DEST + '/' + asset_name)
    if texture is None:
        return None
    # Same settings as the wave icons: UI texture, no mips, sRGB, not streamed.
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('never_stream', True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


def main():
    ok = True
    data_asset = unreal.load_asset(DATA_ASSET)
    if data_asset is None:
        log('data asset not found')
        return False
    icons = data_asset.get_editor_property('m_minimap_icons')
    for value_name, size in ICONS.items():
        enum_value = find_enum(value_name)
        texture = import_texture(value_name)
        if enum_value is None or texture is None:
            log('FAIL %s enum=%s texture=%s' % (value_name, enum_value, texture))
            ok = False
            continue
        row = unreal.MinimapIcon()
        row.import_text('(M_SizeXY=%f,M_Texture="%s")' % (size, texture.get_path_name()))
        icons[enum_value] = row
        log('row %s size=%s texture=%s %dx%d' % (value_name, size, texture.get_path_name(),
                                                texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
    data_asset.set_editor_property('m_minimap_icons', icons)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(data_asset, False)
    log('data asset saved=%s rows=%d' % (saved, len(data_asset.get_editor_property('m_minimap_icons'))))
    for key, row in data_asset.get_editor_property('m_minimap_icons').items():
        log('have %s -> %s' % (key, row.export_text()))
    return ok and saved


log('RESULT ' + ('OK' if main() else 'FAIL'))
