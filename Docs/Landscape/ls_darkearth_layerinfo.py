"""Creates DarkEarth_LayerInfo for the DryCastle landscape master (the tenth layer, see ls_darkearth_build.py).

LayerName is read-only to Python; the console SET trick fills it, but only in a headless run:

    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>"
"""
import unreal

NAME = "DarkEarth"
DATA_DIR = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/DATA"
PATH = "%s/%s_LayerInfo" % (DATA_DIR, NAME)


def run():
    if unreal.EditorAssetLibrary.does_asset_exist(PATH):
        info = unreal.load_asset(PATH)
    else:
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME + "_LayerInfo", DATA_DIR, unreal.LandscapeLayerInfoObject, None)
        info = unreal.load_asset(PATH)
        unreal.SystemLibrary.execute_console_command(None, "set %s LayerName %s" % (info.get_path_name(), NAME))
        info.set_editor_property("no_weight_blend", False)
        info.set_editor_property("layer_usage_debug_color", unreal.LinearColor(0.12, 0.09, 0.07, 1.0))
    name = str(info.get_editor_property("layer_name"))
    if name != NAME:
        # Never leave a misnamed info on disk: importing a landscape with it crashes.
        unreal.log_error("[ls_darkearth_layerinfo] layer name is %s, not saved" % name)
        return
    unreal.EditorAssetLibrary.save_loaded_asset(info, only_if_is_dirty=False)
    unreal.log("[ls_darkearth_layerinfo] saved %s with layer name %s" % (PATH, name))


run()
