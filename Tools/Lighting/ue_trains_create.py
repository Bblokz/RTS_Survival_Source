"""Create independent Trains lighting assets; run in the Unreal editor."""
import unreal

SkyFolder = '/Game/RTS_Survival/Materials/Sky/'
RayFolder = '/Game/RTS_Survival/SpecialEffects/Atmosphere/'
AssetCopies = [
    (SkyFolder + 'MPC_FirstMissionSunbreaks', SkyFolder + 'MPC_TrainsSunbreaks'),
    (SkyFolder + 'MF_FirstMissionSunbreakField', SkyFolder + 'MF_TrainsSunbreakField'),
    (SkyFolder + 'M_LF_FirstMissionSunbreaks', SkyFolder + 'M_LF_TrainsSunbreaks'),
    (RayFolder + 'M_FakeGodRay_FirstMission', RayFolder + 'M_FakeGodRay_Trains'),
    (RayFolder + 'BP_FakeGodRayFirstMission', RayFolder + 'BP_FakeGodRayTrains'),
]
for SourcePath, TargetPath in AssetCopies:
    if not unreal.EditorAssetLibrary.does_asset_exist(TargetPath):
        assert unreal.EditorAssetLibrary.duplicate_asset(SourcePath, TargetPath), TargetPath
    print('ASSET', TargetPath)

Collection = unreal.load_asset(SkyFolder + 'MPC_TrainsSunbreaks')
Defaults = {
    'LegacyWeight': 0.12,
    'SunbreakWorldSize': 11000.0,
    'SunbreakContrast': 1.35,
    'CloudCoverPower': 0.62,
    'CloudDriftScale': 0.32,
    'ShadeTransmission': 0.46,
    'SunTransmission': 1.0,
    'RayDetailWorldSize': 4800.0,
    'RayDetailStrength': 0.40,
    'RayDetailDrift': 4.0,
    'RayDetailFloor': 0.78,
}
Parameters = Collection.get_editor_property('scalar_parameters')
for ParameterIndex, Parameter in enumerate(Parameters):
    Parameter.set_editor_property('default_value', Defaults[str(Parameter.get_editor_property('parameter_name'))])
    Parameters[ParameterIndex] = Parameter
Collection.set_editor_property('scalar_parameters', Parameters)
assert unreal.EditorAssetLibrary.save_loaded_asset(Collection)

Field = unreal.load_asset(SkyFolder + 'MF_TrainsSunbreakField')
for ParameterIndex in range(len(Parameters)):
    Expression = unreal.load_object(None, Field.get_path_name() + ':MaterialExpressionCollectionParameter_' + str(ParameterIndex))
    assert Expression, ParameterIndex
    Expression.set_editor_property('collection', Collection)

Custom = unreal.load_object(None, Field.get_path_name() + ':MaterialExpressionCustom_0')
Custom.set_editor_property('code', '''
// World XY keeps the moving sun pools and each ray's ground anchor synchronized.
float2 Wind = float2(1.0, 0.28) * RayDetailDrift * CloudDriftScale;
float2 Ground = Position.xy - Time * Wind;
float WorldSize = max(SunbreakWorldSize, 500.0);
float2 BroadUV = Ground / WorldSize;
float BroadA = Texture2DSampleLevel(NoiseTexture, NoiseTextureSampler, BroadUV, 0).r;
float BroadB = Texture2DSampleLevel(NoiseTexture, NoiseTextureSampler, BroadUV * 0.43 + float2(0.31, 0.67), 0).r;
float2 Warp = float2(BroadB - 0.5, BroadA - 0.5) * 0.28;
float Broken = Texture2DSampleLevel(NoiseTexture, NoiseTextureSampler, BroadUV * float2(1.0, 1.36) + Warp + float2(0.14, 0.38), 0).r;
float Cloud = pow(saturate(Broken * 0.70 + BroadB * 0.30), max(CloudCoverPower, 0.01));
float Contrast = max(SunbreakContrast, 0.1);
float Opening = smoothstep(0.5 - 0.19 / Contrast, 0.5 + 0.19 / Contrast, Cloud);
float Legacy = lerp(0.65, 1.0, BroadA);
float2 DetailUV = (Position.xy - Time * float2(RayDetailDrift, RayDetailDrift * 0.28)) / max(RayDetailWorldSize, 500.0);
float FineA = Texture2DSampleLevel(NoiseTexture, NoiseTextureSampler, DetailUV + Warp * 0.35, 0).r;
float FineB = Texture2DSampleLevel(NoiseTexture, NoiseTextureSampler, DetailUV * 2.17 + float2(0.43, 0.19), 0).r;
float Fine = lerp(1.0, smoothstep(0.27, 0.66, FineA * 0.76 + FineB * 0.24), saturate(RayDetailStrength));
float Pools = Opening * lerp(RayDetailFloor, 1.0, Fine);
float Transmission = lerp(ShadeTransmission, SunTransmission, Pools) * lerp(1.0, Legacy, saturate(LegacyWeight));
return float3(saturate(Transmission), Opening, Fine);
''')
Custom.set_editor_property('desc', 'Soft irregular pools of sunlight separated by deep cloud shade; shared by Trains rays')
unreal.MaterialEditingLibrary.update_material_function(Field)
assert unreal.EditorAssetLibrary.save_loaded_asset(Field)
print('FIELD_AND_COLLECTION_READY')
