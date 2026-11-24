# Unreal (editor python): builds MF_NomadicDeploy_VAT, the shared VAT deploy offset every nomadic building uses.
# Inputs come from texture / scalar PARAMETERS (NomadicDeployVatOffset, NomadicDeployVatRot, NomadicDeployVatFrames)
# so each building's deploy material instances point at their own bake. Progress = custom primitive data 3.
# Only call on a fresh function; never clear it once masters use it (see ue-python-material-gotchas memory).
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ROOT = 'D:/UE5Projects/RTS_Survival/Source/Tools/NomadicDeploy/'
FOLDER = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/Materials'
NAME = 'MF_NomadicDeploy_VAT'
DEFAULT_TEX = FOLDER + '/T_NomadicDeploy_VatDefault'
PROGRESS_INDEX = 3

EAL.make_directory(FOLDER)
path = FOLDER + '/' + NAME
if EAL.does_asset_exist(path):
    raise RuntimeError('MF already exists; edit it incrementally instead of rebuilding: ' + path)
if not EAL.does_asset_exist(DEFAULT_TEX):
    EAL.duplicate_asset('/Game/RTS_Survival/_Prototypes/DeployAnim/VAT/Textures/T_HQDeploy_VAT_Rot', DEFAULT_TEX)
default_tex = unreal.load_asset(DEFAULT_TEX)
mf = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.MaterialFunction,
                                                               unreal.MaterialFunctionFactoryNew())
mf.set_editor_property('description', 'Nomadic building VAT deploy offset (UV4-5, CPD 3, NomadicDeployVat* params)')


def node(cls, x, y):
    return MEL.create_material_expression_in_function(mf, cls, x, y)


custom = node(unreal.MaterialExpressionCustom, 0, 0)
custom.set_editor_property('code', open(ROOT + 'nomadic_deploy_vat.hlsl').read())
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('description', 'NomadicDeployVatOffsetLocal')
inputs = []
for n in ('LP', 'U4', 'U5', 'P', 'Frames', 'OffsetTex', 'RotTex'):
    ci = unreal.CustomInput()
    ci.set_editor_property('input_name', n)
    inputs.append(ci)
custom.set_editor_property('inputs', inputs)
MEL.connect_material_expressions(node(unreal.MaterialExpressionPreSkinnedPosition, -700, -300), '', custom, 'LP')
for k, uv in enumerate((4, 5)):
    tc = node(unreal.MaterialExpressionTextureCoordinate, -700, -200 + 80 * k)
    tc.set_editor_property('coordinate_index', uv)
    MEL.connect_material_expressions(tc, '', custom, 'U%d' % uv)
progress = node(unreal.MaterialExpressionScalarParameter, -700, 0)
progress.set_editor_property('parameter_name', 'NomadicDeployProgress')
progress.set_editor_property('use_custom_primitive_data', True)
progress.set_editor_property('primitive_data_index', PROGRESS_INDEX)
MEL.connect_material_expressions(progress, '', custom, 'P')
frames = node(unreal.MaterialExpressionScalarParameter, -700, 100)
frames.set_editor_property('parameter_name', 'NomadicDeployVatFrames')
frames.set_editor_property('default_value', 181.0)
MEL.connect_material_expressions(frames, '', custom, 'Frames')
for k, (pin, pname) in enumerate((('OffsetTex', 'NomadicDeployVatOffset'), ('RotTex', 'NomadicDeployVatRot'))):
    t = node(unreal.MaterialExpressionTextureObjectParameter, -700, 220 + 160 * k)
    t.set_editor_property('parameter_name', pname)
    t.set_editor_property('texture', default_tex)
    t.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.connect_material_expressions(t, '', custom, pin)
to_world = node(unreal.MaterialExpressionTransform, 300, 0)
to_world.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
to_world.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
MEL.connect_material_expressions(custom, '', to_world, '')
out = node(unreal.MaterialExpressionFunctionOutput, 600, 0)
out.set_editor_property('output_name', 'WorldPositionOffset')
MEL.connect_material_expressions(to_world, '', out, '')
MEL.update_material_function(mf)
print('built', path, 'saved', EAL.save_loaded_asset(mf, False), EAL.save_loaded_asset(default_tex, False))
