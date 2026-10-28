"""Imports the field trench kit (CustomMapItems/GerDefenses/FieldTrenches/generate_field_trenches.py) and makes
BP_FieldTrench, the hand-placeable trench piece the cover finder reads.

  Meshes/SM_FieldTrench_*   one static mesh per kit piece: German pit materials (pallet atlas, sandbags), complex
                            collision, LOD group LargeProp, no Nanite; cargo_N sockets (trench stand-up cover)
  BP_FieldTrench            child of BP_TrenchMaster (socket cover with soldiers snapped to the sockets), one
                            StaticMeshComponent "Mesh" (default the 250 straight; pick another piece per instance).
                            Collision matches BP_CargoTrench's mesh on every channel (custom ones too), and it does not affect
                            navigation: infantry and tanks keep the landscape's navmesh through the trench.

Socket rotations: the FBX importer turns the SOCKET_ empties of an axis X / Z export to pitch 90 (forward axis up),
and the cover finder reads the facing from the forward axis. After every import the sockets are therefore set to a
pure yaw from field_trench_kit.json (copy it here from the Blender Exports folder after regenerating) (Unreal yaw = -Blender yaw, Y is mirrored), and their locations are
checked against the manifest.

Run inside the editor (execute_python) with STEPS set: 'meshes', 'blueprint', 'report'.
"""
import json

import unreal

BLENDER_EXPORTS = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/FieldTrenches/Exports'
# A copy of Exports/field_trench_kit.json: the editor's sandboxed Python only reads project files.
MANIFEST = 'D:/UE5Projects/RTS_Survival/Source/Tools/FieldTrenches/field_trench_kit.json'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench'
MESH_DIR = ROOT + '/Meshes'
BLUEPRINT = ROOT + '/BP_FieldTrench'
PARENT = '/Game/RTS_Survival/Blueprints/Environment/Trenches/BP_TrenchMaster.BP_TrenchMaster_C'
CARGO_TRENCH = '/Game/RTS_Survival/Blueprints/Environment/Trenches/BP_CargoTrench'
DEFAULT_PIECE = 'SM_FieldTrench_Straight_250_A'
PIECES = ['SM_FieldTrench_Straight_125', 'SM_FieldTrench_Straight_250_A', 'SM_FieldTrench_Straight_250_B',
          'SM_FieldTrench_Straight_250_C', 'SM_FieldTrench_Straight_500_A', 'SM_FieldTrench_Straight_500_B',
          'SM_FieldTrench_Bend_15', 'SM_FieldTrench_Bend_30', 'SM_FieldTrench_Bend_45', 'SM_FieldTrench_Corner_90',
          'SM_FieldTrench_Tee', 'SM_FieldTrench_Cross', 'SM_FieldTrench_End']
MATERIALS = {
    'GerPit_Atlas': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas',
    'GerPit_Sandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def _static_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    data = options.static_mesh_import_data
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def fix_sockets(mesh, record):
    """Pure-yaw socket rotations from the manifest; raises when a socket is missing or misplaced."""
    for entry in record['sockets']:
        socket = mesh.find_socket(entry['name'])
        if socket is None:
            raise RuntimeError(f"{record['name']}: socket {entry['name']} missing after import")
        x, y, z = entry['location']
        location = socket.relative_location
        error = max(abs(location.x - x), abs(location.y + y), abs(location.z - z))
        if error > 0.05:
            raise RuntimeError(f"{record['name']} {entry['name']}: at {location}, expected ({x}, {-y}, {z})")
        socket.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=-entry['yaw']))
        socket.set_editor_property('relative_scale', unreal.Vector(1.0, 1.0, 1.0))
    mesh.modify()


