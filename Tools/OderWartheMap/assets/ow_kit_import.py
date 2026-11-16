"""Oder-Warthe asset kit -> /Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit (headless only).

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi -nosplash
        -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Jobs come from Saved/OderWarthe/assets/import_jobs.json: {"meshes": ["SM_OW_..."], "pine_bps": ["01", ...],
"materials": true|false}. Sources: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe (FBX/, Textures/,
ow_materials.json, ow_blender_report.json) written by ow_textures.py / ow_build.py.

Creates (only inside the Kit folder; nothing shared is touched):
  Textures/T_OW_*            base colours (sRGB, default compression; the pine strip clamps V)
  Materials/M_OW_Kit          opaque master: BaseTex(UV * Tiling) * Tint * macro variation; Roughness / Metallic params;
                              used with instanced static meshes. Created once, never rebuilt in place.
  Materials/MI_OW_*           one instance per ow_materials.json entry
  Meshes/SM_OW_*              legacy FBX import (UCX hulls kept, one convex per UCX), slots bound by name, Nanite off,
                              simple-as-complex collision, LODs from <name>_LOD<n>.fbx
  Blueprints/BP_OW_DeadPine_* duplicates of BP_PolesieDeadSpruce_01_TallSentinel (AWpoTree, TankDestructionType
                              MEDIUM_OR_HEAVY_TANK) with TreeMesh + Tree component swapped (spruce_import.py pattern)
Published assets are re-imported in place (replace_existing), never deleted. Each run merges UE readback rows (bounds,
triangles, slots, collision) into Saved/OderWarthe/assets/ow_kit.json (atomic write).
"""
import json
import os
import time

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/'
KIT = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit'
SAVED = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/assets/'
JOBS = SAVED + 'import_jobs.json'
MANIFEST = SAVED + 'ow_kit.json'
PINE_TEMPLATE = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces/BP_PolesieDeadSpruce_01_TallSentinel'
TREE_LOD_SCREEN = [1.0, 0.3, 0.12]
ROCK_LOD_SCREEN = [1.0, 0.25]

_EAL = unreal.EditorAssetLibrary
_MEL = unreal.MaterialEditingLibrary
_SUB = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
_SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
_SDL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = []


def log(*a):
    line = ' '.join(str(x) for x in a)
    LOG.append(line)
    print('OWKIT ' + line)
    unreal.log_warning('OWKIT ' + line)


# ------------------------------------------------------------------------------------------------------- textures
def import_texture(name):
    path = KIT + '/Textures/' + name
    task = unreal.AssetImportTask()
    task.filename = SRC + 'Textures/' + name + '.png'
    task.destination_path = KIT + '/Textures'
    task.destination_name = name
    task.automated, task.save, task.replace_existing = True, False, True
    _TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    if tex is None:
        raise RuntimeError('texture import failed ' + name)
    tex.set_editor_property('srgb', True)
    if name == 'T_OW_PineBark_D':
        tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    save(tex, name)
    return tex


# ------------------------------------------------------------------------------------------------------- material
def master(default_tex):
    path = KIT + '/Materials/M_OW_Kit'
    if _EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    m = _TOOLS.create_asset('M_OW_Kit', KIT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)

    def node(cls, **props):
        n = _MEL.create_material_expression(m, cls)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, ao, b, bi):
        if not _MEL.connect_material_expressions(a, ao, b, bi):
            raise RuntimeError('link failed %s -> %s.%s' % (ao, b.get_name(), bi))

    def scalar(name, v):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)

    uv = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    til = scalar('Tiling', 1.0)
    uvm = node(unreal.MaterialExpressionMultiply)
    link(uv, '', uvm, 'A')
    link(til, '', uvm, 'B')
    tex = node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name='BaseTex', texture=default_tex)
    link(uvm, '', tex, 'UVs')
    # macro variation: the same map sampled 7.7x larger, red channel -> lerp(MacroDark, 1.08)
    ms = scalar('MacroScale', 0.13)
    uvmm = node(unreal.MaterialExpressionMultiply)
    link(uvm, '', uvmm, 'A')
    link(ms, '', uvmm, 'B')
    tex2 = node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name='BaseTex', texture=default_tex)
    link(uvmm, '', tex2, 'UVs')
    mlerp = node(unreal.MaterialExpressionLinearInterpolate)
    link(scalar('MacroDark', 0.8), '', mlerp, 'A')
    link(scalar('MacroBright', 1.08), '', mlerp, 'B')
    link(tex2, 'R', mlerp, 'Alpha')
    tint = node(unreal.MaterialExpressionVectorParameter, parameter_name='Tint',
                default_value=unreal.LinearColor(1, 1, 1, 1))
    c1 = node(unreal.MaterialExpressionMultiply)
    link(tex, 'RGB', c1, 'A')
    link(tint, '', c1, 'B')
    c2 = node(unreal.MaterialExpressionMultiply)
    link(c1, '', c2, 'A')
    link(mlerp, '', c2, 'B')
    _MEL.connect_material_property(c2, '', unreal.MaterialProperty.MP_BASE_COLOR)
    _MEL.connect_material_property(scalar('Roughness', 0.85), '', unreal.MaterialProperty.MP_ROUGHNESS)
    _MEL.connect_material_property(scalar('Metallic', 0.0), '', unreal.MaterialProperty.MP_METALLIC)
    _MEL.connect_material_property(scalar('Specular', 0.4), '', unreal.MaterialProperty.MP_SPECULAR)
    _MEL.layout_material_expressions(m)
    _MEL.recompile_material(m)
    _EAL.save_loaded_asset(m, False)
    log('master created', path)
    return m


