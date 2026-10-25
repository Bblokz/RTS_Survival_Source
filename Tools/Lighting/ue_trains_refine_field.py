import unreal

Field = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/MF_TrainsSunbreakField')
Custom = unreal.load_object(None, Field.get_path_name() + ':MaterialExpressionCustom_0')
Custom.set_editor_property('code', '''
struct FTrainsCloudField
{
 float Hash(float2 Position)
 {
  float3 Value = frac(float3(Position.xyx) * 0.1031);
  Value += dot(Value, Value.yzx + 33.33);
  return frac((Value.x + Value.y) * Value.z);
 }
 float Sample(float2 Position)
 {
  float2 Cell = floor(Position);
  float2 Blend = frac(Position);
  Blend = Blend * Blend * (3.0 - 2.0 * Blend);
  return lerp(lerp(Hash(Cell), Hash(Cell + float2(1.0, 0.0)), Blend.x),
              lerp(Hash(Cell + float2(0.0, 1.0)), Hash(Cell + 1.0), Blend.x), Blend.y);
 }
};
FTrainsCloudField Field;
// Large smooth openings retain readable light/shade separation at RTS camera height.
float2 Ground = Position.xy - CloudOffset.xy;
float WorldSize = max(SunbreakWorldSize, 500.0);
float2 BroadUV = Ground / WorldSize;
float BroadA = Field.Sample(BroadUV * 1.13 + float2(7.3, 12.7));
float BroadB = Field.Sample(BroadUV * 0.67 + float2(19.1, 3.9));
float2 Warp = float2(BroadB - 0.5, BroadA - 0.5) * 0.6;
float Broken = Field.Sample(BroadUV * float2(2.1, 2.55) + Warp + float2(3.4, 8.8));
float Cloud = pow(saturate(Broken * 0.80 + BroadB * 0.20), max(CloudCoverPower, 0.01));
float Contrast = max(SunbreakContrast, 0.1);
float Opening = smoothstep(0.52 - 0.20 / Contrast, 0.52 + 0.20 / Contrast, Cloud);
// A heavily filtered legacy layer only modulates the open areas gently.
float LegacyNoise = Texture2DSampleLevel(NoiseTexture, NoiseTextureSampler, BroadUV * 0.43, 5).r;
float Legacy = lerp(0.75, 1.0, LegacyNoise);
float2 DetailUV = Ground / max(RayDetailWorldSize, 500.0);
float FineA = Field.Sample(DetailUV + Warp * 0.35 + float2(41.7, 29.4));
float Fine = lerp(1.0, smoothstep(0.24, 0.72, FineA), saturate(RayDetailStrength));
float Pools = Opening * lerp(RayDetailFloor, 1.0, Fine);
float Transmission = lerp(ShadeTransmission, SunTransmission, Pools) * lerp(1.0, Legacy, saturate(LegacyWeight));
return float3(saturate(Transmission), Opening, Fine);
''')
unreal.MaterialEditingLibrary.update_material_function(Field)
assert unreal.EditorAssetLibrary.save_loaded_asset(Field)
for MaterialPath in [
    '/Game/RTS_Survival/Materials/Sky/M_LF_TrainsSunbreaks',
    '/Game/RTS_Survival/Materials/Sky/M_TrainsSunbreakProbe',
    '/Game/RTS_Survival/SpecialEffects/Atmosphere/M_FakeGodRay_Trains',
]:
    Material = unreal.load_asset(MaterialPath)
    unreal.MaterialEditingLibrary.recompile_material(Material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(Material)
print('SMOOTH_FIELD_COMPILED')
