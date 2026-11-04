"""Imports the Breakthrough Experimental Units Factory (run with the UECP execute_python action).

Blender source, socket table and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/
ExperimentalUnitsFactory/Model/README.md. Materials are the faction's shared ones; the net, festoon cables, tarp, windsock and pennant
move on the weights in UV channels 2 and 3, so the main mesh's trim and net slots take the motion instances.
The lamps come after, from ue_building_lamps_import.py (BUILDINGS_TO_RUN = ['ExperimentalFactory']).
The K6d Seilpylon gantry crane (ExperimentalUnitsFactory/Cranes/Model/README.md) comes in with it: SK_BF_ExperimentalFactory_Gantry
and its skeleton from the crane FBX, then the same FBX's two takes, animation only, onto that skeleton.

Safe to rerun after a Blender rebuild: every mesh reimports over the same asset, the placement preview is copied again, and each
new crane sequence replaces the old one in every reference before it takes its name.
"""
import json

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/ExperimentalUnitsFactory/Model/'
CRANE_MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/ExperimentalUnitsFactory/Cranes/Model/'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
HOME = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughExperimentalFactory/Mesh'
CRANES = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughExperimentalFactory/Cranes'
SCRATCH = '/Game/__ExperimentalFactoryCraneImport'
GANTRY = 'SK_BF_ExperimentalFactory_Gantry'
CRANE_ACTIONS = ('A_EUF_Gantry_Idle', 'A_EUF_Gantry_Work')
# The Work loop runs the crane 1.9 m forward and 0.35 m back along the tracks: grow the bounds that way on both horizontal axes
# so it doesn't cull mid-trip, whatever the attachment's yaw.
CRANE_BOUNDS_CM = unreal.Vector(250, 250, 0)
MAIN = 'SM_BF_ExperimentalFactory'
MOVING = ('ExhaustFlap', 'Fan')     # pivot at the hinge or hub, on ExhaustFlap1 and Fan1/Fan2
MOTION = SHARED + '/Motion'         # wind and machine motion instances, made by ue_bf_motion_materials.py
PREVIEW = MAIN + '_Preview'
PREVIEW_MATERIAL = '/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_ConstructionPrev'
# A copy of Model/SM_BF_ExperimentalFactory_Sockets.json: the editor's Python may only open files inside the project. Copy it again after a rebuild.
SOCKETS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/SM_BF_ExperimentalFactory_Sockets.json'
MATERIALS = dict(M_BF_SootedFortress_Concrete=SHARED + '/MI_BF_SootedFortress_UVTile', M_BF_Trim=SHARED + '/M_BF_Trim',
                 M_BF_CamoNet=SHARED + '/M_BF_CamoNet')
MOVING_MATERIALS = {MAIN: dict(M_BF_Trim=MOTION + '/MI_BF_Trim_Flutter', M_BF_CamoNet=MOTION + '/MI_BF_CamoNet_Flutter'),
                    MAIN + '_ExhaustFlap': dict(M_BF_Trim=MOTION + '/MI_BF_Trim_Rattle'),
                    MAIN + '_Fan': dict(M_BF_Trim=MOTION + '/MI_BF_Trim_Spin')}
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary


def save(path):
    if not ASSETS.save_loaded_asset(unreal.load_asset(path), False):
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
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False    # AssetImportTask(save=True) silently misses the disk here
    task.replace_existing = True
    task.options = ui
    ASSET_TOOLS.import_asset_tasks([task])


def finish_mesh(name):
    path = f'{HOME}/{name}'
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('import made nothing at ' + path)
    slots = mesh.get_editor_property('static_materials')
    materials = dict(MATERIALS, **MOVING_MATERIALS.get(name, {}))
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in materials if slot_name.startswith(k)), None)    # Blender may add a suffix
        if key is None:
            raise RuntimeError(f'no material for slot {slot_name} on {name}')
        mesh.set_material(index, unreal.load_asset(materials[key]))    # setting it on the struct only changes a copy
    mesh.set_editor_property('light_map_coordinate_index', 1)
    save(path)
    return mesh


def set_sockets(mesh):
    """FBX empties arrive tilted by 90 degrees; the positions are right, the yaws come from the json."""
    rows = json.loads(open(SOCKETS, encoding='utf-8').read())
    for name, row in rows.items():
        socket = mesh.find_socket(name)
        if socket is None:
            raise RuntimeError('missing socket ' + name)
        socket.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=row['yaw']))
        socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
        where = socket.get_editor_property('relative_location')
        off = max(abs(where.x - row['location_cm'][0]), abs(where.y - row['location_cm'][1]), abs(where.z - row['location_cm'][2]))
        if off > 1.0:
            raise RuntimeError(f'socket {name} is {off:.1f} cm off the json')
    save(f'{HOME}/{MAIN}')
    print('sockets set', len(rows))


def save_package(path):
    """For new assets: save_loaded_asset reports False on some new packages here; save_packages doesn't."""
    obj = unreal.load_asset(path)
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):
        raise RuntimeError('not on disk ' + disk)


