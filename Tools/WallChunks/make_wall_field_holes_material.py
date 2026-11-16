"""Field-texture wall-hole materials (Docs/Environment/WallChunkDamage.md, "Field mode") for the Rus concrete walls.

Use inside the editor (UECP execute_python or the Python console):
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_field_holes_material.py').read())
    build_rus_wall_field_materials()

Why: the slot materials (make_wall_holes_material.py) read 6 holes from custom primitive data, which is full, so a
wall shot many times baked or merged holes into big shapes. Field mode keeps the damage in a small CPU-written
texture of the unrolled wall (UWallHoleDamageComponent + FWallHoleField, MaskStorage = FieldTexture):
    R  front face (left of the wall's centre line)   f = 1 - distance / radius, stored as f * 0.5 + 0.5
    G  back face
    B  tank breach, both faces (UWallTankBreachComponent)
f > 0 is removed; the noise warps the edge. The material unrolls the wall itself from WallFieldPathA/B (up to two
segments: straight walls and L corners) and WallFieldMap; the component sets these on its dynamic instances.

What it builds:
- `<Master>_WallHolesField` next to the opaque source master: a masked copy with the field subgraph, a scorched rim
  around shell holes, a lighter fractured rim around breaches, broken rebar in the breach edge (WallFieldRebar) and
  darkened back faces (two-sided instances show the wall's thickness through a hole).
- `<WallHolesInstance>Field` for each existing `_WallHoles` instance of the Rus walls, same overrides, reparented.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROP = unreal.MaterialProperty
NOISE = '/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/LowResBlurredNoise'
DEFAULT_FIELD = '/Engine/EngineResources/Black'
SUFFIX = '_WallHolesField'
HOLES_FOLDER = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Holes'

# Base colour kept in the ring around a shell hole: grey spall with some soot, not a black ring.
SCORCH = 0.55
INNER_DARKEN = 'WallHoleInnerDarken'
INNER_COLOR = 'WallHoleInnerColor'
REBAR = 'WallFieldRebar'

# Parents before children: a Soviet instance hangs under the field copy of its Rus parent.
WALL_INSTANCES = [
    # (existing _WallHoles instance, rebar 0..1, inner colour, inner darken)
    ('MI_Rus_BunkerConcrete_05_WallHoles', 1.0, (0.09, 0.085, 0.08), 0.65),
    ('MI_SM_Wall_13_Rus_BunkerConcrete_05_WallHoles', 1.0, (0.09, 0.085, 0.08), 0.65),
    ('MI_SM_Rus_ConcreteSandWall_Rus_BunkerConcrete_05_WallHoles', 1.0, (0.09, 0.085, 0.08), 0.65),
    ('MI_SM_Rus_ConcreteSandWall_Rus_SandBag_01_WallHoles', 0.0, (0.16, 0.13, 0.09), 0.55),
    ('MI_SM_Wall_13_SovietConcrete_WallHoles', 1.0, (0.09, 0.085, 0.08), 0.65),
    ('MI_SM_Wall_14_SovietConcrete_WallHoles', 1.0, (0.09, 0.085, 0.08), 0.65),
    ('MI_SM_Rus_ConcreteSandWall_SovietConcrete_WallHoles', 1.0, (0.09, 0.085, 0.08), 0.65),
]

CODE = '''// Unroll the wall: distance along its centre line (one or two segments) and the side of it this pixel is on.
float2 P = LocalPos.xy;
float2 A0 = PathA.xy;
float2 D0 = PathA.zw - A0;
float L0 = max(length(D0), 0.001);
float T0 = saturate(dot(P - A0, D0) / (L0 * L0));
float Dist0 = length(P - (A0 + D0 * T0));
float U = T0 * L0;
float Side = D0.x * (P.y - A0.y) - D0.y * (P.x - A0.x);
if (PathB.z > 1.5)
{
    float2 A1 = PathA.zw;
    float2 D1 = PathB.xy - A1;
    float L1 = max(length(D1), 0.001);
    float T1 = saturate(dot(P - A1, D1) / (L1 * L1));
    float Dist1 = length(P - (A1 + D1 * T1));
    if (Dist1 < Dist0)
    {
        U = PathB.w + T1 * L1;
        Side = D1.x * (P.y - A1.y) - D1.y * (P.x - A1.x);
    }
}
float2 FieldUV = float2((U - Map.x) * Map.z, (LocalPos.z - Map.y) * Map.w);
float4 F = Texture2DSampleLevel(FieldTex, FieldTexSampler, FieldUV, 0);
// Fine value noise on the unrolled wall (14 cm cells): small holes get ragged edges, not perfect circles.
float2 FineP = float2(U, LocalPos.z) / 14.0;
float2 FineCell = floor(FineP);
float2 FineT = frac(FineP);
FineT = FineT * FineT * (3.0 - 2.0 * FineT);
float FineA = frac(sin(dot(FineCell, float2(127.1, 311.7))) * 43758.5453);
float FineB = frac(sin(dot(FineCell + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
float FineC = frac(sin(dot(FineCell + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
float FineD = frac(sin(dot(FineCell + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
float FineNoise = lerp(lerp(FineA, FineB, FineT.x), lerp(FineC, FineD, FineT.x), FineT.y);
// Field values: f = 1 - distance / radius for holes, f > 0 is gone. Noise warps the edges.
float HoleF = ((Side >= 0.0 ? F.r : F.g) * 2.0 - 1.0) - (Noise - 0.5) * 0.4 - (FineNoise - 0.5) * 0.35;
float BreachF = (F.b * 2.0 - 1.0) - (NoiseCoarse - 0.5) * 0.7 - (Noise - 0.5) * 0.25 - (FineNoise - 0.5) * 0.15;
float HoleCut = step(0.0, HoleF);
float BreachCut = step(0.0, BreachF);
// Broken rebar: one 20 cm mat of bars (drawn on the front face only; two-sided shading shows it from behind) that
// sticks into the breach by a random length per bar, some bars snapped off at the edge.
float2 Grid = float2(U, LocalPos.z) / 20.0;
float2 GridDistance = abs(frac(Grid) - 0.5);
float OnBar = step(0.44, max(GridDistance.x, GridDistance.y));
float BarHash = frac(sin(dot(floor(Grid), float2(12.9898, 78.233))) * 43758.5453);
float BarReach = 0.06 + 0.34 * frac(BarHash * 7.13);
float InEdgeBand = BreachCut * step(BreachF, BarReach);
float Rebar = OnBar * InEdgeBand * step(0.35, BarHash) * step(0.5, RebarAmount) * step(0.0, Side);
float Visible = max(1.0 - max(HoleCut, BreachCut), Rebar);
// Scorched ring just outside a shell hole; fresh, lighter fracture just outside a breach.
float ShellRim = smoothstep(-0.3, 0.0, HoleF) * (1.0 - HoleCut);
float BreachRim = smoothstep(-0.35, 0.0, BreachF) * (1.0 - BreachCut);
return float4(Visible, ShellRim, BreachRim, Rebar);'''


def _scalar(material, name, default, x, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', default)
    return node


def _vector(material, name, default, x, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', unreal.LinearColor(*default))
    return node


def _mask(material, x, y, channels):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, x, y)
    for channel in 'rgba':
        node.set_editor_property(channel, channel in channels)
    return node


def _rgba(material, vector_parameter, links):
    """A vector parameter's default output is RGB; the field code needs all four channels: append its A."""
    position = vector_parameter.get_editor_property('material_expression_editor_x'),         vector_parameter.get_editor_property('material_expression_editor_y')
    append = MEL.create_material_expression(material, unreal.MaterialExpressionAppendVector, position[0] + 180,
                                            position[1])
    links += [MEL.connect_material_expressions(vector_parameter, '', append, 'A'),
              MEL.connect_material_expressions(vector_parameter, 'A', append, 'B')]
    return append


