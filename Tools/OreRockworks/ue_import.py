"""Ore Mountains rockworks: textures, materials, meshes and debris into Unreal, headless (commandlet).
Copied from DesertRocks/ue_import.py (the masked hole variant, matte_inside) with the look and values of
OreCrushableRocks/ue_import.py (M_OreCrushRock, which follows the cliff kit's M_OreCliff), so the rockworks read as the
same gneiss as the cliff kit and the crushable rocks.

  bash run_ue.sh ue_import.py import            (everything; rerun ue_blueprints.py afterwards)
  bash run_ue.sh "ue_import.py material" mat    (textures + materials only)

Creates, only under /Game/RTS_Survival/Blueprints/Environment/OreMountains/Rockworks:
  Textures/T_OreRockwork_BC, _N, _ORM        4096 atlas on a 1024 grid (two 2048 cells, six 1024 cells; compose_atlas.py)
  Materials/M_OreRockwork, MI_OreRockwork    opaque look (the M_OreCrushRock Custom-node shading and values)
  Materials/M_OreRockwork_WallHoles          masked copy by Tools/WallChunks/make_wall_holes_material.masked_master_for
  Materials/MI_OreRockwork_WallHoles         NoGrid, two-sided, back faces tinted toward fresh broken gneiss
  Meshes/SM_OreRockwork_R<NN>_<Kind>         UCX hulls (one per block), 2 LODs, Nanite off, slot -> MI_OreRockwork
  Meshes/SM_OreRockwork_Debris_0..5          hole chips and death burst, no collision
Writes Saved/OreMountains/rockworks/ue_import_report.json. Opens no map.
"""
import json
import os
import sys

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks')
import make_wall_holes_material as WH  # noqa: E402

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreRockworks/out'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Rockworks'
TEX = HOME + '/Textures'
MESH = HOME + '/Meshes'
MATS = HOME + '/Materials'
RVT_ENV = '/Game/RTS_Survival/Meshes/RVTs/RVT_Environment'
RVT_HEIGHT = '/Game/RTS_Survival/Meshes/RVTs/RVT_Height'
NOISE = '/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/rockworks/ue_import_report.json'
GROUP = 'Ore Rockwork'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
mel = unreal.MaterialEditingLibrary
failures = []
report = dict(textures={}, material={}, meshes={}, debris={}, failures=failures)

SHADE = r"""
// Ore rockwork: the cliff kit's look (M_OreCliff) on a baked atlas, as M_OreCrushRock.
float hsh = frac(sin(dot(OP.xy * 0.01, float2(12.9898, 78.233))) * 43758.5453);
float hsh2 = frac(hsh * 7.13 + 0.37);
float3 hue = lerp(HueCool, HueWarm, hsh2);
float val = 1.0 + (hsh - 0.5) * ValueJitter;
float cav = lerp(1.0, ORM.r, CavityStrength);
float3 rock = BC.rgb * RockTint * hue * val * lerp(1.0 - MacroAmount, 1.0 + MacroAmount, Macro.r) * lerp(0.45, 1.0, cav);
// dust lies on up-facing rock, first in the hollows of the cliff texture (its height, ORM.b)
float3 wn = normalize(WN);
float up = smoothstep(0.70, 0.96, wn.z);
float dust = saturate(up * DustAmount * (1.25 - ORM.b * 0.7) * lerp(0.75, 1.15, Fine.g));
rock = lerp(rock, DustColor, dust);
// the foot takes the landscape colour within ContactHeight cm of the RVT ground height
float3 ground = lerp(GroundColor, Ground, GroundFromRVT);
float above = WP.z - GroundZ + (Fine.r - 0.5) * 8.0;
float contact = (1.0 - saturate(above / max(ContactHeight, 1.0))) * ContactStrength;
rock = lerp(rock, ground, contact);
float3 nts = normalize(lerp(float3(0, 0, 1), NTS, NormalStrength));
Normal = normalize(lerp(nts, float3(0, 0, 1), max(contact, dust * 0.6)));
Roughness = lerp(ORM.g, 0.95, max(dust, contact));
AO = lerp(lerp(1.0, ORM.r, AOStrength), 1.0, contact * 0.7);
return rock;
"""

