"""Writes the code defaults (kit, presets of every base type, difficulty rows) into DA_SurvivalSovietBases and saves it."""
import unreal

PATH = '/Game/RTS_Survival/SurvivalMode/Data/SovietBases/DA_SurvivalSovietBases'
asset = unreal.load_asset(PATH)
print('SOVIET_ASSET loaded', asset)
asset.call_method('ResetToDefaults')
print('SOVIET_ASSET presets', len(asset.get_editor_property('presets')))
print('SOVIET_ASSET saved', unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False))
