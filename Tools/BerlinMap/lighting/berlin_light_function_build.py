"""Ger_Berlin sun light function: a drifting smoke pall over the city plus a fixed term per district (2026-11-13).

Headless (new assets only, nothing shared is changed):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
      -DisablePlugins=Fab,BpGeneratorUltimate
Idempotent: the assets are duplicated once, the values and the field code are re-applied every run.
Doc: Source/Docs/Lighting/BerlinLighting.md.

Same construction as the First Mission and Trains sunbreaks (Source/Docs/Lighting/*.md): the sun's light function
M_LF_BerlinSmokePall calls the material function MF_BerlinSmokePallField, whose one Custom node computes the direct
sunlight multiplier at a world position; MPC_BerlinSmokePall holds the controls. The three assets are copies of the
First Mission ones, so the graph (world position, time, noise texture, collection parameters) is the proven one; only
the Custom code and the defaults differ. Berlin meaning of the eleven collection parameters:

  SunbreakWorldSize   size of the largest cloud shapes (cm); two finer octaves at about 1/2 and 1/4
  CloudCoverPower     cover threshold on the 0..1 noise (lower = more of the city in shade)
  SunbreakContrast    edge hardness of the shadows (1 = soft clouds)
  ShadeTransmission   sunlight under the thickest smoke (floor: units must stay readable)
  SunTransmission     sunlight in the open
  CloudDriftScale     wind speed multiplier
  RayDetailDrift      wind speed base (cm/s); speed = RayDetailDrift * CloudDriftScale
  RayDetailWorldSize  size of the finer wisps inside the banks
  RayDetailStrength   strength of the wisps
  RayDetailFloor      darkest a wisp makes an open stretch
  LegacyWeight        strength of the fixed district term (0 = the whole city lit alike)
"""
import unreal

SKY = '/Game/RTS_Survival/Materials/Sky/'
COPIES = [(SKY + 'MPC_FirstMissionSunbreaks', SKY + 'MPC_BerlinSmokePall'),
          (SKY + 'MF_FirstMissionSunbreakField', SKY + 'MF_BerlinSmokePallField'),
          (SKY + 'M_LF_FirstMissionSunbreaks', SKY + 'M_LF_BerlinSmokePall')]
DEFAULTS = {
    'SunbreakWorldSize': 9000.0, 'CloudCoverPower': 0.54, 'SunbreakContrast': 1.0,
    'ShadeTransmission': 0.5, 'SunTransmission': 1.0,
    'CloudDriftScale': 1.0, 'RayDetailDrift': 60.0,
    'RayDetailWorldSize': 3500.0, 'RayDetailStrength': 0.3, 'RayDetailFloor': 0.85,
    'LegacyWeight': 1.0,
}
import os
if os.environ.get('BERLIN_LF_DEBUG'):
    # exaggerated, for looking at the pattern only: black smoke shadows, a gale
    DEFAULTS.update({'ShadeTransmission': 0.0, 'CloudDriftScale': 25.0})
# World axes of Ger_Berlin: +X north, +Y east. The wind blows from the west along the lanes, a little to the north.
# District term (fixed in the world): the crater lies under its own pall, the burning east under heavy smoke, the dead
# park in the west and the north along the Spree are the open, brighter parts.
FIELD_CODE = '''
float2 WindDir = normalize(float2(0.22, 1.0));
float2 Across = float2(-WindDir.y, WindDir.x);
float Speed = RayDetailDrift * CloudDriftScale;
float2 P = Position.xy;

// fixed district term: 0 (open sky) .. 1 (under a standing pall)
float CraterDist = length(P - float2(-2167.0, -3000.0));
float Crater = 1.0 - smoothstep(4500.0, 15000.0, CraterDist);
float East = smoothstep(14000.0, 34000.0, P.y) * (1.0 - smoothstep(4000.0, 20000.0, P.x) * 0.6);
float Park = (1.0 - smoothstep(-34000.0, -14000.0, P.y)) * (1.0 - smoothstep(6000.0, 14000.0, abs(P.x + 2500.0)));
float North = smoothstep(12000.0, 20000.0, P.x) * (1.0 - smoothstep(8000.0, 30000.0, P.y));
float Pall = saturate(LegacyWeight) * saturate(max(0.9 * Crater, 0.75 * East));
float Clear = saturate(LegacyWeight) * saturate(max(Park, 0.6 * North));

// Drifting cloud and smoke shadows: soft procedural value noise, three smooth octaves. (The project's packed noise
// texture is veined, not cloud-shaped: thresholded it gave marbling, read sharp it gave flecks.) A wide smoothstep
// keeps every edge gentle, and the shapes are only a little longer along the wind than across it.
struct FBerlinCloud
{
    float Hash(float2 Cell) { return frac(sin(dot(Cell, float2(127.1, 311.7))) * 43758.5453); }
    float Value(float2 Q)
    {
        float2 Cell = floor(Q);
        float2 F = frac(Q);
        F = F * F * (3.0 - 2.0 * F);
        return lerp(lerp(Hash(Cell), Hash(Cell + float2(1.0, 0.0)), F.x),
                    lerp(Hash(Cell + float2(0.0, 1.0)), Hash(Cell + float2(1.0, 1.0)), F.x), F.y);
    }
    float Soft3(float2 Q) { return Value(Q) * 0.56 + Value(Q * 2.03 + 17.3) * 0.30 + Value(Q * 4.11 + 41.7) * 0.14; }
};
FBerlinCloud CloudNoise;
float WorldSize = max(SunbreakWorldSize, 2000.0);
float2 Ground = P - Time * Speed * WindDir;
float2 BankUV = float2(dot(Ground, WindDir), dot(Ground, Across) * 1.3) / WorldSize;
float Cloud = CloudNoise.Soft3(BankUV);
// CloudCoverPower is the cover threshold here: lower = more of the city in shade. A standing pall lowers it locally.
float Threshold = CloudCoverPower - 0.10 * Pall + 0.08 * Clear;
float Soft = 0.17 / max(SunbreakContrast, 0.1);
float Opening = 1.0 - smoothstep(Threshold - Soft, Threshold + Soft, Cloud);

// faint slower variation inside the open stretches
float2 WispGround = P - Time * Speed * 1.25 * WindDir;
float2 WispUV = float2(dot(WispGround, WindDir), dot(WispGround, Across) * 1.4) / max(RayDetailWorldSize, 1000.0);
float Fine = lerp(1.0, smoothstep(0.3, 0.7, CloudNoise.Soft3(WispUV + 73.1)), saturate(RayDetailStrength));

float Pools = Opening * lerp(RayDetailFloor, 1.0, Fine);
float Transmission = lerp(ShadeTransmission, SunTransmission, Pools);
// under a standing pall even the openings are dimmer
Transmission *= 1.0 - 0.22 * Pall;
return float3(saturate(Transmission), Opening, Fine);
'''


