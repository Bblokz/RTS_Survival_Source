"""Procedural wall-hole materials (Docs/Environment/WallChunkDamage.md, A + B) for any wall, gate or tower.

Use inside the editor (UECP execute_python or the Python console):
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_holes_material.py').read())
    mi = make_holes_instance('/Game/.../M_Nuclear_Tower', '/Game/.../Holes', mesh=unreal.load_asset(...),
                             grid_axis='Y', cell_size=150.0, two_sided=True)

What it builds:
- `<Master>_WallHoles` next to the source master: a Masked copy of an opaque master with one Custom node added.
  Built once and reused; never rebuilt in place while instances use it (delete its instances first). After the copy
  the script runs RTS.WallChunks.FinishMaskedMaster: a copied opaque master keeps a constant on its opacity-mask pin
  that wins over the connected node, and Python cannot clear it. Check the result with check_mask_live().
- An instance per wall slot: for a source Material, `MI_<Material>_WallHoles` parented to the masked copy; for a
  source MaterialInstance, a copy of it (`<MI>_WallHoles`, same parameter overrides) reparented onto the masked copy
  of its base material. Grid parameters are set from the mesh, using the same rule as UWallHoleDamageComponent.

The Custom node reads custom primitive data through scalar parameters (UWallHoleDamageComponent, WallHoleMask):
    8..10   grid cells, one bit per cell, 20 per float
    11..34  6 holes as (mesh X, mesh Y, mesh Z, radius); radius 0 = unused
Holes are spheres in the mesh's local space with noise-warped edges; a scorched, rougher rim surrounds them. Grid
cells lie on (along, Z), along = mesh X or Y (WallGridAlongY), and keep a ragged 30 cm strip at their border.
WallGridColumns 0 switches the grid off (towers).

Thick, hollow meshes (walkable walls, towers): pass two_sided=True so the inside shows through a hole instead of
the far side vanishing; the instance overrides Two Sided.
Hollow shells (tree trunks): also pass inner_tint=True. The master then darkens back faces (TwoSidedSign -1) toward
WallHoleInnerColor by WallHoleInnerDarken (0 in the master, inner_darken on the instance), so the inside of a hole
reads as charred wood instead of bark; back faces also lose their emissive. Only applies when the `_WallHoles` master is first built.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROP = unreal.MaterialProperty
NOISE = '/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/LowResBlurredNoise'
FIRST_GRID = 8
GRID_FLOATS = 3
FIRST_HOLE = 11
HOLES = 6
FLOATS_PER_HOLE = 4
SUFFIX = '_WallHoles'
DEBRIS_SOURCE = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Chunks/SM_Wall_14_Chunks_Debris_%d'
DEBRIS_PIECES = 8


def _hole_code():
    lines = []
    for index in range(HOLES):
        lines.append(f'{{ float R = max(H{index}R, 0.01); '
                     f'float Ratio = (length(P - float3(H{index}X, H{index}Y, H{index}Z)) + (Noise - 0.5) * 0.55 * R) / R; '
                     f'MinRatio = min(MinRatio, H{index}R > 0.5 ? Ratio : 1.0e4); }}')
    return '\n'.join(lines)


CODE = '''// Mesh-space position; the grid plane is (along, Z) with along = mesh X or Y.
float3 P = LocalPos;
float2 G = float2(lerp(P.x, P.y, AlongY), P.z);
// A: grid cell of this pixel; its bit in one of three 20-bit words. Columns 0 = no grid.
float HasGrid = step(0.5, Columns);
float Cols = max(Columns, 1.0);
float2 CellF = (G - float2(GridMinAlong, GridMinZ)) / CellSize;
float2 Cell = clamp(floor(CellF), float2(0.0, 0.0), float2(Cols - 1.0, max(Rows, 1.0) - 1.0));
float Index = Cell.x + Cell.y * Cols;
float Word = floor(Index / 20.0);
float Bit = Index - Word * 20.0;
float Mask = Word < 0.5 ? G0 : (Word < 1.5 ? G1 : G2);
float CellHidden = fmod(floor(Mask / exp2(Bit)), 2.0) * HasGrid;
// Ragged cell edge: a strip of material up to 30 cm wide stays along the cell border, cut by the noise.
float2 InCell = (CellF - Cell) * CellSize;
float2 CellExtent = float2(Cell.x > Cols - 1.5 ? (GridMaxAlong - GridMinAlong) - Cell.x * CellSize : CellSize,
                           Cell.y > Rows - 1.5 ? (GridMaxZ - GridMinZ) - Cell.y * CellSize : CellSize);
float EdgeDistance = min(min(InCell.x, CellExtent.x - InCell.x), min(InCell.y, CellExtent.y - InCell.y));
float CellCut = CellHidden * step(Noise * 30.0, EdgeDistance);
// B: six sphere holes, edge warped by the noise; MinRatio < 1 is inside a hole.
float MinRatio = 1.0e4;
''' + _hole_code() + '''
float HoleCut = step(MinRatio, 1.0);
float Visible = 1.0 - max(CellCut, HoleCut);
// Scorched, rougher surface just outside a hole.
float Rim = 1.0 - smoothstep(1.0, 1.45, MinRatio);
return float2(Visible, Rim);'''

GRID_PARAMETERS = [('AlongY', 'WallGridAlongY', 0.0), ('GridMinAlong', 'WallGridMinAlong', 0.0),
                   ('GridMaxAlong', 'WallGridMaxAlong', 100.0), ('GridMinZ', 'WallGridMinZ', 0.0),
                   ('GridMaxZ', 'WallGridMaxZ', 100.0), ('CellSize', 'WallGridCellSize', 150.0),
                   ('Columns', 'WallGridColumns', 0.0), ('Rows', 'WallGridRows', 1.0)]


def _scalar(material, name, default, x, y, primitive_index=None):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', default)
    if primitive_index is not None:
        node.set_editor_property('use_custom_primitive_data', True)
        node.set_editor_property('primitive_data_index', primitive_index)
    return node


def _mask(material, x, y, channels):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, x, y)
    for channel in 'rgba':
        node.set_editor_property(channel, channel in channels)
    return node


INNER_DARKEN = 'WallHoleInnerDarken'
INNER_COLOR = 'WallHoleInnerColor'
# Charred wood: a dark brown-black seen through a hole in a hollow trunk.
INNER_COLOR_DEFAULT = (0.14, 0.085, 0.05)
INNER_DARKEN_DEFAULT = 0.75


def _inject_inner_tint(material, tinted, emissive, links):
    """Back faces lerp toward WallHoleInnerColor by WallHoleInnerDarken and emit nothing (the Radixite glow would
    light the inside; even 25 % of it read as green). Returns the node for BaseColor and the one for Emissive (None if unset).
    TwoSidedSign is +1 on front faces and -1 on back faces (two-sided rendering only)."""
    sign = MEL.create_material_expression(material, unreal.MaterialExpressionTwoSidedSign, 400, -800)
    flipped = MEL.create_material_expression(material, unreal.MaterialExpressionOneMinus, 550, -800)
    back = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 700, -800)
    back.set_editor_property('const_b', 0.5)
    darken = _scalar(material, INNER_DARKEN, 0.0, 550, -700)
    amount = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 850, -750)
    color = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, 700, -650)
    color.set_editor_property('parameter_name', INNER_COLOR)
    color.set_editor_property('default_value', unreal.LinearColor(*INNER_COLOR_DEFAULT, 1.0))
    color_rgb = _mask(material, 850, -650, 'rgb')
    inner = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 1000, -500)
    links += [MEL.connect_material_expressions(sign, '', flipped, ''),
              MEL.connect_material_expressions(flipped, '', back, 'A'),
              MEL.connect_material_expressions(back, '', amount, 'A'),
              MEL.connect_material_expressions(darken, '', amount, 'B'),
              MEL.connect_material_expressions(color, '', color_rgb, ''),
              MEL.connect_material_expressions(tinted, '', inner, 'A'),
              MEL.connect_material_expressions(color_rgb, '', inner, 'B'),
              MEL.connect_material_expressions(amount, '', inner, 'Alpha')]
    if emissive is None:
        return inner, None
    keep = MEL.create_material_expression(material, unreal.MaterialExpressionOneMinus, 1000, -350)
    dimmed = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 1150, -300)
    links += [MEL.connect_material_expressions(back, '', keep, ''),
              MEL.connect_material_expressions(emissive, '', dimmed, 'A'),
              MEL.connect_material_expressions(keep, '', dimmed, 'B')]
    return inner, dimmed


def _inject(material, inner_tint=False, keep_source_opacity=False):
    """Adds the hole subgraph to a fresh masked copy. Returns the number of failed links.
    inner_tint: also darken back faces (the inside of a hollow mesh seen through a hole), see _inject_inner_tint.
    keep_source_opacity: the source was already masked; its opacity mask is multiplied with the holes."""
    base = MEL.get_material_property_input_node(material, PROP.MP_BASE_COLOR)
    rough = MEL.get_material_property_input_node(material, PROP.MP_ROUGHNESS)
    if base is None:
        raise RuntimeError(material.get_path_name() + ': BaseColor is not connected (material attributes?)')
    world = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1400, 900)
    local = MEL.create_material_expression(material, unreal.MaterialExpressionTransformPosition, -1150, 900)
    local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    links = [MEL.connect_material_expressions(world, '', local, '')]
    # Noise on (x + y, z): one sample that roughens edges on every face of a box.
    xy = _mask(material, -900, 1000, 'rg')
    z = _mask(material, -900, 1080, 'b')
    links += [MEL.connect_material_expressions(local, '', xy, ''), MEL.connect_material_expressions(local, '', z, '')]
    along = MEL.create_material_expression(material, unreal.MaterialExpressionDotProduct, -750, 1000)
    one_one = MEL.create_material_expression(material, unreal.MaterialExpressionConstant2Vector, -900, 1160)
    one_one.set_editor_property('r', 1.0)
    one_one.set_editor_property('g', 1.0)
    links += [MEL.connect_material_expressions(xy, '', along, 'A'), MEL.connect_material_expressions(one_one, '', along, 'B')]
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionAppendVector, -600, 1000)
    links += [MEL.connect_material_expressions(along, '', uv, 'A'), MEL.connect_material_expressions(z, '', uv, 'B')]
    tiling = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, 1000)
    tiling.set_editor_property('const_b', 1.0 / 120.0)
    links.append(MEL.connect_material_expressions(uv, '', tiling, 'A'))
    noise = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, -300, 1000)
    noise.set_editor_property('texture', unreal.load_asset(NOISE))
    noise.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    links.append(MEL.connect_material_expressions(tiling, '', noise, 'UVs'))

    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, 100, 900)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    custom.set_editor_property('description', 'WallHoles')
    custom.set_editor_property('code', CODE)
    names = ['LocalPos', 'Noise'] + [pin for pin, _, _ in GRID_PARAMETERS] + [f'G{i}' for i in range(GRID_FLOATS)]
    for index in range(HOLES):
        names += [f'H{index}X', f'H{index}Y', f'H{index}Z', f'H{index}R']
    inputs = []
    for name in names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', name)
        inputs.append(custom_input)
    custom.set_editor_property('inputs', inputs)
    links += [MEL.connect_material_expressions(local, '', custom, 'LocalPos'),
              MEL.connect_material_expressions(noise, 'R', custom, 'Noise')]
    for row, (pin, parameter, default) in enumerate(GRID_PARAMETERS):
        node = _scalar(material, parameter, default, -300, 1150 + row * 70)
        links.append(MEL.connect_material_expressions(node, '', custom, pin))
    for index in range(GRID_FLOATS):
        node = _scalar(material, f'WallHoleGrid{index}', 0.0, -300, 1750 + index * 70, FIRST_GRID + index)
        links.append(MEL.connect_material_expressions(node, '', custom, f'G{index}'))
    for index in range(HOLES):
        for offset, axis in enumerate('XYZR'):
            data_index = FIRST_HOLE + index * FLOATS_PER_HOLE + offset
            node = _scalar(material, f'WallHole{index}{axis}', 0.0, -300,
                           2000 + (index * FLOATS_PER_HOLE + offset) * 70, data_index)
            links.append(MEL.connect_material_expressions(node, '', custom, f'H{index}{axis}'))

    visible = _mask(material, 400, 900, 'r')
    rim = _mask(material, 400, 1000, 'g')
    links += [MEL.connect_material_expressions(custom, '', visible, ''),
              MEL.connect_material_expressions(custom, '', rim, '')]
    # A source that is already masked (camo nets, twig cards) keeps its own cut-out: holes multiply into it.
    source_opacity = MEL.get_material_property_input_node(material, PROP.MP_OPACITY_MASK) if keep_source_opacity else None
    if source_opacity is not None:
        source_output = MEL.get_material_property_input_node_output_name(material, PROP.MP_OPACITY_MASK)
        combined = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 650, 900)
        links += [MEL.connect_material_expressions(visible, '', combined, 'A'),
                  MEL.connect_material_expressions(source_opacity, source_output, combined, 'B'),
                  MEL.connect_material_property(combined, '', PROP.MP_OPACITY_MASK)]
    else:
        links.append(MEL.connect_material_property(visible, '', PROP.MP_OPACITY_MASK))
    scorch = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 400, -500)
    scorch.set_editor_property('const_b', 0.3)
    tinted = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 650, -450)
    links += [MEL.connect_material_expressions(base, '', scorch, 'A'),
              MEL.connect_material_expressions(base, '', tinted, 'A'),
              MEL.connect_material_expressions(scorch, '', tinted, 'B'),
              MEL.connect_material_expressions(rim, '', tinted, 'Alpha')]
    base_out = tinted
    if inner_tint:
        emissive = MEL.get_material_property_input_node(material, PROP.MP_EMISSIVE_COLOR)
        base_out, emissive_out = _inject_inner_tint(material, tinted, emissive, links)
        if emissive_out is not None:
            links.append(MEL.connect_material_property(emissive_out, '', PROP.MP_EMISSIVE_COLOR))
    links.append(MEL.connect_material_property(base_out, '', PROP.MP_BASE_COLOR))
    if rough is not None:
        rough_rim = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 650, -150)
        rough_rim.set_editor_property('const_b', 1.0)
        links += [MEL.connect_material_expressions(rough, '', rough_rim, 'A'),
                  MEL.connect_material_expressions(rim, '', rough_rim, 'Alpha'),
                  MEL.connect_material_property(rough_rim, '', PROP.MP_ROUGHNESS)]
    return links.count(False)


def masked_master_for(source_master_path, inner_tint=False):
    """The `<Master>_WallHoles` masked copy of an opaque master; built on first use.
    inner_tint: add the back-face darkening (WallHoleInnerDarken, default 0, and WallHoleInnerColor) for hollow
    meshes such as tree trunks. Only takes effect when the master is built; an existing master is reused as is."""
    source = unreal.load_asset(source_master_path)
    if not isinstance(source, unreal.Material):
        raise RuntimeError(source_master_path + ' is not a Material')
    target = source.get_path_name().split('.')[0] + SUFFIX
    if EAL.does_asset_exist(target):
        existing = unreal.load_asset(target)
        if inner_tint and INNER_DARKEN not in [str(n) for n in MEL.get_scalar_parameter_names(existing)]:
            unreal.log_warning(f'WALL_HOLES {target} exists without the inner tint; inner_tint ignored')
        return existing
    was_masked = source.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_MASKED
    material = EAL.duplicate_asset(source.get_path_name().split('.')[0], target)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    if not was_masked:
        material.set_editor_property('opacity_mask_clip_value', 0.5)
    material.set_editor_property('used_with_niagara_mesh_particles', True)
    failed = _inject(material, inner_tint, keep_source_opacity=was_masked)
    unreal.SystemLibrary.execute_console_command(None, 'RTS.WallChunks.FinishMaskedMaster Material=' + target)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material, False)
    if failed:
        unreal.log_warning(f'WALL_HOLES {target}: {failed} links failed')
    return material


def check_mask_live(material):
    """Pixel-shader count of the masked copy against its source: the mask adds 60+; under 10 means it compiled away."""
    source = unreal.load_asset(material.get_path_name().split('.')[0][:-len(SUFFIX)])
    masked = MEL.get_statistics(material).num_pixel_shader_instructions
    opaque = MEL.get_statistics(source).num_pixel_shader_instructions
    return masked, opaque


def grid_parameters(mesh, grid_axis, cell_size):
    """WallGrid* values for a mesh, by UWallHoleDamageComponent's rule. grid_axis: 'X', 'Y' or None (no grid)."""
    box = mesh.get_bounding_box()
    along_y = grid_axis == 'Y'
    lo = box.min.y if along_y else box.min.x
    hi = box.max.y if along_y else box.max.x
    columns = 0 if grid_axis is None else max(1, round((hi - lo) / cell_size))
    rows = max(1, round((box.max.z - box.min.z) / cell_size))
    if columns * rows > 60:
        rows = max(1, 60 // columns)
    return {'WallGridAlongY': 1.0 if along_y else 0.0, 'WallGridMinAlong': lo, 'WallGridMaxAlong': hi,
            'WallGridMinZ': box.min.z, 'WallGridMaxZ': box.max.z, 'WallGridCellSize': cell_size,
            'WallGridColumns': float(columns), 'WallGridRows': float(rows)}


def make_holes_instance(slot_material_path, folder, mesh, grid_axis='X', cell_size=150.0, two_sided=False, name=None,
                        inner_tint=False, inner_darken=INNER_DARKEN_DEFAULT, inner_color=INNER_COLOR_DEFAULT):
    """Masked instance for one wall slot. Returns the instance (saved).
    inner_tint: build the master with back-face darkening and set WallHoleInnerDarken / WallHoleInnerColor on the
    instance (needs two_sided=True to show anything). Off by default: existing callers are unchanged."""
    slot = unreal.load_asset(slot_material_path)
    if isinstance(slot, unreal.MaterialInstance):
        name = name or slot.get_name() + SUFFIX
        parent = slot.get_editor_property('parent')
        if isinstance(parent, unreal.MaterialInstance):
            # An instance of an instance: hang the copy under the parent's own holes instance, so the parent's
            # overrides (textures the child does not set) stay. Reparenting straight onto the master loses them.
            master = make_holes_instance(parent.get_path_name().split('.')[0], folder, mesh, grid_axis, cell_size,
                                         two_sided, name + '_Parent', inner_tint, inner_darken, inner_color)
        else:
            master = masked_master_for(slot.get_base_material().get_path_name(), inner_tint)
        target = folder + '/' + name
        instance = unreal.load_asset(target) if EAL.does_asset_exist(target) else \
            EAL.duplicate_asset(slot.get_path_name().split('.')[0], target)
    else:
        master = masked_master_for(slot.get_path_name(), inner_tint)
        name = name or 'MI_' + slot.get_name() + SUFFIX
        target = folder + '/' + name
        instance = unreal.load_asset(target) if EAL.does_asset_exist(target) else \
            unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(instance, master)
    for parameter, value in grid_parameters(mesh, grid_axis, cell_size).items():
        MEL.set_material_instance_scalar_parameter_value(instance, parameter, float(value))
    if inner_tint:
        MEL.set_material_instance_scalar_parameter_value(instance, INNER_DARKEN, float(inner_darken))
        MEL.set_material_instance_vector_parameter_value(instance, INNER_COLOR, unreal.LinearColor(*inner_color, 1.0))
    if two_sided:
        overrides = instance.get_editor_property('base_property_overrides')
        overrides.set_editor_property('override_two_sided', True)
        overrides.set_editor_property('two_sided', True)
        instance.set_editor_property('base_property_overrides', overrides)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    return instance


def make_debris_pieces(folder, prefix, material):
    """Copies of the 8 concrete chunk pieces with `material` on both slots, for this wall's own look."""
    pieces = []
    for index in range(DEBRIS_PIECES):
        target = f'{folder}/{prefix}_Debris_{index}'
        mesh = unreal.load_asset(target) if EAL.does_asset_exist(target) else \
            EAL.duplicate_asset(DEBRIS_SOURCE % index, target)
        mesh.set_material(0, material)
        mesh.set_material(1, material)
        EAL.save_loaded_asset(mesh, False)
        pieces.append(mesh)
    return pieces