def save(asset, what):
    """Save; a package another editor has loaded is file-locked on Windows: keep the old file, report it."""
    if _EAL.save_loaded_asset(asset, False):
        return True
    log('WARN save failed (locked by another editor?)', what)
    LOCKED.append(what)
    return False


LOCKED = []


def instance(name, cfg, parent, textures, rebuild):
    path = KIT + '/Materials/' + name
    if _EAL.does_asset_exist(path) and name not in rebuild:
        return unreal.load_asset(path)
    mi = unreal.load_asset(path) if _EAL.does_asset_exist(path) else _TOOLS.create_asset(
        name, KIT + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    _MEL.set_material_instance_parent(mi, parent)
    _MEL.set_material_instance_texture_parameter_value(mi, 'BaseTex', textures[cfg['tex']])
    t = cfg['tint']
    _MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(t[0], t[1], t[2], 1.0))
    _MEL.set_material_instance_scalar_parameter_value(mi, 'Roughness', cfg['rough'])
    _MEL.set_material_instance_scalar_parameter_value(mi, 'Metallic', cfg['metal'])
    _MEL.set_material_instance_scalar_parameter_value(mi, 'Tiling', 1.0)
    if cfg['tex'] == 'T_OW_PineBark_D':   # the strip encodes height; no macro tiling on it
        _MEL.set_material_instance_scalar_parameter_value(mi, 'MacroDark', 1.0)
        _MEL.set_material_instance_scalar_parameter_value(mi, 'MacroBright', 1.0)
    _MEL.update_material_instance(mi)
    save(mi, name)
    return mi


def materials(force_textures, rebuild):
    cfgs = json.load(open(SRC + 'ow_materials.json'))
    textures = {}
    for cfg in cfgs.values():
        tn = cfg['tex']
        if tn in textures:
            continue
        path = KIT + '/Textures/' + tn
        textures[tn] = import_texture(tn) if (tn in force_textures or not _EAL.does_asset_exist(path)) else unreal.load_asset(path)
    parent = master(textures['T_OW_Concrete_D'])
    out = {}
    for name, cfg in cfgs.items():
        out[name] = instance(name, cfg, parent, textures, rebuild)
    log('materials', sorted(out))
    return out


# ---------------------------------------------------------------------------------------------------------- meshes
def fbx_ui():
    ui = unreal.FbxImportUI()
    for k, v in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                     import_animations=False).items():
        ui.set_editor_property(k, v)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property('static_mesh_import_data')
    for k, v in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                     generate_lightmap_u_vs=False, build_nanite=False, remove_degenerates=True).items():
        d.set_editor_property(k, v)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def import_mesh(name, mats, lods, lod_screen=None, collision='ucx'):
    folder = KIT + '/Meshes'
    task = unreal.AssetImportTask()
    task.filename = SRC + 'FBX/' + name + '.fbx'
    task.destination_path = folder
    task.destination_name = name
    task.automated, task.save, task.replace_existing, task.options = True, False, True, fbx_ui()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        _TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(folder + '/' + name)
    if mesh is None:
        raise RuntimeError('import failed ' + name)
    for k in range(1, lods + 1):
        got = _SUB.import_lod(mesh, k, SRC + 'FBX/%s_LOD%d.fbx' % (name, k))
        if got != k:
            log('WARN', name, 'LOD%d import returned' % k, got)
    if lod_screen and mesh.get_num_lods() > 1:
        _SUB.set_lod_screen_sizes(mesh, lod_screen[:mesh.get_num_lods()])
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    statics = mesh.get_editor_property('static_materials')
    slots = [str(s.get_editor_property('material_slot_name')) for s in statics]
    bound = []
    for i, sn in enumerate(slots):
        key = next((k for k in sorted(mats, key=len, reverse=True) if sn.startswith(k)), None)
        if key is None:
            raise RuntimeError('%s: slot %s has no MI_OW match' % (name, sn))
        mesh.set_material(i, mats[key])
        bound.append(key)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    body = mesh.get_editor_property('body_setup')
    if collision == 'none':
        _SUB.remove_collisions(mesh)
    body = mesh.get_editor_property('body_setup')
    if body is not None:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    if not save(mesh, name):
        return None, bound
    return mesh, bound