def _noise(material, local, scale, x, y, links):
    """LowResBlurredNoise on (x + y, z) / scale: one sample that roughens edges on every face of the wall."""
    xy = _mask(material, x - 450, y, 'rg')
    z = _mask(material, x - 450, y + 80, 'b')
    along = MEL.create_material_expression(material, unreal.MaterialExpressionDotProduct, x - 300, y)
    one_one = MEL.create_material_expression(material, unreal.MaterialExpressionConstant2Vector, x - 450, y + 160)
    one_one.set_editor_property('r', 1.0)
    one_one.set_editor_property('g', 1.0)
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionAppendVector, x - 150, y)
    tiling = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, x, y)
    tiling.set_editor_property('const_b', 1.0 / scale)
    sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, x + 150, y)
    sample.set_editor_property('texture', unreal.load_asset(NOISE))
    sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    links += [MEL.connect_material_expressions(local, '', xy, ''),
              MEL.connect_material_expressions(local, '', z, ''),
              MEL.connect_material_expressions(xy, '', along, 'A'),
              MEL.connect_material_expressions(one_one, '', along, 'B'),
              MEL.connect_material_expressions(along, '', uv, 'A'),
              MEL.connect_material_expressions(z, '', uv, 'B'),
              MEL.connect_material_expressions(uv, '', tiling, 'A'),
              MEL.connect_material_expressions(tiling, '', sample, 'UVs')]
    return sample


