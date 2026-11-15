"""Static meshes with collision for Pripyat (Part A6 and Part B): legacy FBX (UCX kept), materials by slot index.

Set PRY_STATIC = [dict(fbx, dest_folder, name, materials=[paths], collision='ucx'|'box'|'none', lightmap_index=1)]
and exec this file in the server namespace. Saves only the imported meshes.
"""
import unreal

_EAL = unreal.EditorAssetLibrary
_SUBS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def _st_ui():
    ui = unreal.FbxImportUI()
    for k, v in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                     import_animations=False).items():
        ui.set_editor_property(k, v)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property('static_mesh_import_data')
    for k, v in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                     generate_lightmap_u_vs=False, build_nanite=False).items():
        d.set_editor_property(k, v)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def _st_one(job):
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = job['fbx'], job['dest_folder'], job['name']
    task.automated, task.save, task.replace_existing, task.options = True, False, True, _st_ui()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(job['dest_folder'] + '/' + job['name'])
    if mesh is None:
        raise RuntimeError('import failed ' + job['fbx'])
    slots = mesh.get_editor_property('static_materials')
    mats = job.get('materials', [])
    if job.get('slot_map'):
        # The FBX importer drops slots no face uses, so bind by slot name (prefix match), not by index.
        names = [str(s.material_slot_name) for s in slots]
        mats = [next(v for k, v in job['slot_map'].items() if n.startswith(k)) for n in names]
    if mats and len(mats) != len(slots):
        print('WARN %s: %d slots, %d materials given; slots %s' % (job['name'], len(slots), len(mats),
              [str(s.material_slot_name) for s in slots]))
    for i in range(min(len(slots), len(mats))):
        m = unreal.load_asset(mats[i])
        if m is None:
            raise RuntimeError('material missing ' + mats[i])
        mesh.set_material(i, m)
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    if job.get('lightmap_index') is not None:
        mesh.set_editor_property('light_map_coordinate_index', job['lightmap_index'])
    col = job.get('collision', 'ucx')
    if col == 'none':
        _SUBS.remove_collisions(mesh)
    elif col == 'box':
        _SUBS.remove_collisions(mesh)
        _SUBS.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    body = mesh.get_editor_property('body_setup')
    if body is not None:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    if not _EAL.save_loaded_asset(mesh, False):
        raise RuntimeError('save failed ' + job['name'])
    geom = body.get_editor_property('agg_geom') if body else None
    b = mesh.get_bounding_box()
    return dict(slots=[str(s.material_slot_name) for s in slots], tris=mesh.get_num_triangles(0),
                hulls=(len(geom.get_editor_property('convex_elems')), len(geom.get_editor_property('box_elems'))) if geom else None,
                min=[round(b.min.x), round(b.min.y), round(b.min.z)], max=[round(b.max.x), round(b.max.y), round(b.max.z)],
                uv=_SUBS.get_num_uv_channels(mesh, 0))


for _job in globals().get('PRY_STATIC', []):
    print(_job['name'], _st_one(_job))
