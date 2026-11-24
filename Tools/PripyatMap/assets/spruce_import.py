"""Polesie dead spruces -> /Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces.

Meshes: LOD0 from PolesieDeadSpruces/UE/*.FBX (blender_spruce_ucx.py added one UCX trunk box), LOD1/2 from the kit's
LODs/ folder, MI_Env_DeadTree_1 (the kit's UVs are on T_RTS_DeadTree_1, the texture that instance uses).
Blueprints: duplicates of BP_OreDeadSpruce_Skeleton_11m (AWpoTree child; medium + heavy tanks crush it) with the mesh
swapped, exactly as the Ore Mountains dead forest importer made its trees. No alpha cards in this kit, so the Ore
Opacity_Instensity fix does not apply.
exec in the server namespace; set PRY_SPRUCE_ONLY = [names] to limit.
"""
import unreal

_SP_SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PolesieDeadSpruces/'
_SP_HOME = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces'
_SP_MAT = '/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/Materials/MI_Env_DeadTree_1'
_SP_BP_SOURCE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/BP_OreDeadSpruce_Skeleton_11m'
_SP_NAMES = ['01_TallSentinel', '02_BogLean', '03_SnappedCrown', '04_WindFlag', '05_OldGiant', '06_DrownedYoung',
             '07_ForkedSnag', '08_Uprooted']
_SP_LOD_SCREEN = [1.0, 0.3, 0.12]
_EAL = unreal.EditorAssetLibrary
_SUB = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
_SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
_SDL = unreal.SubobjectDataBlueprintFunctionLibrary


def _sp_fbx_ui():
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


def _sp_import(key):
    name = 'SM_DeadSpruce_' + key
    task = unreal.AssetImportTask()
    task.filename = _SP_SRC + 'UE/' + name + '.FBX'
    task.destination_path = _SP_HOME + '/Meshes'
    task.destination_name = 'SM_PolesieDeadSpruce_' + key
    task.automated, task.save, task.replace_existing, task.options = True, False, True, _sp_fbx_ui()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(_SP_HOME + '/Meshes/SM_PolesieDeadSpruce_' + key)
    if mesh is None:
        raise RuntimeError('import failed ' + name)
    for lod in (1, 2):
        got = _SUB.import_lod(mesh, lod, _SP_SRC + 'LODs/' + name + '_LOD%d.FBX' % lod)
        if got != lod:
            print('WARN %s LOD%d import returned %s' % (name, lod, got))
    _SUB.set_lod_screen_sizes(mesh, _SP_LOD_SCREEN[:mesh.get_num_lods()])
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    for i in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(i, unreal.load_asset(_SP_MAT))
    mesh.set_editor_property('light_map_coordinate_index', 1)
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    _EAL.save_loaded_asset(mesh, False)
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems'))
    b = mesh.get_bounding_box()
    return mesh, dict(lods=mesh.get_num_lods(), tris=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                      hulls=hulls, min=[round(b.min.x), round(b.min.y), round(b.min.z)],
                      max=[round(b.max.x), round(b.max.y), round(b.max.z)])


def _sp_components(bp):
    out = {}
    for h in _SDS.k2_gather_subobject_data_for_blueprint(bp):
        o = _SDL.get_object_for_blueprint(_SDL.get_data(h), bp)
        if o is not None and isinstance(o, unreal.ActorComponent):
            out[o.get_name().replace('_GEN_VARIABLE', '')] = o
    return out


def _sp_blueprint(key, mesh):
    path = _SP_HOME + '/BP_PolesieDeadSpruce_' + key
    bp = unreal.load_asset(path) if _EAL.does_asset_exist(path) else _EAL.duplicate_asset(_SP_BP_SOURCE, path)
    if bp is None:
        raise RuntimeError('could not create ' + path)
    cdo = unreal.get_default_object(bp.generated_class())
    cur = cdo.get_editor_property('TreeMesh')
    cdo.set_editor_property('TreeMesh', [mesh] if isinstance(cur, (list, unreal.Array)) else mesh)
    comps = _sp_components(bp)
    tree = comps.get('Tree')
    if tree is None:
        raise RuntimeError(path + ': no Tree component')
    tree.set_editor_property('static_mesh', mesh)
    tree.set_editor_property('override_materials', [])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    _EAL.save_loaded_asset(bp, False)
    cdo = unreal.get_default_object(bp.generated_class())
    return dict(bp=path, tank=str(cdo.get_editor_property('TankDestructionType')),
                tree_mesh=str(cdo.get_editor_property('TreeMesh'))[-60:])


PRY_SPRUCE_RESULT = {}
for _k in globals().get('PRY_SPRUCE_ONLY') or _SP_NAMES:
    _m, _info = _sp_import(_k)
    _info.update(_sp_blueprint(_k, _m))
    PRY_SPRUCE_RESULT[_k] = _info
    print(_k, _info)
