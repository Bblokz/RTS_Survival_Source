# Sets BP_RefineryUnloadStation as Ger_Refinery_Truck's unload station class and saves the truck (the last step of
# ue_import_refinery_crane.py, on its own for when the truck asset was locked by a running game process).
# Headless (DebugGame): UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
import unreal

BP = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/UnloadCrane/BP_RefineryUnloadStation'
TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/Ger_Refinery_Truck'

bp = unreal.load_asset(BP)
truck = unreal.load_asset(TRUCK)
cdo = unreal.get_default_object(truck.generated_class())
cdo.set_editor_property('m_unload_station_class', bp.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(truck)
cdo = unreal.get_default_object(truck.generated_class())
ok = unreal.EditorAssetLibrary.save_loaded_asset(truck, False)
unreal.log_warning('RTS_UNLOAD_IMPORT hook class=%s saved=%s' % (cdo.get_editor_property('m_unload_station_class'), ok))
