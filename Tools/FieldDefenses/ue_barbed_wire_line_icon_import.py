# Step 3 of 3 of the HQ barbed wire line's option picture (see ue_barbed_wire_line_icon.py): imports the graded picture
# as T_BxpOption_GerBarbedWireLine with the field trench picture's texture settings and points
# SB_OptionImage_GerBarbedWireLine at it. A save fails with error 32 while another editor or game has the package open.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -NullRHI
import unreal

PICTURE_PATH = 'D:/UE5Projects/RTS_Shorts/Saved/BarbedWireCaptures/T_BxpOption_GerBarbedWireLine.png'
PICTURE_NAME = 'T_BxpOption_GerBarbedWireLine'
UI_FOLDER = '/Game/RTS_Survival/Blueprints/Environment/BarbedWireLine/UI'
BRUSH_PATH = UI_FOLDER + '/SB_OptionImage_GerBarbedWireLine'
TRENCH_PICTURE_PATH = '/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/UI/T_BxpOption_GerFieldTrench'
LOG_PREFIX = 'BARBED_WIRE_ICON_IMPORT '


def import_picture():
    task = unreal.AssetImportTask()
    task.filename = PICTURE_PATH
    task.destination_path = UI_FOLDER
    task.destination_name = PICTURE_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(UI_FOLDER + '/' + PICTURE_NAME)
    trench_picture = unreal.load_asset(TRENCH_PICTURE_PATH)
    for setting in ('compression_settings', 'lod_group', 'mip_gen_settings', 'srgb', 'never_stream'):
        texture.set_editor_property(setting, trench_picture.get_editor_property(setting))
    texture.set_editor_property('compression_no_alpha', True)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.log(LOG_PREFIX + 'texture saved={}'.format(saved))
    return texture


def point_brush_at(texture):
    brush_asset = unreal.load_asset(BRUSH_PATH)
    brush = brush_asset.get_editor_property('brush')
    brush.set_editor_property('resource_object', texture)
    brush_asset.set_editor_property('brush', brush)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(brush_asset, only_if_is_dirty=False)
    unreal.log(LOG_PREFIX + 'brush saved={} resource={}'.format(saved, texture.get_path_name()))


point_brush_at(import_picture())
