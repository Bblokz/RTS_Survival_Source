"""Imports the weighbridge (1A Waagehaus) and border (6A Grenzsteine) models and builds their Blueprints, headless.
Safe to rerun after a Blender rebuild (wbb_build.py).   sh run_ue.sh ue_wbb_import.py import

Creates only under /Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/:
  Weighbridge/Materials/MI_OreWB_Plaster, MI_OreWB_RoofTile   instances of the adit master M_OreAdit_Surface with the
                                    existing Greek_island house-wall / roof-tile textures (the only new materials)
  Weighbridge/Meshes/SM_OreWB_*     Booth (3 UCX, 2 LODs), Deck (1 UCX, pivot on the top face), BoomPost, BoomArm,
                                    BoomRest, Flagpole
  Weighbridge/BP_OreWB_Booth        CHILD of Rus_Bxp_ContainerHut (health, death, owner, selection stay), mesh override
  Weighbridge/BP_OreWB_BoomBarrier  plain actor: post + arm (tag BoomArm, pitch it to raise) + rest, no collision
  Border/Meshes/SM_OreBorder_*      Stone_A, Stone_B, Stone_C, PostSnapped (no collision)
Legacy FBX importer (Interchange drops UCX hulls); the adit recipe's FBX imports at unit scale. Never saves the parent
hut. Writes Saved/OreMountains/sites/models/wbb_ue_import_report.json.
"""
import json
import os

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreSites/Model/'
SITES = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/'
WB = SITES + 'Weighbridge'
BORDER = SITES + 'Border'
ORE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
A4 = ORE + 'A4Viaduct/'
AD = ORE + 'AditPortals/Materials/'
OW = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Clues/Materials/'
HUT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_ContainerHut'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/models/wbb_ue_import_report.json'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
failures = []
report = dict(meshes={}, materials={}, blueprints={}, failures=failures)

GI = '/Game/Greek_island/Textures/'
NEW_MI = {   # name -> (D, N, packed, scalars, tint)
    'MI_OreWB_Plaster': (GI + 'T_house_wall/T_greek_house_wall_D', GI + 'T_house_wall/T_greek_house_wall_N',
                         GI + 'T_house_wall/T_greek_house_wall_AORM',
                         dict(UVScale=1.0, Desaturate=0.35, RoughScale=1.0, NormalStrength=1.0), (0.93, 0.84, 0.68)),
    'MI_OreWB_RoofTile': (GI + 'T_roof/T_rooftile_D', GI + 'T_roof/T_rooftile_N', GI + 'T_roof/T_rooftile_AORM',
                          dict(UVScale=1.0, Desaturate=0.3, RoughScale=1.0, NormalStrength=1.0), (0.78, 0.56, 0.48)),
}
MATS = {   # Blender slot -> Unreal material instance (existing ones except the two above)
    'M_OreWB_Plaster': WB + '/Materials/MI_OreWB_Plaster',
    'M_OreWB_RoofTile': WB + '/Materials/MI_OreWB_RoofTile',
    'M_OreWB_Granite': A4 + 'MI_A4_Granite',
    'M_OreWB_Concrete': A4 + 'MI_A4_Concrete',
    'M_OreWB_Timber': AD + 'MI_OreAdit_Timber',
    'M_OreWB_Glass': OW + 'MI_OWClue_Glass',
    'M_OreWB_Steel': AD + 'MI_OreAdit_Steel',
    'M_OreWB_Plate': AD + 'MI_OreAdit_Sheet',
    'M_OreWB_PaintRed': OW + 'MI_OWClue_PaintRed',
    'M_OreWB_PaintWhite': OW + 'MI_OWClue_PaintWhite',
    'M_OreWB_Lamp': AD + 'MI_OreAdit_Lamp',
    'M_OreWB_Dark': OW + 'MI_OWClue_RoofTar',
    'M_OreBorder_Stone': A4 + 'MI_A4_Rubble',
}
PIECES = {   # name -> (folder, keep collision, LODs)
    'SM_OreWB_Booth': (WB, True, ((1.0, 1.0), (0.5, 0.25))),
    'SM_OreWB_Deck': (WB, True, None),
    'SM_OreWB_BoomPost': (WB, True, None),
    'SM_OreWB_BoomRest': (WB, True, None),
    'SM_OreWB_BoomArm': (WB, False, None),
    'SM_OreWB_Flagpole': (WB, False, None),
    'SM_OreBorder_Stone_A': (BORDER, False, None),
    'SM_OreBorder_Stone_B': (BORDER, False, None),
    'SM_OreBorder_Stone_C': (BORDER, False, None),
    'SM_OreBorder_PostSnapped': (BORDER, False, None),
}


def log(*a):
    unreal.log_warning('ORE_SITES_WBB ' + ' '.join(str(x) for x in a))