def _inject(material, keep_source_opacity):
    """Adds the field subgraph to a fresh masked copy. Returns the number of failed links."""
    base = MEL.get_material_property_input_node(material, PROP.MP_BASE_COLOR)
    base_out = MEL.get_material_property_input_node_output_name(material, PROP.MP_BASE_COLOR)
    rough = MEL.get_material_property_input_node(material, PROP.MP_ROUGHNESS)
    rough_out = MEL.get_material_property_input_node_output_name(material, PROP.MP_ROUGHNESS) if rough else ''
    emissive = MEL.get_material_property_input_node(material, PROP.MP_EMISSIVE_COLOR)
    emissive_out = MEL.get_material_property_input_node_output_name(material, PROP.MP_EMISSIVE_COLOR) \
        if emissive else ''
    if base is None:
        raise RuntimeError(material.get_path_name() + ': BaseColor is not connected (material attributes?)')
    links = []
    world = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1700, 900)
    # Without the material's own offsets (wall gate VAT): the mask must not move with the vertices it hides.
    world.set_editor_property('world_position_shader_offset',
                              unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    local = MEL.create_material_expression(material, unreal.MaterialExpressionTransformPosition, -1500, 900)
    local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    links.append(MEL.connect_material_expressions(world, '', local, ''))
    noise = _noise(material, local, 120.0, -500, 1000, links)
    noise_coarse = _noise(material, local, 420.0, -500, 1300, links)

    field = MEL.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -300, 1600)
    field.set_editor_property('parameter_name', 'WallFieldTexture')
    field.set_editor_property('texture', unreal.load_asset(DEFAULT_FIELD))
    path_a = _vector(material, 'WallFieldPathA', (0.0, 0.0, 100.0, 0.0), -300, 1750)
    path_b = _vector(material, 'WallFieldPathB', (100.0, 0.0, 1.0, 100.0), -300, 1900)
    field_map = _vector(material, 'WallFieldMap', (0.0, 0.0, 0.01, 0.01), -300, 2050)
    rebar = _scalar(material, REBAR, 1.0, -300, 2200)

    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, 100, 1200)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('description', 'WallHolesField')
    custom.set_editor_property('code', CODE)
    inputs = []
    for name in ['LocalPos', 'Noise', 'NoiseCoarse', 'FieldTex', 'PathA', 'PathB', 'Map', 'RebarAmount']:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', name)
        inputs.append(custom_input)
    custom.set_editor_property('inputs', inputs)
    links += [MEL.connect_material_expressions(local, '', custom, 'LocalPos'),
              MEL.connect_material_expressions(noise, 'R', custom, 'Noise'),
              MEL.connect_material_expressions(noise_coarse, 'R', custom, 'NoiseCoarse'),
              MEL.connect_material_expressions(field, '', custom, 'FieldTex'),
              MEL.connect_material_expressions(_rgba(material, path_a, links), '', custom, 'PathA'),
              MEL.connect_material_expressions(_rgba(material, path_b, links), '', custom, 'PathB'),
              MEL.connect_material_expressions(_rgba(material, field_map, links), '', custom, 'Map'),
              MEL.connect_material_expressions(rebar, '', custom, 'RebarAmount')]

    visible = _mask(material, 400, 1100, 'r')
    shell_rim = _mask(material, 400, 1200, 'g')
    breach_rim = _mask(material, 400, 1300, 'b')
    rebar_mask = _mask(material, 400, 1400, 'a')
    for node in (visible, shell_rim, breach_rim, rebar_mask):
        links.append(MEL.connect_material_expressions(custom, '', node, ''))

    source_opacity = MEL.get_material_property_input_node(material, PROP.MP_OPACITY_MASK) if keep_source_opacity else None
    if source_opacity is not None:
        source_output = MEL.get_material_property_input_node_output_name(material, PROP.MP_OPACITY_MASK)
        combined = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 650, 1100)
        links += [MEL.connect_material_expressions(visible, '', combined, 'A'),
                  MEL.connect_material_expressions(source_opacity, source_output, combined, 'B'),
                  MEL.connect_material_property(combined, '', PROP.MP_OPACITY_MASK)]
    else:
        links.append(MEL.connect_material_property(visible, '', PROP.MP_OPACITY_MASK))

    # Base colour: scorch around shell holes, pale fresh fracture around breaches, rust on the rebar.
    scorch = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 400, -700)
    scorch.set_editor_property('const_b', SCORCH)
    scorched = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 650, -650)
    fracture = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 400, -550)
    fracture.set_editor_property('const_b', 1.35)
    fracture_amount = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 650, -500)
    fracture_amount.set_editor_property('const_b', 0.75)
    fractured = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 850, -600)
    rust = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, 850, -450)
    rust.set_editor_property('constant', unreal.LinearColor(0.11, 0.055, 0.03, 1.0))
    rusted = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 1050, -550)
    links += [MEL.connect_material_expressions(base, base_out, scorch, 'A'),
              MEL.connect_material_expressions(base, base_out, scorched, 'A'),
              MEL.connect_material_expressions(scorch, '', scorched, 'B'),
              MEL.connect_material_expressions(shell_rim, '', scorched, 'Alpha'),
              MEL.connect_material_expressions(base, base_out, fracture, 'A'),
              MEL.connect_material_expressions(breach_rim, '', fracture_amount, 'A'),
              MEL.connect_material_expressions(scorched, '', fractured, 'A'),
              MEL.connect_material_expressions(fracture, '', fractured, 'B'),
              MEL.connect_material_expressions(fracture_amount, '', fractured, 'Alpha'),
              MEL.connect_material_expressions(fractured, '', rusted, 'A'),
              MEL.connect_material_expressions(rust, '', rusted, 'B'),
              MEL.connect_material_expressions(rebar_mask, '', rusted, 'Alpha')]

    # Back faces (two-sided instances: the inside of the wall seen through a hole) lean toward a dark inner colour.
    sign = MEL.create_material_expression(material, unreal.MaterialExpressionTwoSidedSign, 1050, -850)
    flipped = MEL.create_material_expression(material, unreal.MaterialExpressionOneMinus, 1200, -850)
    back = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 1350, -850)
    back.set_editor_property('const_b', 0.5)
    darken = _scalar(material, INNER_DARKEN, 0.0, 1200, -750)
    amount = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 1500, -800)
    inner_color = _vector(material, INNER_COLOR, (0.09, 0.085, 0.08, 1.0), 1350, -700)
    inner_rgb = _mask(material, 1500, -700, 'rgb')
    inner = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 1650, -600)
    links += [MEL.connect_material_expressions(sign, '', flipped, ''),
              MEL.connect_material_expressions(flipped, '', back, 'A'),
              MEL.connect_material_expressions(back, '', amount, 'A'),
              MEL.connect_material_expressions(darken, '', amount, 'B'),
              MEL.connect_material_expressions(inner_color, '', inner_rgb, ''),
              MEL.connect_material_expressions(rusted, '', inner, 'A'),
              MEL.connect_material_expressions(inner_rgb, '', inner, 'B'),
              MEL.connect_material_expressions(amount, '', inner, 'Alpha'),
              MEL.connect_material_property(inner, '', PROP.MP_BASE_COLOR)]
    if emissive is not None:
        keep = MEL.create_material_expression(material, unreal.MaterialExpressionOneMinus, 1650, -350)
        dimmed = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 1800, -350)
        links += [MEL.connect_material_expressions(back, '', keep, ''),
                  MEL.connect_material_expressions(emissive, emissive_out, dimmed, 'A'),
                  MEL.connect_material_expressions(keep, '', dimmed, 'B'),
                  MEL.connect_material_property(dimmed, '', PROP.MP_EMISSIVE_COLOR)]

    if rough is not None:
        rim_max = MEL.create_material_expression(material, unreal.MaterialExpressionMax, 650, -250)
        rough_rim = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 850, -200)
        rough_rim.set_editor_property('const_b', 1.0)
        rough_bar = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 1050, -200)
        rough_bar.set_editor_property('const_b', 0.55)
        links += [MEL.connect_material_expressions(shell_rim, '', rim_max, 'A'),
                  MEL.connect_material_expressions(breach_rim, '', rim_max, 'B'),
                  MEL.connect_material_expressions(rough, rough_out, rough_rim, 'A'),
                  MEL.connect_material_expressions(rim_max, '', rough_rim, 'Alpha'),
                  MEL.connect_material_expressions(rough_rim, '', rough_bar, 'A'),
                  MEL.connect_material_expressions(rebar_mask, '', rough_bar, 'Alpha'),
                  MEL.connect_material_property(rough_bar, '', PROP.MP_ROUGHNESS)]
    return links.count(False)