# M_OreCrushRock values (= MI_OreCliff RockTint / dust / ground); ContactHeight 15 cm instead of 10 (bigger rocks)
SCALARS = [('ValueJitter', 0.16), ('CavityStrength', 0.85), ('MacroAmount', 0.16), ('MacroWorldSize', 5200.0),
           ('DustAmount', 0.75), ('GroundFromRVT', 0.45), ('ContactHeight', 15.0), ('ContactStrength', 0.85),
           ('NormalStrength', 0.65), ('AOStrength', 1.0)]
VECTORS = [('RockTint', (0.74, 0.67, 0.58)), ('DustColor', (0.47, 0.41, 0.31)), ('GroundColor', (0.43, 0.37, 0.27)),
           ('HueCool', (0.96, 0.98, 1.03)), ('HueWarm', (1.04, 1.0, 0.95))]
HOLES_MI = 'MI_OreRockwork_WallHoles'
HOLES_INNER_DARKEN = 0.55
# fresh broken gneiss (linear): greyer and a little darker than the weathered tan surface (about (0.22, 0.20, 0.17))
HOLES_INNER_COLOR = (0.15, 0.14, 0.125)
HOLES_INNER_FILL = 0.25      # emissive multiplier on back faces (the inside of a hole); 0 = the desert behaviour


def save(obj):
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        failures.append('save failed ' + obj.get_path_name())


def import_texture(name, kind):
    task = unreal.AssetImportTask()
    task.filename = f'{SRC}/{name}.png'
    task.destination_path = TEX
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{TEX}/{name}')
    if kind == 'bc':
        tex.set_editor_property('srgb', True)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    elif kind == 'n':
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel', False)
    else:
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    save(tex)
    report['textures'][name] = [tex.blueprint_get_size_x(), tex.blueprint_get_size_y()]
    return tex


