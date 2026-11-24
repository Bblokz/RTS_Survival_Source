# Imports the refinery knuckle-boom crane meshes (export/SM_RefUnload_*.fbx from
# _AssetSource/RefineryUnloadCrane/build_knuckle_crane.py), creates BP_RefineryUnloadStation (parent
# AKnuckleCraneUnloadStation) and sets it as Ger_Refinery_Truck's unload station class. Rope, grab head, jaws, crystal
# and ingot are the HQ crane's meshes (run ue_import_unload_station.py first on a fresh project).
# Headless, on a build that has AKnuckleCraneUnloadStation (DebugGame):
#   UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
# Idempotent: re-imports over existing meshes and re-sets the properties.
import unreal

SRC = 'D:/UE5Projects/RTS_Survival/_AssetSource/RefineryUnloadCrane/export'
DST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/UnloadCrane'
HQ_DST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/UnloadCrane'
TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/Ger_Refinery_Truck'
# Mesh -> material (found by asset name in the project).
MESHES = {
    'SM_RefUnload_Portal': 'MI_SupplyPoint_CraneSteel',
    'SM_RefUnload_Turret': 'MI_SupplyPoint_CraneSteel',
    'SM_RefUnload_Boom1': 'MI_SupplyPoint_CraneSteel',
    'SM_RefUnload_Boom2': 'MI_SupplyPoint_CraneSteel',
    'SM_RefUnload_Ram1Cyl': 'MI_SupplyPoint_CraneSteel',
    'SM_RefUnload_Ram2Cyl': 'MI_SupplyPoint_CraneSteel',
    'SM_RefUnload_Ram1Rod': 'M_BF_Crane_Steel',
    'SM_RefUnload_Ram2Rod': 'M_BF_Crane_Steel',
}
# Station property -> mesh asset path.
PROPS = {
    'rail_mesh': DST + '/SM_RefUnload_Portal',
    'turret_mesh': DST + '/SM_RefUnload_Turret',
    'boom1_mesh': DST + '/SM_RefUnload_Boom1',
    'boom2_mesh': DST + '/SM_RefUnload_Boom2',
    'ram1_cylinder_mesh': DST + '/SM_RefUnload_Ram1Cyl',
    'ram1_rod_mesh': DST + '/SM_RefUnload_Ram1Rod',
    'ram2_cylinder_mesh': DST + '/SM_RefUnload_Ram2Cyl',
    'ram2_rod_mesh': DST + '/SM_RefUnload_Ram2Rod',
    'rope_mesh': HQ_DST + '/SM_HQUnload_Rope',
    'grab_head_mesh': HQ_DST + '/SM_HQUnload_GrabHead',
    'jaw_mesh': HQ_DST + '/SM_HQUnload_Jaw',
    'crystal_mesh': HQ_DST + '/SM_HQUnload_Crystal',
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
    path = DST + '/BP_RefineryUnloadStation'
    bp = unreal.load_asset(path)
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.KnuckleCraneUnloadStation)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_RefineryUnloadStation', DST, unreal.Blueprint,
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


meshes = [import_mesh(name, mat) for name, mat in MESHES.items()]
bp = make_station_bp()
truck = hook_truck(bp)
for asset in [m for m in meshes if m] + [bp, truck]:
    ok = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log('saved %s %s' % (asset.get_path_name(), ok))
log('DONE')
