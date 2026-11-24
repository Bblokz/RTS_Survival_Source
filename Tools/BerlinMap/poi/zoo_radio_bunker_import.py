"""Zoo radio bunker POI for Ger_Berlin: imports SM_Berlin_ZooRadioBunker and assigns its four slots.

Blender source (generator, validator, previews, report):
    D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinZooRadioBunker/
Usage (headless -run=pythonscript wrapper, or UECP execute_python):
    exec(open(r'<this file>').read()); result = run()
Nothing runs at import time. Safe to rerun after a Blender rebuild (replaces the mesh, updates the rubble instance).

Assets in /Game/RTS_Survival/Blueprints/Environment/Berlin/ZooRadioBunker:
- SM_Berlin_ZooRadioBunker: one static mesh, UCX hulls from the FBX (bunker block, blast walls, rubble heaps, cable trench,
  mast foot; none in front of the door), LightMapUV in UV1, no auto collision.
  Pivot = ground centre of the footprint. +X = entrance (door in the +X wall, 6 m clear ground expected in front of it).
  -X = cable-trench side: face it toward the Zoo flak tower. Rubble heaps lie on -X and on +Y (Unreal mirrors Blender's Y).
- MI_ZRB_RubbleTile: child of DBK_Concrete_Rubble_Tile_Instance_Default with Tiling 1 (the mesh's rubble UVs already
  hold a 2.5 m world tile).
Other slots reuse project materials unchanged:
  M_BF_SootedFortress_Concrete -> MI_BF_SootedFortress_UVTile (4 m world tile in UVMap, as every Breakthrough building)
  M_BF_Crane_WornSteel / M_BF_Crane_PaintedSteel -> the HQ cranes' steel materials (1.6 m tile in UVMap)
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinZooRadioBunker/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/Berlin/ZooRadioBunker'
MESH = 'SM_Berlin_ZooRadioBunker'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/'
RUBBLE_PARENT = '/Game/DBK/Material_Instances/DBK_Concrete_Rubble_Tile_Instance_Default'
RUBBLE_MI = 'MI_ZRB_RubbleTile'
# FBX slot name prefix -> material path (the rubble entry is filled in by make_rubble_instance)
MATERIALS = {
    'M_BF_SootedFortress_Concrete': SHARED + 'MI_BF_SootedFortress_UVTile',
    'M_BF_Crane_WornSteel': SHARED + 'M_BF_Crane_WornSteel',
    'DBK_Concrete_Rubble_Tile': HOME + '/' + RUBBLE_MI,
    'M_BF_Crane_PaintedSteel': SHARED + 'M_BF_Crane_PaintedSteel',
}


def _save(asset):
    asset.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outermost()], False):
        raise RuntimeError('save failed ' + asset.get_path_name())
    rel = asset.get_outermost().get_name()[len('/Game/'):]
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + rel + '.uasset'
    if not unreal.Paths.file_exists(disk):    # a save can report success and still miss the disk
        raise RuntimeError('not on disk ' + disk)


def _import_mesh():
    """Legacy FBX: Interchange drops UCX hulls and ignores FbxImportUI."""
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
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + MESH + '.FBX'
    task.destination_path = HOME
    task.destination_name = MESH
    task.automated = True
    task.save = False          # AssetImportTask(save=True) silently misses the disk here
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(HOME + '/' + MESH)
    if mesh is None:
        raise RuntimeError('import made nothing at ' + HOME + '/' + MESH)
    return mesh


def _make_rubble_instance():
    edit = unreal.MaterialEditingLibrary
    parent = unreal.load_asset(RUBBLE_PARENT)
    if parent is None:
        raise RuntimeError('missing ' + RUBBLE_PARENT)
    path = HOME + '/' + RUBBLE_MI
    mi = unreal.load_asset(path)
    if mi is None:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            RUBBLE_MI, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    edit.set_material_instance_parent(mi, parent)
    names = [str(n) for n in edit.get_scalar_parameter_names(parent)]
    before = {n: edit.get_material_instance_scalar_parameter_value(parent, n) for n in names if 'Tiling' in n}
    for n in before:
        edit.set_material_instance_scalar_parameter_value(mi, n, 1.0)
    edit.update_material_instance(mi)
    _save(mi)
    return mi, before


def run():
    rubble, parent_tiling = _make_rubble_instance()
    mesh = _import_mesh()
    assigned = {}
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in MATERIALS if slot_name.startswith(k)), None)
        if key is None:
            raise RuntimeError('no material for slot ' + slot_name)
        material = unreal.load_asset(MATERIALS[key])
        if material is None:
            raise RuntimeError('missing material ' + MATERIALS[key])
        mesh.set_material(index, material)    # setting it on the struct only changes a copy
        assigned[slot_name] = material.get_path_name()
    if unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0) >= 2:
        mesh.set_editor_property('light_map_coordinate_index', 1)
    _save(mesh)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    if hulls == 0:
        raise RuntimeError('no UCX hulls imported (Interchange still active?)')
    box = mesh.get_bounding_box()
    result = {
        'mesh': mesh.get_path_name(),
        'triangles': mesh.get_num_triangles(0),
        'convex_hulls': hulls,
        'uv_channels': unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0),
        'bounds_min_cm': [round(v, 1) for v in (box.min.x, box.min.y, box.min.z)],
        'bounds_max_cm': [round(v, 1) for v in (box.max.x, box.max.y, box.max.z)],
        'materials': assigned,
        'rubble_parent_tiling_was': parent_tiling,
        'entrance': '+X', 'tower_side': '-X',
    }
    unreal.log('ZOO_RADIO_BUNKER ' + repr(result))
    return result
