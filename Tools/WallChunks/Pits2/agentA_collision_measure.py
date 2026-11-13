"""Agent A, throwaway editor only: simple-collision traces against the 5 pit meshes, for the current collision and
for V-HACD candidates on BS3 / ML20 (in memory, never saved). Writes <key>_coll.json for agentA_gap.py.
  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
MESHES = {
    'TwinMG42': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Meshes/SM_GerPit_TwinMG42_Sandbags',
    'BS3': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Meshes/SM_BS3Pit',
    'Mortar120': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusMortarPit/Meshes/SM_Rus120MortarPit',
    'B4Stepped': '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/SM_B4Pit_SteppedSquare',
    'ML20Twin': '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Meshes/SM_ML20Pit_TwinShoulders',
}
CANDIDATES = {}  # V-HACD candidates measured in the first run (BS3 7 hulls 68%, ML20 32/64 hulls 98-99%)
ONLY = ['BS3']
ORIGIN = unreal.Vector(0, 0, -200000)
S = {'t': 0.0, 'done': False, 'log': []}


def log(message):
    S['log'].append(message)
    unreal.log('AGENTA_COLL ' + message)


def make_rays(mesh):
    b = mesh.get_bounding_box()
    reach = max(b.max.x - b.min.x, b.max.y - b.min.y) + 600
    top = b.max.z
    rays = []
    for ai in range(72):
        ang = math.radians(ai * 5)
        d = (-math.cos(ang), -math.sin(ang))
        for frac in (0.25, 0.45, 0.65, 0.85):
            z = top * frac
            for off in (-0.3, 0.0, 0.3):
                lateral = (-d[1] * off * reach * 0.4, d[0] * off * reach * 0.4)
                sx = -d[0] * reach + lateral[0]
                sy = -d[1] * reach + lateral[1]
                rays.append([sx, sy, z, d[0], d[1], 0.0, reach * 2])
        # arcing shells: 15 degrees down
        for off in (-0.2, 0.2):
            dz = -math.sin(math.radians(15))
            h = math.cos(math.radians(15))
            sx = -d[0] * reach + (-d[1] * off * reach * 0.4)
            sy = -d[1] * reach + (d[0] * off * reach * 0.4)
            rays.append([sx, sy, top * 0.6 + reach * math.tan(math.radians(15)), d[0] * h, d[1] * h, dz, reach * 2.5])
    return rays


def trace_all(world, rays):
    hits = []
    for r in rays:
        a = ORIGIN + unreal.Vector(r[0], r[1], r[2])
        b = a + unreal.Vector(r[3], r[4], r[5]) * r[6]
        hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        hits.append(t[3] if t and t[0] else None)
    return hits


def spawn(mesh):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, ORIGIN)
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(mesh)
    comp.set_collision_profile_name('BlockAll')
    return actor


def measure(key):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    mesh = unreal.load_asset(MESHES[key])
    rays = make_rays(mesh)
    actor = spawn(mesh)
    current = trace_all(world, rays)
    eas.destroy_actor(actor)
    count = sum(1 for h in current if h is not None)
    if count == 0:
        return False
    data = {'rays': rays, 'meshes': {'current': current}}
    log('%s current hits %d/%d' % (key, count, len(rays)))
    for hulls, verts in CANDIDATES.get(key, []):
        ok = sms.set_convex_decomposition_collisions(mesh, hulls, verts, 1000000)
        actor = spawn(mesh)
        name = 'vhacd%d' % hulls
        data['meshes'][name] = trace_all(world, rays)
        eas.destroy_actor(actor)
        log('%s %s ok=%s convex=%d hits %d' % (key, name, ok, sms.get_convex_collision_count(mesh),
                                               sum(1 for h in data['meshes'][name] if h is not None)))
    with open(ROOT + 'agentA_' + key + '_coll.json', 'w') as handle:
        json.dump(data, handle)
    return True


def finish():
    S['done'] = True
    with open(ROOT + 'agentA_collision_measure_log.json', 'w') as handle:
        json.dump(S['log'], handle, indent=1)
    # never save: candidates live in memory only
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    S['t'] += delta
    if S['done'] or S['t'] < 15.0:
        return
    if not S.get('loaded'):
        # Static meshes compile asynchronously after load: retry each mesh until its collision answers traces.
        S['loaded'] = True
        S['todo'] = [k for k in MESHES if 'ONLY' not in globals() or k in ONLY]
        for key in S['todo']:
            unreal.load_asset(MESHES[key])
        S['next'] = S['t'] + 5.0
        log('meshes loaded')
        return
    if S['t'] < S['next']:
        return
    S['next'] = S['t'] + 5.0
    try:
        for key in list(S['todo']):
            if measure(key):
                S['todo'].remove(key)
            else:
                log('%s no collision hits yet at t=%.0f' % (key, S['t']))
    except Exception as error:  # noqa: BLE001
        log('ERROR %r' % (error,))
        finish()
        return
    if not S['todo'] or S['t'] > 200.0:
        if S['todo']:
            log('ERROR never hit: %s' % S['todo'])
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