def readback(mesh):
    b = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    ag = body.get_editor_property('agg_geom') if body else None
    return dict(
        bounds_cm=[[round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)],
                   [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)]],
        triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
        material_slots=[str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')],
        materials=[mesh.get_material(i).get_path_name().split('.')[0] for i in
                   range(len(mesh.get_editor_property('static_materials')))],
        collision=dict(convex=len(ag.get_editor_property('convex_elems')) if ag else 0,
                       boxes=len(ag.get_editor_property('box_elems')) if ag else 0,
                       trace='simple_as_complex'),
        nanite=bool(mesh.get_editor_property('nanite_settings').enabled),
    )


# ------------------------------------------------------------------------------------------------------ blueprints
def components(bp):
    out = {}
    for h in _SDS.k2_gather_subobject_data_for_blueprint(bp):
        o = _SDL.get_object_for_blueprint(_SDL.get_data(h), bp)
        if o is not None and isinstance(o, unreal.ActorComponent):
            out[o.get_name().replace('_GEN_VARIABLE', '')] = o
    return out


def pine_bp(key, mesh):
    path = KIT + '/Blueprints/BP_OW_DeadPine_' + key
    bp = unreal.load_asset(path) if _EAL.does_asset_exist(path) else _EAL.duplicate_asset(PINE_TEMPLATE, path)
    if bp is None:
        raise RuntimeError('could not create ' + path)
    cdo = unreal.get_default_object(bp.generated_class())
    cur = cdo.get_editor_property('TreeMesh')
    cdo.set_editor_property('TreeMesh', [mesh] if isinstance(cur, (list, unreal.Array)) else mesh)
    comps = components(bp)
    tree = comps.get('Tree')
    if tree is None:
        raise RuntimeError(path + ': no Tree component, have %s' % list(comps))
    tree.set_editor_property('static_mesh', mesh)
    tree.set_editor_property('override_materials', [])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not save(bp, path):
        raise RuntimeError('save failed ' + path)
    cdo = unreal.get_default_object(bp.generated_class())
    comps = components(bp)
    tm = comps['Tree'].get_editor_property('static_mesh')
    info = dict(tank_destruction=str(cdo.get_editor_property('TankDestructionType')),
                tree_mesh=str(cdo.get_editor_property('TreeMesh').get_name()),
                tree_component_mesh=tm.get_name() if tm else None,
                is_wpo_tree=isinstance(cdo, unreal.WpoTree), health=cdo.get_editor_property('Health'),
                components=sorted(comps))
    log('pine bp', path, json.dumps(info))
    return path, info


# -------------------------------------------------------------------------------------------------------- manifest
def write_manifest(rows):
    data = json.load(open(MANIFEST)) if os.path.exists(MANIFEST) else {}
    data.setdefault('kit', 'Oder-Warthe asset kit (Festungsfront Oder-Warthe-Bogen, 1976 decay)')
    data['conventions'] = {
        'units': 'cm; bounds_cm are local-space mesh bounds from the UE readback',
        'pivot': 'ground level at the footprint centre unless the row says otherwise (sluice gate: deck level)',
        'facing': 'local +X = front unless the row says otherwise',
        'collision': 'UCX convex hulls, simple-as-complex',
        'axes': 'Blender +X = UE +X, Blender +Y = UE -Y (FBX handedness); bounds here are UE local space',
        'materials': KIT + '/Materials (M_OW_Kit master + MI_OW_*); look checked only in Blender previews, '
                     'UE look unverified (no RHI in the headless import)',
        'source': SRC + ' (FBX, .blend, textures); scripts in Source/Tools/OderWartheMap/assets',
    }
    data.setdefault('assets', {})
    for k, v in rows.items():
        data['assets'][k] = v
    data['updated'] = time.strftime('%Y-%m-%d %H:%M:%S')
    tmp = MANIFEST + '.tmp'
    with open(tmp, 'w') as f:
        json.dump(data, f, indent=1)
    os.replace(tmp, MANIFEST)
    log('manifest rows', sorted(rows))


