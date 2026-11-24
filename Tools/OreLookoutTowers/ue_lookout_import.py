"""Imports the Schachtwarte lookout tower model into Unreal, headless. Safe to rerun after a Blender rebuild.

  sh run_ue_py.sh ue_lookout_import.py <log>        (Development commandlet, null RHI)

Creates, only under /Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers/Meshes:
  SM_OreLookout_Schachtwarte   the whole site, 13 UCX hulls, 13 sockets (cargo_*, entrance_01, Sheave_N/S, HeldPanel,
                               Reveal, LadderBottom/Top), 3 LODs, Nanite off
  SM_OreLookout_Sheave         one sheave wheel (pivot on the hub, spins about local X), no collision
  SM_OreLookout_HeldPanel      orange tarpaulin shown while the tower is held (pivot on the top rail), no collision
Legacy FBX importer (Interchange drops UCX hulls), as the A4 viaduct and the adit portals; uniform scale 0.01 (see fbx_ui).
No new materials: the slots get existing instances (MATS below). Writes Saved/OreMountains/lookout/ue_import_report.json.
"""
import json
import os

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreLookoutTowers/Model/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers'
ORE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/lookout/ue_import_report.json'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
failures = []
report = dict(meshes={}, failures=failures)

# Blender slot -> existing project material instance (both tiling on world-scale 2 m UVs or world space).
MATS = {
    'M_OreLookout_Steel': ORE + 'AditPortals/Materials/MI_OreAdit_Steel',       # riveted rusty steel
    'M_OreLookout_Rope': ORE + 'A4Viaduct/MI_A4_RebarRust',                    # darker rusted wire rope
    'M_OreLookout_Stone': ORE + 'A4Viaduct/MI_A4_Granite',                     # ashlar granite walls
    'M_OreLookout_StoneTrim': ORE + 'A4Viaduct/MI_A4_Rubble',                  # paler, dusty wall caps and lintels
    'M_OreLookout_Concrete': ORE + 'A4Viaduct/MI_A4_Concrete',                 # collar cap, footings, floor slab
    'M_OreLookout_Panel': ORE + 'AditPortals/Materials/MI_OreAdit_Tarp',       # orange tarpaulin
}
PIECES = ['SM_OreLookout_Schachtwarte', 'SM_OreLookout_Sheave', 'SM_OreLookout_HeldPanel']
LODS = {'SM_OreLookout_Schachtwarte': ((1.0, 1.0), (0.5, 0.35), (0.25, 0.15)),
        'SM_OreLookout_Sheave': ((1.0, 1.0), (0.5, 0.2)),
        'SM_OreLookout_HeldPanel': None}


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        failures.append('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'
    if not os.path.isfile(disk):
        failures.append('not on disk ' + disk)


def expected_bounds(info, name):
    """Unreal cm from the Blender report: the main mesh from its design-frame stats (X = 100 * design y north,
    Y = 100 * design x east); sheave and panel from their export bounds (pivot frame, symmetric in Y)."""
    if name == 'SM_OreLookout_Schachtwarte':
        lo, hi = info['stats'][name]['lo'], info['stats'][name]['hi']
        return dict(min=[lo[1] * 100, lo[0] * 100, lo[2] * 100], max=[hi[1] * 100, hi[0] * 100, hi[2] * 100])
    lo, hi = info['export'][name]['bounds_cm']
    return dict(min=[lo[0], -hi[1], lo[2]], max=[hi[0], -lo[1], hi[2]])


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
    # ol_d_build.py writes centimetre data into a metre-unit FBX (scene scale_length 1.0, unlike the A4 viaduct's
    # 0.01), so Unreal's unit conversion would make it 100x too big: scale back here (meshes, UCX hulls, sockets).
    d.set_editor_property('import_uniform_scale', 0.01)
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


def fix_socket_rotations(mesh, info):
    """The legacy importer reads the empties' yaw wrongly from the scaled FBX (0 or -26.6 instead of 180 / 90): set the
    yaw the Blender build meant (sockets_ue) with unit scale. Only UCargo seats would use it; the sheave and panel
    components are placed unrotated."""
    for sname, s in info.get('sockets_ue', {}).items():
        sock = mesh.find_socket(sname)
        if sock is None:
            continue
        sock.set_editor_property('relative_rotation', unreal.Rotator(0.0, 0.0, s['yaw']))
        sock.set_editor_property('relative_scale', unreal.Vector(1.0, 1.0, 1.0))
    mesh.modify()


def import_mesh(name, info):
    path = '%s/Meshes/%s' % (HOME, name)
    if EAL.does_asset_exist(path):
        # a reimport over the old mesh keeps its stored import settings (the uniform scale was ignored): start clean
        EAL.delete_asset(path)
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'FBX/%s.FBX' % name
    task.destination_path = HOME + '/Meshes'
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = fbx_ui()
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if mesh is None:
        failures.append('import failed ' + name)
        return
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    set_lods(mesh, LODS[name])
    slots = mesh.get_editor_property('static_materials')
    names = []
    for i, sm in enumerate(slots):
        sname = str(sm.get_editor_property('material_slot_name'))
        names.append(sname)
        key = next((k for k in MATS if sname.startswith(k) and (sname == k or not sname[len(k):].isalpha())), None)
        mi = unreal.load_asset(MATS[key]) if key else None
        if mi is None:
            failures.append('%s slot %s has no material' % (name, sname))
            continue
        mesh.set_material(i, mi)
    if name != 'SM_OreLookout_Schachtwarte':
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
    else:
        fix_socket_rotations(mesh, info)
    save(path)
    box = mesh.get_bounding_box()
    got = dict(min=[round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
               max=[round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)])
    expect = expected_bounds(info, name)
    err = max(abs(got[k][i] - expect[k][i]) for k in ('min', 'max') for i in range(3))
    body = mesh.get_editor_property('body_setup')
    geom = body.get_editor_property('agg_geom') if body else None
    convex = len(geom.get_editor_property('convex_elems')) if geom else 0
    boxes = len(geom.get_editor_property('box_elems')) if geom else 0
    socks = mesh.get_sockets_by_tag('')
    sock = {}
    for s in socks:
        loc = s.get_editor_property('relative_location')
        rot = s.get_editor_property('relative_rotation')
        sock[str(s.get_editor_property('socket_name'))] = dict(loc=[round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
                                                               yaw=round(rot.yaw, 1))
    report['meshes'][name] = dict(asset=path, bounds_cm=got, expected_cm=expect, bounds_error_cm=round(err, 1),
                                  lod_triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                                  convex_hulls=convex, box_hulls=boxes, sockets=sock, slots=names,
                                  materials=[mesh.get_material(i).get_name() if mesh.get_material(i) else None
                                             for i in range(len(slots))])
    if err > 3.0:
        failures.append('%s bounds differ from the Blender build by %.1f cm (axis or scale wrong)' % (name, err))
    want = info['export'][name]
    if convex + boxes != want['ucx']:
        failures.append('%s: %d hulls, Blender wrote %d' % (name, convex + boxes, want['ucx']))
    if len(sock) != want['sockets']:
        failures.append('%s: %d sockets, Blender wrote %d' % (name, len(sock), want['sockets']))
    for sname, s in info.get('sockets_ue', {}).items() if name == 'SM_OreLookout_Schachtwarte' else []:
        g = sock.get(sname)
        if g is None:
            failures.append('socket %s missing' % sname)
            continue
        d = max(abs(g['loc'][i] - s['loc_cm'][i]) for i in range(3))
        if d > 2.0:
            failures.append('socket %s at %s, expected %s' % (sname, g['loc'], s['loc_cm']))
        if abs((g['yaw'] - s['yaw'] + 180.0) % 360.0 - 180.0) > 0.5:
            failures.append('socket %s yaw %.1f, expected %.1f' % (sname, g['yaw'], s['yaw']))


def main():
    info = json.load(open(MODEL + 'OreLookout_Schachtwarte_sockets.json'))
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in PIECES:
            import_mesh(name, info)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1)
    unreal.log_warning('LOOKOUT_IMPORT failures=%d %s' % (len(failures), json.dumps(failures)))
    for k, v in report['meshes'].items():
        unreal.log_warning('LOOKOUT_IMPORT %s err=%.1fcm hulls=%d+%d sockets=%d lods=%s mats=%s' % (
            k, v['bounds_error_cm'], v['convex_hulls'], v['box_hulls'], len(v['sockets']), v['lod_triangles'],
            v['materials']))


main()
