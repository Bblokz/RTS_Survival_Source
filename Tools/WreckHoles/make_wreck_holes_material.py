"""Procedural shell holes on vehicle wrecks (Docs/Environment/WreckHoleDamage.md). Editor Python library.

Use inside the editor (UECP execute_python or the Python console):
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WreckHoles/make_wreck_holes_material.py', encoding='utf-8').read())
    damaged = make_damaged_material('/Game/.../MI_SomeWreck')   # builds what is missing, returns the masked material
    register(damaged_source_pairs)                              # writes DA_WreckHoleMaterials

What it builds, all next to the source assets (one convention, so any tool can find them):
- `<Master>_WreckHoles`: a Masked, two-sided copy of an opaque master with the hole subgraph. Built once and reused;
  never rebuilt in place while instances use it (delete its instances first). After the copy the script runs
  RTS.WallChunks.FinishMaskedMaster, because a copied opaque master keeps a constant on its opacity-mask pin.
- `<MaterialInstance>_WreckHoles`: a copy of a slot instance (same overrides) reparented onto the holes copy of its
  parent (a parent instance gets its own `_WreckHoles` copy, recursively, so its overrides stay).
- A slot that uses a master directly gets the master's `_WreckHoles` copy itself.

The hole subgraph reads custom primitive data 11..34 (UWreckHoleDamageComponent, WreckHoleMask): 6 holes as
(mesh X, mesh Y, mesh Z, radius), radius 0 = unused. Holes are spheres in the mesh's local space with noise-warped
edges and a scorched rim. Back faces (the inside of the hull, seen through a hole) are drawn charred black.
M_WreckChar_Overlay gets the same hole cut on its opacity (patch_char_overlay), or its char would cover the hole.

Skipped (report them, don't hack around them): masked or translucent masters (camo nets, foliage), masters that use
material attributes or Substrate, non-surface domains.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROP = unreal.MaterialProperty
NOISE = '/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/LowResBlurredNoise'
FIRST_HOLE = 11
HOLES = 6
FLOATS_PER_HOLE = 4
SUFFIX = '_WreckHoles'
MAP_PATH = '/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckHoles/DA_WreckHoleMaterials'
CHAR_OVERLAY = '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/M_WreckChar_Overlay'
OVERLAY_TAG = 'WreckHoles'
# Hole edge noise: tiling in cm (smaller = finer, more jagged edge) and how far it warps the edge (share of radius).
NOISE_TILE_CM = 35.0
EDGE_WARP = 0.5
# Scorched rim out to this share of the radius; the rim's colour multiplier and roughness.
RIM_OUTER = 1.55
RIM_DARKEN = 0.12
RIM_ROUGHNESS = 0.95
# The inside of the hull seen through a hole.
INTERIOR_COLOUR = (0.012, 0.010, 0.009)


def _hole_code():
    lines = []
    for index in range(HOLES):
        lines.append(f'{{ float R = max(H{index}R, 0.01); '
                     f'float Ratio = (length(P - float3(H{index}X, H{index}Y, H{index}Z)) + (Noise - 0.5) * {EDGE_WARP} * R) / R; '
                     f'MinRatio = min(MinRatio, H{index}R > 0.5 ? Ratio : 1.0e4); }}')
    return '\n'.join(lines)


CODE = '''// Six sphere holes in mesh space, edge warped by the noise; MinRatio < 1 is inside a hole.
float3 P = LocalPos;
float MinRatio = 1.0e4;
''' + _hole_code() + f'''
float Visible = 1.0 - step(MinRatio, 1.0);
// Scorched, rougher metal just outside a hole.
float Rim = 1.0 - smoothstep(1.0, {RIM_OUTER}, MinRatio);
return float2(Visible, Rim);'''


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


def _hole_nodes(material, links, x=-1400, y=900):
    """Adds the hole Custom node; returns (visible, rim) mask nodes."""
    world = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, x, y)
    local = MEL.create_material_expression(material, unreal.MaterialExpressionTransformPosition, x + 250, y)
    local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    links.append(MEL.connect_material_expressions(world, '', local, ''))
    # Noise on (x + y, z): one sample that roughens edges on every face of a box.
    xy = _mask(material, x + 500, y + 100, 'rg')
    z = _mask(material, x + 500, y + 180, 'b')
    links += [MEL.connect_material_expressions(local, '', xy, ''), MEL.connect_material_expressions(local, '', z, '')]
    along = MEL.create_material_expression(material, unreal.MaterialExpressionDotProduct, x + 650, y + 100)
    one_one = MEL.create_material_expression(material, unreal.MaterialExpressionConstant2Vector, x + 500, y + 260)
    one_one.set_editor_property('r', 1.0)
    one_one.set_editor_property('g', 1.0)
    links += [MEL.connect_material_expressions(xy, '', along, 'A'), MEL.connect_material_expressions(one_one, '', along, 'B')]
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionAppendVector, x + 800, y + 100)
    links += [MEL.connect_material_expressions(along, '', uv, 'A'), MEL.connect_material_expressions(z, '', uv, 'B')]
    tiling = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, x + 950, y + 100)
    tiling.set_editor_property('const_b', 1.0 / NOISE_TILE_CM)
    links.append(MEL.connect_material_expressions(uv, '', tiling, 'A'))
    noise = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, x + 1100, y + 100)
    noise.set_editor_property('texture', unreal.load_asset(NOISE))
    noise.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    links.append(MEL.connect_material_expressions(tiling, '', noise, 'UVs'))

    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, x + 1500, y)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    custom.set_editor_property('description', OVERLAY_TAG)
    custom.set_editor_property('code', CODE)
    names = ['LocalPos', 'Noise']
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
    for index in range(HOLES):
        for offset, axis in enumerate('XYZR'):
            data_index = FIRST_HOLE + index * FLOATS_PER_HOLE + offset
            node = _scalar(material, f'WreckHole{index}{axis}', 0.0, x + 1100,
                           y + 300 + (index * FLOATS_PER_HOLE + offset) * 70, data_index)
            links.append(MEL.connect_material_expressions(node, '', custom, f'H{index}{axis}'))
    visible = _mask(material, x + 1800, y, 'r')
    rim = _mask(material, x + 1800, y + 100, 'g')
    links += [MEL.connect_material_expressions(custom, '', visible, ''),
              MEL.connect_material_expressions(custom, '', rim, '')]
    return visible, rim


def _inject(material, darken_back_faces):
    """Adds the hole subgraph to a fresh masked copy. Returns the number of failed links.
    darken_back_faces: only for masters that were single-sided; a two-sided master's back faces are real surfaces
    (open-topped hull walls, thin plates) and keep their look."""
    base = MEL.get_material_property_input_node(material, PROP.MP_BASE_COLOR)
    rough = MEL.get_material_property_input_node(material, PROP.MP_ROUGHNESS)
    if base is None:
        raise RuntimeError(material.get_path_name() + ': BaseColor is not connected (material attributes?)')
    links = []
    visible, rim = _hole_nodes(material, links)
    links.append(MEL.connect_material_property(visible, '', PROP.MP_OPACITY_MASK))
    # Back faces are the hull's inside: 0 on the front, 1 on the back (always 0 for an originally two-sided master).
    if darken_back_faces:
        sign = MEL.create_material_expression(material, unreal.MaterialExpressionTwoSidedSign, 400, -700)
        back = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 550, -700)
        back.set_editor_property('const_b', -0.5)
        back_offset = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 700, -700)
        back_offset.set_editor_property('const_b', 0.5)
        links += [MEL.connect_material_expressions(sign, '', back, 'A'),
                  MEL.connect_material_expressions(back, '', back_offset, 'A')]
    else:
        back_offset = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, 700, -700)
        back_offset.set_editor_property('r', 0.0)
    scorch = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 400, -500)
    scorch.set_editor_property('const_b', RIM_DARKEN)
    tinted = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 650, -450)
    interior = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, 650, -600)
    interior.set_editor_property('constant', unreal.LinearColor(*INTERIOR_COLOUR, 1.0))
    inside = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 900, -450)
    links += [MEL.connect_material_expressions(base, '', scorch, 'A'),
              MEL.connect_material_expressions(base, '', tinted, 'A'),
              MEL.connect_material_expressions(scorch, '', tinted, 'B'),
              MEL.connect_material_expressions(rim, '', tinted, 'Alpha'),
              MEL.connect_material_expressions(tinted, '', inside, 'A'),
              MEL.connect_material_expressions(interior, '', inside, 'B'),
              MEL.connect_material_expressions(back_offset, '', inside, 'Alpha'),
              MEL.connect_material_property(inside, '', PROP.MP_BASE_COLOR)]
    rim_or_back = MEL.create_material_expression(material, unreal.MaterialExpressionMax, 650, -250)
    links += [MEL.connect_material_expressions(rim, '', rim_or_back, 'A'),
              MEL.connect_material_expressions(back_offset, '', rim_or_back, 'B')]
    rough_rim = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 900, -150)
    rough_rim.set_editor_property('const_a', 0.6)
    rough_rim.set_editor_property('const_b', RIM_ROUGHNESS)
    if rough is not None:
        links.append(MEL.connect_material_expressions(rough, '', rough_rim, 'A'))
    links += [MEL.connect_material_expressions(rim_or_back, '', rough_rim, 'Alpha'),
              MEL.connect_material_property(rough_rim, '', PROP.MP_ROUGHNESS)]
    return links.count(False)


def can_take_holes(material):
    """(ok, reason) for a master: only opaque surface materials without material attributes."""
    base = material.get_base_material() if material else None
    if base is None:
        return False, 'no base material'
    if base.get_editor_property('material_domain') != unreal.MaterialDomain.MD_SURFACE:
        return False, 'not a surface material'
    if base.get_editor_property('blend_mode') != unreal.BlendMode.BLEND_OPAQUE:
        return False, 'blend mode ' + str(base.get_editor_property('blend_mode'))
    if base.get_editor_property('use_material_attributes'):
        return False, 'uses material attributes'
    if MEL.get_material_property_input_node(base, PROP.MP_BASE_COLOR) is None:
        return False, 'base colour not connected'
    if isinstance(material, unreal.MaterialInstance):
        overrides = material.get_editor_property('base_property_overrides')
        if overrides.get_editor_property('override_blend_mode') and \
                overrides.get_editor_property('blend_mode') != unreal.BlendMode.BLEND_OPAQUE:
            return False, 'instance overrides blend mode'
    return True, ''


def _package(asset):
    return asset.get_path_name().split('.')[0]


def masked_master_for(source_master):
    """The `<Master>_WreckHoles` masked, two-sided copy of an opaque master; built on first use."""
    if isinstance(source_master, str):
        source_master = unreal.load_asset(source_master)
    if not isinstance(source_master, unreal.Material):
        raise RuntimeError(str(source_master) + ' is not a Material')
    if _package(source_master).endswith(SUFFIX):
        return source_master
    target = _package(source_master) + SUFFIX
    if EAL.does_asset_exist(target):
        return unreal.load_asset(target)
    was_two_sided = source_master.get_editor_property('two_sided')
    material = EAL.duplicate_asset(_package(source_master), target)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('opacity_mask_clip_value', 0.5)
    material.set_editor_property('two_sided', True)
    failed = _inject(material, darken_back_faces=not was_two_sided)
    unreal.SystemLibrary.execute_console_command(None, 'RTS.WallChunks.FinishMaskedMaster Material=' + target)
    material = unreal.load_asset(target)
    material.set_editor_property('two_sided', True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material, False)
    if failed:
        unreal.log_warning(f'WRECK_HOLES {target}: {failed} links failed')
    return material


def _force_two_sided(instance):
    overrides = instance.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_two_sided', True)
    overrides.set_editor_property('two_sided', True)
    instance.set_editor_property('base_property_overrides', overrides)


def make_damaged_material(source):
    """Masked holes material for one slot material (instance or master); builds what is missing. None if skipped."""
    if isinstance(source, str):
        source = unreal.load_asset(source)
    ok, reason = can_take_holes(source)
    if not ok:
        unreal.log_warning(f'WRECK_HOLES skip {source.get_path_name() if source else source}: {reason}')
        return None
    if isinstance(source, unreal.Material):
        return masked_master_for(source)
    target = _package(source) + SUFFIX
    if EAL.does_asset_exist(target):
        return unreal.load_asset(target)
    parent = source.get_editor_property('parent')
    # An instance of an instance hangs under the parent's own holes copy, so the parent's overrides stay.
    damaged_parent = make_damaged_material(parent) if isinstance(parent, unreal.MaterialInstance) \
        else masked_master_for(source.get_base_material())
    if damaged_parent is None:
        return None
    instance = EAL.duplicate_asset(_package(source), target)
    MEL.set_material_instance_parent(instance, damaged_parent)
    _force_two_sided(instance)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    return instance


def check_mask_live(damaged):
    """(masked PS, opaque PS) of a holes master against its source; the mask adds ~60+, under 10 means it compiled away."""
    master = damaged.get_base_material()
    source = unreal.load_asset(_package(master)[:-len(SUFFIX)])
    return (MEL.get_statistics(master).num_pixel_shader_instructions,
            MEL.get_statistics(source).num_pixel_shader_instructions)


def material_map():
    """DA_WreckHoleMaterials (UWreckHoleMaterialMap); created on first use."""
    if EAL.does_asset_exist(MAP_PATH):
        return unreal.load_asset(MAP_PATH)
    folder, name = MAP_PATH.rsplit('/', 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.load_class(None, '/Script/RTS_Survival.WreckHoleMaterialMap'), unreal.DataAssetFactory())


def register(pairs):
    """Writes (source, damaged) pairs into DA_WreckHoleMaterials and saves it."""
    data_asset = material_map()
    for source, damaged in pairs:
        if source is not None and damaged is not None:
            data_asset.register_damaged_material(source, damaged)
    EAL.save_loaded_asset(data_asset, False)
    return data_asset.get_pair_count()


def patch_char_overlay():
    """Adds the hole cut to M_WreckChar_Overlay's opacity (translucent overlay on every charred wreck). Idempotent."""
    overlay = unreal.load_asset(CHAR_OVERLAY)
    opacity = MEL.get_material_property_input_node(overlay, PROP.MP_OPACITY)
    if opacity is None:
        raise RuntimeError('M_WreckChar_Overlay: Opacity is not connected')
    if isinstance(opacity, unreal.MaterialExpressionMultiply) and \
            opacity.get_editor_property('desc') == OVERLAY_TAG:
        return 'already patched'
    links = []
    visible, _ = _hole_nodes(overlay, links, x=-2600, y=1400)
    cut = MEL.create_material_expression(overlay, unreal.MaterialExpressionMultiply, -300, 600)
    cut.set_editor_property('desc', OVERLAY_TAG)
    links += [MEL.connect_material_expressions(opacity, '', cut, 'A'),
              MEL.connect_material_expressions(visible, '', cut, 'B'),
              MEL.connect_material_property(cut, '', PROP.MP_OPACITY)]
    MEL.recompile_material(overlay)
    EAL.save_loaded_asset(overlay, False)
    return f'patched, {links.count(False)} links failed'
