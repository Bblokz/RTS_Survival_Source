"""Imports the Rus factory A2 (Sdvizhnye) meshes and makes its test Blueprint (run in the editor with UECP execute_python:
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/RusFactoryA2/ue_rusfactory_a2_import.py').read())).

Blender source and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/RusFactory/Model
(RusFactoryA2_Report.txt). Copy Model/rusfactory_a2_ue.json next to this script after a rebuild (the editor's Python may
only read inside the project).

  FactoryA2/SM_Rus_WarFactory_A2       the original bunker + bay, shaft, rails, crane tower (drop-in for SM_Rus_WarFactory:
                                       same pivot and frame; complex collision = the original's SM_Collision_RusWarFactory)
  FactoryA2/SK_RusFactory_A2_Crane     jib + both bay leaves + bones 'load' / 'lorry_load'; sockets TurretHook / TurretLorry
                                       carry the T-34/76 turret meshes (SM_T34_76_TurretBaseMesh + SK_34_76WeaponSystem)
  FactoryA2/A_RusFactoryA2_Work, _Idle the 30 s work loop (starts and ends = Idle) and the idle pose
  Rus_Building_Factory_A2              a copy of Rus_Building_Factory with the A2 mesh and the machinery (crane, turrets, the
                                       project's ZiS-5V lorry). The live Rus_Building_Factory is not touched.

Phase 'assets' (this file's main) imports and makes the sockets' numbers; the Blueprint components and graph pins are set by
the uecp calls listed in the session notes (add_component needs the sockets to exist first).
Safe to rerun: meshes reimport in place, sequences replace the old ones in every reference.
"""
import json

import unreal

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/RusFactoryA2/'
CFG = json.loads(open(TOOLS + 'rusfactory_a2_ue.json', encoding='utf-8').read())
MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/RusFactory/Model/'
RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings'
HOME = RUS + '/FactoryA2'
SCRATCH = '/Game/__RusFactoryA2Import'
STATIC = 'SM_Rus_WarFactory_A2'
CRANE = 'SK_RusFactory_A2_Crane'
SEQUENCES = {'AN_RusFactoryA2_Work': 'A_RusFactoryA2_Work', 'AN_RusFactoryA2_Idle': 'A_RusFactoryA2_Idle'}
ORIGINAL_COLLISION = RUS + '/FactoryMeshes/SM_Collision_RusWarFactory'
SOURCE_BP = RUS + '/Rus_Building_Factory'
# Set RUSA2_TARGET_BP before exec to work on another Blueprint (2026-11-15: the live Rus_Building_Factory took A2 in place).
TARGET_BP = globals().get('RUSA2_TARGET_BP', RUS + '/Rus_Building_Factory')  # the live factory (A2 applied in place; test copy deleted 2026-11-15)
# The boom swings the hook 7-10 m from the tower: grow the crane's bounds so it never culls mid-swing.
CRANE_BOUNDS_CM = unreal.Vector(900, 900, 300)
# Turret mesh heading (Unreal yaw) at each load bone's rest: barrel along the bay on the hook, over the tailgate on the lorry.
SOCKETS = {'TurretHook': ('load', 180.0), 'TurretLorry': ('lorry_load', -90.0)}
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
RESULT = {}


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):
        raise RuntimeError('not on disk ' + disk)


def legacy_import(task):
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        ASSET_TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')


def fbx_task(filename, destination, name, ui):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    if name:
        task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    return task