def field_master_for(source_master_path, rebuild=False):
    """The `<Master>_WallHolesField` masked copy of an opaque master; built on first use (or rebuilt when asked and
    no instance is loaded that would keep the old shader)."""
    source = unreal.load_asset(source_master_path)
    if not isinstance(source, unreal.Material):
        raise RuntimeError(source_master_path + ' is not a Material')
    target = source.get_path_name().split('.')[0] + SUFFIX
    if EAL.does_asset_exist(target) and not rebuild:
        return unreal.load_asset(target)
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)
    was_masked = source.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_MASKED
    material = EAL.duplicate_asset(source.get_path_name().split('.')[0], target)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    if not was_masked:
        material.set_editor_property('opacity_mask_clip_value', 0.5)
    material.set_editor_property('used_with_niagara_mesh_particles', True)
    failed = _inject(material, keep_source_opacity=was_masked)
    unreal.SystemLibrary.execute_console_command(None, 'RTS.WallChunks.FinishMaskedMaster Material=' + target)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material, False)
    unreal.log(f'WALL_FIELD master {target} failed_links={failed} '
               f'ps={MEL.get_statistics(material).num_pixel_shader_instructions}')
    return material


def field_instance_for(holes_instance_name, rebar, inner_color, inner_darken):
    """`<instance>Field`: a copy of a Rus wall `_WallHoles` instance on the field master (or on the field copy of its
    parent instance), two-sided with darkened back faces."""
    source = unreal.load_asset(HOLES_FOLDER + '/' + holes_instance_name)
    if source is None:
        raise RuntimeError('missing ' + holes_instance_name)
    parent = source.get_editor_property('parent')
    if isinstance(parent, unreal.MaterialInstance):
        new_parent = unreal.load_asset(parent.get_path_name().split('.')[0] + 'Field')
        if new_parent is None:
            raise RuntimeError('build the parent first: ' + parent.get_name() + 'Field')
    else:
        slot_master = parent.get_path_name().split('.')[0]
        if not slot_master.endswith('_WallHoles'):
            raise RuntimeError(holes_instance_name + ' is not on a _WallHoles master: ' + slot_master)
        new_parent = field_master_for(slot_master[:-len('_WallHoles')])
    target = HOLES_FOLDER + '/' + holes_instance_name + 'Field'
    instance = unreal.load_asset(target) if EAL.does_asset_exist(target) else \
        EAL.duplicate_asset(source.get_path_name().split('.')[0], target)
    MEL.set_material_instance_parent(instance, new_parent)
    MEL.set_material_instance_scalar_parameter_value(instance, REBAR, float(rebar))
    MEL.set_material_instance_scalar_parameter_value(instance, INNER_DARKEN, float(inner_darken))
    MEL.set_material_instance_vector_parameter_value(instance, INNER_COLOR, unreal.LinearColor(*inner_color, 1.0))
    overrides = instance.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_two_sided', True)
    overrides.set_editor_property('two_sided', True)
    instance.set_editor_property('base_property_overrides', overrides)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    return instance


