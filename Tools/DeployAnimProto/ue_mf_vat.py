# Unreal (editor python). PROTOTYPE 2: builds MF_HQDeploy_VAT, the baked rigid-piece deploy offset.
# Per-piece data from UV4-5 (vat_bake.py), textures T_HQDeploy_VAT_Offset/_Rot, progress from Custom Primitive
# Data 0-3 (same meaning as prototype 1, see deploy_vat.hlsl). Rebuilt from scratch each run.
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ROOT = 'D:/UE5Projects/RTS_Survival/Source/Tools/DeployAnimProto/'
BASE = '/Game/RTS_Survival/_Prototypes/DeployAnim/VAT'
FOLDER = BASE + '/Materials'
NAME = 'MF_HQDeploy_VAT'

EAL.make_directory(FOLDER)
path = FOLDER + '/' + NAME
if EAL.does_asset_exist(path):
    mf = unreal.load_asset(path)
    MEL.delete_all_material_expressions_in_function(mf)
else:
    mf = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.MaterialFunction,
                                                                   unreal.MaterialFunctionFactoryNew())
mf.set_editor_property('description', 'PROTOTYPE deploy animation: baked rigid-piece VAT (UV4-5 + CPD 0-3)')
offset_tex = unreal.load_asset(BASE + '/Textures/T_HQDeploy_VAT_Offset')
rot_tex = unreal.load_asset(BASE + '/Textures/T_HQDeploy_VAT_Rot')


def node(cls, x, y):
    return MEL.create_material_expression_in_function(mf, cls, x, y)


custom = node(unreal.MaterialExpressionCustom, 0, 0)
custom.set_editor_property('code', open(ROOT + 'deploy_vat.hlsl').read())
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('description', 'DeployVatOffsetLocal')
names = ['LP', 'U4', 'U5', 'T', 'CpdMode', 'CpdValue', 'CpdDuration', 'CpdDirection', 'Frames', 'OffsetTex', 'RotTex']
inputs = []
for n in names:
    ci = unreal.CustomInput()
    ci.set_editor_property('input_name', n)
    inputs.append(ci)
custom.set_editor_property('inputs', inputs)

local = node(unreal.MaterialExpressionPreSkinnedPosition, -600, -300)
MEL.connect_material_expressions(local, '', custom, 'LP')
for k, uv in enumerate((4, 5)):
    tc = node(unreal.MaterialExpressionTextureCoordinate, -600, -200 + 80 * k)
    tc.set_editor_property('coordinate_index', uv)
    MEL.connect_material_expressions(tc, '', custom, 'U%d' % uv)
time = node(unreal.MaterialExpressionTime, -600, 0)
MEL.connect_material_expressions(time, '', custom, 'T')
for k, n in enumerate(('CpdMode', 'CpdValue', 'CpdDuration', 'CpdDirection')):
    p = node(unreal.MaterialExpressionScalarParameter, -600, 100 + 90 * k)
    p.set_editor_property('parameter_name', 'Deploy' + n[3:])
    p.set_editor_property('use_custom_primitive_data', True)
    p.set_editor_property('primitive_data_index', k)
    p.set_editor_property('default_value', 0.0)
    MEL.connect_material_expressions(p, '', custom, n)
frames = node(unreal.MaterialExpressionConstant, -600, 480)
frames.set_editor_property('r', float(offset_tex.blueprint_get_size_y()))
MEL.connect_material_expressions(frames, '', custom, 'Frames')
for k, (n, tex) in enumerate((('OffsetTex', offset_tex), ('RotTex', rot_tex))):
    obj = node(unreal.MaterialExpressionTextureObject, -600, 560 + 120 * k)
    obj.set_editor_property('texture', tex)
    obj.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.connect_material_expressions(obj, '', custom, n)
to_world = node(unreal.MaterialExpressionTransform, 300, 0)
to_world.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
to_world.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
MEL.connect_material_expressions(custom, '', to_world, '')
out = node(unreal.MaterialExpressionFunctionOutput, 600, 0)
out.set_editor_property('output_name', 'WorldPositionOffset')
MEL.connect_material_expressions(to_world, '', out, '')
MEL.update_material_function(mf)
print('built', path, 'frames', offset_tex.blueprint_get_size_y())
EAL.save_asset(path, only_if_is_dirty=False)