def fail(msg):
    failures.append(msg)
    log('FAIL', msg)


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        fail('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path.replace('/Game/', '', 1) + '.uasset'
    if not os.path.isfile(disk):
        fail('not on disk ' + disk)
    return disk


# ------------------------------------------------------------------------------------------------ materials
def make_instances():
    master = unreal.load_asset(AD + 'M_OreAdit_Surface')
    if master is None:
        fail('missing M_OreAdit_Surface')
        return
    for name, (d, n, pk, scalars, tint) in NEW_MI.items():
        path = WB + '/Materials/' + name
        mi = unreal.load_asset(path)
        if mi is None:
            mi = TOOLS.create_asset(name, WB + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        mel.clear_all_material_instance_parameters(mi)
        td, tn, tp = (unreal.load_asset(p) for p in (d, n, pk))
        if None in (td, tn, tp):
            fail('%s: texture missing' % name)
            continue
        mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTex', td)
        mel.set_material_instance_texture_parameter_value(mi, 'NormalTex', tn)
        comp = str(tp.get_editor_property('compression_settings')).upper()
        if 'MASKS' in comp:
            mel.set_material_instance_texture_parameter_value(mi, 'PackedMasksTex', tp)
            mel.set_material_instance_scalar_parameter_value(mi, 'UseMasksPacked', 1.0)
        else:
            mel.set_material_instance_texture_parameter_value(mi, 'PackedTex', tp)
        for k, v in scalars.items():
            mel.set_material_instance_scalar_parameter_value(mi, k, v)
        mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        mel.update_material_instance(mi)
        save(path)
        report['materials'][name] = dict(packed=tp.get_name(), compression=comp, srgb=bool(tp.get_editor_property('srgb')))


# ------------------------------------------------------------------------------------------------ meshes
def fbx_ui():
    ui = unreal.FbxImportUI()
    for k, v in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                     import_animations=False).items():
        ui.set_editor_property(k, v)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property('static_mesh_import_data')
    for k, v in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                     generate_lightmap_u_vs=True, build_nanite=False, remove_degenerates=True).items():
        d.set_editor_property(k, v)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def set_lods(mesh, lods):
    if not lods:
        return
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    opts = unreal.StaticMeshReductionOptions()
    opts.auto_compute_lod_screen_size = False
    red = []
    for pct, screen in lods:
        r = unreal.StaticMeshReductionSettings()
        r.percent_triangles = pct
        r.screen_size = screen
        red.append(r)
    opts.reduction_settings = red
    sub.set_lods(mesh, opts)


def import_mesh(name, info):
    folder, keep_collision, lods = PIECES[name]
    path = '%s/Meshes/%s' % (folder, name)
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'FBX/%s.fbx' % name
    task.destination_path = folder + '/Meshes'
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = fbx_ui()
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if mesh is None:
        fail('import failed ' + name)
        return None
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    set_lods(mesh, lods)
    slots = mesh.get_editor_property('static_materials')
    names = []
    for i, sm in enumerate(slots):
        sname = str(sm.get_editor_property('material_slot_name'))
        names.append(sname)
        key = next((k for k in MATS if sname == k or (sname.startswith(k) and not sname[len(k):].isalpha())), None)
        mi = unreal.load_asset(MATS[key]) if key else None
        if mi is None:
            fail('%s slot %s has no material' % (name, sname))
            continue
        mesh.set_material(i, mi)
    p = info['pieces'][name]
    if not keep_collision:
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
    save(path)
    box = mesh.get_bounding_box()
    got = dict(min=[round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
               max=[round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)])
    lo, hi = p['bounds_m']['min'], p['bounds_m']['max']
    expect = dict(min=[lo[1] * 100, lo[0] * 100, lo[2] * 100], max=[hi[1] * 100, hi[0] * 100, hi[2] * 100])
    err = max(abs(got[k][i] - expect[k][i]) for k in ('min', 'max') for i in range(3))
    body = mesh.get_editor_property('body_setup')
    geom = body.get_editor_property('agg_geom') if body else None
    convex = len(geom.get_editor_property('convex_elems')) if geom else 0
    socks = mesh.get_sockets_by_tag('')
    sock = {str(s.get_editor_property('socket_name')): [round(c, 1) for c in (s.get_editor_property('relative_location').x,
            s.get_editor_property('relative_location').y, s.get_editor_property('relative_location').z)] for s in socks}
    report['meshes'][name] = dict(asset=path, bounds_cm=got, expected_cm=expect, bounds_error_cm=round(err, 1),
                                  lod_triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                                  convex_hulls=convex, sockets=sock, slots=names,
                                  materials=[mesh.get_material(i).get_name() if mesh.get_material(i) else None
                                             for i in range(len(slots))])
    if err > 3.0:
        fail('%s bounds differ from the Blender build by %.1f cm (axis or scale wrong)' % (name, err))
    want = p['ucx'] if keep_collision else 0
    if convex != want:
        fail('%s: %d convex hulls, expected %d' % (name, convex, want))
    if sorted(sock) != sorted(p['sockets']):
        fail('%s: sockets %s, expected %s' % (name, sorted(sock), sorted(p['sockets'])))
    return mesh


