# Imports the metal depot bucket elevator meshes (export/SM_DepotElevator_*.fbx from
# _AssetSource/MetalDepotUnload/build_bucket_elevator.py -- export), creates BP_DepotElevatorStation (parent
# ABucketElevatorUnloadStation) and sets it as Ger_MetalVault_Truck's unload station class. The bars are the HQ crane's
# ingot (run ue_import_unload_station.py first on a fresh project).
# Headless, on a build that has ABucketElevatorUnloadStation (DebugGame):
#   UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
# Idempotent: re-imports over existing meshes and re-sets the properties.
import unreal

SRC = 'D:/UE5Projects/RTS_Survival/_AssetSource/MetalDepotUnload/export'
DST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/Opel_MetalVault/UnloadElevator'
HQ_DST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/UnloadCrane'
TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/Opel_MetalVault/Ger_MetalVault_Truck'
# Mesh -> material (found by asset name in the project).
MESHES = {
    'SM_DepotElevator_Frame': 'MI_SupplyPoint_CraneSteel',
    'SM_DepotElevator_Apron': 'MI_SupplyPoint_CraneSteel',
    'SM_DepotElevator_Bucket': 'M_BF_Crane_Steel',
    'SM_DepotElevator_Pulley': 'M_BF_Crane_Steel',
}
# Station property -> mesh asset path.
PROPS = {
    'rail_mesh': DST + '/SM_DepotElevator_Frame',
    'apron_mesh': DST + '/SM_DepotElevator_Apron',
    'bucket_mesh': DST + '/SM_DepotElevator_Bucket',
    'pulley_mesh': DST + '/SM_DepotElevator_Pulley',
    'bar_mesh': HQ_DST + '/SM_HQUnload_Ingot',
}
LOG = 'RTS_UNLOAD_IMPORT'


def log(msg):
    unreal.log_warning('%s %s' % (LOG, msg))


def find_asset(name):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(package_paths=['/Game'], recursive_paths=True)
    for data in reg.get_assets(flt):
        if str(data.asset_name) == name:
            return data.get_asset()
    return None


def import_mesh(name, material_name):
    task = unreal.AssetImportTask()
    task.filename = '%s/%s.fbx' % (SRC, name)
    task.destination_path = DST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_materials = False
    opts.import_textures = False
    opts.import_as_skeletal = False
    opts.static_mesh_import_data.combine_meshes = True
    opts.static_mesh_import_data.auto_generate_collision = False
    opts.static_mesh_import_data.generate_lightmap_u_vs = True
    task.options = opts
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset('%s/%s' % (DST, name))
    mat = find_asset(material_name)
    if mesh and mat:
        mesh.set_material(0, mat)
    if mesh:
        box = mesh.get_bounding_box()
        log('mesh %s min=%s max=%s material=%s' % (name, box.min, box.max, mat.get_path_name() if mat else None))
    else:
        log('FAILED import %s' % name)
    return mesh


def make_station_bp():
    path = DST + '/BP_DepotElevatorStation'
    bp = unreal.load_asset(path)
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.BucketElevatorUnloadStation)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_DepotElevatorStation', DST, unreal.Blueprint,
                                                                     factory)
    cdo = unreal.get_default_object(bp.generated_class())
    for prop, mesh_path in PROPS.items():
        mesh = unreal.load_asset(mesh_path)
        if mesh is None:
            log('MISSING mesh %s for %s' % (mesh_path, prop))
            continue
        cdo.set_editor_property(prop, mesh)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log('station bp %s' % bp.get_path_name())
    return bp


def hook_truck(bp):
    truck = unreal.load_asset(TRUCK)
    cdo = unreal.get_default_object(truck.generated_class())
    cdo.set_editor_property('m_unload_station_class', bp.generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(truck)
    cdo = unreal.get_default_object(truck.generated_class())
    log('truck unload class = %s' % cdo.get_editor_property('m_unload_station_class'))
    return truck


def report_harvester_hulls():
    # The elevator docks harvesters by the rear of the mesh with the cargo socket; log it for each German harvester.
    for name in ('SM_PZ4Harvester_Hull', 'SM_HarvesterPanzer1_Hull'):
        mesh = find_asset(name)
        if not mesh:
            log('hull %s not found' % name)
            continue
        box = mesh.get_bounding_box()
        sockets = ['%s(%d,%d,%d)' % (s.get_editor_property('socket_name'), s.relative_location.x,
                                     s.relative_location.y, s.relative_location.z) for s in mesh.get_sockets_by_tag('')]
        log('hull %s min_x=%d max_z=%d sockets=%s' % (name, box.min.x, box.max.z, sockets))


meshes = [import_mesh(name, mat) for name, mat in MESHES.items()]
bp = make_station_bp()
truck = hook_truck(bp)
for asset in [m for m in meshes if m] + [bp, truck]:
    ok = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log('saved %s %s' % (asset.get_path_name(), ok))
report_harvester_hulls()
log('DONE')