def import_static():
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('static_mesh_lod_group', 'LargeProp')  # the original's LOD group (4 LODs)
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    legacy_import(fbx_task(MODEL + STATIC + '.FBX', HOME, STATIC, ui))
    path = f'{HOME}/{STATIC}'
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('import made nothing at ' + path)
    materials = CFG['materials']
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in materials if name.startswith(k)), None)
        if key is None:
            raise RuntimeError(f'no material for slot {name}')
        mesh.set_material(index, unreal.load_asset(materials[key]))
    mesh.set_editor_property('light_map_coordinate_index', 2)
    mesh.set_editor_property('complex_collision_mesh', unreal.load_asset(ORIGINAL_COLLISION))
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    save(path)
    box = mesh.get_bounding_box()
    RESULT['static'] = {'tris': mesh.get_num_triangles(0), 'lods': mesh.get_num_lods(),
                        'bounds': [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
                        'materials': [mesh.get_material(i).get_name() for i in range(len(mesh.get_editor_property('static_materials')))]}


def crane_ui(skeleton, with_mesh):
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
    return ui


def take_name(target, new):
    old = unreal.load_asset(target)
    referencers = []
    if old is not None:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        referencers = [str(p) for p in registry.get_referencers(target, unreal.AssetRegistryDependencyOptions()) or []]
        if not ASSETS.consolidate_assets(new, [old]):
            raise RuntimeError('could not replace ' + target)
        ASSETS.delete_asset(target)
    if not ASSETS.rename_loaded_asset(new, target):
        raise RuntimeError('could not move to ' + target)
    save(target)
    if referencers:
        packages = [unreal.load_package(p) for p in referencers]
        unreal.EditorLoadingAndSavingUtils.save_packages([p for p in packages if p], False)


def import_crane():
    fbx = MODEL + CRANE + '_Animated.FBX'
    legacy_import(fbx_task(fbx, HOME, CRANE, crane_ui(unreal.load_asset(f'{HOME}/{CRANE}_Skeleton'), True)))
    skeleton = unreal.load_asset(f'{HOME}/{CRANE}_Skeleton')
    if skeleton is None:
        raise RuntimeError('the crane FBX made no skeleton')
    legacy_import(fbx_task(fbx, SCRATCH, None, crane_ui(skeleton, False)))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([SCRATCH], True)
    found = {}
    for data in registry.get_assets_by_path(SCRATCH, recursive=True):
        asset = data.get_asset()
        if isinstance(asset, unreal.AnimSequence):
            for take in SEQUENCES:
                if str(data.asset_name).endswith(take):
                    found[take] = asset
    if sorted(found) != sorted(SEQUENCES):
        raise RuntimeError(f'expected takes {sorted(SEQUENCES)}, found {sorted(found)}')
    lengths = {}
    for take, final in SEQUENCES.items():
        take_name(f'{HOME}/{final}', found[take])
        lengths[final] = round(unreal.load_asset(f'{HOME}/{final}').get_play_length(), 3)
    registry.scan_paths_synchronous([SCRATCH], True)
    for data in registry.get_assets_by_path(SCRATCH, recursive=True):
        ASSETS.delete_loaded_asset(data.get_asset())
    mesh = unreal.load_asset(f'{HOME}/{CRANE}')
    slots = []
    for index in range(len(mesh.get_editor_property('materials'))):
        slot = mesh.get_editor_property('materials')[index]
        name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in CFG['materials'] if name.startswith(k)), None)
        if key is None:
            raise RuntimeError(f'no material for crane slot {name}')
        slot.set_editor_property('material_interface', unreal.load_asset(CFG['materials'][key]))
        slots.append(slot)
    mesh.set_editor_property('materials', slots)
    mesh.set_editor_property('positive_bounds_extension', CRANE_BOUNDS_CM)
    mesh.set_editor_property('negative_bounds_extension', CRANE_BOUNDS_CM)
    save(f'{HOME}/{CRANE}')
    save(f'{HOME}/{CRANE}_Skeleton')
    RESULT['crane'] = {'lengths': lengths, 'bones': skeleton.get_editor_property('bone_tree') and len(skeleton.get_editor_property('bone_tree')),
                       'materials': [str(s.get_editor_property('material_interface').get_name()) for s in slots]}


def socket_numbers():
    """Socket transforms so the turret mesh sits on the bone head with its barrel along the heading above."""
    skeleton = unreal.load_asset(f'{HOME}/{CRANE}_Skeleton')
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    out = {}
    for socket, (bone, yaw) in SOCKETS.items():
        bone_world = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
        desired = unreal.Transform(bone_world.translation, unreal.Rotator(roll=0, pitch=0, yaw=yaw), unreal.Vector(1, 1, 1))
        rel = unreal.MathLibrary.make_relative_transform(desired, bone_world)
        r = rel.rotation.rotator()
        out[socket] = {'bone': bone, 'bone_world_cm': [round(v, 2) for v in (bone_world.translation.x, bone_world.translation.y,
                                                                              bone_world.translation.z)],
                       'location': [round(v, 4) for v in (rel.translation.x, rel.translation.y, rel.translation.z)],
                       'rotation_pyr': [round(r.pitch, 4), round(r.yaw, 4), round(r.roll, 4)]}
    RESULT['sockets'] = out
    open(TOOLS + 'sockets_computed.json', 'w', encoding='utf-8').write(json.dumps(out, indent=1))


def make_blueprint():
    if not ASSETS.does_asset_exist(TARGET_BP):
        if ASSETS.duplicate_asset(SOURCE_BP, TARGET_BP) is None:
            raise RuntimeError('could not copy the factory Blueprint')
    bp = unreal.load_asset(TARGET_BP)
    subs = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subs.k2_gather_subobject_data_for_blueprint(bp)
    names = {str(lib.get_variable_name(lib.get_data(h))): h for h in handles}
    if 'A2_Machinery' not in names:
        parent = names.get('BuildingMeshComponent', handles[0])
        params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=unreal.SceneComponent, blueprint_context=bp)
        handle, fail = subs.add_new_subobject(params)
        if not fail.is_empty():
            raise RuntimeError(f'add A2_Machinery: {fail}')
        subs.rename_subobject(handle, unreal.Text('A2_Machinery'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(TARGET_BP)
    handles = subs.k2_gather_subobject_data_for_blueprint(bp)
    RESULT['bp_components'] = sorted(str(lib.get_variable_name(lib.get_data(h))) for h in handles)


if not globals().get('RUSA2_BP_ONLY'):  # RUSA2_BP_ONLY: only add A2_Machinery to TARGET_BP
    import_static()
    import_crane()
    socket_numbers()
make_blueprint()
print('RUSFACTORYA2 ' + json.dumps(RESULT))
