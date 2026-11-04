"""Creates the skirt materials of the baked-skirt test technique in /Game/RTS_Survival/Tests/AddOnArmorBreak.

The skirts are baked into one mesh with the hull; every shard carries its id (side * 40 + panel * 8 + shard, rails
999) in UV channel 2. Which shards are broken is a bitmask in Custom Primitive Data 0..3, 20 shards per float.
  M_TestSkirtSteelBakedMask  masked; a broken shard is clipped away (opacity mask)
  M_TestSkirtSteelBakedWpo   opaque; a broken shard's vertices fold onto the object origin (world position offset).
                             Made by the editor command RTS.AddOnArmorTest.BuildBakedWpoMaterial from the mask one:
                             Python cannot connect the world position offset pin.
M_TestSkirtSteel (made by ue_import_skirt_pieces.py) is the opaque reference that cannot hide anything.

Run: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi
"""
import unreal

FOLDER = '/Game/RTS_Survival/Tests/AddOnArmorBreak'
STEEL_COLOUR = unreal.LinearColor(0.30, 0.235, 0.115, 1.0)
SHARD_ID_UV = 2
MASK_WORDS = 4
VISIBILITY_HLSL = '''float Id = floor(ShardId.x + 0.5);
float Word = floor(Id / 20.0);
float Bit = Id - Word * 20.0;
float Mask = Word < 0.5 ? Mask0 : (Word < 1.5 ? Mask1 : (Word < 2.5 ? Mask2 : (Word < 3.5 ? Mask3 : 0.0)));
float Broken = fmod(floor(Mask / exp2(Bit)), 2.0);
return 1.0 - Broken;'''
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def add_surface(material):
    colour = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, 0)
    colour.set_editor_property('constant', STEEL_COLOUR)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 150)
    roughness.set_editor_property('r', 0.62)
    metallic = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 250)
    metallic.set_editor_property('r', 0.25)
    mel.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metallic, '', unreal.MaterialProperty.MP_METALLIC)


def add_visibility(material):
    """@return The custom node that outputs 1 for an intact shard and 0 for a broken one."""
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 400)
    uv.set_editor_property('coordinate_index', SHARD_ID_UV)
    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -500, 450)
    custom.set_editor_property('code', VISIBILITY_HLSL)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom.set_editor_property('description', 'ShardVisible')
    inputs = []
    for name in ['ShardId'] + [f'Mask{index}' for index in range(MASK_WORDS)]:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', name)
        inputs.append(custom_input)
    custom.set_editor_property('inputs', inputs)
    mel.connect_material_expressions(uv, '', custom, 'ShardId')
    for index in range(MASK_WORDS):
        parameter = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 500 + index * 90)
        parameter.set_editor_property('parameter_name', f'BrokenMask{index}')
        parameter.set_editor_property('use_custom_primitive_data', True)
        parameter.set_editor_property('primitive_data_index', index)
        mel.connect_material_expressions(parameter, '', custom, f'Mask{index}')
    return custom


def new_material(name):
    path = f'{FOLDER}/{name}'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('two_sided', True)
    for usage in ('used_with_niagara_mesh_particles', 'used_with_static_lighting'):
        material.set_editor_property(usage, True)
    add_surface(material)
    return material, path


def make_mask_material():
    material, path = new_material('M_TestSkirtSteelBakedMask')
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('opacity_mask_clip_value', 0.5)
    visible = add_visibility(material)
    mel.connect_material_property(visible, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(material)
    eal.save_asset(path, only_if_is_dirty=False)


make_mask_material()
print('BAKED_MATERIALS done')
