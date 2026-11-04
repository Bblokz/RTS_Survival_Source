"""Attachment Blueprints for the Breakthrough Mechanized Depot test truck (run with the UECP execute_python action
after ue_mech_depot_import.py).

  Attachment_BF_MechDepot_Dressing: the four dressing meshes, spawned by the truck on the mesh's Dressing socket (origin),
  plus the generator fan and the two exhaust rain flaps, which move in their materials (ue_bf_motion_materials.py).
  Attachment_BF_MechDepot_Crane: a copy of the HQ's rail crane attachment (NomadicAttachmentSkeletal) on ABP_BF_RailCrane_Depot
  and AM_RailCrane_Depot_Working, spawned by the truck on socket CraneStow. Run ue_rail_crane_depot_import.py first: it
  imports the depot actions and makes the AnimBP and montage. The crane works while the depot trains, like the HQ's.

Run with the UECP execute_uecp_script action (the crane's montage entries and the truck's socket are pin defaults).
Safe to rerun: an existing Blueprint is updated in place, so the truck's references to it stay valid.
"""
import json

import unreal

ROOT ='/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughMechDepot'
MESH = ROOT + '/Mesh'
TRUCK = ROOT + '/BF_MechanizedDepot_Truck'
CRANE = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughHQ/Cranes/Rail'
HQ_CRANE = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughHQ/Attachments/Attachment_BF_HQ_RailCrane'
DEPOT_MONTAGE = CRANE + '/AM_RailCrane_Depot_Working.AM_RailCrane_Depot_Working'
SOCKETS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/SM_BF_MechDepot_Sockets.json'
# (component, mesh, socket): pivots at the hub and the hinge, so MF_BF_Spin and MF_BF_Rattle turn them in place
MOVING_PARTS = (('GenFan', 'SM_BF_MechDepot_Fan', 'GenFan'),
                ('ExhaustFlap1', 'SM_BF_MechDepot_ExhaustFlap', 'ExhaustFlap1'),
                ('ExhaustFlap2', 'SM_BF_MechDepot_ExhaustFlap', 'ExhaustFlap2'))
ASSETS = unreal.EditorAssetLibrary
SUBOBJECTS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def save(path):
    if not ASSETS.save_loaded_asset(unreal.load_asset(path), False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):
        raise RuntimeError('not on disk ' + disk)


def actor_blueprint(name):
    path = f'{ROOT}/{name}'
    if ASSETS.does_asset_exist(path):
        return unreal.load_asset(path), path
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.Actor)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError('could not make ' + path)
    return bp, path


def add_component(bp, cls, name):
    """The Blueprint's component template called name, made under the root when it is missing."""
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = SUBOBJECTS.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles:
        data = library.get_data(handle)
        if str(library.get_variable_name(data)) == name:
            return library.get_object_for_blueprint(data, bp)
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=cls, blueprint_context=bp)
    handle, fail = SUBOBJECTS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f'add {name}: {fail}')
    SUBOBJECTS.rename_subobject(handle, unreal.Text(name))
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)


def quiet(component):
    """Looks only: no collision, no effect on the navmesh, which the truck's nav boxes and the main mesh own."""
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    component.set_editor_property('generate_overlap_events', False)


def dressing():
    bp, path = actor_blueprint('Attachment_BF_MechDepot_Dressing')
    for part in ('Sandbags', 'Crates', 'FieldSupply', 'Tanks'):
        comp = add_component(bp, unreal.StaticMeshComponent, part)
        comp.set_editor_property('static_mesh', unreal.load_asset(f'{MESH}/SM_BF_MechDepot_{part}'))
        quiet(comp)
    # Moving parts, on their sockets' transforms: the attachment sits on the Dressing socket, which is the mesh origin.
    rows = json.loads(open(SOCKETS, encoding='utf-8').read())
    for name, mesh, socket in MOVING_PARTS:
        comp = add_component(bp, unreal.StaticMeshComponent, name)
        comp.set_editor_property('static_mesh', unreal.load_asset(f'{MESH}/{mesh}'))
        where = rows[socket]['location_cm']
        comp.set_editor_property('relative_location', unreal.Vector(*where))
        comp.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=rows[socket]['yaw']))
        quiet(comp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    print('made', path)


def crane():
    """A copy of the HQ's Attachment_BF_HQ_RailCrane on the depot AnimBP and montage. Needs execute_uecp_script (the montage
    entries are pin defaults, which only the uecp.blueprint actions can set); under execute_python it stops before the pins."""
    path = f'{ROOT}/Attachment_BF_MechDepot_Crane'
    # Never deletes: a class check on a Blueprint held by a running PIE read false once and deleted the working crane.
    if not ASSETS.does_asset_exist(path):
        if ASSETS.duplicate_asset(HQ_CRANE, path) is None:
            raise RuntimeError('could not copy ' + HQ_CRANE)
    bp = unreal.load_asset(path)
    comp = add_component(bp, unreal.SkeletalMeshComponent, 'Crane')
    comp.set_editor_property('anim_class', unreal.load_asset(f'{CRANE}/ABP_BF_RailCrane_Depot').generated_class())
    uecp = globals().get('uecp')
    if uecp is None:
        raise RuntimeError('run with execute_uecp_script to point the montage entries at ' + DEPOT_MONTAGE)
    snap = uecp.blueprint.get_graph_snapshot(blueprint_path=path, graph_name='EventGraph', view='compact', search='NomadicMontageEntry')
    for node in snap['nodes']:
        uecp.blueprint.set_pin_default(blueprint_path=path, graph_name='EventGraph', node_id=node['id'], pin_name='Montage',
                                       value=DEPOT_MONTAGE)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    print('made', path, 'montage entries', len(snap['nodes']))


def truck_crane_socket():
    """The truck spawns the crane on CraneStow, the track end: the depot actions carry it from there to socket Crane."""
    uecp = globals().get('uecp')
    if uecp is None:
        return
    snap = uecp.blueprint.get_graph_snapshot(blueprint_path=TRUCK, graph_name='SetupVehicleSpecificSettings', view='compact',
                                             search='BuildingAttachment')
    for node in snap['nodes']:
        pins = {p['name']: p.get('default', '') for p in node['pins']}
        # after crane() replaced the old Blueprint the class pin can read empty, so the old socket name finds the entry too
        if 'Attachment_BF_MechDepot_Crane' in (pins.get('ActorToSpawn') or '') or pins.get('SocketName') in ('Crane', 'CraneStow'):
            uecp.blueprint.set_pin_default(blueprint_path=TRUCK, graph_name='SetupVehicleSpecificSettings', node_id=node['id'],
                                           pin_name='ActorToSpawn', value=f'{ROOT}/Attachment_BF_MechDepot_Crane.Attachment_BF_MechDepot_Crane_C')
            uecp.blueprint.set_pin_default(blueprint_path=TRUCK, graph_name='SetupVehicleSpecificSettings', node_id=node['id'],
                                           pin_name='SocketName', value='CraneStow')
    bp = unreal.load_asset(TRUCK)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(TRUCK)
    print('truck crane on CraneStow')


dressing()
crane()
truck_crane_socket()
