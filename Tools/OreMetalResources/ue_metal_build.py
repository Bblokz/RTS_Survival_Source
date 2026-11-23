"""Builds the Ore Mountains metal resources in Unreal (headless commandlet, works with -nullrhi).

    powershell -File Source/Tools/OreMineralFlora/trees/run_ue_cmd.ps1 -Script <this file> -ScriptArgs "Blutbarren Gelbguss" -Tag ore_metal_build -NullRhi

Without arguments every resource is built. Per resource: material instance, the intact and the fractured static mesh
(FBX from D:/Blender/.../OreMetalResources/Models/fbx), the geometry collection (RTS.FarmBuildings.BuildCollection +
RTS.Apartment.FlattenGeometryCollection, then the stock metal thresholds), and the Blueprint: a duplicate of
Resource_Metal_Barrels whose construction-script literals are retargeted with RTS.Blueprint.SetPinDefaults.
Only assets under the two OreMountains folders are written. The stock assets are read, never saved.
Result: Saved/OreMountains/metal/build_result.json
"""
import json
import os
import sys

import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
R = '/Game/RTS_Survival/Blueprints/Resources/'
HOME = R + 'Metal/OreMountains/'
BP_HOME = R + 'ChildBps/Metal/OreMountains/'
STOCK_MAT = R + 'Metal/Meshes/Materials/M_RTS_MetalResource'
STOCK_BP = R + 'ChildBps/Metal/Resource_Metal_Barrels'
ORE_MAT = HOME + 'Materials/M_RTS_MetalResource_Ore'
NOISE = '/Game/RTS_Survival/Textures/Noise/T_RTSNoise_MacroVariation'
FBX = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreMetalResources/Models/fbx/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/metal/'
# Row Metalbarrels of DT_ResourceSettings: base capacity 800, 50 per harvest. Amount = ceil(800 * WorthMlt) * random scale
# (1.00-1.15), rounded to 5.
BASE_CAPACITY = 800
# name: tint, crust colour A, crust colour B, WorthMlt. The tint multiplies the stock base colour, which averages 0.52
# (T_Metal_Aluminum_D, linear, times a macro variation of about 1), so tint = wanted base colour / 0.52.
RESOURCES = {
    'Blutbarren': dict(tint=(1.42, 0.153, 0.075), crust_a=(0.42, 0.045, 0.015), crust_b=(0.10, 0.028, 0.018), worth=1.875),
    'Glaskopf': dict(tint=(0.96, 0.134, 0.075), crust_a=(0.22, 0.028, 0.012), crust_b=(0.035, 0.009, 0.006), worth=1.5),
    'Katzengold': dict(tint=(1.83, 1.23, 0.38), crust_a=(0.26, 0.12, 0.03), crust_b=(0.08, 0.035, 0.014), worth=1.5),
    'Gelbguss': dict(tint=(1.6, 0.98, 0.17), crust_a=(0.3, 0.14, 0.03), crust_b=(0.13, 0.06, 0.02), worth=1.25),
}
ONLY = [a for a in sys.argv[1:] if a in RESOURCES]
REPORT = json.load(open(FBX + '../build_report.json'))
RESULT = {}


def log(msg):
    unreal.log('FLORA_METAL ' + msg)


def own(path):
    """Refuse to touch anything outside the two new folders."""
    if not (path.startswith(HOME) or path.startswith(BP_HOME)):
        raise RuntimeError('not an Ore metal asset: ' + path)
    return path


def save(path):
    return EAL.save_asset(own(path), only_if_is_dirty=False)


