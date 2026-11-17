"""Job 4 (outside the editor, UECP actions): repoint the duplicated Pz 38(t) R graph pins to the Panzerwerfer, rebuild
the hull armour boxes for the Maultier body, set the wreck's subtype pins, compile both. Idempotent.
Run: python pw_bp_graph.py   (MSYS_NO_PATHCONV=1 under Git Bash)"""
import json

import pw_uecp as u

PW = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Panzerwerfer'
UNIT = PW + '/Ger_Vehicle_Panzerwerfer'
WRECK = PW + '/Env_Dest_Panzerwerfer'
TRACK_MI = PW + '/Materials/MI_PanzerWerfer_Tracks.MI_PanzerWerfer_Tracks'

# Hull armour for the Sd.Kfz. 4/1 body (UE frame, cm). Transform scale 1 with the true half extents in the box
# (UE_LESSONS: InitArmorCalculation applies the transform scale twice). Real plate: 8 mm, the cab/bonnet front 10 mm.
# Keyed by the getter that fed the copied Pz 38(t) box, so the plate types keep their hierarchy role.
ARMOR = {
    'FrontLocation': dict(loc=(232, 0, 105), half=(15, 95, 60), mm=10, plate='Plate_Front'),
    'LocationUpperGlacis': dict(loc=(130, 0, 175), half=(100, 95, 25), mm=8, plate='Plate_FrontUpperGlacis'),
    'LowerGlacisLocation': dict(loc=(225, 0, 50), half=(20, 95, 25), mm=8, plate='Plate_FrontLowerGlacis'),
    'RightLocation': dict(loc=(-50, 100, 120), half=(290, 10, 95), mm=8, plate='Plate_SideRight'),
    'LeftLocation': dict(loc=(-50, -100, 120), half=(290, 10, 95), mm=8, plate='Plate_SideLeft'),
    'LocationRearUpperGlacis': dict(loc=(-335, 0, 110), half=(12, 95, 65), mm=8, plate='Plate_Rear'),
}


def ok(r, what):
    good = isinstance(r, dict) and r.get('ok', True) and not r.get('error')
    print(('OK  ' if good else 'ERR ') + what, '' if good else json.dumps(r)[:400])
    return good


def nodes(bp, graph, search=None):
    args = dict(blueprint_path=bp, graph_name=graph, detail='full', include_unconnected_pins=True)
    if search:
        args['search'] = search
    return u.act('get_blueprint_graph', **args).get('nodes', [])


def pin(node, name):
    for p in node.get('pins', []):
        if p['name'] == name:
            return p
    return None


def set_pin(bp, graph, node_id, name, value):
    return ok(u.act('set_pin_default', blueprint_path=bp, graph_name=graph, node_id=node_id, pin_name=name,
                    value=value), '%s.%s = %s' % (node_id[:6], name, value))


def first(bp, graph, title):
    for n in nodes(bp, graph, title):
        if n.get('title', '').split('\n')[0] == title:
            return n
    raise RuntimeError('node %s not found in %s' % (title, graph))


# ---------------- unit ----------------
n = first(UNIT, 'EventGraph', 'SetUnitSubtype')
set_pin(UNIT, 'EventGraph', n['id'], 'NewTankSubtype', 'Tank_Panzerwerfer')
n = first(UNIT, 'EventGraph', 'Make Destroy Spawn Actor')
set_pin(UNIT, 'EventGraph', n['id'], 'ActorToSpawn', WRECK + '.Env_Dest_Panzerwerfer_C')
n = first(UNIT, 'ReferenceCasts', 'InitTrackedAnimationInstance')
set_pin(UNIT, 'ReferenceCasts', n['id'], 'TrackBaseMaterial', TRACK_MI)
set_pin(UNIT, 'ReferenceCasts', n['id'], 'LeftTrackMaterialIndex', '0')
set_pin(UNIT, 'ReferenceCasts', n['id'], 'RightTrackMaterialIndex', '1')
n = first(UNIT, 'ReferenceCasts', 'InitTrackedTank')
# Longer than the Pz 38(t) (5.9 m): keep path points further from navmesh corners.
set_pin(UNIT, 'ReferenceCasts', n['id'], 'TankCornerOffset', '200.0')

# Armour: Make Armor Settings <- Make Transform <- Get <Var>. Cut the transform/box wires and write literals.
graph = nodes(UNIT, 'SetupArmor')
by_id = {g['id']: g for g in graph}
for g in graph:
    if g.get('title', '').split('\n')[0] != 'Make Armor Settings':
        continue
    tr = pin(g, 'ArmorBoxTransform')
    links = tr.get('connected_to') or []
    key = None
    if links:
        mt = by_id[links[0]['node_id']]
        loc = pin(mt, 'Location')
        if loc and loc.get('connected_to'):
            key = loc['connected_to'][0]['pin']
    if key is None:
        # Already rewritten (idempotent rerun): find it by the stored transform.
        for k, a in ARMOR.items():
            if tr.get('default', '').startswith('%.6f,%.6f,%.6f' % a['loc']):
                key = k
    if key not in ARMOR:
        print('skip armour node', g['id'][:6], key)
        continue
    a = ARMOR[key]
    for pname in ('ArmorBoxTransform', 'ArmorBox'):
        if (pin(g, pname).get('connected_to') or []):
            ok(u.act('disconnect_pins', blueprint_path=UNIT, graph_name='SetupArmor', node_id=g['id'], pin_name=pname,
                     disconnect_all=True), 'cut %s %s' % (key, pname))
    set_pin(UNIT, 'SetupArmor', g['id'], 'ArmorBoxTransform',
            '%.6f,%.6f,%.6f|0.000000,0.000000,0.000000|1.000000,1.000000,1.000000' % a['loc'])
    hx, hy, hz = a['half']
    set_pin(UNIT, 'SetupArmor', g['id'], 'ArmorBox',
            '(Min=(X=%.6f,Y=%.6f,Z=%.6f),Max=(X=%.6f,Y=%.6f,Z=%.6f),IsValid=True)' % (-hx, -hy, -hz, hx, hy, hz))
    set_pin(UNIT, 'SetupArmor', g['id'], 'ArmorValue', '%.6f' % a['mm'])
    if a['plate']:
        set_pin(UNIT, 'SetupArmor', g['id'], 'ArmorType', a['plate'])
    print('   armour', key, pin(g, 'ArmorType').get('default'))
print(json.dumps(u.act('compile_blueprint', blueprint_path=UNIT))[:500])

# ---------------- wreck ----------------
n = first(WRECK, 'UserConstructionScript', 'Set Tank Subtype')
set_pin(WRECK, 'UserConstructionScript', n['id'], 'Tank Subtype', 'Tank_Panzerwerfer')
n = first(WRECK, 'UserConstructionScript', 'GetDestroyedTankHealth')
set_pin(WRECK, 'UserConstructionScript', n['id'], 'TankSubtype', 'Tank_Panzerwerfer')
print(json.dumps(u.act('compile_blueprint', blueprint_path=WRECK))[:500])