def log(message):
    unreal.log_warning('BERLIN_LIGHT ' + message)


def run():
    eal = unreal.EditorAssetLibrary
    for source, target in COPIES:
        if not eal.does_asset_exist(target):
            if not eal.duplicate_asset(source, target):
                raise RuntimeError('duplicate failed: ' + target)
    collection = unreal.load_asset(SKY + 'MPC_BerlinSmokePall')
    params = collection.get_editor_property('scalar_parameters')
    names = []
    for index, parameter in enumerate(params):
        name = str(parameter.get_editor_property('parameter_name'))
        names.append(name)
        parameter.set_editor_property('default_value', DEFAULTS[name])
        params[index] = parameter
    collection.set_editor_property('scalar_parameters', params)
    eal.save_loaded_asset(collection, False)
    field = unreal.load_asset(SKY + 'MF_BerlinSmokePallField')
    bound = 0
    for index in range(len(params)):
        expression = unreal.load_object(None, field.get_path_name() + ':MaterialExpressionCollectionParameter_%d' % index)
        if expression is None:
            raise RuntimeError('collection parameter node %d missing in the field function' % index)
        expression.set_editor_property('collection', collection)
        bound += 1
    custom = unreal.load_object(None, field.get_path_name() + ':MaterialExpressionCustom_0')
    if custom is None:
        raise RuntimeError('Custom node missing in the field function')
    inputs = [str(i.get_editor_property('input_name')) for i in custom.get_editor_property('inputs')]
    custom.set_editor_property('code', FIELD_CODE)
    custom.set_editor_property('desc', 'Berlin: drifting smoke banks along the wind plus a fixed district term')
    unreal.MaterialEditingLibrary.update_material_function(field)
    eal.save_loaded_asset(field, False)
    # The light function itself is rebuilt from scratch every run: absolute world position -> field -> transmission.
    # (Repointing the copied First Mission material at the Berlin field dropped its pin links and left the sun black.)
    mel = unreal.MaterialEditingLibrary
    light_function = unreal.load_asset(SKY + 'M_LF_BerlinSmokePall')
    mel.delete_all_material_expressions(light_function)
    light_function.set_editor_property('material_domain', unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    function_inputs = [str(e.get_editor_property('input_name')) for e in unreal.ObjectIterator(unreal.MaterialExpressionFunctionInput)
                       if e.get_path_name().startswith(field.get_path_name() + ':')]
    function_outputs = [str(e.get_editor_property('output_name')) for e in unreal.ObjectIterator(unreal.MaterialExpressionFunctionOutput)
                        if e.get_path_name().startswith(field.get_path_name() + ':')]
    call = mel.create_material_expression(light_function, unreal.MaterialExpressionMaterialFunctionCall, -500, 0)
    call.set_material_function(field)
    linked_inputs = []
    for name in function_inputs:
        if 'pos' in name.lower():
            position = mel.create_material_expression(light_function, unreal.MaterialExpressionWorldPosition, -900, 0)
            if not mel.connect_material_expressions(position, '', call, name):
                raise RuntimeError('could not wire world position into ' + name)
            linked_inputs.append(name)
    out_name = next((n for n in function_outputs if 'trans' in n.lower()), function_outputs[0] if function_outputs else '')
    mask = mel.create_material_expression(light_function, unreal.MaterialExpressionComponentMask, -200, 0)
    mask.set_editor_property('r', True)
    mask.set_editor_property('g', False)
    mask.set_editor_property('b', False)
    mask.set_editor_property('a', False)
    if not mel.connect_material_expressions(call, out_name, mask, ''):
        raise RuntimeError('could not wire the field output %r' % out_name)
    if not mel.connect_material_property(mask, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError('could not wire emissive')
    mel.recompile_material(light_function)
    eal.save_loaded_asset(light_function, False)
    log('parameters %s' % names)
    log('custom inputs %s; collection nodes bound %d' % (inputs, bound))
    log('field inputs %s outputs %s; wired position into %s, output %r' % (function_inputs, function_outputs, linked_inputs, out_name))


if __name__ == '__main__':
    run()
