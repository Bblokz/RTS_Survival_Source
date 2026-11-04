"""Test set-up for the Breakthrough Experimental Units Factory and its K6d Seilpylon gantry crane (run with the UECP
execute_uecp_script action after ue_experimental_factory_import.py; the montage and the Blueprint pins go through uecp actions).

  ABP_BF_EUF_Gantry           a copy of the HQ rail crane's AnimBP (Idle state -> DefaultSlot) on the gantry skeleton, idling in
                              A_EUF_Gantry_Idle.
  AM_EUF_Gantry_Working       the working loop: section Work (A_EUF_Gantry_Work, loops to itself) and Stop (A_EUF_Gantry_Idle). Work
                              starts and ends in the Idle pose, so the crane never jumps; NomadicSkeletalAttachment plays Stop when
                              training stops.
  Attachment_BF_EUF_Gantry    a copy of the HQ's Attachment_BF_HQ_RailCrane (NomadicAttachmentSkeletal) on the gantry mesh, its
                              AnimBP and montage, no slewing (rotation bone gantry, 0 degrees).
  Attachment_BF_EUF_Dressing  the lamps mesh and the moving parts (fans on Fan1/Fan2, the exhaust rain flap on ExhaustFlap1), on
                              the Nav socket (the mesh origin).
  BF_ExperimentalFactory_Truck  a copy of GerT3/Ger_Experimental_Truck (same truck, data and training; the German truck is
                              untouched) on SM_BF_ExperimentalFactory and its _Preview: the gantry on Crane1, the dressing on Nav,
                              nav blockers on Nav1-3 sized from SM_BF_ExperimentalFactory_NavBoxes.json. Its other attachments
                              (welder crane on Crane2, flag pole) find no socket and are skipped.

Copy Model/SM_BF_ExperimentalFactory_Sockets.json and _NavBoxes.json next to this script after a Blender rebuild.
Safe to rerun: existing assets are updated in place, so references to them stay valid.
"""
import json

import unreal

ROOT = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughExperimentalFactory'
MESH = ROOT + '/Mesh'
CRANES = ROOT + '/Cranes'
TRUCK = ROOT + '/BF_ExperimentalFactory_Truck'
GER_TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT3/Ger_Experimental_Truck'
HQ_RAIL = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughHQ'
HQ_ABP = HQ_RAIL + '/Cranes/Rail/ABP_BF_RailCrane'
HQ_CRANE = HQ_RAIL + '/Attachments/Attachment_BF_HQ_RailCrane'
GANTRY = CRANES + '/SK_BF_ExperimentalFactory_Gantry'
ABP = CRANES + '/ABP_BF_EUF_Gantry'
MONTAGE = CRANES + '/AM_EUF_Gantry_Working'
WORK, IDLE = CRANES + '/A_EUF_Gantry_Work', CRANES + '/A_EUF_Gantry_Idle'
CRANE_BP = ROOT + '/Attachment_BF_EUF_Gantry'
DRESSING_BP = ROOT + '/Attachment_BF_EUF_Dressing'
TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/'
SOCKETS = TOOLS + 'SM_BF_ExperimentalFactory_Sockets.json'
NAV_BOXES = TOOLS + 'SM_BF_ExperimentalFactory_NavBoxes.json'
MOVING_PARTS = (('Fan1', 'SM_BF_ExperimentalFactory_Fan', 'Fan1'), ('Fan2', 'SM_BF_ExperimentalFactory_Fan', 'Fan2'),
                ('ExhaustFlap1', 'SM_BF_ExperimentalFactory_ExhaustFlap', 'ExhaustFlap1'))
ASSETS = unreal.EditorAssetLibrary
SUBOBJECTS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
UECP = globals().get('uecp')
if UECP is None:
    raise RuntimeError('run with execute_uecp_script: the montage and the Blueprint pins need the uecp actions')


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):    # a save can report success and still miss the disk
        raise RuntimeError('not on disk ' + disk)


def duplicate(source, target):
    """Never deletes: a class check on a Blueprint held by a running PIE once read false and deleted a working copy."""
    if not ASSETS.does_asset_exist(target):
        if ASSETS.duplicate_asset(source, target) is None:
            raise RuntimeError(f'could not copy {source} to {target}')
    return unreal.load_asset(target)


def component(bp, cls, name):
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
    return library.get_object_for_blueprint(library.get_data(handle), bp)


def quiet(comp):
    """Looks only: no collision, no effect on the navmesh, which the truck's nav boxes and the main mesh own."""
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)


def pins(path, graph, search):
    return UECP.blueprint.get_graph_snapshot(blueprint_path=path, graph_name=graph, view='compact', search=search)['nodes']


def set_pin(path, graph, node, pin, value):
    UECP.blueprint.set_pin_default(blueprint_path=path, graph_name=graph, node_id=node['id'], pin_name=pin, value=value)


def anim_blueprint():
    abp = duplicate(HQ_ABP, ABP)
    abp.set_editor_property('target_skeleton', unreal.load_asset(GANTRY + '_Skeleton'))
    nodes = UECP.animation.get_anim_graph_nodes(anim_blueprint_path=ABP, include_nested=True)['nodes']
    player = next(n for n in nodes if n['node_class'] == 'AnimGraphNode_SequencePlayer')
    UECP.animation.set_anim_node_property(anim_blueprint_path=ABP, node_guid=player['node_guid'], property_name='Sequence',
                                          value=IDLE + '.A_EUF_Gantry_Idle')
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    save(ABP)
    print('ABP', ABP, 'skeleton', abp.get_editor_property('target_skeleton').get_name())