# ------------------------------------------------------------------------------------------------ material
def expr(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError('link failed %s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


def build_material():
    """Copy of the stock material: its base colour, metallic and roughness chains stay as they are and feed new nodes."""
    if EAL.does_asset_exist(ORE_MAT):
        log('material exists, kept: ' + ORE_MAT)
        mat = unreal.load_asset(ORE_MAT)
        patch_crust_detail(mat)
        return mat
    mat = EAL.duplicate_asset(STOCK_MAT, own(ORE_MAT))
    P = unreal.MaterialProperty
    stock = {}
    for key, prop in (('base', P.MP_BASE_COLOR), ('metallic', P.MP_METALLIC), ('rough', P.MP_ROUGHNESS)):
        node = MEL.get_material_property_input_node(mat, prop)
        stock[key] = (node, MEL.get_material_property_input_node_output_name(mat, prop))
        if node is None:
            raise RuntimeError('stock material has no node on ' + key)
    X = 600
    vc = expr(mat, unreal.MaterialExpressionVertexColor, X, 900)
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, X, 1150)
    tiling = expr(mat, unreal.MaterialExpressionScalarParameter, X, 1250, parameter_name='CrustNoiseTiling', default_value=2.5,
                  group='OreCrust')
    uv_mul = expr(mat, unreal.MaterialExpressionMultiply, X + 250, 1150)
    link(uv, '', uv_mul, 'A')
    link(tiling, '', uv_mul, 'B')
    # the stock graph samples the same noise texture; take its sampler type so the new sample compiles the same way
    stock_sample = unreal.load_object(None, '%s.%s:MaterialExpressionTextureSample_49' % (ORE_MAT, ORE_MAT.split('/')[-1]))
    noise_tex = unreal.load_asset(NOISE)
    if stock_sample is not None and stock_sample.get_editor_property('texture') == noise_tex:
        sampler_type = stock_sample.get_editor_property('sampler_type')
    else:
        srgb = noise_tex.get_editor_property('srgb')
        gray = noise_tex.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_GRAYSCALE
        ST = unreal.MaterialSamplerType
        sampler_type = (ST.SAMPLERTYPE_GRAYSCALE if srgb else ST.SAMPLERTYPE_LINEAR_GRAYSCALE) if gray else \
            (ST.SAMPLERTYPE_COLOR if srgb else ST.SAMPLERTYPE_LINEAR_COLOR)
    log('noise sampler type %s (from stock sample: %s)' % (sampler_type, stock_sample is not None))
    noise = expr(mat, unreal.MaterialExpressionTextureSample, X + 450, 1150, texture=noise_tex, sampler_type=sampler_type,
                 sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    link(uv_mul, '', noise, 'UVs')
    # mask = saturate((G * (1 + N) - 0.5) * CrustSharpness) * CrustAmount
    one_plus = expr(mat, unreal.MaterialExpressionAdd, X + 700, 1100, const_b=1.0)
    link(noise, 'R', one_plus, 'A')
    g_mul = expr(mat, unreal.MaterialExpressionMultiply, X + 900, 950)
    link(vc, 'G', g_mul, 'A')
    link(one_plus, '', g_mul, 'B')
    minus = expr(mat, unreal.MaterialExpressionSubtract, X + 1100, 950, const_b=0.5)
    link(g_mul, '', minus, 'A')
    sharp = expr(mat, unreal.MaterialExpressionScalarParameter, X + 1100, 1100, parameter_name='CrustSharpness', default_value=6.0,
                 group='OreCrust')
    sharp_mul = expr(mat, unreal.MaterialExpressionMultiply, X + 1300, 950)
    link(minus, '', sharp_mul, 'A')
    link(sharp, '', sharp_mul, 'B')
    sat = expr(mat, unreal.MaterialExpressionSaturate, X + 1500, 950)
    link(sharp_mul, '', sat, '')
    amount = expr(mat, unreal.MaterialExpressionScalarParameter, X + 1500, 1100, parameter_name='CrustAmount', default_value=1.0,
                  group='OreCrust')
    mask = expr(mat, unreal.MaterialExpressionMultiply, X + 1700, 950)
    link(sat, '', mask, 'A')
    link(amount, '', mask, 'B')
    # crust colour = lerp(A, B, vertex blue) * lerp(0.7, 1.25, N)
    col_a = expr(mat, unreal.MaterialExpressionVectorParameter, X + 900, 1300, parameter_name='CrustColorA',
                 default_value=unreal.LinearColor(0.42, 0.045, 0.015, 1.0), group='OreCrust')
    col_b = expr(mat, unreal.MaterialExpressionVectorParameter, X + 900, 1500, parameter_name='CrustColorB',
                 default_value=unreal.LinearColor(0.20, 0.028, 0.012, 1.0), group='OreCrust')
    col_lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 1200, 1350)
    link(col_a, '', col_lerp, 'A')
    link(col_b, '', col_lerp, 'B')
    link(vc, 'B', col_lerp, 'Alpha')
    shade = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 1200, 1550, const_a=0.7, const_b=1.25)
    link(noise, 'R', shade, 'Alpha')
    crust = expr(mat, unreal.MaterialExpressionMultiply, X + 1450, 1400)
    link(col_lerp, '', crust, 'A')
    link(shade, '', crust, 'B')
    # tinted stock metal
    tint = expr(mat, unreal.MaterialExpressionVectorParameter, X + 1200, 600, parameter_name='Tint',
                default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0), group='OreMetal')
    tinted = expr(mat, unreal.MaterialExpressionMultiply, X + 1450, 600)
    link(stock['base'][0], stock['base'][1], tinted, 'A')
    link(tint, '', tinted, 'B')
    base = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 1950, 700)
    link(tinted, '', base, 'A')
    link(crust, '', base, 'B')
    link(mask, '', base, 'Alpha')
    inv = expr(mat, unreal.MaterialExpressionOneMinus, X + 1950, 950)
    link(mask, '', inv, '')
    metallic = expr(mat, unreal.MaterialExpressionMultiply, X + 2150, 900)
    link(stock['metallic'][0], stock['metallic'][1], metallic, 'A')
    link(inv, '', metallic, 'B')
    crust_rough = expr(mat, unreal.MaterialExpressionScalarParameter, X + 1950, 1200, parameter_name='CrustRoughness', default_value=0.85,
                       group='OreCrust')
    rough = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 2150, 1100)
    link(stock['rough'][0], stock['rough'][1], rough, 'A')
    link(crust_rough, '', rough, 'B')
    link(mask, '', rough, 'Alpha')
    for node, prop in ((base, P.MP_BASE_COLOR), (metallic, P.MP_METALLIC), (rough, P.MP_ROUGHNESS)):
        if not MEL.connect_material_property(node, '', prop):
            raise RuntimeError('could not connect ' + str(prop))
    MEL.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_GEOMETRY_COLLECTIONS)
    MEL.recompile_material(mat)
    save(ORE_MAT)
    log('material built: %s expressions=%d' % (ORE_MAT, MEL.get_num_material_expressions(mat)))
    patch_crust_detail(mat)
    return mat


