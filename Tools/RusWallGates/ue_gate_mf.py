"""Unreal editor python: builds MF_WallGate_VAT (once). Same rigid-piece VAT as MF_NomadicDeploy_VAT
(Source/Tools/NomadicDeploy/nomadic_deploy_vat.hlsl), but the piece/pivot data is read from TexCoord 2 / 3 instead of
4 / 5: the wall masters are also compiled for skeletal meshes, whose vertex factory has at most 4 UV sets.
Parameters: WallGateVatOffset, WallGateVatRot, WallGateVatFrames; progress = WallGateProgress = custom primitive data 3
(AWallGate: 1 closed / rest, 0 open). Never rebuild it in place once masters use it.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
HLSL = 'D:/UE5Projects/RTS_Survival/Source/Tools/NomadicDeploy/nomadic_deploy_vat.hlsl'
FOLDER = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/Materials'
NAME = 'MF_WallGate_VAT'
DEFAULT_TEX = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/Materials/T_NomadicDeploy_VatDefault'

path = FOLDER + '/' + NAME
if EAL.does_asset_exist(path):
    print('exists', path)
else:
    mf = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.MaterialFunction,
                                                                   unreal.MaterialFunctionFactoryNew())
    mf.set_editor_property('description', 'Wall gate VAT leaf offset (UV2-3 piece/pivot, CPD 3 progress, WallGateVat* params)')

    def node(cls, x, y):
        return MEL.create_material_expression_in_function(mf, cls, x, y)

    custom = node(unreal.MaterialExpressionCustom, 0, 0)
    # the shader body names its inputs U4 / U5; they are fed from TexCoord 2 / 3 here
    custom.set_editor_property('code', open(HLSL).read())
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description', 'WallGateVatOffsetLocal')
    inputs = []
    for n in ('LP', 'U4', 'U5', 'P', 'Frames', 'OffsetTex', 'RotTex'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)
    MEL.connect_material_expressions(node(unreal.MaterialExpressionPreSkinnedPosition, -700, -300), '', custom, 'LP')
    for k, (uv, pin) in enumerate(((2, 'U4'), (3, 'U5'))):
        tc = node(unreal.MaterialExpressionTextureCoordinate, -700, -200 + 80 * k)
        tc.set_editor_property('coordinate_index', uv)
        MEL.connect_material_expressions(tc, '', custom, pin)
    progress = node(unreal.MaterialExpressionScalarParameter, -700, 0)
    progress.set_editor_property('parameter_name', 'WallGateProgress')
    progress.set_editor_property('default_value', 1.0)
    progress.set_editor_property('use_custom_primitive_data', True)
    progress.set_editor_property('primitive_data_index', 3)
    MEL.connect_material_expressions(progress, '', custom, 'P')
    frames = node(unreal.MaterialExpressionScalarParameter, -700, 100)
    frames.set_editor_property('parameter_name', 'WallGateVatFrames')
    frames.set_editor_property('default_value', 61.0)
    MEL.connect_material_expressions(frames, '', custom, 'Frames')
    for k, (pin, pname) in enumerate((('OffsetTex', 'WallGateVatOffset'), ('RotTex', 'WallGateVatRot'))):
        t = node(unreal.MaterialExpressionTextureObjectParameter, -700, 220 + 160 * k)
        t.set_editor_property('parameter_name', pname)
        t.set_editor_property('texture', unreal.load_asset(DEFAULT_TEX))
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
    print('built', path, 'saved', EAL.save_loaded_asset(mf, False))
