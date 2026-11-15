"""Procedural wall holes on Rus_Building_Factory_A2 instead of damage meshes (run with UECP execute_uecp_script:
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/RusFactoryA2/ue_rusfactory_a2_holes.py', encoding='utf-8').read())).

Same recipe as the barracks / refineries (Source/Docs/Environment/WallChunkDamage.md, Tools/WallChunks/AGENT_BRIEF.md):
  - FactoryA2/Holes/MI_SM_Rus_WarFactory_A2_MI_Rus_BunkerConcrete_05_WallHoles: two-sided, NoGrid, on the shared
    M_RusBunker_Concrete_WallHoles master (built by make_wall_holes_material.py, not edited here) + 8 debris pieces
  - WallHoleDamage component: BuildingMeshComponent, holes on slot 0 (the bunker concrete) only; grid / hole / collapse
    settings copied from Rus_Building_Barracks (NoGrid, outset 0, 0.85 cm per mm, 3600 cm2 per piece, max 5, collapse
    pieces on death, its sound); the metal, camo nets, sandbags and atlas trim keep their materials
  - graph: the 100 / 66 / 33 % SetStaticMesh swaps removed (ExplosionsAt66 / At33 kept, wired straight from the switch);
    the unused GeometryCollection component and its CollapseMesh call removed
Safe to rerun.
"""
import unreal

UECP = globals().get('uecp')
if UECP is None:
    raise RuntimeError('run with execute_uecp_script')
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_holes_material.py', encoding='utf-8').read())

RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings'
HOME = RUS + '/FactoryA2'
HOLES = HOME + '/Holes'
BP = globals().get('RUSA2_TARGET_BP', RUS + '/Rus_Building_Factory')  # the live factory (A2 applied in place; test copy deleted 2026-11-15)
REFERENCE = RUS + '/Rus_Building_Barracks'
MESH = HOME + '/SM_Rus_WarFactory_A2'
CONCRETE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/MI_Rus_BunkerConcrete_05'
MI_NAME = 'MI_SM_Rus_WarFactory_A2_MI_Rus_BunkerConcrete_05_WallHoles'
SUBS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary
OUT = {}


def templates(bp):
    out = {}
    for handle in SUBS.k2_gather_subobject_data_for_blueprint(bp):
        data = LIB.get_data(handle)
        out[str(LIB.get_variable_name(data))] = LIB.get_object_for_blueprint(data, bp)
    return out


def materials():
    mesh = unreal.load_asset(MESH)
    mi = make_holes_instance(CONCRETE, HOLES, mesh=mesh, grid_axis=None, two_sided=True, name=MI_NAME)
    OUT['ps_masked_vs_opaque'] = check_mask_live(mi.get_base_material())
    pieces = make_debris_pieces(HOLES, 'SM_Rus_WarFactory_A2', mi)
    return mesh, mi, pieces


def component(mesh, mi, pieces):
    bp = unreal.load_asset(BP)
    if 'WallHoleDamage' not in templates(bp):
        UECP.component.add_component(blueprint_path=BP, component_class='WallHoleDamageComponent', component_name='WallHoleDamage')
        bp = unreal.load_asset(BP)
    comp = templates(bp)['WallHoleDamage']
    ref = templates(unreal.load_asset(REFERENCE))['WallHoleDamage']
    for prop in ('grid_settings', 'hole_settings', 'collapse_settings'):
        comp.set_editor_property(prop, ref.get_editor_property(prop))
    comp.set_editor_property('wall_mesh_component_name', 'BuildingMeshComponent')
    slots = len(mesh.get_editor_property('static_materials'))
    comp.set_editor_property('damaged_materials', [mi] + [None] * (slots - 1))
    comp.set_editor_property('debris_meshes', pieces)
    grid = comp.get_editor_property('grid_settings')
    OUT['component'] = {'grid_axis': str(grid.get_editor_property('grid_axis')),
                        'outset': grid.get_editor_property('hole_centre_outset_share'),
                        'area_per_piece': grid.get_editor_property('area_per_debris_piece'),
                        'radius_per_mm': comp.get_editor_property('hole_settings').get_editor_property('hole_radius_per_calibre_mm'),
                        'collapse_on_death': comp.get_editor_property('collapse_settings').get_editor_property('collapse_into_chunks_on_death'),
                        'damaged_materials': [m.get_name() if m else None for m in comp.get_editor_property('damaged_materials')],
                        'debris': len(comp.get_editor_property('debris_meshes'))}