# ------------------------------------------------------------------------------------------------ blueprints
def handles_by_name(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        obj = SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(h), bp)
        if obj is not None:
            out[obj.get_name().replace('_GEN_VARIABLE', '')] = (h, obj)
    return out


def add(bp, name, cls, parent_handle):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp)
    handle, reason = SDS.add_new_subobject(params)
    if not SDL.is_handle_valid(handle):
        raise RuntimeError('add %s failed: %s' % (name, reason))
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle, SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)


def no_collision(comp):
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)


def build_booth(meshes):
    path = WB + '/BP_OreWB_Booth'
    parent = unreal.load_asset(HUT)
    if parent is None:
        fail('parent hut missing')
        return
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent.generated_class())
    bp = TOOLS.create_asset('BP_OreWB_Booth', WB, unreal.Blueprint, factory)
    comps = handles_by_name(bp)
    log('booth child components', sorted(comps))
    h_mesh, mesh_comp = comps['BuildingMeshComponent']
    mesh_comp.set_editor_property('static_mesh', meshes['SM_OreWB_Booth'])
    mesh_comp.set_editor_property('override_materials', [])          # the parent overrides slot 0 with container blue
    if 'ProgressBarWidget' in comps:
        comps['ProgressBarWidget'][1].set_editor_property('relative_location', unreal.Vector(0.0, 0.0, 720.0))
    if 'SelectedDecal' in comps:
        d = comps['SelectedDecal'][1]
        d.set_editor_property('relative_location', unreal.Vector(34.0, 0.0, 0.0))
        d.set_editor_property('relative_scale3d', unreal.Vector(30.0, 816.0, 625.0))
    # flag pole beside the west gable (Flag socket), no collision so the placer's footprint stays the house
    flag_loc = meshes['SM_OreWB_Booth'].find_socket('Flag')
    loc = flag_loc.get_editor_property('relative_location') if flag_loc else unreal.Vector(80.0, -330.0, 0.0)
    _, flag = add(bp, 'FlagPole', unreal.StaticMeshComponent, h_mesh)
    flag.set_editor_property('static_mesh', meshes['SM_OreWB_Flagpole'])
    flag.set_editor_property('mobility', mesh_comp.get_editor_property('mobility'))
    flag.set_editor_property('relative_location', loc)
    no_collision(flag)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    disk = save(path)
    report['blueprints']['BP_OreWB_Booth'] = dict(asset=path, disk=disk, parent=HUT,
                                                  mobility=str(mesh_comp.get_editor_property('mobility')))


def build_boom(meshes):
    path = WB + '/BP_OreWB_BoomBarrier'
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.Actor)
    bp = TOOLS.create_asset('BP_OreWB_BoomBarrier', WB, unreal.Blueprint, factory)
    handles = SDS.k2_gather_subobject_data_for_blueprint(bp)
    root_h = handles[1] if len(handles) > 1 else handles[0]
    root = SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(root_h), bp)
    root.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    parts = (('Post', 'SM_OreWB_BoomPost', (0.0, 0.0, 0.0), unreal.ComponentMobility.STATIC, ()),
             ('Rest', 'SM_OreWB_BoomRest', (1040.0, 0.0, 0.0), unreal.ComponentMobility.STATIC, ()),
             ('Arm', 'SM_OreWB_BoomArm', (0.0, 0.0, 110.0), unreal.ComponentMobility.MOVABLE, ('BoomArm',)))
    for name, mesh, loc, mob, tags in parts:
        _, c = add(bp, name, unreal.StaticMeshComponent, root_h)
        c.set_editor_property('static_mesh', meshes[mesh])
        c.set_editor_property('mobility', mob)
        c.set_editor_property('relative_location', unreal.Vector(*loc))
        no_collision(c)
        if tags:
            c.set_editor_property('component_tags', list(tags))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    disk = save(path)
    report['blueprints']['BP_OreWB_BoomBarrier'] = dict(asset=path, disk=disk)


def main():
    info = json.load(open(MODEL + 'OreSites_WeighbridgeBorder_model.json'))
    hut_disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + HUT.replace('/Game/', '', 1) + '.uasset'
    hut_mtime = os.path.getmtime(hut_disk)
    make_instances()
    meshes = {}
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in PIECES:
            meshes[name] = import_mesh(name, info)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    if all(meshes.values()):
        build_booth(meshes)
        build_boom(meshes)
    report['hut_parent_untouched'] = os.path.getmtime(hut_disk) == hut_mtime
    if not report['hut_parent_untouched']:
        fail('the parent hut .uasset changed on disk')
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1)
    for k, v in report['meshes'].items():
        log(k, 'err=%.1fcm hulls=%d sockets=%s lods=%s mats=%s' % (v['bounds_error_cm'], v['convex_hulls'], list(v['sockets']),
                                                                  v['lod_triangles'], v['materials']))
    log('RESULT', 'OK' if not failures else 'FAIL %s' % failures)


main()
