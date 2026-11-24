"""Wall decals for the Reichstag ruin (run with the UECP execute_python action in the shared editor; asset work only).

Art: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/BerlinReichstagBase/Decals/gen_decals.py -> out/*.png.
Placement: rt_decal_plan.py -> RT_Decals.json (here, the editor's Python may only open files inside the project).

Makes, under /Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin/Decals:
- T_RT_RuinDecals_BCO / _NRS (4 x 4 atlas) and T_RT_Inscription_BCO / _NRS (DEM DEUTSCHEN VOLKE)
- M_RT_RuinDecal        deferred decal, colour + normal + roughness: NOT USED. In fresh (throwaway) editors it kept
                        drawing as default black boxes; relief is baked into the atlas colour instead
- M_RT_RuinDecal_Paint  deferred decal, colour + roughness only, so the stone keeps its relief: every decal
- MI_RT_RuinDecal, MI_RT_RuinDecal_Paint (atlas), MI_RT_Inscription (Grid 1)
and rebuilds the Decal_### components of BP_ReichstagRuin_B2 from the plan (old ones are removed first).

DecalColor contract: R = (cell + 0.5) / 16, G = per-decal variation, B = yaw of the outward face normal / 360 (the
material fades the decal where the receiver turns away from that face: window reveals, ledges, the ground),
A = opacity. The decal's X axis is the outward normal and its Y axis world up, so the texture reads upright.
    exec(open(r'<this file>').read())                     # textures, materials, decals
    RT_DECALS_ONLY = True; exec(...)                       # only rebuild the components (after a new plan)
"""
import json
import math

import unreal

ART = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/BerlinReichstagBase/Decals/out/'
PLAN = 'D:/UE5Projects/RTS_Survival/Source/Tools/ReichstagRuin/RT_Decals.json'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin'
FOLDER = ROOT + '/Decals'
BP_PATH = ROOT + '/BP_ReichstagRuin_B2'
PREFIX = 'Decal_'
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
SUBOBJECTS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary

# A wall decal (X = outward normal, Y = up) gets its V running up the wall, so the image is flipped here to read upright;
# the normal map's green (+ = image down) flips with it.
ATLAS_UV = """float G = max(Grid, 1.0);
float Cell = min(floor(saturate(CellR) * G * G), G * G - 1.0);
float Row = floor(Cell / G);
float2 Flip = float2(UV.x, 1.0 - UV.y);
return (float2(Cell - G * Row, Row) + clamp(Flip, 0.004, 0.996)) / G;"""
BASE_COLOR = """return Bco.rgb * Tint * lerp(0.88, 1.12, Var);"""
NORMAL = """float2 XY = (Nrs.rg * 2.0 - 1.0) * float2(1.0, -1.0) * NormalStrength;
return float3(XY, sqrt(saturate(1.0 - dot(XY, XY))));"""
ROUGH = """return saturate(Nrs.b * RoughnessMul);"""
# Receiver normal from screen derivatives (DBuffer decals can't read the GBuffer normal); the face normal is the
# decal's own X axis in world space (Local -> World transform, which is DecalToWorld in a decal), so the decal fades on
# window reveals, ledges and the ground at any actor rotation. (It used to be a BP-local yaw in DecalColor.B, which
# hid every decal once the ruin was rotated in Ger_Berlin.)
OPACITY = """float3 N = cross(ddx(Wp), ddy(Wp));
N *= rsqrt(max(dot(N, N), 1e-12));
float3 F = normalize(Face);
float Facing = abs(dot(N, F));
return saturate(BcoA * DecalA * Opacity) * smoothstep(FacingStart, FacingEnd, Facing);"""


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        print('SAVE FAILED', path)


