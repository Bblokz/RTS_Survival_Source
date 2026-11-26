# Points SB_OptionImage_GerBarbedWireLine back at the scavengers' barbed wire icon (T_FieldConstruction_Barbedwire),
# for when the rendered option picture (ue_barbed_wire_line_icon.py) is not right.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -NullRHI
import unreal

BRUSH_PATH = '/Game/RTS_Survival/Blueprints/Environment/BarbedWireLine/UI/SB_OptionImage_GerBarbedWireLine'
ICON_PATH = '/Game/RTS_Survival/Textures/GameUI/ActionUI/FieldConstruction/T_FieldConstruction_Barbedwire'

brush_asset = unreal.load_asset(BRUSH_PATH)
brush = brush_asset.get_editor_property('brush')
brush.set_editor_property('resource_object', unreal.load_asset(ICON_PATH))
brush_asset.set_editor_property('brush', brush)
saved = unreal.EditorAssetLibrary.save_loaded_asset(brush_asset, only_if_is_dirty=False)
unreal.log('BARBED_WIRE_ICON_FALLBACK brush saved={}'.format(saved))
