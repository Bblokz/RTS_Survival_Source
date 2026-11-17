"""Driver for the 2026-11-17 Strike Division BXP round, run from a shell against this session's own editor
(round2_uecp.py finds its UECP port). Steps:
  python round2_build.py pins <Job>        BP_InitBuildingExp meshes + expansion type, Bp_OnHealthChanged meshes
  python round2_build.py lib "<call>"       run round2_editor_lib.py (+ the wall-holes library) with one call line
The editor-side work (imports, materials, holes, components) lives in round2_editor_lib.py.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from round2_uecp import act, py  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
WALL_HOLES_LIB = os.path.join(HERE, '..', 'WallChunks', 'make_wall_holes_material.py')
GERBXPS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps'
JOBS = {
    'RadioVan': (GERBXPS + '/MechDepot/RadioVan/Ger_Bxp_MechDepotRadioVan',
                 GERBXPS + '/MechDepot/RadioVan/Meshes/SM_GerMechDepot_RadioVan', 'BTX_GerMechDepotRadioVan'),
    'WurfrahmenShop': (GERBXPS + '/MechDepot/WurfrahmenShop/Ger_Bxp_MechDepotWurfrahmenShop',
                       GERBXPS + '/MechDepot/WurfrahmenShop/Meshes/SM_GerMechDepot_WurfrahmenShop',
                       'BTX_GerMechDepotWurfrahmenShop'),
    'SniperLoft': (GERBXPS + '/Barracks/SniperLoft/Ger_Bxp_BarracksSniperLoft',
                   GERBXPS + '/Barracks/SniperLoft/Meshes/SM_GerBarracks_SniperLoft', 'BTX_GerBarracksSniperLoft'),
}


def checked(action, **params):
    result = act(action, **params)
    if not result.get('ok', True) or result.get('success') is False:
        print('FAILED', action, json.dumps(params)[:300], '->', json.dumps(result)[:900])
        sys.exit(1)
    return result


def nodes_calling(blueprint, graph, function):
    snapshot = checked('get_graph_snapshot', blueprint_path=blueprint, graph_name=graph, view='compact')
    return [n['id'] for n in snapshot['nodes']
            if n.get('op') == 'call' and n.get('detail', {}).get('function') == function]


def pins(job):
    blueprint, mesh, expansion_type = JOBS[job]
    mesh_ref = '%s.%s' % (mesh, mesh.split('/')[-1])
    init = nodes_calling(blueprint, 'EventGraph', 'BP_InitBuildingExp')
    if len(init) != 1:
        print('FAILED BP_InitBuildingExp lookup', init)
        sys.exit(1)
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewBuildingExpansionType', expansion_type)):
        checked('set_pin_default', blueprint_path=blueprint, graph_name='EventGraph', node_id=init[0], pin_name=pin,
                value=value)
    # The copied 100 % / 50 % mesh swap: both branches point at the one mesh (the health levels list is empty anyway,
    # so it never fires; this keeps a later edit of that list from swapping to the copied expansion's meshes).
    for node in nodes_calling(blueprint, 'Bp_OnHealthChanged', 'SetStaticMesh'):
        checked('set_pin_default', blueprint_path=blueprint, graph_name='Bp_OnHealthChanged', node_id=node,
                pin_name='NewMesh', value=mesh_ref)
    compiled = checked('compile_blueprint', blueprint_path=blueprint)
    print('compile', job, 'errors', compiled.get('error_count'), compiled.get('errors'))
    print('save', checked('save_asset', asset_path=blueprint).get('ok'))


def lib(call_line):
    code = (open(WALL_HOLES_LIB, encoding='utf-8').read() + '\n'
            + open(os.path.join(HERE, 'round2_editor_lib.py'), encoding='utf-8').read() + '\n' + call_line)
    print(py(code))


if __name__ == '__main__':
    if sys.argv[1] == 'pins':
        pins(sys.argv[2])
    elif sys.argv[1] == 'lib':
        lib(sys.argv[2])