def import_texture(name, srgb, compression):
    task = unreal.AssetImportTask()
    task.filename = ART + name + '.png'
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(f'{FOLDER}/{name}')
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('compression_settings', compression)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('virtual_texture_streaming', False)
    save(f'{FOLDER}/{name}')
    print('TEX', name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    return tex


def custom(mat, x, y, desc, code, out_type, inputs):
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property('description', desc)
    n.set_editor_property('code', code)
    n.set_editor_property('output_type', out_type)
    ins = []
    for nm in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', nm)
        ins.append(ci)
    n.set_editor_property('inputs', ins)
    return n


def param(mat, cls, name, value, x, y):
    n = MEL.create_material_expression(mat, cls, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    n.set_editor_property('group', 'RuinDecal')
    return n


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f'connect failed {a.get_name()}.{a_out} -> {b.get_name()}.{b_in}')


OPACITY_INPUTS = ['BcoA', 'DecalA', 'Opacity', 'Wp', 'Face', 'FacingStart', 'FacingEnd']


def face_axis(mat):
    """The decal's X axis (its projection direction = the wall's outward normal) in world space."""
    x = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -1250, 900)
    x.set_editor_property('constant', unreal.LinearColor(1, 0, 0, 0))
    t = MEL.create_material_expression(mat, unreal.MaterialExpressionTransform, -950, 900)
    t.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    t.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(x, '', t, '')
    return t


def upgrade_face_input():
    """Moves existing masters' RuinOpacity from the DecalColor.B yaw to the world-space decal axis."""
    for name in ('M_RT_RuinDecal', 'M_RT_RuinDecal_Paint'):
        mat = unreal.load_asset(f'{FOLDER}/{name}')
        if mat is None:
            continue
        op = [e for e in unreal.ObjectIterator(unreal.MaterialExpressionCustom)
              if e.get_outer() == mat and str(e.get_editor_property('description')) == 'RuinOpacity']
        if not op:
            continue
        op = op[0]
        names = [str(i.get_editor_property('input_name')) for i in op.get_editor_property('inputs')]
        nodes = [e for e in unreal.ObjectIterator(unreal.MaterialExpression) if e.get_outer() == mat]
        if 'Face' in names and any(isinstance(e, unreal.MaterialExpressionTransform) for e in nodes):
            print('face', name, 'already upgraded')
            continue
        for e in nodes:     # leftovers of an interrupted run
            if isinstance(e, (unreal.MaterialExpressionConstant3Vector, unreal.MaterialExpressionTransform)):
                MEL.delete_material_expression(mat, e)
        nodes = [e for e in unreal.ObjectIterator(unreal.MaterialExpression) if e.get_outer() == mat]
        def find(cls, pname=None):
            for e in nodes:
                if isinstance(e, cls) and (pname is None or str(e.get_editor_property('parameter_name')) == pname):
                    return e
            raise RuntimeError(f'{name}: no {cls.__name__} {pname or ""}')
        ins = []
        for nm in OPACITY_INPUTS:
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', nm)
            ins.append(ci)
        op.set_editor_property('inputs', ins)       # drops the old links; re-made below
        link(find(unreal.MaterialExpressionTextureSampleParameter2D, 'RuinBCO'), 'A', op, 'BcoA')
        link(find(unreal.MaterialExpressionDecalColor), 'A', op, 'DecalA')
        link(find(unreal.MaterialExpressionScalarParameter, 'Opacity'), '', op, 'Opacity')
        link(find(unreal.MaterialExpressionWorldPosition), '', op, 'Wp')
        link(find(unreal.MaterialExpressionScalarParameter, 'FacingStart'), '', op, 'FacingStart')
        link(find(unreal.MaterialExpressionScalarParameter, 'FacingEnd'), '', op, 'FacingEnd')
        op.set_editor_property('code', OPACITY)
        link(face_axis(mat), '', op, 'Face')
        MEL.recompile_material(mat)
        save(f'{FOLDER}/{name}')
        print('face', name, 'upgraded')