def delete_rus_wall_field_materials():
    """Removes the field instances (children first) and both field masters, for a clean rebuild."""
    for name, _, _, _ in reversed(WALL_INSTANCES):
        target = HOLES_FOLDER + '/' + name + 'Field'
        if EAL.does_asset_exist(target):
            EAL.delete_asset(target)
    for master in ['/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/M_RusBunker_Concrete',
                   '/Game/SandBagPack/Materials/MM_Rus_SandBag_01']:
        if EAL.does_asset_exist(master + SUFFIX):
            EAL.delete_asset(master + SUFFIX)


def _reachable_expressions(material):
    """Every expression feeding the material outputs (Python has no direct list of a material's expressions)."""
    seen = []
    queue = []
    for prop in (PROP.MP_BASE_COLOR, PROP.MP_ROUGHNESS, PROP.MP_OPACITY_MASK, PROP.MP_EMISSIVE_COLOR, PROP.MP_NORMAL,
                 PROP.MP_METALLIC, PROP.MP_SPECULAR, PROP.MP_AMBIENT_OCCLUSION):
        node = MEL.get_material_property_input_node(material, prop)
        if node is not None:
            queue.append(node)
    while queue:
        node = queue.pop()
        if node is None or node in seen:
            continue
        seen.append(node)
        queue.extend(n for n in MEL.get_inputs_for_material_expression(material, node) if n is not None)
    return seen


