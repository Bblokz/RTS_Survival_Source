# Imports the HQ loop crane meshes (export/SM_HQUnload_*.fbx from _AssetSource/HQUnloadCrane/export_unload_assets.py),
# creates BP_HQUnloadStation (parent AResourceUnloadStation) and sets it as Ger_HQ_Truck's unload station class.
# Headless, on a build that has AResourceUnloadStation (DebugGame):
#   UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
# Idempotent: re-imports over existing meshes and re-sets the properties.
import unreal

SRC = 'D:/UE5Projects/RTS_Survival/_AssetSource/HQUnloadCrane/export'
DST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/UnloadCrane'
HQ_TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/Ger_HQ_Truck'
MESHES = ['SM_HQUnload_Rail', 'SM_HQUnload_TrolleyBody', 'SM_HQUnload_Rope', 'SM_HQUnload_GrabHead',
          'SM_HQUnload_Jaw', 'SM_HQUnload_Crystal', 'SM_HQUnload_Ingot']
# Material per mesh: found by asset name in the project.
MATERIALS = {
    'SM_HQUnload_Rail': 'MI_SupplyPoint_CraneSteel',
    'SM_HQUnload_TrolleyBody': 'MI_SupplyPoint_CraneSteel',
    'SM_HQUnload_GrabHead': 'MI_SupplyPoint_CraneSteel',
    'SM_HQUnload_Rope': 'M_BF_Crane_Steel',
    'SM_HQUnload_Jaw': 'M_BF_Crane_Steel',
    'SM_HQUnload_Crystal': 'MM_Crystal_Emissive',
    'SM_HQUnload_Ingot': 'M_Metal_Burnished_Steel',
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


def import_mesh(name):
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
    mat = find_asset(MATERIALS[name])
    if mesh and mat:
        mesh.set_material(0, mat)
    if mesh:
        box = mesh.get_bounding_box()
        log('mesh %s min=%s max=%s material=%s' % (name, box.min, box.max, mat.get_path_name() if mat else None))
    else:
        log('FAILED import %s' % name)
    return mesh


def make_station_bp(meshes):
    path = DST + '/BP_HQUnloadStation'
    bp = unreal.load_asset(path)
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.ResourceUnloadStation)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_HQUnloadStation', DST, unreal.Blueprint,
                                                                     factory)
    cdo = unreal.get_default_object(bp.generated_class())
    for prop, mesh in (('rail_mesh', 'SM_HQUnload_Rail'), ('trolley_body_mesh', 'SM_HQUnload_TrolleyBody'),
                       ('rope_mesh', 'SM_HQUnload_Rope'), ('grab_head_mesh', 'SM_HQUnload_GrabHead'),
                       ('jaw_mesh', 'SM_HQUnload_Jaw'), ('crystal_mesh', 'SM_HQUnload_Crystal'),
                       ('bar_mesh', 'SM_HQUnload_Ingot')):
        cdo.set_editor_property(prop, meshes[mesh])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log('station bp %s' % bp.get_path_name())
    return bp


def hook_truck(bp):
    truck = unreal.load_asset(HQ_TRUCK)
    cdo = unreal.get_default_object(truck.generated_class())
    cdo.set_editor_property('m_unload_station_class', bp.generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(truck)
    cdo = unreal.get_default_object(truck.generated_class())
    log('truck unload class = %s' % cdo.get_editor_property('m_unload_station_class'))
    return truck


meshes = {name: import_mesh(name) for name in MESHES}
bp = make_station_bp(meshes)
truck = hook_truck(bp)
for asset in list(meshes.values()) + [bp, truck]:
    ok = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log('saved %s %s' % (asset.get_path_name(), ok))
log('DONE')