def crane_task(skeleton, with_mesh, destination, name):
    """Legacy FBX import of the crane: its mesh and skeleton (with_mesh), or its takes only onto skeleton."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', with_mesh)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('import_animations', not with_mesh)
    ui.set_editor_property('mesh_type_to_import',
                           unreal.FBXImportType.FBXIT_SKELETAL_MESH if with_mesh else unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property('original_import_type', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('create_physics_asset', False)
    if skeleton is not None:
        ui.set_editor_property('skeleton', skeleton)
    data = ui.get_editor_property('skeletal_mesh_import_data')
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    anim = ui.get_editor_property('anim_sequence_import_data')
    anim.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    anim.set_editor_property('remove_redundant_keys', False)
    task = unreal.AssetImportTask()
    task.filename = CRANE_MODEL + GANTRY + '.FBX'
    task.destination_path = destination
    if name:
        task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        ASSET_TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')


def take_name(target, new):
    """Gives a freshly imported sequence its final name; on a rerun the old sequence's references move to the new one first."""
    old = unreal.load_asset(target)
    referencers = []
    if old is not None:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        referencers = [str(p) for p in registry.get_referencers(target, unreal.AssetRegistryDependencyOptions()) or []]
        if not ASSETS.consolidate_assets(new, [old]):
            raise RuntimeError('could not replace ' + target)
        ASSETS.delete_asset(target)     # removes only the redirector consolidate leaves behind
    if not ASSETS.rename_loaded_asset(new, target):
        raise RuntimeError('could not move to ' + target)
    save_package(target)
    if referencers:
        packages = [unreal.load_package(p) for p in referencers]
        if not unreal.EditorLoadingAndSavingUtils.save_packages([p for p in packages if p], False):
            raise RuntimeError(f'could not save the referencers of {target}: {referencers}')


def crane():
    """The gantry's mesh and skeleton, its two takes onto that skeleton (via a scratch folder: the legacy importer names them
    <file>_Anim_Armature_<action> and brings a mesh copy along), then the HQ cranes' steel materials and grown bounds."""
    path = f'{CRANES}/{GANTRY}'
    crane_task(unreal.load_asset(f'{path}_Skeleton'), True, CRANES, GANTRY)
    skeleton = unreal.load_asset(f'{path}_Skeleton')
    if skeleton is None:
        raise RuntimeError('the crane FBX made no skeleton')
    crane_task(skeleton, False, SCRATCH, None)
    found = {a: unreal.load_asset(f'{SCRATCH}/{GANTRY}_Anim_Armature_{a}') for a in CRANE_ACTIONS}
    found = {a: seq for a, seq in found.items() if isinstance(seq, unreal.AnimSequence)}
    if sorted(found) != sorted(CRANE_ACTIONS):
        raise RuntimeError(f'expected {CRANE_ACTIONS}, imported {sorted(found)}')
    for action in CRANE_ACTIONS:
        take_name(f'{CRANES}/{action}', found[action])
        print(action, 'length', round(unreal.load_asset(f'{CRANES}/{action}').get_play_length(), 3))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([SCRATCH], True)
    for data in registry.get_assets_by_path(SCRATCH, recursive=True):
        ASSETS.delete_loaded_asset(data.get_asset())
    mesh = unreal.load_asset(path)
    slots = []
    for slot in mesh.get_editor_property('materials'):     # copies: edit them, then write the whole list back
        name = str(slot.get_editor_property('material_slot_name'))
        material = unreal.load_asset(f'{SHARED}/{name}')    # the HQ cranes' M_BF_Crane_PaintedSteel and M_BF_Crane_WornSteel
        if material is None:
            raise RuntimeError(f'no material {name} in {SHARED}')
        slot.set_editor_property('material_interface', material)
        slots.append(slot)
    mesh.set_editor_property('materials', slots)
    mesh.set_editor_property('positive_bounds_extension', CRANE_BOUNDS_CM)
    mesh.set_editor_property('negative_bounds_extension', CRANE_BOUNDS_CM)
    save_package(path)
    save_package(f'{path}_Skeleton')
    box = mesh.get_bounds().box_extent
    print(GANTRY, 'extent', [round(v) for v in (box.x, box.y, box.z)],
          'materials', [str(m.get_editor_property('material_interface').get_name()) for m in mesh.get_editor_property('materials')])


def make_preview():
    """Placement ghost for the truck: the main mesh with every slot on the construction preview material."""
    path = f'{HOME}/{PREVIEW}'
    if ASSETS.does_asset_exist(path):
        ASSETS.delete_asset(path)
    if ASSETS.duplicate_asset(f'{HOME}/{MAIN}', path) is None:
        raise RuntimeError('preview copy failed')
    mesh = unreal.load_asset(path)
    ghost = unreal.load_asset(PREVIEW_MATERIAL)
    for index in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(index, ghost)
    save(path)
    return mesh


def report(mesh):
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    print(mesh.get_name(), 'tris', mesh.get_num_triangles(0), 'convex', hulls,
          'bounds', [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
          'materials', [m.get_name() for m in (mesh.get_material(i) for i in range(len(mesh.get_editor_property('static_materials'))))])


def main():
    names = [MAIN] + [f'{MAIN}_{part}' for part in MOVING]
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in names:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    main_mesh = finish_mesh(MAIN)
    set_sockets(main_mesh)
    report(main_mesh)
    for name in names[1:]:
        report(finish_mesh(name))
    report(make_preview())
    crane()


main()