def update_field_masters_in_place():
    """Pushes the current CODE and scorch level into the existing field masters without rebuilding them (their
    instances and the wall Blueprints keep pointing at the same assets)."""
    out = []
    for master in ['/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/M_RusBunker_Concrete',
                   '/Game/SandBagPack/Materials/MM_Rus_SandBag_01']:
        material = unreal.load_asset(master + SUFFIX)
        expressions = _reachable_expressions(material)
        customs = [e for e in expressions if isinstance(e, unreal.MaterialExpressionCustom)
                   and e.get_editor_property('description') == 'WallHolesField']
        scorches = [e for e in expressions if isinstance(e, unreal.MaterialExpressionMultiply)
                    and abs(e.get_editor_property('const_b') - 0.3) < 1e-4]
        for custom in customs:
            custom.set_editor_property('code', CODE)
        for scorch in scorches:
            scorch.set_editor_property('const_b', SCORCH)
        MEL.recompile_material(material)
        saved = EAL.save_loaded_asset(material, False)
        out.append((material.get_name(), len(customs), len(scorches), saved,
                    MEL.get_statistics(material).num_pixel_shader_instructions))
    return out


def build_rus_wall_field_materials(rebuild_masters=False):
    if rebuild_masters:
        for master in ['/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/M_RusBunker_Concrete',
                       '/Game/SandBagPack/Materials/MM_Rus_SandBag_01']:
            field_master_for(master, rebuild=True)
    built = []
    for name, rebar, inner_color, inner_darken in WALL_INSTANCES:
        built.append(field_instance_for(name, rebar, inner_color, inner_darken).get_path_name())
    for path in built:
        unreal.log('WALL_FIELD instance ' + path)
    return built