def graph():
    nodes = UECP.blueprint.get_graph_snapshot(blueprint_path=BP, graph_name='EventGraph', view='compact')['nodes']
    by_fn = {}
    for n in nodes:
        by_fn.setdefault(n.get('detail', {}).get('function'), []).append(n)
    switch = next((n for n in nodes if n['node_class'] == 'K2Node_SwitchEnum'), None)
    swaps = [n['id'] for n in by_fn.get('SetStaticMesh', [])]
    removed = []
    if swaps:
        UECP.blueprint.delete_nodes(blueprint_path=BP, graph_name='EventGraph', node_ids=swaps)
        removed += ['SetStaticMesh'] * len(swaps)
    for pin, fn in (('Level_66Percent', 'ExplosionsAt66'), ('Level_33Percent', 'ExplosionsAt33')):
        target = by_fn.get(fn, [None])[0]
        if switch and target:
            UECP.blueprint.connect_pins(blueprint_path=BP, graph_name='EventGraph', from_node=switch['id'], from_pin=pin,
                                        to_node=target['id'], to_pin='execute')
    collapse = by_fn.get('CollapseMesh', [])
    if collapse:
        cid = collapse[0]['id']
        feeders = [n['id'] for n in nodes if n.get('is_pure') and any(
            c['node_id'] == cid for p in n.get('pins', []) for c in p.get('connected_to', []))
            and all(c['node_id'] == cid for p in n.get('pins', []) for c in p.get('connected_to', []))]
        UECP.blueprint.delete_nodes(blueprint_path=BP, graph_name='EventGraph', node_ids=[cid] + feeders)
        removed += ['CollapseMesh'] + ['feeder'] * len(feeders)
    if 'GeometryCollection' in templates(unreal.load_asset(BP)):
        UECP.blueprint.delete_components(blueprint_path=BP, items=['GeometryCollection'])
        removed.append('GeometryCollection component')
    OUT['removed'] = removed
    fix_visibility()
    after =UECP.blueprint.get_graph_snapshot(blueprint_path=BP, graph_name='EventGraph', view='compact')['nodes']
    sw = next(n for n in after if n['node_class'] == 'K2Node_SwitchEnum')
    OUT['switch'] = {p['name']: [c['node_id'][:6] for c in p.get('connected_to', [])] for p in sw['pins'] if p.get('connected_to')}
    OUT['explosions'] = {n['detail']['function']: n['id'][:6] for n in after if n.get('detail', {}).get('function', '').startswith('Explosions')}


VISIBILITY = {'BP_OnStartBxpConstruction': False, 'BP_OnFinishedExpansionConstruction': True,
              'BP_OnStartPackUpBxp': False, 'BP_OnCancelledPackUpBxp': True, 'BP_OnUnitDies': False}


def fix_visibility():
    """The SetVisibility nodes from ue_rusfactory_a2_blueprint.py came out with bPropagateToChildren false (place_node
    ignored that input): set both pins by the event that runs each node (directly or through a keep-chain sequence)."""
    nodes = UECP.blueprint.get_graph_snapshot(blueprint_path=BP, graph_name='EventGraph', view='compact')['nodes']
    by_id = {n['id']: n for n in nodes}
    fixed = []
    for n in nodes:
        name = n.get('detail', {}).get('name')
        if n['op'] != 'event' or name not in VISIBILITY:
            continue
        frontier = [e['to'] for e in n.get('exec_to', [])]
        seen = set()
        while frontier:
            nid = frontier.pop()
            if nid in seen or nid not in by_id:
                continue
            seen.add(nid)
            node = by_id[nid]
            if node.get('detail', {}).get('function') == 'SetVisibility':
                for pin, value in (('bNewVisibility', VISIBILITY[name]), ('bPropagateToChildren', True)):
                    UECP.blueprint.set_pin_default(blueprint_path=BP, graph_name='EventGraph', node_id=nid, pin_name=pin,
                                                   value='true' if value else 'false')
                fixed.append((name, VISIBILITY[name]))
            elif node['node_class'] == 'K2Node_ExecutionSequence':
                frontier += [e['to'] for e in node.get('exec_to', [])]
    OUT['visibility_fixed'] = fixed


def finish():
    bp = unreal.load_asset(BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    bp = unreal.load_asset(BP)
    bp.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False):
        raise RuntimeError('save failed')
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    OUT['dirty'] = dirty


m, i, p = materials()
component(m, i, p)
graph()
finish()
print('RUSA2HOLES', OUT)
