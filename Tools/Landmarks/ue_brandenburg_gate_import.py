"""Imports the war-damaged Brandenburg Gate (run with the UECP execute_python action). Safe to rerun after a Blender rebuild.

Blender source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BrandenburgGateRuin/ (generator, validator,
report). The stone uses the Breakthrough concrete technique (FlakBunker/Material/ProceduralSource/README.md): same master code,
baked shared surface, the building's own curvature map and baked soot field in LightMapUV, the same normal finish.

Assets in /Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate:
- SM_BrandenburgGate_Ruin (16 UCX hulls) and its maps T_BG_BrandenburgGate_Curv / _Fields.
- M_BG_Sandstone_Procedural: a duplicate of the live M_BF_SootedFortress_Procedural with one addition, BaseColor x BG_StoneTint.
  The BF master itself is never touched. (Its JSON spec predates NomadicDeploy, so build_material_safely.py would strip that:
  this is why the gate duplicates the live asset instead of rebuilding from the spec.)
- MI_BG_Sandstone (slot 0, weathered stone) and MI_BG_SandstoneBroken (slot 1, fresh break faces and rubble).
- MI_BG_Sandstone_BF: the same settings on the unforked MI_BF_SootedFortress_UVTile, kept as the untinted fallback.
- M_BG_BronzePatina (slot 2): verdigris patina over dark bronze from the BF noise texture in object space
  (colours and PatinaAmount are parameters).
- MI_BG_RebarRust (slot 3, the FBX slot is still named DBK_Rebar): a rust instance of the patina material. DBK_Rebar itself
  was tried first and nearly vanished on the 6 cm tubes.

    STAGES = ('maps', 'mesh', 'master', 'instances', 'bronze', 'assign'); exec(open(r'<this file>').read())   # default: all
The master duplicate is only made once; delete it to remake it after the BF master changes.
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BrandenburgGateRuin/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate'
MESH = 'SM_BrandenburgGate_Ruin'
BF_MAT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/FlakBunker/Material/'
BF_MASTER = BF_MAT + 'M_BF_SootedFortress_Procedural'
BF_UVTILE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/MI_BF_SootedFortress_UVTile'
NOISE = BF_MAT + 'T_BF_ConcreteNoise'
REBAR = HOME + '/MI_BG_RebarRust'   # not /Game/DBK/Materials/DBK_Rebar: see make_rebar
MASTER = HOME + '/M_BG_Sandstone_Procedural'
BRONZE = 'M_BG_BronzePatina'
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary
STAGES = globals().get('STAGES', ('maps', 'mesh', 'master', 'instances', 'bronze', 'assign'))

# Stone: the BF switch set of the HQ (baked surface, curvature map, baked fields, soot without heat), no camo or lamps.
SWITCHES = dict(BF_UseBakedSurface=True, BF_UseCurvatureMap=True, BF_UseMeshCurvature=False, BF_UseBakedFields=True,
                BF_UseHeatSoot=True, BF_UseHeat=False, BF_UseRebar=True, BF_UseDamage=True, BF_UseCracks=True,
                BF_UseWeathering=True, BF_UseCamo=False, BF_UseCamoMap=False, BF_UseLampPools=False)
# Battered harder than the BF buildings (wear 0.25 there): this is a 1945 ruin.
STONE = dict(BF_WearAmount=0.5, BF_SpallAmount=0.65, BF_ImpactAmount=0.55, BF_CrackAmount=0.5, BF_SootAmount=1.0,
             BF_RebarAmount=1.0, BF_RebarExposure=0.5, BF_DustAmount=0.55, BF_StreakAmount=0.7, BF_GroundGrimeAmount=0.7,
             BF_OilAmount=0.15, BF_FieldsLampScale=1.0, BF_WearSeed=31.0)
STONE['BF_SootAmount'] = 0.8
BROKEN = dict(BF_WearAmount=0.65, BF_SpallAmount=0.8, BF_DustAmount=0.75, BF_StreakAmount=0.25, BF_SootAmount=0.55,
              BF_OilAmount=0.0, BF_WearSeed=57.0)
STONE_TINT = (1.50, 1.12, 0.70)      # Elbe sandstone over the BF concrete albedo (linear multiplier; 1.32/1.08/0.78 read olive)
BROKEN_TINT = (1.70, 1.32, 0.90)     # fresh, paler stone inside the breaks


def save(obj):
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + obj.get_path_name())


def import_map(name):
    """As ue_building_maps_import.import_map: exact 8-bit values, no mips, clamped, never streamed."""
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.png'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    ASSET_TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{HOME}/{name}')
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('never_stream', True)
    save(tex)
    return tex


def import_mesh():
    """Legacy FBX: Interchange drops UCX hulls and ignores FbxImportUI (ue_flak88_bunker_import.py)."""
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
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = f'{MODEL}{MESH}.FBX'
    task.destination_path = HOME
    task.destination_name = MESH
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        ASSET_TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(f'{HOME}/{MESH}')
    mesh.set_editor_property('light_map_coordinate_index', 1)
    save(mesh)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    uvs = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0)
    print('MESH', mesh.get_num_triangles(0), 'tris; uv channels', uvs, '; hulls', hulls,
          '; bounds', [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)])
    return mesh


def make_master():
    """Duplicate of the live BF master with BaseColor x BG_StoneTint. Added incrementally on our own copy only."""
    mat = unreal.load_asset(MASTER)
    if mat is not None:
        print('master exists', MASTER)
        return mat
    mat = unreal.EditorAssetLibrary.duplicate_asset(BF_MASTER, MASTER)
    src = EDIT.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    out = EDIT.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    if src is None:
        raise RuntimeError('BaseColor input not found on the duplicate')
    tint = EDIT.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -600)
    tint.set_editor_property('parameter_name', 'BG_StoneTint')
    tint.set_editor_property('group', '00_Surface')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    mul = EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, -600)
    mul.set_editor_property('desc', 'Brandenburg Gate: sandstone tint over the BF concrete colour')
    assert EDIT.connect_material_expressions(src, out, mul, 'A'), 'tint A'
    assert EDIT.connect_material_expressions(tint, '', mul, 'B'), 'tint B'
    assert EDIT.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR), 'base colour'
    EDIT.recompile_material(mat)
    save(mat)
    print('master duplicated', MASTER, 'base colour came from', src.get_name(), repr(out))
    return mat


def instance(name, parent, scalars, switches=None, tint=None, textures=None):
    path = f'{HOME}/{name}'
    mi = unreal.load_asset(path)
    if mi is None:
        mi = ASSET_TOOLS.create_asset(name, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    EDIT.set_material_instance_parent(mi, parent)
    for k, v in (switches or {}).items():
        EDIT.set_material_instance_static_switch_parameter_value(mi, k, v)
    for k, v in scalars.items():
        EDIT.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (textures or {}).items():
        EDIT.set_material_instance_texture_parameter_value(mi, k, v)
    if tint is not None:
        EDIT.set_material_instance_vector_parameter_value(mi, 'BG_StoneTint', unreal.LinearColor(*tint, 1.0))
    EDIT.update_material_instance(mi)
    save(mi)
    return mi


def make_instances():
    uvtile = unreal.load_asset(BF_UVTILE)
    maps = dict(BF_CurvatureMap=unreal.load_asset(f'{HOME}/T_BG_BrandenburgGate_Curv'),
                BF_FieldsMap=unreal.load_asset(f'{HOME}/T_BG_BrandenburgGate_Fields'))
    # UVTile's own overrides carry over to the sandstone master's instance, so both read the same as the BF buildings
    carried = {str(n): EDIT.get_material_instance_scalar_parameter_value(uvtile, n) for n in EDIT.get_scalar_parameter_names(uvtile)
               if str(n).startswith('BF_')}
    instance('MI_BG_Sandstone_BF', uvtile, STONE, SWITCHES, textures=maps)
    stone = instance('MI_BG_Sandstone', unreal.load_asset(MASTER), {**carried, **STONE}, SWITCHES, STONE_TINT, maps)
    instance('MI_BG_SandstoneBroken', stone, BROKEN, tint=BROKEN_TINT)
    print('instances made; carried', len(carried), 'UVTile scalars')


BRONZE_HLSL = r'''
// Verdigris over dark bronze: the BF noise texture read three ways in object space, patina gathering on
// faces that look up and running down below them. Returns rgb colour and a = patina amount.
float3 w = pow(abs(Nrm), 4.0); w /= (w.x + w.y + w.z + 1e-4);
float3 q = P / 180.0;
float4 a = Tex.SampleLevel(TexSampler, q.yz, 0);
float4 b = Tex.SampleLevel(TexSampler, q.xz + 0.37, 0);
float4 c = Tex.SampleLevel(TexSampler, q.xy + 0.71, 0);
float4 n = a * w.x + b * w.y + c * w.z;
float up = saturate(Nrm.z);
float runs = saturate(n.b * 1.6 - 0.45) * (1.0 - up);
float patina = saturate((n.g - 0.5) * 2.2 + up * 0.55 + runs * 0.6 + 0.25);
patina = smoothstep(0.35, 0.75, patina);
float3 bronze = lerp(BronzeDark, BronzeLight, n.r);
float3 verd = lerp(VerdDark, VerdLight, n.a);
float3 col = lerp(bronze, verd, patina * PatinaAmount) * lerp(0.75, 1.05, n.r);
return float4(col, patina * PatinaAmount);
'''
BRONZE_PARAMS = (  # name, default (linear), y position
    ('BronzeDark', (0.09, 0.06, 0.035), -500), ('BronzeLight', (0.17, 0.11, 0.06), -400),
    ('VerdDark', (0.07, 0.13, 0.10), -300), ('VerdLight', (0.13, 0.21, 0.16), -200))


def make_bronze():
    # Never clear a material a loaded mesh uses (delete_all_material_expressions killed this editor once): to change the
    # graph, delete the asset first (unassign it, or let assign() swap in a new name), then rerun.
    path = f'{HOME}/{BRONZE}'
    if unreal.load_asset(path) is not None:
        print('bronze exists')
        return
    mat = ASSET_TOOLS.create_asset(BRONZE, HOME, unreal.Material, unreal.MaterialFactoryNew())
    pos = EDIT.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, -100)
    tp = EDIT.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -900, -100)
    tp.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    tp.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    EDIT.connect_material_expressions(pos, '', tp, '')
    nrm = EDIT.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1100, 100)
    tn = EDIT.create_material_expression(mat, unreal.MaterialExpressionTransform, -900, 100)
    tn.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    tn.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    EDIT.connect_material_expressions(nrm, '', tn, '')
    tex = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureObject, -900, 300)
    tex.set_editor_property('texture', unreal.load_asset(NOISE))
    tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    cu = EDIT.create_material_expression(mat, unreal.MaterialExpressionCustom, -600, 0)
    cu.set_editor_property('code', BRONZE_HLSL)
    cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    cu.set_editor_property('description', 'BG bronze patina')
    names = ['P', 'Nrm', 'Tex'] + [p[0] for p in BRONZE_PARAMS] + ['PatinaAmount']
    pins = []
    for pin in names:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', pin)
        pins.append(item)
    cu.set_editor_property('inputs', pins)
    assert EDIT.connect_material_expressions(tp, '', cu, 'P')
    assert EDIT.connect_material_expressions(tn, '', cu, 'Nrm')
    assert EDIT.connect_material_expressions(tex, '', cu, 'Tex')
    for pname, rgb, y in BRONZE_PARAMS:
        vp = EDIT.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, y)
        vp.set_editor_property('parameter_name', pname)
        vp.set_editor_property('group', 'Bronze')
        vp.set_editor_property('default_value', unreal.LinearColor(*rgb, 1.0))
        assert EDIT.connect_material_expressions(vp, '', cu, pname)
    pa = EDIT.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, -100)
    pa.set_editor_property('parameter_name', 'PatinaAmount')
    pa.set_editor_property('group', 'Bronze')
    pa.set_editor_property('default_value', 0.85)
    assert EDIT.connect_material_expressions(pa, '', cu, 'PatinaAmount')
    rgb = EDIT.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, -100)
    for k, v in (('r', True), ('g', True), ('b', True), ('a', False)):
        rgb.set_editor_property(k, v)
    pat = EDIT.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, 100)
    for k, v in (('r', False), ('g', False), ('b', False), ('a', True)):
        pat.set_editor_property(k, v)
    EDIT.connect_material_expressions(cu, '', rgb, '')
    EDIT.connect_material_expressions(cu, '', pat, '')
    metal = EDIT.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -100, 100)
    metal.set_editor_property('const_a', 0.85)
    metal.set_editor_property('const_b', 0.1)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -100, 250)
    rough.set_editor_property('const_a', 0.38)
    rough.set_editor_property('const_b', 0.78)
    EDIT.connect_material_expressions(pat, '', metal, 'Alpha')
    EDIT.connect_material_expressions(pat, '', rough, 'Alpha')
    EDIT.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.recompile_material(mat)
    save(mat)
    print('bronze made')


def make_rebar():
    """DBK_Rebar is built for DBK's own rebar meshes and all but vanishes on the gate's 6 cm tubes, so the bars get a
    rust instance of the patina material: dark mill-scale steel, rust gathering where the patina would."""
    instance('MI_BG_RebarRust', unreal.load_asset(f'{HOME}/{BRONZE}'), {'PatinaAmount': 0.9}, tint=None)
    mi = unreal.load_asset(f'{HOME}/MI_BG_RebarRust')
    for name, rgb in (('BronzeDark', (0.035, 0.032, 0.03)), ('BronzeLight', (0.08, 0.065, 0.05)),
                      ('VerdDark', (0.13, 0.05, 0.02)), ('VerdLight', (0.28, 0.11, 0.045))):
        EDIT.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(*rgb, 1.0))
    EDIT.update_material_instance(mi)
    save(mi)
    print('rebar rust made')


def assign(fallback=False):
    mesh = unreal.load_asset(f'{HOME}/{MESH}')
    stone = 'MI_BG_Sandstone_BF' if fallback else 'MI_BG_Sandstone'
    by_slot = {'M_BG_Sandstone': f'{HOME}/{stone}', 'M_BG_SandstoneBroken': f'{HOME}/{stone}' if fallback else f'{HOME}/MI_BG_SandstoneBroken',
               'M_BG_Bronze': f'{HOME}/{BRONZE}', 'DBK_Rebar': REBAR}
    for i, s in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(s.get_editor_property('material_slot_name'))
        key = next((k for k in by_slot if slot.startswith(k) and not (k == 'M_BG_Sandstone' and slot.startswith('M_BG_SandstoneBroken'))), None)
        if key is None:
            raise RuntimeError('unexpected slot ' + slot)
        mesh.set_material(i, unreal.load_asset(by_slot[key]))
        print('slot', i, slot, '->', by_slot[key].rsplit('/', 1)[1])
    save(mesh)
    old = f'{HOME}/M_BG_Bronze'   # first-pass bronze (fixed colours), replaced by M_BG_BronzePatina
    if unreal.EditorAssetLibrary.does_asset_exist(old) and not unreal.EditorAssetLibrary.find_package_referencers_for_asset(old):
        unreal.EditorAssetLibrary.delete_asset(old)
        print('deleted', old)


if 'maps' in STAGES:
    for n in ('T_BG_BrandenburgGate_Curv', 'T_BG_BrandenburgGate_Fields'):
        import_map(n)
if 'mesh' in STAGES:
    import_mesh()
if 'master' in STAGES:
    make_master()
if 'instances' in STAGES:
    make_instances()
if 'bronze' in STAGES:
    make_bronze()
    make_rebar()
if 'assign' in STAGES:
    assign(globals().get('FALLBACK', False))
