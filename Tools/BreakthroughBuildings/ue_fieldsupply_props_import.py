"""Imports the Breakthrough FieldSupply props as standalone meshes (run with the UECP execute_python action).

Blender source and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/AssetsForFactionIdentity/
FieldSupply/README.md. All ten meshes use the faction's shared M_BF_FieldSupply (the same atlas the Armory, Mechanized Depot and Oxide
Works dressing meshes use), so this script imports meshes only and never touches the material or its textures.

Rerun after a Blender rebuild: unreferenced meshes are deleted and imported fresh, because importing over an existing asset reuses its
stored import settings (the first import came in at 1/100 scale and a plain reimport kept that). Once the meshes are placed or used by a
Blueprint the script refuses to delete them; reimport those from the Content Browser instead.
"""
import unreal

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/AssetsForFactionIdentity/FieldSupply/'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
HOME = SHARED + '/FieldSupply'
MATERIAL = SHARED + '/M_BF_FieldSupply'
NAMES = ('SM_BF_Jerrycan_Fuel', 'SM_BF_Jerrycan_Water', 'SM_BF_Jerrycan_Stack', 'SM_BF_AmmoCan_MG', 'SM_BF_AmmoCan_Stack',
         'SM_BF_ShellCarrier_Single', 'SM_BF_ShellCarrier_Triple', 'SM_BF_Barrel_3_Oil', 'SM_BF_FuelStand_HandCrank', 'SM_BF_FuelPump')
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MESHES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    # save_loaded_asset reports False on a fresh import now and then; save_packages doesn't
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):    # a save can report success and still miss the disk
        raise RuntimeError('not on disk ' + disk)


def import_mesh(name):
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
    data.set_editor_property('import_uniform_scale', 100.0)    # the FBXs are in metres and arrive 100x too small (and weld away tris) otherwise
    task = unreal.AssetImportTask()
    task.filename = SOURCE + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False    # AssetImportTask(save=True) silently misses the disk here
    task.replace_existing = True
    task.options = ui
    ASSET_TOOLS.import_asset_tasks([task])


def finish_mesh(name, material):
    path = f'{HOME}/{name}'
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('import made nothing at ' + path)
    for index in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(index, material)    # setting it on the struct only changes a copy
    if MESHES.get_num_uv_channels(mesh, 0) < 2:
        raise RuntimeError(name + ' has no LightMapUV channel')
    mesh.set_editor_property('light_map_coordinate_index', 1)
    save(path)
    return mesh


def report(mesh):
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    geom = body.get_editor_property('agg_geom') if body else None
    hulls = len(geom.get_editor_property('convex_elems')) if geom else 0
    boxes = len(geom.get_editor_property('box_elems')) if geom else 0
    print(mesh.get_name(), 'tris', mesh.get_num_triangles(0), 'convex', hulls, 'box', boxes, 'uv', MESHES.get_num_uv_channels(mesh, 0),
          'size_cm', [round(v) for v in (box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z)],
          'min_z', round(box.min.z, 1), 'materials', [mesh.get_material(i).get_name() for i in range(len(mesh.get_editor_property('static_materials')))])


def main():
    material = unreal.load_asset(MATERIAL)
    if material is None:
        raise RuntimeError('missing ' + MATERIAL)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for name in NAMES:
        path = f'{HOME}/{name}'
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            refs = [str(r) for r in registry.get_referencers(path, unreal.AssetRegistryDependencyOptions())]
            if refs:
                raise RuntimeError(f'{name} is used by {refs}: reimport it by hand instead')
            unreal.EditorAssetLibrary.delete_asset(path)
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in NAMES:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    for name in NAMES:
        report(finish_mesh(name, material))


main()
