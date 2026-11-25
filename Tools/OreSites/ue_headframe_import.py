"""Base B headframe (concept 2D Erzbunkerschacht): imports the Blender model and builds the child Blueprint, headless.
Rerunnable after a Blender rebuild (deletes and recreates its own assets only).

  sh run_ue_py.sh ue_headframe_import.py <log>          (Development commandlet, null RHI)

Creates, only under /Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/BaseB:
  Meshes/SM_OreHF_Headframe     headframe + shaft hall + struts + rope stand + ore bunker + chute + lorry bay apron;
                                6 UCX hulls, 16 sockets (cargo_roof_01-08, entrance_01, Turret_Ampulomet, Banner,
                                LorryBay, Star, Top, Sheave_A/B), 3 LODs, Nanite off. Legacy FBX importer (Interchange
                                drops UCX hulls), adit recipe (0.01 scene, no uniform scale).
  Materials/MI_OreHF_Red        the red star (instance of the adit surface master, flat paint)
  BP_OreBaseB_Headframe         CHILD of Rus_BXP_LargeGuardTower (health, owner, garrison, Ampulomet turret, wall
                                holes, vertical-collapse death stay). Child overrides only (SubobjectDataSubsystem):
                                  BuildingMeshComponent -> SM_OreHF_Headframe, override materials cleared
                                  WallHoleDamage.DamagedMaterials -> [watchtower concrete hole MI] (slot 0 only)
                                  ChildActor (Ampulomet) -> bunker roof nest; Flag (banner) -> bunker south face,
                                  no collision / no nav
Slot materials: slot 0 = M_RusBunker_Concrete (twin of the hole MI), the rest existing Ore / A4 instances.
Writes Saved/OreMountains/sites/models/ue_headframe_report.json.
"""
import json
import os

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreSites/Headframe2D/Model/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/BaseB'
ORE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
RUSB = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/'
PARENT = RUSB + 'Rus_BXP_LargeGuardTower'
BP_NAME = 'BP_OreBaseB_Headframe'
MESH = 'SM_OreHF_Headframe'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/models/ue_headframe_report.json'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
failures = []
report = dict(failures=failures)

MATS = {
    'M_OreHF_Concrete': RUSB + 'Materials/M_RusBunker_Concrete',
    'M_OreHF_Steel': ORE + 'AditPortals/Materials/MI_OreAdit_Steel',
    'M_OreHF_Sheet': ORE + 'AditPortals/Materials/MI_OreAdit_Sheet',
    'M_OreHF_Rope': ORE + 'A4Viaduct/MI_A4_RebarRust',
    'M_OreHF_Sandbag': ORE + 'AditPortals/Materials/MI_OreAdit_Sandbag',
    'M_OreHF_Timber': ORE + 'AditPortals/Materials/MI_OreAdit_Timber',
    'M_OreHF_Dark': ORE + 'AditPortals/Materials/MI_OreAdit_Throat',
    'M_OreHF_Red': HOME + '/Materials/MI_OreHF_Red',
}
CONCRETE_HOLES = RUSB + 'Holes/MI_SM_Rus_Watchtower_M_RusBunker_Concrete_WallHoles'
LODS = ((1.0, 1.0), (0.5, 0.35), (0.25, 0.15))
BANNER_SCALE = 0.55


def log(*a):
    unreal.log_warning('ORE_HF ' + ' '.join(str(x) for x in a))


def fail(msg):
    failures.append(msg)
    log('FAIL', msg)


def disk_path(path):
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        fail('save failed ' + path)
    if not os.path.isfile(disk_path(path)):
        fail('not on disk ' + disk_path(path))


# ---- material ----------------------------------------------------------------------------------------------------
def red_material():
    path = HOME + '/Materials/MI_OreHF_Red'
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    master = unreal.load_asset(ORE + 'AditPortals/Materials/M_OreAdit_Surface')
    mi = TOOLS.create_asset('MI_OreHF_Red', HOME + '/Materials', unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    MEL.set_material_instance_scalar_parameter_value(mi, 'TexAmount', 0.0)
    MEL.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.62)
    MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(0.42, 0.025, 0.018, 1.0))
    MEL.update_material_instance(mi)
    save(path)


# ---- mesh --------------------------------------------------------------------------------------------------------
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