def N(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, getattr(unreal, 'MaterialExpression' + cls), x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def C(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        failures.append('%s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


def build_material(bc, nr, orm):
    path = MATS + '/M_OreRockwork'
    mat = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if mat is None:
        mat = TOOLS.create_asset('M_OreRockwork', MATS, unreal.Material, unreal.MaterialFactoryNew())
    else:
        for _ in range(8):
            mel.delete_all_material_expressions(mat)
            if mel.get_num_material_expressions(mat) == 0:
                break
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('used_with_niagara_mesh_particles', True)
    mat.set_editor_property('used_with_static_lighting', False)
    mat.set_editor_property('two_sided', False)
    T = unreal.MaterialSamplerType
    y = [-900]

    def S(name, value):
        y[0] += 80
        return N(mat, 'ScalarParameter', -1500, y[0], parameter_name=name, default_value=value, group=GROUP)

    def V(name, rgb):
        y[0] += 80
        return N(mat, 'VectorParameter', -1500, y[0], parameter_name=name,
                 default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0), group=GROUP)

    t_bc = N(mat, 'TextureSampleParameter2D', -1900, -900, parameter_name='Atlas_BC', texture=bc,
             sampler_type=T.SAMPLERTYPE_COLOR, group=GROUP)
    t_n = N(mat, 'TextureSampleParameter2D', -1900, -650, parameter_name='Atlas_N', texture=nr,
            sampler_type=T.SAMPLERTYPE_NORMAL, group=GROUP)
    t_orm = N(mat, 'TextureSampleParameter2D', -1900, -400, parameter_name='Atlas_ORM', texture=orm,
              sampler_type=T.SAMPLERTYPE_MASKS, group=GROUP)
    to_world = N(mat, 'Transform', -1650, -600,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    C(t_n, 'RGB', to_world, '')
    wp = N(mat, 'WorldPosition', -2300, 0)
    op = N(mat, 'ObjectPositionWS', -2300, -150)
    wxy = N(mat, 'ComponentMask', -2150, 100, r=True, g=True, b=False, a=False)
    C(wp, '', wxy, '')
    msize = S('MacroWorldSize', 5200.0)
    muv = N(mat, 'Divide', -2000, 100)
    C(wxy, '', muv, 'A')
    C(msize, '', muv, 'B')
    macro = N(mat, 'TextureSample', -1850, 100, texture=unreal.load_asset(NOISE), sampler_type=T.SAMPLERTYPE_MASKS)
    C(muv, '', macro, 'UVs')
    fuv = N(mat, 'Divide', -2000, 250, const_b=60.0)
    C(wxy, '', fuv, 'A')
    fine = N(mat, 'TextureSample', -1850, 250, texture=unreal.load_asset(NOISE), sampler_type=T.SAMPLERTYPE_MASKS)
    C(fuv, '', fine, 'UVs')
    renv = N(mat, 'RuntimeVirtualTextureSample', -1900, 450, virtual_texture=unreal.load_asset(RVT_ENV),
             material_type=unreal.RuntimeVirtualTextureMaterialType.BASE_COLOR_NORMAL_SPECULAR)
    rh = N(mat, 'RuntimeVirtualTextureSample', -1900, 700, virtual_texture=unreal.load_asset(RVT_HEIGHT),
           material_type=unreal.RuntimeVirtualTextureMaterialType.WORLD_HEIGHT)

    custom = N(mat, 'Custom', -900, -200, code=SHADE, description='OreRockworkShade',
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    spec = [('BC', t_bc, 'RGB'), ('NTS', t_n, 'RGB'), ('ORM', t_orm, 'RGBA'), ('WN', to_world, ''), ('WP', wp, ''),
            ('OP', op, ''), ('Macro', macro, 'RGBA'), ('Fine', fine, 'RGBA'), ('Ground', renv, 'BaseColor'),
            ('GroundZ', rh, 'WorldHeight')]
    for name, value in SCALARS:
        spec.append((name.replace('_', ''), msize if name == 'MacroWorldSize' else S(name, value), ''))
    for name, rgb in VECTORS:
        spec.append((name, V(name, rgb), ''))
    spec = [s for s in spec if s[0] != 'MacroWorldSize']
    ins = []
    for name, _, _ in spec:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', name)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    outs = []
    for name, kind in (('Normal', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                       ('Roughness', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                       ('AO', unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', name)
        co.set_editor_property('output_type', kind)
        outs.append(co)
    custom.set_editor_property('additional_outputs', outs)
    for name, node, out in spec:
        C(node, out, custom, name)
    for prop, out in ((unreal.MaterialProperty.MP_BASE_COLOR, ''), (unreal.MaterialProperty.MP_NORMAL, 'Normal'),
                      (unreal.MaterialProperty.MP_ROUGHNESS, 'Roughness'),
                      (unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'AO')):
        if not mel.connect_material_property(custom, out, prop):
            failures.append('custom.%s -> %s' % (out, prop))
    mel.connect_material_property(N(mat, 'Constant', -500, 300, r=0.3), '', unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    save(mat)
    mi_path = MATS + '/MI_OreRockwork'
    mi = unreal.load_asset(mi_path) if EAL.does_asset_exist(mi_path) else None
    if mi is None:
        mi = TOOLS.create_asset('MI_OreRockwork', MATS, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    mel.update_material_instance(mi)
    save(mi)
    try:
        st = mel.get_statistics(mat)
        report['material'] = dict(pixel_shader_instructions=st.num_pixel_shader_instructions,
                                  vertex_shader_instructions=st.num_vertex_shader_instructions,
                                  samplers=st.num_samplers, pixel_texture_samples=st.num_pixel_texture_samples)
        if st.num_pixel_shader_instructions == 0:
            # normal in a commandlet (no shader map compiled); rock_engine_test.py reads the real numbers
            report['material']['note'] = 'statistics 0 in the commandlet: check them in the throwaway editor'
    except Exception as exc:  # noqa: BLE001
        report['material'] = dict(error=str(exc))
    report['material']['expressions'] = mel.get_num_material_expressions(mat)
    return mi


def fbx_task(name, collision):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    # build_rocks.py exports metres with FBX_SCALE_UNITS, which lands 100x small here
    data.set_editor_property('import_uniform_scale', 100.0)
    task = unreal.AssetImportTask()
    task.filename = f'{SRC}/fbx/{name}.fbx'
    task.destination_path = MESH
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    return task


def import_mesh(name, mi, want_dims_m, lods, distance_field, collision=True):
    path = f'{MESH}/{name}'
    if EAL.does_asset_exist(path):
        # a reimport keeps the old import settings: delete and import fresh
        EAL.delete_asset(path)
    TOOLS.import_asset_tasks([fbx_task(name, collision)])
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        failures.append('import failed ' + name)
        return None
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    if not collision:
        SMS.remove_collisions(mesh)
    try:
        bs = SMS.get_lod_build_settings(mesh, 0)
        bs.set_editor_property('generate_lightmap_u_vs', False)
        bs.set_editor_property('recompute_normals', False)
        bs.set_editor_property('recompute_tangents', False)
        if not distance_field:
            bs.set_editor_property('distance_field_resolution_scale', 0.0)
        SMS.set_lod_build_settings(mesh, 0, bs)
    except Exception as exc:  # noqa: BLE001
        failures.append(f'{name} build settings: {exc}')
    if lods:
        opts = unreal.StaticMeshReductionOptions()
        opts.auto_compute_lod_screen_size = False
        red = []
        for pct, screen in lods:
            r = unreal.StaticMeshReductionSettings()
            r.percent_triangles = pct
            r.screen_size = screen
            red.append(r)
        opts.reduction_settings = red
        SMS.set_lods(mesh, opts)
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, mi)
    save(mesh)
    box = mesh.get_bounding_box()
    dims = [box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z]
    err = max(abs(dims[i] - want_dims_m[i] * 100.0) for i in range(3))
    body = mesh.get_editor_property('body_setup')
    geom = body.get_editor_property('agg_geom') if body else None
    hulls = len(geom.get_editor_property('convex_elems')) if geom else 0
    row = dict(dims_cm=[round(d, 1) for d in dims], bounds_min_z=round(box.min.z, 1), dims_error_cm=round(err, 1),
               lod_triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())], convex_hulls=hulls,
               material=mesh.get_material(0).get_name())
    if err > 2.0:
        failures.append(f'{name}: bounds differ from Blender by {err:.1f} cm (scale or axis wrong)')
    if collision and abs(box.min.z) > 1.0:          # debris pivot is its centre on purpose
        failures.append(f'{name}: pivot not at the base (min z {box.min.z:.1f})')
    return row


def matte_inside(m):
    """The inside of a hole is the rock's back faces (two-sided). With the rock's roughness, specular and baked normal
    they read glossy and wet up close; back faces get roughness 1, specular 0 and a flat normal (matte broken stone)."""
    PROP = unreal.MaterialProperty
    # The holes library feeds its rim lerp from the roughness node's default output; on M_OreRockwork that node is the
    # Custom node, whose default output is the colour, so the damaged rock went glossy. Wire its Roughness output.
    custom = mel.get_material_property_input_node(m, PROP.MP_NORMAL)
    rim = mel.get_material_property_input_node(m, PROP.MP_ROUGHNESS)
    if not mel.connect_material_expressions(custom, 'Roughness', rim, 'A'):
        raise RuntimeError('matte_inside: could not wire the Custom Roughness output into the rim lerp')
    sign = mel.create_material_expression(m, unreal.MaterialExpressionTwoSidedSign, 400, -1100)
    flip = mel.create_material_expression(m, unreal.MaterialExpressionOneMinus, 550, -1100)
    back = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, 700, -1100)
    back.set_editor_property('const_b', 0.5)
    ok = [mel.connect_material_expressions(sign, '', flip, ''), mel.connect_material_expressions(flip, '', back, 'A')]
    for prop, value, y in ((PROP.MP_ROUGHNESS, 1.0, -1250), (PROP.MP_SPECULAR, 0.0, -1350)):
        src = mel.get_material_property_input_node(m, prop)
        lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, 900, y)
        lerp.set_editor_property('const_b', value)
        out = ''  # roughness: the holes library's rim lerp; specular: a constant
        if src is not None:
            ok.append(mel.connect_material_expressions(src, out, lerp, 'A'))
        ok += [mel.connect_material_expressions(back, '', lerp, 'Alpha'), mel.connect_material_property(lerp, '', prop)]
    nsrc = mel.get_material_property_input_node(m, PROP.MP_NORMAL)
    flat = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, 750, -1450)
    flat.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    nlerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, 900, -1450)
    ok += [mel.connect_material_expressions(nsrc, 'Normal', nlerp, 'A'),
           mel.connect_material_expressions(flat, '', nlerp, 'B'),
           mel.connect_material_expressions(back, '', nlerp, 'Alpha'),
           mel.connect_material_property(nlerp, '', PROP.MP_NORMAL)]
    # Inner fill (rockworks only): a hole into a 3-4 m solid block looks into back faces that the rock itself shadows
    # completely, so it read as a black void in the first fire test. Back faces emit WallHoleInnerFillColor x
    # WallHoleInnerFill: dim broken stone instead of a void (front faces emit nothing).
    fill_color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, 700, -1650)
    fill_color.set_editor_property('parameter_name', 'WallHoleInnerFillColor')
    fill_color.set_editor_property('default_value', unreal.LinearColor(*HOLES_INNER_COLOR, 1.0))
    fill_amount = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, 700, -1550)
    fill_amount.set_editor_property('parameter_name', 'WallHoleInnerFill')
    fill_amount.set_editor_property('default_value', HOLES_INNER_FILL)
    fill_rgb = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, 850, -1650)
    for channel in 'rgba':
        fill_rgb.set_editor_property(channel, channel in 'rgb')
    fill_a = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, 1000, -1600)
    fill_b = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, 1150, -1600)
    if mel.get_material_property_input_node(m, PROP.MP_EMISSIVE_COLOR) is not None:
        raise RuntimeError('matte_inside: emissive already connected; the inner fill would replace it')
    ok += [mel.connect_material_expressions(fill_color, '', fill_rgb, ''),
           mel.connect_material_expressions(fill_rgb, '', fill_a, 'A'),
           mel.connect_material_expressions(fill_amount, '', fill_a, 'B'),
           mel.connect_material_expressions(fill_a, '', fill_b, 'A'),
           mel.connect_material_expressions(back, '', fill_b, 'B'),
           mel.connect_material_property(fill_b, '', PROP.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('matte_inside: %d failed links' % ok.count(False))
    mel.recompile_material(m)
    EAL.save_asset(m.get_path_name())
    report['holes_matte_inside'] = True


def build_holes(any_mesh):
    """The masked hole variant by the shared library: rebuilt from the current M_OreRockwork every run."""
    master = MATS + '/M_OreRockwork' + WH.SUFFIX
    if EAL.does_asset_exist(master) and not EAL.delete_asset(master):
        raise RuntimeError('could not delete the stale ' + master)
    mi = WH.make_holes_instance(MATS + '/MI_OreRockwork', MATS, any_mesh, grid_axis=None, two_sided=True, name=HOLES_MI,
                                inner_tint=True, inner_darken=HOLES_INNER_DARKEN, inner_color=HOLES_INNER_COLOR)
    m = unreal.load_asset(master)
    matte_inside(m)
    names = [str(n) for n in mel.get_scalar_parameter_names(m)]
    report['holes'] = dict(master=m.get_path_name(), blend=str(m.get_editor_property('blend_mode')),
                           has_hole_params='WallHole0R' in names and 'WallHoleInnerDarken' in names,
                           instance=mi.get_path_name(), parent=mi.get_editor_property('parent').get_name(),
                           two_sided=mi.get_editor_property('base_property_overrides').get_editor_property('two_sided'))
    return mi


def main():
    # 'material' as script argument: textures + material only (meshes and Blueprints untouched)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    rocks = json.load(open(SRC + '/rocks.json'))
    bc = import_texture('T_OreRockwork_BC', 'bc')
    nr = import_texture('T_OreRockwork_N', 'n')
    orm = import_texture('T_OreRockwork_ORM', 'orm')
    mi = build_material(bc, nr, orm)
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name, info in sorted(rocks.items(), key=lambda kv: kv[1]['index']):
            if 'material' in args:
                break
            asset = 'SM_OreRockwork_' + name
            row = import_mesh(asset, mi, info['dims'], [(1.0, 1.0), (0.5, 0.25)], True)
            if row is None:
                continue
            if row['convex_hulls'] != len(info['hulls']):
                failures.append(f"{asset}: {row['convex_hulls']} hulls, expected {len(info['hulls'])}")
            report['meshes'][asset] = row
            for deb in info.get('debris', []):
                dname = 'SM_OreRockwork_' + deb['name']
                drow = import_mesh(dname, mi, deb['dims'], None, False, collision=False)
                if drow is not None:
                    report['debris'][dname] = drow
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    for a in EAL.list_assets(MESH, recursive=False):
        if isinstance(unreal.load_asset(a), unreal.MaterialInterface):
            EAL.delete_asset(a)
    meshes = [unreal.load_asset(a) for a in EAL.list_assets(MESH, recursive=False)]
    meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh) and 'Debris' not in m.get_name()]
    if meshes:
        try:
            build_holes(meshes[0])
        except Exception as exc:  # noqa: BLE001
            failures.append('holes: %r' % exc)
    else:
        failures.append('holes: no rock mesh to read bounds from')
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT.replace('.json', '_material.json') if 'material' in args else REPORT, 'w') as f:
        json.dump(report, f, indent=1)
    unreal.log_warning('ORW_IMPORT done: %d meshes, %d debris, material %s, %d failures' % (
        len(report['meshes']), len(report['debris']), json.dumps(report['material']), len(failures)))
    for line in failures:
        unreal.log_warning('ORW_IMPORT failure: ' + line)


main()
