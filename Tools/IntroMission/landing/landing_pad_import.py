"""Imports the Soviet landing pad (Blender: CustomMapItems/IntroMission/LandingPads, README there) into
/Game/RTS_Survival/Blueprints/Environment/IntroMission/LandingField.

  SM_LandingPad_Soviet        2100 x 2100 x 19.2 cm, pivot bottom centre, +X = the digits' up, Nanite off, LOD0 only,
                              one UCX convex hull (the slab, ramps included), simple collision used as complex
  T_LandingPad_Soviet_BC/N/ORM/Numbers
  M_LandingPad_Soviet         master: BaseColor/Normal/ORM, RGBA number mask picked by NumberIndex (1-4) and painted in
                              NumberColor, BaseTint over the base colour; static switch IsGlass turns it into the edge-light lens (LightColor x LightIntensity)
  MI_LandingPad_Soviet_1..4   slot 0 (pad), NumberIndex 1..4; the mesh default is _1
  MI_LandingPad_Soviet_Glass  slot 1 (lens glass), IsGlass on, LightIntensity / LightColor

Legacy FBX import (Interchange drops UCX hulls). Re-runnable: textures and mesh re-import in place, MIs are updated, the
master graph is only built when the master does not exist yet (never rebuilt live: delete_all on a used master killed
the editor once). Refuses to run during PIE.

Run:  python Source/Tools/IntroMission/landing/landing_cmd.py Source/Tools/IntroMission/landing/landing_pad_import.py
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/IntroMission/LandingPads/Model/SM_LandingPad_Soviet/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/IntroMission/LandingField'
MESH = 'SM_LandingPad_Soviet'
MASTER = 'M_LandingPad_Soviet'
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIGHT_COLOR = unreal.LinearColor(1.0, 0.62, 0.30, 1.0)      # warm amber-white (linear)
LIGHT_INTENSITY = 20.0
NUMBER_COLOR = unreal.LinearColor(0.66, 0.63, 0.53, 1.0)    # the white of the circle (sRGB 0.83 0.81 0.75)


def save(obj):
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + obj.get_path_name())


def import_texture(suffix, setup):
    name = 'T_LandingPad_Soviet_' + suffix
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.png'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{HOME}/{name}')
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    setup(tex)
    save(tex)
    return tex


def tex_bc(t):
    t.set_editor_property('srgb', True)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)


def tex_n(t):
    t.set_editor_property('srgb', False)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    t.set_editor_property('flip_green_channel', True)      # the PNG is OpenGL (green up)


def tex_orm(t):
    t.set_editor_property('srgb', False)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)


def tex_numbers(t):
    # four independent masks overlap in the corners: block compression would bleed them into each other
    t.set_editor_property('srgb', False)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)


def add_tint(mat, bc, col):
    """BaseTint (vector, default white) multiplies the base colour texture under the number paint: the in-map
    contrast knob against the apron."""
    tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, -800)
    tint.set_editor_property('parameter_name', 'BaseTint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, -500)
    for a, ap, b, bp in ((bc, 'RGB', mul, 'A'), (tint, '', mul, 'B'), (mul, '', col, 'A')):
        if not MEL.connect_material_expressions(a, ap, b, bp):
            raise RuntimeError('tint link failed')


def ensure_tint(mat):
    """Add BaseTint to a master built before it existed, incrementally (never delete_all on a live master)."""
    if 'BaseTint' in [str(n) for n in MEL.get_vector_parameter_names(mat)]:
        return False
    sw = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)     # IsGlass switch
    col = [n for n in MEL.get_inputs_for_material_expression(mat, sw) if isinstance(n, unreal.MaterialExpressionLinearInterpolate)][0]
    bc = [n for n in MEL.get_inputs_for_material_expression(mat, col) if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D)][0]
    add_tint(mat, bc, col)
    MEL.recompile_material(mat)
    save(mat)
    return True


def build_master(tex):
    path = f'{HOME}/{MASTER}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    mat = TOOLS.create_asset(MASTER, HOME, unreal.Material, unreal.MaterialFactoryNew())
    try:
        build_graph(mat, tex)
    except Exception:
        # a half-built master would be reported as 'kept' on the next run: remove it (nothing references it yet)
        unreal.EditorAssetLibrary.delete_asset(path)
        raise
    MEL.recompile_material(mat)
    save(mat)
    return mat, True


def build_graph(mat, tex):
    def node(cls, x, y, **props):
        n = MEL.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, ap, b, bp):
        if not MEL.connect_material_expressions(a, ap, b, bp):
            raise RuntimeError(f'link failed {a.get_name()}.{ap} -> {b.get_name()}.{bp}')

    S = unreal.MaterialSamplerType
    bc = node(unreal.MaterialExpressionTextureSampleParameter2D, -1400, -400, parameter_name='BaseColor', texture=tex['BC'], sampler_type=S.SAMPLERTYPE_COLOR)
    orm = node(unreal.MaterialExpressionTextureSampleParameter2D, -1400, 0, parameter_name='ORM', texture=tex['ORM'], sampler_type=S.SAMPLERTYPE_MASKS)
    nrm = node(unreal.MaterialExpressionTextureSampleParameter2D, -1400, 400, parameter_name='Normal', texture=tex['N'], sampler_type=S.SAMPLERTYPE_NORMAL)
    num = node(unreal.MaterialExpressionTextureSampleParameter2D, -1400, 800, parameter_name='Numbers', texture=tex['Numbers'], sampler_type=S.SAMPLERTYPE_LINEAR_COLOR)
    idx = node(unreal.MaterialExpressionScalarParameter, -1400, 1200, parameter_name='NumberIndex', default_value=1.0)
    # mask = sum_k channel_k * saturate(1 - |NumberIndex - k|)
    total = None
    for k, ch in enumerate(('R', 'G', 'B', 'A')):
        y = 700 + 160 * k
        sub = node(unreal.MaterialExpressionSubtract, -1150, y, const_b=float(k + 1))
        link(idx, '', sub, 'A')
        ab = node(unreal.MaterialExpressionAbs, -1000, y)
        link(sub, '', ab, '')
        om = node(unreal.MaterialExpressionOneMinus, -880, y)
        link(ab, '', om, '')
        sat = node(unreal.MaterialExpressionSaturate, -760, y)
        link(om, '', sat, '')
        mul = node(unreal.MaterialExpressionMultiply, -640, y)
        link(num, ch, mul, 'A')
        link(sat, '', mul, 'B')
        if total is None:
            total = mul
        else:
            add = node(unreal.MaterialExpressionAdd, -500, y)
            link(total, '', add, 'A')
            link(mul, '', add, 'B')
            total = add
    ncol = node(unreal.MaterialExpressionVectorParameter, -900, -600, parameter_name='NumberColor', default_value=NUMBER_COLOR)
    nrough = node(unreal.MaterialExpressionScalarParameter, -900, -100, parameter_name='NumberRoughness', default_value=0.55)
    col = node(unreal.MaterialExpressionLinearInterpolate, -400, -400)
    add_tint(mat, bc, col)
    link(ncol, '', col, 'B')
    link(total, '', col, 'Alpha')
    rough = node(unreal.MaterialExpressionLinearInterpolate, -400, -100)
    link(orm, 'G', rough, 'A')
    link(nrough, '', rough, 'B')
    link(total, '', rough, 'Alpha')
    metal = node(unreal.MaterialExpressionLinearInterpolate, -400, 100, const_b=0.0)
    link(orm, 'B', metal, 'A')
    link(total, '', metal, 'Alpha')
    # glass branch
    lcol = node(unreal.MaterialExpressionVectorParameter, -900, 1500, parameter_name='LightColor', default_value=LIGHT_COLOR)
    lint = node(unreal.MaterialExpressionScalarParameter, -900, 1700, parameter_name='LightIntensity', default_value=LIGHT_INTENSITY)
    emis = node(unreal.MaterialExpressionMultiply, -600, 1550)
    link(lcol, '', emis, 'A')
    link(lint, '', emis, 'B')
    glass_col = node(unreal.MaterialExpressionConstant3Vector, -600, 1300, constant=unreal.LinearColor(0.03, 0.025, 0.02, 1))
    flat_n = node(unreal.MaterialExpressionConstant3Vector, -600, 1400, constant=unreal.LinearColor(0, 0, 1, 1))
    zero = node(unreal.MaterialExpressionConstant, -600, 1750, r=0.0)
    one = node(unreal.MaterialExpressionConstant, -600, 1850, r=1.0)
    glass_rough = node(unreal.MaterialExpressionConstant, -600, 1950, r=0.12)
    P = unreal.MaterialProperty
    outs = [  # property, pad source (node, pin), glass source (node, pin)
        (P.MP_BASE_COLOR, (col, ''), (glass_col, '')),
        (P.MP_ROUGHNESS, (rough, ''), (glass_rough, '')),
        (P.MP_METALLIC, (metal, ''), (zero, '')),
        (P.MP_NORMAL, (nrm, 'RGB'), (flat_n, '')),
        (P.MP_AMBIENT_OCCLUSION, (orm, 'R'), (one, '')),
        (P.MP_EMISSIVE_COLOR, (zero, ''), (emis, '')),
    ]
    for i, (prop, pad, glass) in enumerate(outs):
        sw = node(unreal.MaterialExpressionStaticSwitchParameter, -150, -400 + 260 * i, parameter_name='IsGlass', default_value=False)
        link(glass[0], glass[1], sw, 'True')
        link(pad[0], pad[1], sw, 'False')
        if not MEL.connect_material_property(sw, '', prop):
            raise RuntimeError(f'property link failed {prop}')


def instance(name, master, scalars=(), switches=(), vectors=(), initial_scalars=()):
    """scalars/switches/vectors are enforced on every run; initial_scalars only when the MI is created (tuning
    such as LightIntensity survives a re-run)."""
    path = f'{HOME}/{name}'
    created = not unreal.EditorAssetLibrary.does_asset_exist(path)
    if created:
        TOOLS.create_asset(name, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    MEL.set_material_instance_parent(mi, master)
    for k, v in list(scalars) + (list(initial_scalars) if created else []):
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in vectors:
        MEL.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in switches:
        MEL.set_material_instance_static_switch_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    save(mi)
    return mi


def import_mesh():
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + MESH + '.FBX'
    task.destination_path = HOME
    task.destination_name = MESH
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    TOOLS.import_asset_tasks([task])
    return unreal.load_asset(f'{HOME}/{MESH}')


def finish(mesh, pad_mi, glass_mi):
    slots = mesh.get_editor_property('static_materials')
    for i, slot in enumerate(slots):
        name = str(slot.get_editor_property('material_slot_name'))
        mesh.set_material(i, glass_mi if name.endswith('Glass') else pad_mi)
    ns = mesh.get_editor_property('nanite_settings')
    if ns.get_editor_property('enabled'):
        ns.set_editor_property('enabled', False)
        mesh.set_editor_property('nanite_settings', ns)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    mesh.set_editor_property('light_map_resolution', 128)
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    save(mesh)
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems'))
    print(MESH, 'tris', mesh.get_num_triangles(0), 'lods', mesh.get_num_lods(), 'hulls', hulls,
          'slots', [(str(s.get_editor_property('material_slot_name')), s.get_editor_property('material_interface').get_name())
                    for s in mesh.get_editor_property('static_materials')])


if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError('PIE is running: no imports during PIE')
unreal.EditorAssetLibrary.make_directory(HOME)
tex = {
    'BC': import_texture('BC', tex_bc),
    'N': import_texture('N', tex_n),
    'ORM': import_texture('ORM', tex_orm),
    'Numbers': import_texture('Numbers', tex_numbers),
}
master, built = build_master(tex)
if not built and ensure_tint(master):
    print('master: BaseTint added')
stats = MEL.get_statistics(master)
print('master', master.get_path_name(), 'built now' if built else 'kept', 'ps instructions', stats.num_pixel_shader_instructions)
pads = [instance(f'MI_LandingPad_Soviet_{k}', master, scalars=[('NumberIndex', float(k))]) for k in (1, 2, 3, 4)]
glass = instance('MI_LandingPad_Soviet_Glass', master, switches=[('IsGlass', True)])   # LightIntensity/LightColor: master defaults, tune on this MI
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
try:
    mesh = import_mesh()
finally:
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
finish(mesh, pads[0], glass)