def set_lods(mesh):
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    opts = unreal.StaticMeshReductionOptions()
    opts.auto_compute_lod_screen_size = False
    red = []
    for pct, screen in LODS:
        r = unreal.StaticMeshReductionSettings()
        r.percent_triangles = pct
        r.screen_size = screen
        red.append(r)
    opts.reduction_settings = red
    sub.set_lods(mesh, opts)


def import_mesh(info):
    path = '%s/Meshes/%s' % (HOME, MESH)
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'FBX/%s.fbx' % MESH
    task.destination_path = HOME + '/Meshes'
    task.destination_name = MESH
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = fbx_ui()
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if mesh is None:
        fail('import failed ' + MESH)
        return None
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    set_lods(mesh)
    slots = mesh.get_editor_property('static_materials')
    names = []
    for i, sm in enumerate(slots):
        sname = str(sm.get_editor_property('material_slot_name'))
        names.append(sname)
        key = next((k for k in MATS if sname == k or (sname.startswith(k) and not sname[len(k):].isalpha())), None)
        mi = unreal.load_asset(MATS[key]) if key else None
        if mi is None:
            fail('slot %d %s has no material' % (i, sname))
            continue
        mesh.set_material(i, mi)
    if not names or not names[0].startswith('M_OreHF_Concrete'):
        fail('slot 0 is %s, must be M_OreHF_Concrete (wall-hole twin)' % (names[:1],))
    # the legacy importer reads the empties' rotation from the scaled FBX unreliably: set the intended yaw, unit scale
    for sname, s in info['sockets_ue'].items():
        sock = mesh.find_socket(sname)
        if sock is None:
            continue
        sock.set_editor_property('relative_rotation', unreal.Rotator(0.0, 0.0, s['yaw']))
        sock.set_editor_property('relative_scale', unreal.Vector(1.0, 1.0, 1.0))
    mesh.modify()
    save(path)
    # read back
    box = mesh.get_bounding_box()
    got = dict(min=[round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
               max=[round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)])
    exp = info['bounds_ue_cm']
    err = max(abs(got[k][i] - exp[k][i]) for k in ('min', 'max') for i in range(3))
    body = mesh.get_editor_property('body_setup')
    geom = body.get_editor_property('agg_geom') if body else None
    convex = len(geom.get_editor_property('convex_elems')) if geom else 0
    boxes = len(geom.get_editor_property('box_elems')) if geom else 0
    sock = {}
    for s in mesh.get_sockets_by_tag(''):
        loc = s.get_editor_property('relative_location')
        rot = s.get_editor_property('relative_rotation')
        sock[str(s.get_editor_property('socket_name'))] = dict(loc=[round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
                                                               yaw=round(rot.yaw, 1))
    report['mesh'] = dict(asset=path, bounds_cm=got, expected_cm=exp, bounds_error_cm=round(err, 1),
                          lod_triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                          convex_hulls=convex, box_hulls=boxes, sockets=sock, slots=names,
                          materials=[mesh.get_material(i).get_path_name() if mesh.get_material(i) else None
                                     for i in range(len(slots))])
    if err > 3.0:
        fail('bounds differ from the Blender build by %.1f cm (axis or scale wrong): %s vs %s' % (err, got, exp))
    if convex + boxes != info['export']['ucx']:
        fail('%d hulls, Blender wrote %d' % (convex + boxes, info['export']['ucx']))
    for sname, s in info['sockets_ue'].items():
        g = sock.get(sname)
        if g is None:
            fail('socket %s missing' % sname)
            continue
        if max(abs(g['loc'][i] - s['loc_cm'][i]) for i in range(3)) > 2.0:
            fail('socket %s at %s, expected %s' % (sname, g['loc'], s['loc_cm']))
    return mesh


# ---- blueprint ---------------------------------------------------------------------------------------------------
def handles_by_name(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDS.k2_find_subobject_data_from_handle(h)
        obj = SDL.get_object_for_blueprint(data, bp)
        if obj is None:
            continue
        out[obj.get_name().replace('_GEN_VARIABLE', '')] = (h, data, obj)
    return out


def build_bp(mesh, info):
    path = HOME + '/' + BP_NAME
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    parent_bp = unreal.load_asset(PARENT)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent_bp.generated_class())
    bp = TOOLS.create_asset(BP_NAME, HOME, unreal.Blueprint, factory)
    if bp is None:
        fail('blueprint not created')
        return None
    comps = handles_by_name(bp)
    report['bp_components'] = sorted(comps)
    s = info['sockets_ue']

    def comp(name):
        if name not in comps:
            fail('child has no component ' + name)
            return None
        return comps[name][2]

    m = comp('BuildingMeshComponent')
    if m:
        m.set_editor_property('static_mesh', mesh)
        m.set_editor_property('override_materials', [])
    w = comp('WallHoleDamage')
    if w:
        w.set_editor_property('damaged_materials', [unreal.load_asset(CONCRETE_HOLES)])
    ca = comp('ChildActor')
    if ca:
        p = s['Turret_Ampulomet']['loc_cm']
        ca.set_editor_property('relative_location', unreal.Vector(p[0], p[1], p[2]))
        ca.set_editor_property('relative_rotation', unreal.Rotator(0.0, 0.0, 180.0))
    fl = comp('Flag')
    if fl:
        p = s['Banner']['loc_cm']
        # banner mesh: a plane in its XY, width along X, hanging along -Y from a top pivot, face +Z:
        # X -> world -Y (west), Y -> world +Z (up), so the face points south at the default camera
        rot = unreal.MathLibrary.make_rot_from_xy(unreal.Vector(0.0, -1.0, 0.0), unreal.Vector(0.0, 0.0, 1.0))
        fl.set_editor_property('relative_location', unreal.Vector(p[0] - 6.0, p[1], p[2]))
        fl.set_editor_property('relative_rotation', rot)
        fl.set_editor_property('relative_scale3d', unreal.Vector(BANNER_SCALE, BANNER_SCALE, BANNER_SCALE))
        fl.set_collision_profile_name('NoCollision')
        fl.set_editor_property('can_ever_affect_navigation', False)
        report['banner_rotation'] = [rot.roll, rot.pitch, rot.yaw]
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    return bp


def readback(bp_path):
    bp = unreal.load_asset(bp_path)
    comps = handles_by_name(bp)
    out = {}
    for name in ('BuildingMeshComponent', 'WallHoleDamage', 'ChildActor', 'Flag'):
        if name not in comps:
            continue
        o = comps[name][2]
        d = {}
        if isinstance(o, unreal.SceneComponent):
            l = o.get_editor_property('relative_location')
            d['loc'] = [round(l.x, 1), round(l.y, 1), round(l.z, 1)]
        if isinstance(o, unreal.StaticMeshComponent):
            sm = o.get_editor_property('static_mesh')
            d['mesh'] = sm.get_name() if sm else None
            d['override_materials'] = [x.get_name() if x else None for x in o.get_editor_property('override_materials')]
            d['collision'] = str(o.get_collision_profile_name())
        if name == 'WallHoleDamage':
            d['damaged_materials'] = [x.get_name() if x else None for x in o.get_editor_property('damaged_materials')]
        out[name] = d
    gen = bp.generated_class()
    out['parent_ok'] = unreal.MathLibrary.class_is_child_of(gen, unreal.load_asset(PARENT).generated_class())
    return out


def parent_readback():
    bp = unreal.load_asset(PARENT)
    comps = handles_by_name(bp)
    m = comps['BuildingMeshComponent'][2]
    fl = comps['Flag'][2]
    w = comps['WallHoleDamage'][2]
    return dict(mesh=m.get_editor_property('static_mesh').get_name(),
                flag_loc=str(fl.get_editor_property('relative_location')),
                holes=[x.get_name() if x else None for x in w.get_editor_property('damaged_materials')])


def main():
    info = json.load(open(MODEL + 'OreHeadframe2D_model.json'))
    report['parent_before'] = parent_readback()
    red_material()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        mesh = import_mesh(info)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    if mesh is not None:
        build_bp(mesh, info)
        report['bp'] = readback(HOME + '/' + BP_NAME)
    report['parent_after'] = parent_readback()
    if report['parent_after'] != report['parent_before']:
        fail('parent Blueprint changed: %s -> %s' % (report['parent_before'], report['parent_after']))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1, default=str)
    log('mesh', json.dumps({k: report.get('mesh', {}).get(k) for k in ('bounds_error_cm', 'lod_triangles',
                                                                        'convex_hulls', 'box_hulls')}))
    log('bp', json.dumps(report.get('bp'), default=str))
    log('parent', json.dumps(report['parent_after']))
    log('RESULT', 'OK' if not failures else 'FAIL %s' % json.dumps(failures))


main()
