"""Imports the Base A ore plant (3B) and heap station (4A Erzrolle) meshes, headless. Rerunnable (deletes + reimports).

  sh run_ue_py.sh ue_import.py import          (Development commandlet, null RHI)

Source: D:/Blender/.../OreSites/Model_OrePlant_HeapStation (op_build.py writes FBX/ and OrePlant_HeapStation_model.json).
Legacy FBX importer (Interchange drops UCX hulls), the adit recipe (0.01 scene, no uniform scale), 3 LODs, Nanite off,
lightmap UVs generated. No new materials: every slot gets an existing project material instance (MATS).
Report: Saved/OreMountains/sites/ue_oreplant/import_report.json
"""
import json
import os

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreSites/Model_OrePlant_HeapStation/'
SITES = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/import_report.json'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
failures = []
report = dict(meshes={}, failures=failures)

ORE = SITES
MATS = {
    'M_OreSite_Steel': ORE + 'AditPortals/Materials/MI_OreAdit_Steel',
    'M_OreSite_Sheet': ORE + 'AditPortals/Materials/MI_OreAdit_Sheet',
    'M_OreSite_Corrugated': '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/Materials/MI_RTS_RustedCorrugatedPanel',
    'M_OreSite_Concrete': ORE + 'A4Viaduct/MI_A4_Concrete',
    'M_OreSite_Timber': ORE + 'AditPortals/Materials/MI_OreAdit_Timber',
    'M_OreSite_Roof': '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Clues/Materials/MI_OWClue_RoofTar',
    'M_OreSite_Dark': ORE + 'AditPortals/Materials/MI_OreAdit_Throat',
    'M_OreSite_Rope': ORE + 'A4Viaduct/MI_A4_RebarRust',
    'M_OreSite_Paint': ORE + 'AditPortals/Materials/MI_OreAdit_Paint',
    'M_OreSite_Rust': ORE + 'A4Viaduct/MI_A4_RebarRust',
    'M_OreSite_Ore': ORE + 'A4Viaduct/MI_A4_Rubble',
}
HOME = {'BaseA': SITES + 'Sites/BaseA/Meshes', 'HeapStations': SITES + 'HeapStations/Meshes'}
PIECES = {
    'SM_OreBaseA_CrusherHouse': ('BaseA', True),
    'SM_OreBaseA_SiloTable': ('BaseA', True),
    'SM_OreBaseA_NissenShed': ('BaseA', True),
    'SM_OreHeap_Erzrolle': ('HeapStations', True),
    'SM_OreHeap_ErzrolleChute': ('HeapStations', False),
    'SM_OreHeap_ErzrolleChuteBroken': ('HeapStations', False),
}


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        failures.append('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'
    if not os.path.isfile(disk):
        failures.append('not on disk ' + disk)
    return disk


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


def import_mesh(name, sub_dir, keep_collision, info):
    home = HOME[sub_dir]
    path = '%s/%s' % (home, name)
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'FBX/%s.fbx' % name
    task.destination_path = home
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
    set_lods(mesh, ((1.0, 1.0), (0.5, 0.3), (0.25, 0.12)))
    slots = mesh.get_editor_property('static_materials')
    names = []
    for i, sm in enumerate(slots):
        sname = str(sm.get_editor_property('material_slot_name'))
        names.append(sname)
        key = next((k for k in MATS if sname == k or sname.startswith(k + '_') or sname.startswith(k + '.')), None)
        if key is None:
            key = next((k for k in MATS if sname.startswith(k)), None)
        mi = unreal.load_asset(MATS[key]) if key else None
        if mi is None:
            failures.append('%s slot %s has no material' % (name, sname))
            continue
        mesh.set_material(i, mi)
    if not keep_collision:
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
    disk = save(path)
    box = mesh.get_bounding_box()
    got = dict(min=[round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
               max=[round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)])
    b = info['pieces'][name]['bounds_m']
    expect = dict(min=[b['min'][1] * 100, b['min'][0] * 100, b['min'][2] * 100],
                  max=[b['max'][1] * 100, b['max'][0] * 100, b['max'][2] * 100])
    err = max(abs(got[k][i] - expect[k][i]) for k in ('min', 'max') for i in range(3))
    body = mesh.get_editor_property('body_setup')
    geom = body.get_editor_property('agg_geom') if body else None
    convex = len(geom.get_editor_property('convex_elems')) if geom else 0
    report['meshes'][name] = dict(asset=path, disk=disk, disk_bytes=os.path.getsize(disk) if os.path.isfile(disk) else 0,
                                  bounds_cm=got, expected_cm=expect, bounds_error_cm=round(err, 1),
                                  lod_triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                                  convex_hulls=convex, slots=names,
                                  materials=[mesh.get_material(i).get_name() if mesh.get_material(i) else None
                                             for i in range(len(slots))])
    if err > 3.0:
        failures.append('%s bounds differ from the Blender build by %.1f cm (axis or scale wrong)' % (name, err))
    want = info['pieces'][name]['ucx'] if keep_collision else 0
    if convex != want:
        failures.append('%s: %d hulls, Blender wrote %d' % (name, convex, want))


def main():
    info = json.load(open(MODEL + 'OrePlant_HeapStation_model.json'))
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name, (sub_dir, coll) in PIECES.items():
            import_mesh(name, sub_dir, coll, info)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1)
    unreal.log_warning('OREPLANT_IMPORT failures=%d %s' % (len(failures), json.dumps(failures)))
    for k, v in report['meshes'].items():
        unreal.log_warning('OREPLANT_IMPORT %s err=%.1fcm hulls=%d lods=%s mats=%s' % (
            k, v['bounds_error_cm'], v['convex_hulls'], v['lod_triangles'], v['materials']))


main()
