"""Rus_Building_Factory_A2 set-up, part 2 (run with UECP execute_uecp_script after ue_rusfactory_a2_import.py and the
seven add_component calls: A2_Crane under A2_Machinery; A2_TurretHook / A2_GunHook on A2_Crane socket TurretHook;
A2_TurretLorry / A2_GunLorry on socket TurretLorry; A2_Lorry under A2_Machinery; A2_LorryWheels under A2_Lorry).

  - component templates: meshes, the crane's looping A_RusFactoryA2_Work, no collision, no navmesh effect
  - graph: every building-mesh pin (BP_InitBuildingExp and the 100/66/33 % health swaps) -> SM_Rus_WarFactory_A2
    (the A2 damage meshes don't exist yet, so damage doesn't change the look for now)
  - graph: A2_Machinery hidden while the expansion is built or packed up, shown when built, hidden when it dies
Safe to rerun: pins are set by value and the visibility nodes are only added once (looked up by their comment).
"""
import json

import unreal

UECP = globals().get('uecp')
if UECP is None:
    raise RuntimeError('run with execute_uecp_script')
CFG = json.loads(open('D:/UE5Projects/RTS_Survival/Source/Tools/RusFactoryA2/rusfactory_a2_ue.json', encoding='utf-8').read())
RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings'
HOME = RUS + '/FactoryA2'
BP = globals().get('RUSA2_TARGET_BP', RUS + '/Rus_Building_Factory')  # the live factory (A2 applied in place; test copy deleted 2026-11-15)
MESH = HOME + '/SM_Rus_WarFactory_A2.SM_Rus_WarFactory_A2'
OLD_MESHES = ('SM_RusBunker_MG', 'SM_Rus_WarFactory', 'SM_Rus_WarFactoryAt66', 'SM_Rus_WarFactoryAt33')
VISIBILITY = {'BP_OnStartBxpConstruction': False, 'BP_OnFinishedExpansionConstruction': True,
              'BP_OnStartPackUpBxp': False, 'BP_OnCancelledPackUpBxp': True, 'BP_OnUnitDies': False}
SUBS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary
RESULT = {}


def templates(bp):
    out = {}
    for handle in SUBS.k2_gather_subobject_data_for_blueprint(bp):
        data = LIB.get_data(handle)
        out[str(LIB.get_variable_name(data))] = LIB.get_object_for_blueprint(data, bp)
    return out


def quiet(comp):
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)


def set_components():
    bp = unreal.load_asset(BP)
    comps = templates(bp)
    # the spawned mesh is the inherited component's default (BP_InitBuildingExp's pin doesn't set it): override it here
    comps['BuildingMeshComponent'].set_editor_property('static_mesh', unreal.load_asset(MESH.split('.')[0]))
    crane = comps['A2_Crane']
    crane.set_editor_property('skeletal_mesh_asset', unreal.load_asset(HOME + '/SK_RusFactory_A2_Crane'))
    crane.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    play = unreal.SingleAnimationPlayData()
    play.set_editor_property('anim_to_play', unreal.load_asset(HOME + '/A_RusFactoryA2_Work'))
    play.set_editor_property('saved_looping', True)
    play.set_editor_property('saved_playing', True)
    play.set_editor_property('saved_play_rate', 1.0)
    crane.set_editor_property('animation_data', play)
    quiet(crane)
    turret = unreal.load_asset(CFG['turret_meshes']['base'])
    gun = unreal.load_asset(CFG['turret_meshes']['gun'])
    for name in ('A2_TurretHook', 'A2_TurretLorry'):
        comps[name].set_editor_property('static_mesh', turret)
        quiet(comps[name])
    for name in ('A2_GunHook', 'A2_GunLorry'):
        comps[name].set_editor_property('skeletal_mesh_asset', gun)
        quiet(comps[name])
    lorry = comps['A2_Lorry']
    lorry.set_editor_property('static_mesh', unreal.load_asset(CFG['lorry']['hull']))
    loc = CFG['lorry']['location_ue_cm']
    lorry.set_editor_property('relative_location', unreal.Vector(*loc))
    lorry.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=CFG['lorry']['yaw_ue_deg']))
    quiet(lorry)
    wheels = comps['A2_LorryWheels']
    wheels.set_editor_property('skeletal_mesh_asset', unreal.load_asset(CFG['lorry']['chassis']))
    quiet(wheels)
    RESULT['components'] = sorted(k for k in comps if k.startswith('A2_'))


def nodes(search=None):
    return UECP.blueprint.get_graph_snapshot(blueprint_path=BP, graph_name='EventGraph', view='compact',
                                             **({'search': search} if search else {}))['nodes']


def set_mesh_pins():
    changed = []
    for node in nodes():
        for pin in node.get('pins', []):
            if pin.get('name') in ('NewBuildingMesh', 'NewMesh') and any(f'/{m}.' in pin.get('default', '') for m in OLD_MESHES):
                UECP.blueprint.set_pin_default(blueprint_path=BP, graph_name='EventGraph', node_id=node['id'],
                                               pin_name=pin['name'], value=MESH)
                changed.append(node['detail'].get('function'))
    RESULT['mesh_pins'] = changed


def wire_visibility():
    graph = nodes()
    events = {n['detail'].get('name'): n for n in graph if n['op'] == 'event'}
    done = []
    for i, (event, visible) in enumerate(VISIBILITY.items()):
        ev = events[event]
        then = next((p for p in ev['pins'] if p['name'] == 'then'), {})
        already = any(c for c in then.get('connected_to', []) if any(
            n['id'] == c['node_id'] and n.get('detail', {}).get('function') == 'SetVisibility' for n in graph))
        if already:
            done.append((event, 'kept'))
            continue
        x, y = -1200, 2400 + 260 * i
        get = UECP.blueprint.place_node(blueprint_path=BP, graph_name='EventGraph', handle='var.get.A2_Machinery', x=x, y=y + 120)
        call = UECP.blueprint.place_node(blueprint_path=BP, graph_name='EventGraph', handle='fn.SceneComponent.SetVisibility',
                                         x=x + 260, y=y, inputs={'bNewVisibility': visible, 'bPropagateToChildren': True})
        get_id = get.get('node_id') or get.get('id')
        call_id = call.get('node_id') or call.get('id')
        UECP.blueprint.connect_pins(blueprint_path=BP, graph_name='EventGraph', from_node=get_id, from_pin='A2_Machinery',
                                    to_node=call_id, to_pin='self')
        UECP.blueprint.connect_pins(blueprint_path=BP, graph_name='EventGraph', from_node=ev['id'], from_pin='then',
                                    to_node=call_id, to_pin='execute', keep_existing_chain=True)
        done.append((event, visible))
    RESULT['visibility'] = done


def finish():
    bp = unreal.load_asset(BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    obj = unreal.load_asset(BP)
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed')
    sk = unreal.load_asset(HOME + '/SK_RusFactory_A2_Crane_Skeleton')
    sk.modify()
    unreal.EditorLoadingAndSavingUtils.save_packages([sk.get_outermost()], False)
    # add_component must not have written into the parent Blueprint (it did once on another building): nothing of ours dirty
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    RESULT['dirty_building_packages'] = [d for d in dirty if 'BuildingExpansion' in d or 'RusBuildings' in d]


set_components()
set_mesh_pins()
wire_visibility()
finish()
print('RUSFACTORYA2_BP ' + json.dumps(RESULT, default=str))