def montage():
    """Work (loops) then Stop. A rerun keeps the existing montage: the import replaced its sequences in place."""
    if not ASSETS.does_asset_exist(MONTAGE):
        UECP.animation.create_montage(source_animation=WORK, name=MONTAGE.split('/')[-1], save_path=CRANES)
        work = unreal.load_asset(WORK).get_play_length()
        UECP.animation.link_montage_slot(montage_path=MONTAGE, slot_name='DefaultSlot', animation_path=IDLE, start_time=work)
        UECP.animation.rename_montage_section(montage_path=MONTAGE, old_name='Default', new_name='Work')
        UECP.animation.add_montage_section(montage_path=MONTAGE, section_name='Stop', start_time=work)
        UECP.animation.set_montage_section_link(montage_path=MONTAGE, section_name='Work', next_section_name='Work')
    save(MONTAGE)
    print('montage', json.dumps(UECP.animation.get_montage_summary(montage_path=MONTAGE).get('sections')))


def crane():
    bp = duplicate(HQ_CRANE, CRANE_BP)
    comp = component(bp, unreal.SkeletalMeshComponent, 'Crane')
    comp.set_editor_property('skeletal_mesh_asset', unreal.load_asset(GANTRY))
    comp.set_editor_property('anim_class', unreal.load_asset(ABP).generated_class())
    for node in pins(CRANE_BP, 'EventGraph', 'NomadicMontageEntry'):
        set_pin(CRANE_BP, 'EventGraph', node, 'Montage', MONTAGE + '.AM_EUF_Gantry_Working')
    for node in pins(CRANE_BP, 'EventGraph', 'NomadicMontageSettings'):
        set_pin(CRANE_BP, 'EventGraph', node, 'RotationBoneName', 'gantry')    # no slewing: 0 degrees either way
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(CRANE_BP)
    print('made', CRANE_BP)


def dressing():
    if ASSETS.does_asset_exist(DRESSING_BP):
        bp = unreal.load_asset(DRESSING_BP)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.Actor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(DRESSING_BP.split('/')[-1], ROOT, unreal.Blueprint, factory)
    lamps = component(bp, unreal.StaticMeshComponent, 'Lamps')
    lamps.set_editor_property('static_mesh', unreal.load_asset(f'{MESH}/SM_BF_ExperimentalFactory_Lamps'))
    quiet(lamps)
    rows = json.loads(open(SOCKETS, encoding='utf-8').read())
    for name, mesh, socket in MOVING_PARTS:      # pivots at the hub and the hinge, so MF_BF_Spin and MF_BF_Rattle turn them in place
        comp = component(bp, unreal.StaticMeshComponent, name)
        comp.set_editor_property('static_mesh', unreal.load_asset(f'{MESH}/{mesh}'))
        comp.set_editor_property('relative_location', unreal.Vector(*rows[socket]['location_cm']))
        comp.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=rows[socket]['yaw']))
        quiet(comp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(DRESSING_BP)
    print('made', DRESSING_BP)


def truck():
    duplicate(GER_TRUCK, TRUCK)
    graph = 'SetupVehicleSpecificSettings'
    setup = next(n for n in pins(TRUCK, graph, 'BP_SetupNomadicVehicle') if n.get('detail', {}).get('function') == 'BP_SetupNomadicVehicle')
    set_pin(TRUCK, graph, setup, 'NewBuildingMesh', f'{MESH}/SM_BF_ExperimentalFactory.SM_BF_ExperimentalFactory')
    set_pin(TRUCK, graph, setup, 'NewPreviewMesh', f'{MESH}/SM_BF_ExperimentalFactory_Preview.SM_BF_ExperimentalFactory_Preview')
    for node in pins(TRUCK, graph, 'BuildingAttachment'):
        if node.get('detail', {}).get('struct') != 'BuildingAttachment':
            continue
        values = {p['name']: p.get('default', '') for p in node['pins']}
        if values.get('SocketName') in ('Crane', 'Crane1'):            # the German double crane: our gantry
            set_pin(TRUCK, graph, node, 'ActorToSpawn', CRANE_BP + '.Attachment_BF_EUF_Gantry_C')
            set_pin(TRUCK, graph, node, 'SocketName', 'Crane1')
        elif values.get('SocketName') in ('Crane2', 'Nav'):            # the welder crane: the dressing on the mesh origin
            set_pin(TRUCK, graph, node, 'ActorToSpawn', DRESSING_BP + '.Attachment_BF_EUF_Dressing_C')
            set_pin(TRUCK, graph, node, 'SocketName', 'Nav')
    boxes = json.loads(open(NAV_BOXES, encoding='utf-8').read())
    for node in pins(TRUCK, graph, 'BuildingNavModifierAttachment'):
        if node.get('detail', {}).get('struct') != 'BuildingNavModifierAttachment':
            continue
        socket = {p['name']: p.get('default', '') for p in node['pins']}.get('SocketName')
        if socket in boxes:
            set_pin(TRUCK, graph, node, 'Scale', ','.join(f'{v:.6f}' for v in boxes[socket]['scale']))
    bp = unreal.load_asset(TRUCK)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(TRUCK)
    print('made', TRUCK)


anim_blueprint()
montage()
crane()
dressing()
truck()