def build_master(name, with_normal, bco, nrs):
    path = f'{FOLDER}/{name}'
    if EAL.does_asset_exist(path):
        print('master exists', path)
        return unreal.load_asset(path)
    F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tc = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    dc = MEL.create_material_expression(mat, unreal.MaterialExpressionDecalColor, -1600, 250)
    grid = param(mat, unreal.MaterialExpressionScalarParameter, 'Grid', 4.0, -1600, -150)
    uv = custom(mat, -1250, 0, 'AtlasCellUV', ATLAS_UV, unreal.CustomMaterialOutputType.CMOT_FLOAT2, ['UV', 'CellR', 'Grid'])
    link(tc, '', uv, 'UV')
    link(dc, 'R', uv, 'CellR')
    link(grid, '', uv, 'Grid')
    s_bco = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -950, -200)
    s_bco.set_editor_property('parameter_name', 'RuinBCO')
    s_bco.set_editor_property('texture', bco)
    s_bco.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    s_nrs = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -950, 200)
    s_nrs.set_editor_property('parameter_name', 'RuinNRS')
    s_nrs.set_editor_property('texture', nrs)
    s_nrs.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(uv, '', s_bco, 'UVs')
    link(uv, '', s_nrs, 'UVs')
    tint = param(mat, unreal.MaterialExpressionVectorParameter, 'Tint', unreal.LinearColor(1, 1, 1, 1), -650, -450)
    opac = param(mat, unreal.MaterialExpressionScalarParameter, 'Opacity', 1.0, -650, 600)
    f0 = param(mat, unreal.MaterialExpressionScalarParameter, 'FacingStart', 0.55, -650, 700)
    f1 = param(mat, unreal.MaterialExpressionScalarParameter, 'FacingEnd', 0.85, -650, 800)
    rmul = param(mat, unreal.MaterialExpressionScalarParameter, 'RoughnessMul', 1.0, -650, 450)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -950, 750)
    wp.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    bc = custom(mat, -350, -300, 'RuinBaseColor', BASE_COLOR, F3, ['Bco', 'Var', 'Tint'])
    link(s_bco, 'RGBA', bc, 'Bco')
    link(dc, 'G', bc, 'Var')
    link(tint, '', bc, 'Tint')
    rg = custom(mat, -350, 350, 'RuinRoughness', ROUGH, F1, ['Nrs', 'RoughnessMul'])
    link(s_nrs, 'RGBA', rg, 'Nrs')
    link(rmul, '', rg, 'RoughnessMul')
    op = custom(mat, -350, 600, 'RuinOpacity', OPACITY, F1, OPACITY_INPUTS)
    link(s_bco, 'A', op, 'BcoA')
    link(dc, 'A', op, 'DecalA')
    link(face_axis(mat), '', op, 'Face')
    link(opac, '', op, 'Opacity')
    link(wp, '', op, 'Wp')
    link(f0, '', op, 'FacingStart')
    link(f1, '', op, 'FacingEnd')
    MEL.connect_material_property(bc, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rg, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
    if with_normal:
        nstr = param(mat, unreal.MaterialExpressionScalarParameter, 'NormalStrength', 1.0, -650, 250)
        nm = custom(mat, -350, 150, 'RuinNormal', NORMAL, F3, ['Nrs', 'NormalStrength'])
        link(s_nrs, 'RGBA', nm, 'Nrs')
        link(nstr, '', nm, 'NormalStrength')
        MEL.connect_material_property(nm, '', unreal.MaterialProperty.MP_NORMAL)
    MEL.recompile_material(mat)
    save(path)
    print('built', path)
    return mat


def update_code():
    """Brings existing masters' Custom nodes up to the code above (they are only built once)."""
    codes = {'AtlasCellUV': ATLAS_UV, 'RuinNormal': NORMAL, 'RuinBaseColor': BASE_COLOR, 'RuinRoughness': ROUGH, 'RuinOpacity': OPACITY}
    for name in ('M_RT_RuinDecal', 'M_RT_RuinDecal_Paint'):
        mat = unreal.load_asset(f'{FOLDER}/{name}')
        if mat is None:
            continue
        changed = 0
        for e in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
            if e.get_outer() == mat and str(e.get_editor_property('description')) in codes:
                code = codes[str(e.get_editor_property('description'))]
                if str(e.get_editor_property('code')) != code:
                    e.set_editor_property('code', code)
                    changed += 1
        if changed:
            MEL.recompile_material(mat)
            save(f'{FOLDER}/{name}')
        print('code', name, 'updated nodes', changed)


def instance(name, parent, textures=None, scalars=None):
    path = f'{FOLDER}/{name}'
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in (textures or {}).items():
        MEL.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    save(path)
    return mi


def materials():
    TC = unreal.TextureCompressionSettings
    bco = import_texture('T_RT_RuinDecals_BCO', True, TC.TC_DEFAULT)
    nrs = import_texture('T_RT_RuinDecals_NRS', False, TC.TC_MASKS)
    ibco = import_texture('T_RT_Inscription_BCO', True, TC.TC_DEFAULT)
    inrs = import_texture('T_RT_Inscription_NRS', False, TC.TC_MASKS)
    full = build_master('M_RT_RuinDecal', True, bco, nrs)
    paint = build_master('M_RT_RuinDecal_Paint', False, bco, nrs)
    upgrade_face_input()
    update_code()
    return {'relief': instance('MI_RT_RuinDecal', full),
            'paint': instance('MI_RT_RuinDecal_Paint', paint),
            'inscription': instance('MI_RT_Inscription', paint, {'RuinBCO': ibco, 'RuinNRS': inrs}, {'Grid': 1.0})}


def load_materials():
    return {'relief': unreal.load_asset(f'{FOLDER}/MI_RT_RuinDecal'), 'paint': unreal.load_asset(f'{FOLDER}/MI_RT_RuinDecal_Paint'),
            'inscription': unreal.load_asset(f'{FOLDER}/MI_RT_Inscription')}


def decals(mats):
    plan = json.loads(open(PLAN, encoding='utf-8').read())['decals']
    bp = unreal.load_asset(BP_PATH)
    handles = SUBOBJECTS.k2_gather_subobject_data_for_blueprint(bp)
    old = [h for h in handles if str(LIB.get_variable_name(LIB.get_data(h))).startswith(PREFIX)]
    if old:
        SUBOBJECTS.delete_subobjects(handles[0], old, bp)
        print('removed', len(old), 'old decals')
    root = SUBOBJECTS.k2_gather_subobject_data_for_blueprint(bp)[0]
    up = unreal.Vector(0, 0, 1)
    for i, d in enumerate(plan):
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.DecalComponent, blueprint_context=bp)
        handle, fail = SUBOBJECTS.add_new_subobject(params)
        if not fail.is_empty():
            raise RuntimeError(f'add decal {i}: {fail}')
        SUBOBJECTS.rename_subobject(handle, unreal.Text(f'{PREFIX}{i:03d}'))
        comp = LIB.get_object_for_blueprint(LIB.get_data(handle), bp)
        nx, ny = d['normal']
        rot = unreal.MathLibrary.make_rot_from_xy(unreal.Vector(nx, ny, 0), up)
        cell = 0 if d['cell'] == 'inscription' else d['cell']
        yaw01 = (math.degrees(math.atan2(ny, nx)) % 360.0) / 360.0
        comp.set_editor_property('decal_material', mats[d['material']])
        comp.set_editor_property('decal_size', unreal.Vector(d['depth_cm'], d['height_cm'] / 2, d['width_cm'] / 2))
        comp.set_editor_property('relative_location', unreal.Vector(*d['location_cm']))
        comp.set_editor_property('relative_rotation', rot)
        comp.set_editor_property('decal_color', unreal.LinearColor((cell + 0.5) / 16.0, d['var'], yaw01, d['opacity']))
        comp.set_editor_property('sort_order', d['sort'])
        comp.set_editor_property('fade_screen_size', 0.0)    # the 0.01 default hides small decals at RTS zoom
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not globals().get('RT_DECALS_NOSAVE'):      # the throwaway capture run builds them in memory only
        save(BP_PATH)
    print('DECALS', len(plan), 'on', BP_PATH)


if globals().get('RT_DECALS_UPGRADE_ONLY'):    # existing masters -> world-space face axis, no decal rebuild
    upgrade_face_input()
elif globals().get('RT_DECALS_CODE_ONLY'):
    update_code()
else:
    mats = load_materials() if globals().get('RT_DECALS_ONLY') else materials()
    decals(mats)
