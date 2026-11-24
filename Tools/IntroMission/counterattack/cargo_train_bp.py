"""Create / update BP_SovietCargoTrain (child of ASovietCargoTrain) with its class defaults (2026-11-18).

Exec'd through train_editor_server.py (`python train_cmd.py cargo_train_bp.py`). Saves only the Blueprint.
"""
import unreal

DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/'
NAME = 'BP_SovietCargoTrain'
TRAIN = '/Game/RTS_Survival/Blueprints/GroundVehicles/Trains/Soviet/Meshes/'
DEFAULTS = {
    'locomotive_mesh': unreal.load_asset(TRAIN + 'SM_SovietTrain_Locomotive'),
    'flatcar_mesh': unreal.load_asset(TRAIN + 'SM_SovietTrain_CargoCar'),
    'goods_wagon_mesh': unreal.load_asset(TRAIN + 'SM_SovietTrain_FuelCar'),
    # SM_SovietTrain_CargoCar is a container well car: well floor 48 cm, side sills 171-180 cm above the rail, the
    # well only ~2.6 m wide. Tanks (3.2-3.4 m wide) straddle the well with their tracks on the side sills.
    'flatcar_deck_height': 175.0,
    # The siding's rail mesh ends at the path start (y 27448, beyond the north edge); hide cars past it instead of
    # showing them on the extrapolated straight over the horizon skirt.
    'hide_cars_beyond_path_start': True,
}
DEFAULTS.update(globals().get('BP_DEFAULT_OVERRIDES', {}))

path = DIR + NAME
bp = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if bp is None:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.SovietCargoTrain)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DIR.rstrip('/'), unreal.Blueprint, factory)
    print('created', bp.get_path_name())
cdo = unreal.get_default_object(bp.generated_class())
for key, value in DEFAULTS.items():
    cdo.set_editor_property(key, value)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
cdo = unreal.get_default_object(bp.generated_class())
for key in DEFAULTS:
    value = cdo.get_editor_property(key)
    print(key, value.get_name() if hasattr(value, 'get_name') else value)