def import_meshes():
    with open(MANIFEST) as handle:
        records = {p['name']: p for p in json.load(handle)['pieces']}
    for name in PIECES:
        task = unreal.AssetImportTask()
        task.filename = f'{BLENDER_EXPORTS}/{name}.fbx'
        task.destination_path = MESH_DIR
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.options = _static_options()
        asset_tools.import_asset_tasks([task])
        path = f'{MESH_DIR}/{name}'
        mesh = unreal.load_asset(path)
        if mesh is None:
            raise RuntimeError('Import produced nothing for ' + name)
        for index, slot in enumerate(mesh.static_materials):
            target = MATERIALS.get(str(slot.material_slot_name))
            if target:
                mesh.set_material(index, unreal.load_asset(target))
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_editor_property('lod_group', 'LargeProp')
        fix_sockets(mesh, records[name])
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(
            mesh, unreal.MeshNaniteSettings(enabled=False), apply_changes=True)
        # StaticMesh.Sockets is protected from Python; the uecp action list_mesh_sockets reads them.
        print('FIELDTRENCH_IMPORT', name, 'tris', mesh.get_num_triangles(0), 'cargo_1',
              mesh.find_socket('cargo_1') is not None, 'saved', eal.save_asset(path, only_if_is_dirty=False))


def make_blueprint():
    if not eal.does_asset_exist(BLUEPRINT):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.load_object(None, PARENT))
        asset_tools.create_asset('BP_FieldTrench', ROOT, unreal.Blueprint, factory)
    blueprint = unreal.load_asset(BLUEPRINT)
    mesh = _template(blueprint, 'StaticMeshComponent')
    if mesh is None:
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        root = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)[0]
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=blueprint))
        subsystem.rename_subobject_member_variable(blueprint, handle, 'Mesh')
        mesh = _template(blueprint, 'StaticMeshComponent')
    mesh.modify()
    mesh.set_static_mesh(unreal.load_asset(f'{MESH_DIR}/{DEFAULT_PIECE}'))
    mesh.set_editor_property('can_ever_affect_navigation', False)
    # Collision copied channel by channel from BP_CargoTrench's mesh: the project's custom channels (PlayerObj,
    # EnemyObj, Landscape, ...) default to other responses, and with those a soldier leaving a socket landed 33 cm
    # off his navmesh point.
    reference = _template(unreal.load_asset(CARGO_TRENCH), 'StaticMeshComponent')
    mesh.set_collision_enabled(reference.get_collision_enabled())
    mesh.set_collision_object_type(reference.get_collision_object_type())
    channels = [getattr(unreal.CollisionChannel, n) for n in dir(unreal.CollisionChannel) if n.startswith('ECC_')]
    for channel in channels:
        mesh.set_collision_response_to_channel(channel, reference.get_collision_response_to_channel(channel))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    different = [str(c) for c in channels
                 if mesh.get_collision_response_to_channel(c) != reference.get_collision_response_to_channel(c)]
    print('FIELDTRENCH_BP configured; channels differing from BP_CargoTrench:', different,
          'saved', eal.save_asset(BLUEPRINT, only_if_is_dirty=False))


def _template(blueprint, class_name):
    """The blueprint's own component template of that class (not inherited ones)."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        component = library.get_object(library.get_data(handle))
        if component is not None and component.get_class().get_name() == class_name:
            return component
    return None


def report():
    for name in PIECES:
        mesh = unreal.load_asset(f'{MESH_DIR}/{name}')
        sockets = []
        for k in range(1, 20):
            s = mesh.find_socket(f'cargo_{k}')
            if s is not None:
                r = s.relative_rotation
                forward = r.get_forward_vector()
                sockets.append((f'cargo_{k}', [round(v, 2) for v in (s.relative_location.x, s.relative_location.y,
                                                                      s.relative_location.z)],
                                [round(r.pitch, 2), round(r.yaw, 2), round(r.roll, 2)],
                                [round(forward.x, 3), round(forward.y, 3), round(forward.z, 3)]))
        bounds = mesh.get_bounding_box()
        print('FIELDTRENCH_REPORT', name, 'tris', mesh.get_num_triangles(0),
              'nanite', mesh.get_editor_property('nanite_settings').enabled,
              'materials', [str(m.material_interface.get_name()) if m.material_interface else None
                            for m in mesh.static_materials],
              'bounds', [round(v, 1) for v in (bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y,
                                               bounds.max.z)],
              'sockets', sockets)


_steps = globals().get('STEPS') or ['meshes', 'blueprint', 'report']
if 'meshes' in _steps:
    import_meshes()
if 'blueprint' in _steps:
    make_blueprint()
if 'report' in _steps:
    report()
