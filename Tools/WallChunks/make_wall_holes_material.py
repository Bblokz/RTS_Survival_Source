"""Builds M_RusBunker_Concrete_WallHoles: the procedural-hole wall master (WallHitMasking.md options A + B).

Run inside the editor (UECP execute_python or the Python console). Then run
  RTS.WallChunks.FinishMaskedMaster Material=<this material>
because a duplicated opaque master keeps a constant on its opacity mask pin that Python cannot clear.

Masked copy of M_RusBunker_Concrete, plus:
- local position (absolute world position -> local), its X/Z is the wall face;
- a noise sample on the face (LowResBlurredNoise, 1.2 m tiling) that roughens hole and cell edges;
- one Custom node reading custom primitive data through scalar parameters:
    8..10  grid cells, one bit per cell, 20 per float (UWallHoleDamageComponent, WallHoleMask)
    11..34 eight holes as (mesh X, mesh Z, radius); radius 0 = unused
  It returns (visible, rim): visible clips through the opacity mask, rim darkens and roughens the concrete
  around each hole;
- grid shape as scalar parameters (WallGridMinX/MinZ/CellSize/Columns/Rows), set per wall in the instance; they must
  follow the component's rule: round(size / cell), the last column and row take the remainder.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROP = unreal.MaterialProperty
SOURCE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/M_RusBunker_Concrete'
FOLDER = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Holes'
TARGET = FOLDER + '/M_RusBunker_Concrete_WallHoles'
NOISE = '/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/LowResBlurredNoise'
FIRST_GRID = 8
GRID_FLOATS = 3
FIRST_HOLE = 11
HOLES = 8


def hole_code():
    lines = []
    for index in range(HOLES):
        x, z, r = (f'H{index}X', f'H{index}Z', f'H{index}R')
        lines.append(f'{{ float R = max({r}, 0.01); '
                     f'float Ratio = (length(P - float2({x}, {z})) + (Noise - 0.5) * 0.55 * R) / R; '
                     f'MinRatio = min(MinRatio, {r} > 0.5 ? Ratio : 1.0e4); }}')
    return '\n'.join(lines)


CODE = '''// Wall face position (mesh X, mesh Z) and a 0..1 noise for ragged edges.
float2 P = float2(LocalPos.x, LocalPos.z);
// A: grid cell of this pixel; its bit in one of three 20-bit words.
float2 CellF = (P - float2(GridMinX, GridMinZ)) / CellSize;
float2 Cell = clamp(floor(CellF), float2(0.0, 0.0), float2(Columns - 1.0, Rows - 1.0));
float Index = Cell.x + Cell.y * Columns;
float Word = floor(Index / 20.0);
float Bit = Index - Word * 20.0;
float Mask = Word < 0.5 ? G0 : (Word < 1.5 ? G1 : G2);
float CellHidden = fmod(floor(Mask / exp2(Bit)), 2.0);
// Ragged cell edge: a strip of concrete up to 30 cm wide stays along the cell border, cut by the noise.
float2 InCell = (CellF - Cell) * CellSize;
float2 CellExtent = float2(Cell.x > Columns - 1.5 ? (GridMaxX - GridMinX) - Cell.x * CellSize : CellSize,
                           Cell.y > Rows - 1.5 ? (GridMaxZ - GridMinZ) - Cell.y * CellSize : CellSize);
float EdgeDistance = min(min(InCell.x, CellExtent.x - InCell.x), min(InCell.y, CellExtent.y - InCell.y));
float CellCut = CellHidden * step(Noise * 30.0, EdgeDistance);
// B: eight round holes, edge warped by the noise; MinRatio < 1 is inside a hole.
float MinRatio = 1.0e4;
''' + hole_code() + '''
float HoleCut = step(MinRatio, 1.0);
float Visible = 1.0 - max(CellCut, HoleCut);
// Scorched, rougher concrete just outside a hole.
float Rim = 1.0 - smoothstep(1.0, 1.45, MinRatio);
return float2(Visible, Rim);'''


def scalar(material, name, default, x, y, primitive_index=None):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', default)
    if primitive_index is not None:
        node.set_editor_property('use_custom_primitive_data', True)
        node.set_editor_property('primitive_data_index', primitive_index)
    return node


def main():
    if not EAL.does_directory_exist(FOLDER):
        EAL.make_directory(FOLDER)
    if EAL.does_asset_exist(TARGET):
        raise RuntimeError('exists: ' + TARGET + ' (delete it first; never clear a live master)')
    material = EAL.duplicate_asset(SOURCE, TARGET)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('opacity_mask_clip_value', 0.5)
    material.set_editor_property('used_with_niagara_mesh_particles', True)
    base = MEL.get_material_property_input_node(material, PROP.MP_BASE_COLOR)
    rough = MEL.get_material_property_input_node(material, PROP.MP_ROUGHNESS)

    world = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1400, 900)
    local = MEL.create_material_expression(material, unreal.MaterialExpressionTransformPosition, -1150, 900)
    local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    MEL.connect_material_expressions(world, '', local, '')
    face = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -900, 1000)
    for channel, enabled in (('r', True), ('g', False), ('b', True), ('a', False)):
        face.set_editor_property(channel, enabled)
    MEL.connect_material_expressions(local, '', face, '')
    tiling = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -700, 1000)
    tiling.set_editor_property('const_b', 1.0 / 120.0)
    MEL.connect_material_expressions(face, '', tiling, 'A')
    noise = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, -500, 1000)
    noise.set_editor_property('texture', unreal.load_asset(NOISE))
    noise.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    MEL.connect_material_expressions(tiling, '', noise, 'UVs')

    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, 0, 900)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    custom.set_editor_property('description', 'WallHoles')
    custom.set_editor_property('code', CODE)
    names = ['LocalPos', 'Noise', 'GridMinX', 'GridMinZ', 'GridMaxX', 'GridMaxZ', 'CellSize', 'Columns', 'Rows']
    names += [f'G{i}' for i in range(GRID_FLOATS)]
    for index in range(HOLES):
        names += [f'H{index}X', f'H{index}Z', f'H{index}R']
    inputs = []
    for name in names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', name)
        inputs.append(custom_input)
    custom.set_editor_property('inputs', inputs)
    links = [MEL.connect_material_expressions(local, '', custom, 'LocalPos'),
             MEL.connect_material_expressions(noise, 'R', custom, 'Noise')]
    grid_defaults = [('GridMinX', 'WallGridMinX', -2260.0), ('GridMinZ', 'WallGridMinZ', 0.0),
                     ('GridMaxX', 'WallGridMaxX', 0.0), ('GridMaxZ', 'WallGridMaxZ', 500.0),
                     ('CellSize', 'WallGridCellSize', 150.0), ('Columns', 'WallGridColumns', 15.0),
                     ('Rows', 'WallGridRows', 3.0)]
    for row, (pin, parameter, default) in enumerate(grid_defaults):
        node = scalar(material, parameter, default, -400, 1150 + row * 70)
        links.append(MEL.connect_material_expressions(node, '', custom, pin))
    for index in range(GRID_FLOATS):
        node = scalar(material, f'WallHoleGrid{index}', 0.0, -400, 1700 + index * 70, FIRST_GRID + index)
        links.append(MEL.connect_material_expressions(node, '', custom, f'G{index}'))
    for index in range(HOLES):
        for offset, axis in enumerate('XZR'):
            data_index = FIRST_HOLE + index * 3 + offset
            node = scalar(material, f'WallHole{index}{axis}', 0.0, -400, 1950 + (index * 3 + offset) * 70, data_index)
            links.append(MEL.connect_material_expressions(node, '', custom, f'H{index}{axis}'))

    visible = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, 300, 900)
    for channel, enabled in (('r', True), ('g', False), ('b', False), ('a', False)):
        visible.set_editor_property(channel, enabled)
    rim = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, 300, 1000)
    for channel, enabled in (('r', False), ('g', True), ('b', False), ('a', False)):
        rim.set_editor_property(channel, enabled)
    MEL.connect_material_expressions(custom, '', visible, '')
    MEL.connect_material_expressions(custom, '', rim, '')
    links.append(MEL.connect_material_property(visible, '', PROP.MP_OPACITY_MASK))

    # Rim: base colour towards a scorched dark, roughness up.
    scorch = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 300, -500)
    scorch.set_editor_property('const_b', 0.3)
    MEL.connect_material_expressions(base, '', scorch, 'A')
    tinted = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 550, -450)
    MEL.connect_material_expressions(base, '', tinted, 'A')
    MEL.connect_material_expressions(scorch, '', tinted, 'B')
    MEL.connect_material_expressions(rim, '', tinted, 'Alpha')
    links.append(MEL.connect_material_property(tinted, '', PROP.MP_BASE_COLOR))
    rough_rim = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 550, -150)
    rough_rim.set_editor_property('const_b', 1.0)
    MEL.connect_material_expressions(rough, '', rough_rim, 'A')
    MEL.connect_material_expressions(rim, '', rough_rim, 'Alpha')
    links.append(MEL.connect_material_property(rough_rim, '', PROP.MP_ROUGHNESS))

    MEL.recompile_material(material)
    EAL.save_loaded_asset(material, False)
    print('WALL_HOLES material links', links.count(True), '/', len(links))


main()