def main():
    jobs = json.load(open(JOBS))
    blender = json.load(open(SRC + 'ow_blender_report.json'))
    notes = blender
    for sub in ('Meshes', 'Materials', 'Textures', 'Blueprints'):
        _EAL.make_directory(KIT + '/' + sub)
    if jobs.get('materials', True):
        mats = materials(jobs.get('force_textures', []), jobs.get('rebuild_materials', []))
    else:
        mats = {n: unreal.load_asset(KIT + '/Materials/' + n) for n in json.load(open(SRC + 'ow_materials.json'))}
    rows = {}
    if jobs.get('verify_material'):
        m = unreal.load_asset(KIT + '/Materials/M_OW_Kit')
        for prop in ('MP_BASE_COLOR', 'MP_ROUGHNESS', 'MP_METALLIC', 'MP_SPECULAR'):
            node = _MEL.get_material_property_input_node(m, getattr(unreal.MaterialProperty, prop))
            log('master input', prop, node.get_name() if node else 'MISSING')
        log('master expressions', _MEL.get_num_material_expressions(m),
            'textures', [t.get_name() for t in _MEL.get_used_textures(m)])
        for n in sorted(mats):
            mi = mats[n]
            tv = _MEL.get_material_instance_texture_parameter_value(mi, 'BaseTex')
            tint = _MEL.get_material_instance_vector_parameter_value(mi, 'Tint')
            log('mi', n, 'parent', mi.get_editor_property('parent').get_name(), 'BaseTex', tv.get_name() if tv else None,
                'Tint', round(tint.r, 3), round(tint.g, 3), round(tint.b, 3),
                'Rough', _MEL.get_material_instance_scalar_parameter_value(mi, 'Roughness'))
    for name in jobs.get('meshes', []):
        b = blender[name]
        lods = len(b.get('lods', []))
        screen = TREE_LOD_SCREEN if 'Pine' in name else ROCK_LOD_SCREEN
        mesh, bound = import_mesh(name, mats, lods, screen)
        if mesh is None:
            continue
        rb = readback(mesh)
        n = notes.get(name, {})
        row = dict(id=name, kind='static_mesh', asset_path=mesh.get_path_name(), package=KIT + '/Meshes/' + name)
        row.update(rb)
        row.update(dict(blender_bounds_cm=[b['min'], b['max']], ucx_hulls_exported=b['hulls'],
                        pivot=n.get('pivot', 'ground level, footprint centre'), facing=n.get('facing', '+X front'),
                        notes=n.get('notes', b.get('notes', '')), previews=n.get('previews', [])))
        ok = rb['collision']['convex'] == b['hulls'] or (b['hulls'] == 0)
        row['checks'] = dict(collision_matches_ucx=ok, nanite_off=not rb['nanite'])
        rows[name] = row
        log('mesh', name, json.dumps(rb))
    for key in jobs.get('pine_bps', []):
        mesh = unreal.load_asset(KIT + '/Meshes/SM_OW_DeadPine_' + key)
        path, info = pine_bp(key, mesh)
        sm = rows.get('SM_OW_DeadPine_' + key) or {}
        n = notes.get('SM_OW_DeadPine_' + key, {})
        rows['BP_OW_DeadPine_' + key] = dict(id='BP_OW_DeadPine_' + key, kind='blueprint',
                                             asset_path=path + '.BP_OW_DeadPine_' + key + '_C',
                                             package=path, mesh=mesh.get_path_name(),
                                             bounds_cm=sm.get('bounds_cm') or readback(mesh)['bounds_cm'],
                                             pivot='trunk base at ground level', facing='any yaw',
                                             notes='Crushable AWpoTree (copy of BP_PolesieDeadSpruce_01_TallSentinel): '
                                                   'medium and heavy tanks crush it. ' + n.get('notes', ''),
                                             blueprint=info, previews=n.get('previews', []))
    write_manifest(rows)
    with open(SAVED + 'import_log.txt', 'a') as f:
        f.write('\n'.join(LOG) + '\n')
    log('OW_KIT_IMPORT_DONE locked=%s' % LOCKED)


main()