def own_expressions(mat):
    return [e for e in unreal.ObjectIterator(unreal.MaterialExpression) if e.get_outer() == mat and unreal.SystemLibrary.is_valid(e)]


def patch_crust_detail(mat):
    """Second, finer sample of the macro noise that breaks up the crust colour (the crust was one flat tone at game
    distance). Same texture as the stock material samples, so no new texture binding. Patches the existing graph in
    place and does nothing when the CrustDetailTiling parameter is already there."""
    nodes = own_expressions(mat)
    for e in nodes:
        if isinstance(e, unreal.MaterialExpressionScalarParameter) and str(e.get_editor_property('parameter_name')) == 'CrustDetailTiling':
            log('crust detail already in the material')
            return
    shade = [e for e in nodes if isinstance(e, unreal.MaterialExpressionLinearInterpolate)
             and abs(e.get_editor_property('const_a') - 0.7) < 1e-4 and abs(e.get_editor_property('const_b') - 1.25) < 1e-4]
    if len(shade) != 1:
        raise RuntimeError('crust shade node not found (%d candidates)' % len(shade))
    shade = shade[0]
    users = [e for e in nodes if isinstance(e, unreal.MaterialExpressionMultiply) and shade in list(MEL.get_inputs_for_material_expression(mat, e))]
    if len(users) != 1:
        raise RuntimeError('crust colour multiply not found (%d candidates)' % len(users))
    crust = users[0]
    noise_tex = unreal.load_asset(NOISE)
    samples = [e for e in nodes if isinstance(e, unreal.MaterialExpressionTextureSample) and e.get_editor_property('texture') == noise_tex]
    if not samples:
        raise RuntimeError('no sample of the macro noise in the material')
    X, Y = 600, 1800
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, X, Y)
    tiling = expr(mat, unreal.MaterialExpressionScalarParameter, X, Y + 100, parameter_name='CrustDetailTiling', default_value=11.0,
                  group='OreCrust')
    uv_mul = expr(mat, unreal.MaterialExpressionMultiply, X + 250, Y)
    link(uv, '', uv_mul, 'A')
    link(tiling, '', uv_mul, 'B')
    detail = expr(mat, unreal.MaterialExpressionTextureSample, X + 450, Y, texture=noise_tex,
                  sampler_type=samples[0].get_editor_property('sampler_type'),
                  sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    link(uv_mul, '', detail, 'UVs')
    detail_shade = expr(mat, unreal.MaterialExpressionLinearInterpolate, X + 700, Y, const_a=0.5, const_b=1.4)
    link(detail, 'R', detail_shade, 'Alpha')
    both = expr(mat, unreal.MaterialExpressionMultiply, X + 950, Y)
    link(shade, '', both, 'A')
    link(detail_shade, '', both, 'B')
    link(both, '', crust, 'B')
    if both not in list(MEL.get_inputs_for_material_expression(mat, crust)):
        raise RuntimeError('crust detail was not wired in')
    MEL.recompile_material(mat)
    save(ORE_MAT)
    log('crust detail added: expressions=%d' % MEL.get_num_material_expressions(mat))


def build_instance(name, spec, parent):
    path = own(HOME + 'Materials/MI_OreMetal_' + name)
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset('MI_OreMetal_' + name, HOME + 'Materials',
                                                                                     unreal.MaterialInstanceConstant,
                                                                                     unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for param, key in (('Tint', 'tint'), ('CrustColorA', 'crust_a'), ('CrustColorB', 'crust_b')):
        MEL.set_material_instance_vector_parameter_value(mi, param, unreal.LinearColor(*spec[key], 1.0))
    MEL.update_material_instance(mi)
    save(path)
    return mi


# ------------------------------------------------------------------------------------------------ meshes
def import_mesh(fbx_name, dest, material, boxes=None):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    # Blender exports metres with FBX_SCALE_NONE; Unreal converts the unit itself, so no extra scale here
    data.set_editor_property('import_uniform_scale', 1.0)
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    data.set_editor_property('remove_degenerates', True)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = FBX + fbx_name + '.fbx', own(dest).rstrip('/'), fbx_name
    task.automated, task.replace_existing, task.save, task.options = True, True, False, ui
    TOOLS.import_asset_tasks([task])
    path = dest + fbx_name
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('import failed: ' + fbx_name)
    mesh.set_material(0, material)
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    body = mesh.get_editor_property('body_setup')
    if boxes:
        # box collision like the stock metal meshes (sizes come from the Blender build, in metres)
        geom = body.get_editor_property('agg_geom')
        elems = []
        for spec in boxes:
            elem = unreal.KBoxElem()
            elem.set_editor_property('center', unreal.Vector(spec['centre'][0] * 100.0, -spec['centre'][1] * 100.0, spec['centre'][2] * 100.0))
            elem.set_editor_property('x', spec['size'][0] * 100.0)
            elem.set_editor_property('y', spec['size'][1] * 100.0)
            elem.set_editor_property('z', spec['size'][2] * 100.0)
            elems.append(elem)
        geom.set_editor_property('box_elems', elems)
        geom.set_editor_property('convex_elems', [])
        body.set_editor_property('agg_geom', geom)
    save(path)
    box = mesh.get_bounding_box()
    if max(box.max.x - box.min.x, box.max.y - box.min.y) > 950.0 or max(box.max.x - box.min.x, box.max.y - box.min.y) < 200.0:
        raise RuntimeError('%s has a wrong size: %s .. %s' % (fbx_name, box.min, box.max))
    geom = mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
    info = dict(path=path, tris=mesh.get_num_triangles(0), lods=mesh.get_num_lods(),
                bounds_cm=[[round(box.min.x), round(box.min.y), round(box.min.z)], [round(box.max.x), round(box.max.y), round(box.max.z)]],
                collision={k: len(geom.get_editor_property(k)) for k in ('box_elems', 'convex_elems')},
                slots=[s.material_interface.get_path_name() if s.material_interface else None for s in mesh.static_materials])
    return mesh, info


# ------------------------------------------------------------------------------------------------ collection
def build_collection(name, fractured_path):
    gc_path = own(HOME + 'Geometry/GC_OreMetal_' + name)
    unreal.SystemLibrary.execute_console_command(None, 'RTS.FarmBuildings.BuildCollection %s %s Overwrite' % (fractured_path, gc_path))
    unreal.SystemLibrary.execute_console_command(None, 'RTS.Apartment.FlattenGeometryCollection %s' % gc_path)
    gc = unreal.load_asset(gc_path)
    if gc is None:
        raise RuntimeError('collection was not built: ' + gc_path)
    applied = {}
    # the stock metal collections: thresholds 1000 / 800 / 500, nothing removed on sleep (the actor destroys itself)
    for prop, value in (('damage_threshold', [1000.0, 800.0, 500.0]), ('remove_on_max_sleep', False), ('enable_nanite', False)):
        try:
            gc.set_editor_property(prop, value)
            applied[prop] = str(gc.get_editor_property(prop))
        except Exception as e:  # noqa: BLE001
            applied[prop] = 'ERR ' + str(e)[:120]
    save(gc_path)
    return gc_path, applied


# ------------------------------------------------------------------------------------------------ blueprint
def build_blueprint(name, spec, mesh, mesh_path, gc_path):
    bp_path = own(BP_HOME + 'Resource_Metal_Ore_' + name)
    if EAL.does_asset_exist(bp_path):
        bp = unreal.load_asset(bp_path)
    else:
        bp = EAL.duplicate_asset(STOCK_BP, bp_path)
    if bp is None or not bp.get_path_name().startswith(BP_HOME):
        raise RuntimeError('duplicate failed: ' + bp_path)
    # the duplicate's own component templates (never the stock Blueprint's)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    SBL = unreal.SubobjectDataBlueprintFunctionLibrary
    touched = []
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        o = SBL.get_object_for_blueprint(sds.k2_find_subobject_data_from_handle(h), bp)
        if o is None or not o.get_path_name().startswith(BP_HOME):
            continue
        if isinstance(o, unreal.StaticMeshComponent) and o.get_name().startswith('Barrels'):
            o.set_editor_property('static_mesh', mesh)
            o.set_editor_property('override_materials', [])
            touched.append(o.get_name())
    obj_path = '%s.%s' % (mesh_path, mesh_path.split('/')[-1])
    gc_obj = '%s.%s' % (gc_path, gc_path.split('/')[-1])
    worth = '%.6f' % spec['worth']
    g = 'UserConstructionScript'
    edits = [dict(graph=g, node='K2Node_CallFunction_8', pin='NewMesh', value=obj_path),
             dict(graph=g, node='K2Node_CallFunction_10', pin='NewMesh', value=obj_path),
             dict(graph=g, node='K2Node_VariableSet_4', pin='Geo Collection', value=gc_obj),
             dict(graph=g, node='K2Node_VariableSet_5', pin='Geo Collection', value=gc_obj),
             dict(graph=g, node='K2Node_VariableSet_6', pin='WorthMlt', value=worth),
             dict(graph=g, node='K2Node_VariableSet_7', pin='WorthMlt', value=worth)]
    job = OUT + 'pin_edits_%s.json' % name
    json.dump([dict(blueprint=bp_path, edits=edits)], open(job, 'w'), indent=1)
    unreal.SystemLibrary.execute_console_command(None, 'RTS.Blueprint.SetPinDefaults ' + job)
    lo = int(-(-(BASE_CAPACITY * spec['worth']) // 1))
    return dict(path=bp_path, templates=touched, worth_mlt=spec['worth'], amount_min=lo, amount_max=int(round(lo * 1.15 / 5.0)) * 5)


def export_stock_texture():
    """One-off, read only: the stock base colour texture as an image, to measure its average for the tint maths.
    No exporter is forced (a forced PNG exporter asserts on unsupported textures)."""
    target = OUT + 'inspect/T_Metal_Aluminum_D.png'
    if os.path.exists(target) or 'ExportStockTexture' not in sys.argv:
        return
    tex = unreal.load_asset(R + 'Metal/Meshes/Materials/Textures/T_Metal_Aluminum_D')
    if tex is None:
        return
    task = unreal.AssetExportTask()
    task.object, task.filename, task.automated, task.prompt, task.replace_identical = tex, target, True, False, True
    log('stock texture export %s' % unreal.Exporter.run_asset_export_task(task))


def main():
    os.makedirs(OUT, exist_ok=True)
    parent = build_material()
    for name, spec in RESOURCES.items():
        if ONLY and name not in ONLY:
            continue
        try:
            mi = build_instance(name, spec, parent)
            if 'MaterialsOnly' in sys.argv:
                log('%s material instance updated' % name)
                continue
            mesh, mesh_info = import_mesh('SM_OreMetal_' + name, HOME + 'Meshes/', mi, REPORT[name]['collision_boxes_m'])
            frac, frac_info = import_mesh('SM_OreMetal_%s_Fractured' % name, HOME + 'Meshes/Fractured/', mi)
            gc_path, gc_props = build_collection(name, frac_info['path'])
            bp_info = build_blueprint(name, spec, mesh, mesh_info['path'], gc_path)
            RESULT[name] = dict(material_instance=mi.get_path_name(), mesh=mesh_info, fractured=frac_info, collection=gc_path,
                                collection_props=gc_props, blueprint=bp_info)
            log('%s OK %s' % (name, json.dumps(RESULT[name])[:900]))
        except Exception as e:  # noqa: BLE001
            RESULT[name] = 'ERR ' + str(e)
            log('%s FAILED %s' % (name, str(e)[:400]))
    path = OUT + 'build_result.json'
    old = json.load(open(path)) if os.path.exists(path) else {}
    old.update(RESULT)
    json.dump(old, open(path, 'w'), indent=1)
    export_stock_texture()
    log('BUILD_DONE')


main()
