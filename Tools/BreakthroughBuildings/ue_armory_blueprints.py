"""Test set-up for the Breakthrough Armory, design H Feldbahn (run with the UECP execute_uecp_script action after
ue_armory_import.py and ue_building_lamps_import.py with BUILDINGS_TO_RUN = ['Armory']; the truck's pins go through uecp actions).

  Attachment_BF_Armory_Dressing  the three ammunition dressing meshes (rounds, FieldSupply, crates) and the lamps, on the Nav socket
                                 (the mesh origin). Looks only: no collision, no navigation.
  BF_Armory_Truck                a copy of GerT2/Armory/Ger_Armory_Truck (same truck, data and subtype; the German truck is untouched)
                                 on SM_BF_Armory and its _Preview. The German truck spawns no attachments and has one nav box on Nav;
                                 this copy gets the dressing on Nav and two nav boxes, Nav1 (both blocks with their aprons) and Nav2
                                 (the yard: railway, wagons, dock), sized from SM_BF_Armory_NavBoxes.json.

Copy Model/SM_BF_Armory_NavBoxes.json next to this script after a Blender rebuild.
Safe to rerun: existing assets are updated in place and the added graph nodes are found again, so references stay valid.
"""
import json

import unreal

ROOT = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughArmory'
MESH = ROOT + '/Mesh'
TRUCK = ROOT + '/BF_Armory_Truck'
GER_TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/Armory/Ger_Armory_Truck'
DRESSING_BP = ROOT + '/Attachment_BF_Armory_Dressing'
DRESSING_CLASS = DRESSING_BP + '.Attachment_BF_Armory_Dressing_C'
NAV_BOXES = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/SM_BF_Armory_NavBoxes.json'
GRAPH = 'SetupVehicleSpecificSettings'
PARTS = ('Rounds', 'FieldSupply', 'Crates', 'Lamps')
ASSETS = unreal.EditorAssetLibrary
SUBOBJECTS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
UECP = globals().get('uecp')
if UECP is None:
    raise RuntimeError('run with execute_uecp_script: the truck pins need the uecp actions')


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):    # a save can report success and still miss the disk
        raise RuntimeError('not on disk ' + disk)


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


def dressing():
    if ASSETS.does_asset_exist(DRESSING_BP):
        bp = unreal.load_asset(DRESSING_BP)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.Actor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(DRESSING_BP.split('/')[-1], ROOT, unreal.Blueprint, factory)
    for part in PARTS:
        comp = component(bp, unreal.StaticMeshComponent, part)
        comp.set_editor_property('static_mesh', unreal.load_asset(f'{MESH}/SM_BF_Armory_{part}'))
        # looks only: the main mesh's UCX and the truck's nav boxes own collision and the navmesh
        comp.set_collision_profile_name('NoCollision')
        comp.set_editor_property('can_ever_affect_navigation', False)
        comp.set_editor_property('generate_overlap_events', False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(DRESSING_BP)
    print('made', DRESSING_BP)


def nodes(search=None):
    args = dict(search=search) if search else {}
    return UECP.blueprint.get_graph_snapshot(blueprint_path=TRUCK, graph_name=GRAPH, view='compact', **args)['nodes']


def feeds(node, target_id, target_pin=None):
    """True when one of node's outputs is wired into target_id (at target_pin, when given)."""
    return any(c['node_id'] == target_id and (target_pin is None or c['pin'] == target_pin)
               for p in node['pins'] for c in p.get('connected_to', []))


def set_pin(node_id, pin, value):
    UECP.blueprint.set_pin_default(blueprint_path=TRUCK, graph_name=GRAPH, node_id=node_id, pin_name=pin, value=value)


def array_feeding(pin_name):
    """The MakeArray node wired into BP_SetupNomadicVehicle's pin_name."""
    setup = next(n for n in nodes('BP_SetupNomadicVehicle') if n.get('detail', {}).get('function') == 'BP_SetupNomadicVehicle')
    for node in nodes():
        if node['node_class'] == 'K2Node_MakeArray' and feeds(node, setup['id'], pin_name):
            return setup, node
    raise RuntimeError('no MakeArray on ' + pin_name)


def struct_entry(struct, tag, array, index, x, y):
    """The Make<struct> node tagged tag (placed and wired into array[index] when missing); returns its id."""
    if any(n['id'] == tag for n in nodes()):
        return tag
    UECP.blueprint.place_node(blueprint_path=TRUCK, graph_name=GRAPH, handle='k2.Make ' + struct, id=tag, x=x, y=y)
    UECP.blueprint.connect_pins(blueprint_path=TRUCK, graph_name=GRAPH,
                                items=[dict(**{'from': tag, 'from_pin': struct, 'to': array['id'], 'to_pin': f'[{index}]'})])
    return tag


def truck():
    if not ASSETS.does_asset_exist(TRUCK):
        if ASSETS.duplicate_asset(GER_TRUCK, TRUCK) is None:
            raise RuntimeError('could not copy ' + GER_TRUCK)
    setup, attachments = array_feeding('NewAttachmentsToSpawn')
    set_pin(setup['id'], 'NewBuildingMesh', f'{MESH}/SM_BF_Armory.SM_BF_Armory')
    set_pin(setup['id'], 'NewPreviewMesh', f'{MESH}/SM_BF_Armory_Preview.SM_BF_Armory_Preview')
    # the dressing: the German truck spawns nothing, so its attachment array gets one entry
    entry = struct_entry('BuildingAttachment', 'ArmDressing', attachments, 0, -400, 0)
    set_pin(entry, 'ActorToSpawn', DRESSING_CLASS)
    set_pin(entry, 'SocketName', 'Nav')
    set_pin(entry, 'Scale', '1.000000,1.000000,1.000000')
    # nav boxes: the German truck's one entry (on Nav) becomes Nav1, a second entry is added for Nav2
    boxes = json.loads(open(NAV_BOXES, encoding='utf-8').read())
    _, nav_array = array_feeding('NewBuildingNavModifiers')
    first = next(n['id'] for n in nodes() if n.get('detail', {}).get('struct') == 'BuildingNavModifierAttachment'
                 and n['id'] != 'ArmNav2' and feeds(n, nav_array['id']))
    second = struct_entry('BuildingNavModifierAttachment', 'ArmNav2', nav_array, 1, 0, 200)
    for node_id, socket in ((first, 'Nav1'), (second, 'Nav2')):
        set_pin(node_id, 'SocketName', socket)
        set_pin(node_id, 'Scale', ','.join(f'{v:.6f}' for v in boxes[socket]['scale']))
    bp = unreal.load_asset(TRUCK)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(TRUCK)
    for search in ('BuildingAttachment', 'BuildingNavModifierAttachment'):
        for node in nodes(search):
            if node.get('detail', {}).get('struct') == search:
                print(search, {p['name']: p.get('default', '') for p in node['pins'] if p.get('default')})
    print('made', TRUCK)


dressing()
truck()
