import unreal

SkyFolder = '/Game/RTS_Survival/Materials/Sky/'
ProbePath = SkyFolder + 'M_TrainsSunbreakProbe'
if not unreal.EditorAssetLibrary.does_asset_exist(ProbePath):
    assert unreal.EditorAssetLibrary.duplicate_asset(SkyFolder + 'M_FirstMissionSunbreakProbe', ProbePath)
Probe = unreal.load_asset(ProbePath)
MapPosition = unreal.load_object(None, Probe.get_path_name() + ':MaterialExpressionCustom_0')
MapPosition.set_editor_property('code', 'return float3(float2(-42000.0,-51000.0)+UV*float2(58000.0,89000.0),0.0);')
FunctionCall = unreal.load_object(None, Probe.get_path_name() + ':MaterialExpressionMaterialFunctionCall_0')
assert FunctionCall.set_material_function(unreal.load_asset(SkyFolder + 'MF_TrainsSunbreakField'))
assert unreal.MaterialEditingLibrary.connect_material_expressions(MapPosition, '', FunctionCall, 'Position')
unreal.MaterialEditingLibrary.recompile_material(Probe)
assert unreal.EditorAssetLibrary.save_loaded_asset(Probe)
print('PROBE_READY')
print(unreal.RenderingLibrary.read_render_target_raw.__doc__)
print(unreal.GameplayStatics.break_hit_result.__doc__)
