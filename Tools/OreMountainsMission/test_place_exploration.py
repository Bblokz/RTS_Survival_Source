"""Headless test of place_exploration.py on a THROWAWAY copy of the Ore map. Saves nothing.

The copy is a file copy of Ger_2ndOreMountains.umap made outside Unreal (the real map is never opened):
    Content/RTS_Survival/Maps/Tmp/Ger_2ndOreMountains_ExploreTest.umap   (delete it after the run)
Run with the DebugGame commandlet (AOreHeapStation is not in the Development DLL yet):
    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended ...
Report: Saved/OreMountains/exploration/test_report.json, log lines ORE_EXPLORE_TEST.
"""
import importlib
import json
import math
import sys

import unreal

TEST_MAP = '/Game/RTS_Survival/Maps/Tmp/Ger_2ndOreMountains_ExploreTest'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/exploration/test_report.json'
sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission')
failures = []
out = {}


def log(text):
    unreal.log_warning('ORE_EXPLORE_TEST ' + str(text))


def fail(text):
    failures.append(text)
    log('FAIL ' + text)


def explore_actors():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return [a for a in sub.get_all_level_actors() if a.get_actor_label().startswith('ORE_Explore_')]


def owner_of(actor):
    for comp in actor.get_components_by_class(unreal.ActorComponent):
        if hasattr(comp, 'get_owning_player') and 'RTSComponent' in comp.get_class().get_name():
            return comp.get_owning_player()
    return None


def collision_of(actor):
    rows = []
    for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
        enabled = str(comp.get_collision_enabled()).split('.')[-1]
        if enabled != 'NO_COLLISION':
            rows.append('%s:%s:%s' % (comp.get_name(), comp.get_collision_profile_name(), enabled))
    return rows


def station_state(st):
    return {'held': st.get_is_held(), 'holder': st.get_holding_player(), 'owner': owner_of(st),
            'health': round(st.get_station_health())}


def station_state_machine():
    """Capture, damage, knock-out and recapture on a placed station. Editor world: no BeginPlay and no game state,
    so the drop-off registration, the unit manager and the fog report errors or do nothing; held, owner and health
    are what this checks."""
    stations = [a for a in explore_actors() if '_Station_HB3' in a.get_actor_label()]
    if not stations:
        fail('no HB3 station for the state machine')
        return None
    st = stations[0]
    steps = [('start', station_state(st))]
    st.on_station_captured(1)
    steps.append(('captured', station_state(st)))
    unreal.GameplayStatics.apply_damage(st, 1000.0, None, None, None)
    steps.append(('damage 1000', station_state(st)))
    unreal.GameplayStatics.apply_damage(st, 2000.0, None, None, None)
    steps.append(('damage 2000', station_state(st)))
    unreal.GameplayStatics.apply_damage(st, 500.0, None, None, None)
    steps.append(('damage while knocked out', station_state(st)))
    st.on_station_captured(1)
    steps.append(('recaptured', station_state(st)))
    st.knock_out_station()
    steps.append(('knock_out_station', station_state(st)))
    expect = [(False, -1, 0, 2500), (True, 1, 1, 2500), (True, 1, 1, 1500), (False, -1, 2, 2500),
              (False, -1, 2, 2500), (True, 1, 1, 2500), (False, -1, 2, 2500)]
    for (name, got), (held, holder, owner, health) in zip(steps, expect):
        if (got['held'], got['holder'], got['owner'], got['health']) != (held, holder, owner, health):
            fail('state machine %s: got %s, expected held %s holder %s owner %s health %s' %
                 (name, got, held, holder, owner, health))
    return steps


def preplaced_mode(pe):
    """HQ_MODE 'preplaced': a built HQ is placed and the player start stops the profile units."""
    pe.HQ_MODE = 'preplaced'
    try:
        report = pe.place_all()
    finally:
        pe.HQ_MODE = 'profile'
    hq = [a for a in explore_actors() if a.get_actor_label() == 'ORE_Explore_Base_HQ']
    result = {'hq': None, 'flag': report.get('player_start_no_profile_units'), 'warnings': report['warnings']}
    if hq:
        loc = hq[0].get_actor_location()
        result['hq'] = [round(loc.x), round(loc.y), round(loc.z), hq[0].get_editor_property('start_as_converted_to_building')]
    if not hq or result['flag'] is not True:
        fail('preplaced mode: %s' % result)
    return result


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(TEST_MAP)
    if world is None:
        fail('could not load ' + TEST_MAP)
        return
    out['level'] = world.get_path_name()
    import place_exploration as pe
    importlib.reload(pe)
    first = pe.place_all()
    count_1 = len(explore_actors())
    second = pe.place_all()
    count_2 = len(explore_actors())
    out['counts'] = [count_1, count_2]
    if count_1 != count_2 or count_1 != len(second['placed']):
        fail('not idempotent: %s, report %d' % (out['counts'], len(second['placed'])))
    out['warnings'] = second['warnings']
    out['ground'] = second['ground']
    out['route'] = second.get('route')

    rows = []
    for actor in explore_actors():
        label = actor.get_actor_label()
        loc = actor.get_actor_location()
        row = {'label': label, 'class': actor.get_class().get_name(), 'loc': [round(loc.x), round(loc.y), round(loc.z)],
               'yaw': round(actor.get_actor_rotation().yaw, 1), 'owner': owner_of(actor),
               'collision': collision_of(actor), 'tags': [str(t) for t in actor.get_editor_property('tags')]}
        ground = pe.height_file_z(loc.x, loc.y)
        trace = pe.trace_z(loc.x, loc.y)
        row['ground_file'] = round(ground)
        row['ground_trace'] = None if trace is None else round(trace)
        if '_Base_' in label:
            row['start_as_building'] = actor.get_editor_property('start_as_converted_to_building')
            if not row['start_as_building'] or row['owner'] != 1:
                fail('%s not a pre-built player building: %s' % (label, row))
        if '_Station_' in label:
            mesh = actor.get_editor_property('station_mesh').get_editor_property('static_mesh')
            row['station_mesh'] = mesh.get_name() if mesh else None
            row['drop_off_point'] = str(actor.get_editor_property('drop_off_point').get_world_location())
            if mesh is None or row['owner'] != 0:
                fail('%s: mesh %s owner %s' % (label, row['station_mesh'], row['owner']))
        if '_Buried_' in label or '_Tower_' in label or '_Station_' in label:
            if not row['collision']:
                fail('%s has no colliding component' % label)
        if abs(loc.z - ground) > 300.0:
            fail('%s sits %.0f cm off the ground file' % (label, loc.z - ground))
        rows.append(row)
    out['actors'] = rows
    # Placed items must keep apart from each other (rough: 8 m between pivots).
    for i, a in enumerate(rows):
        for b in rows[i + 1:]:
            d = math.hypot(a['loc'][0] - b['loc'][0], a['loc'][1] - b['loc'][1])
            if d < 800.0:
                fail('%s and %s only %.1f m apart' % (a['label'], b['label'], d / 100.0))
    out['state_machine'] = station_state_machine()
    out['preplaced_mode'] = preplaced_mode(pe)
    # Player start (the profile's HQ truck spawns here) and its editable flags.
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in sub.get_all_level_actors():
        if actor.get_class().get_name() == 'RTS_PLayerStart_C':
            loc = actor.get_actor_location()
            flags = {}
            for name in ('DoNotLoadProfileUnits',):
                try:
                    flags[name] = str(actor.get_editor_property(name))
                except Exception:
                    pass
            out['player_start'] = {'loc': [round(loc.x), round(loc.y), round(loc.z)], 'flags': flags}


try:
    main()
except Exception as error:
    import traceback
    fail('exception %s %s' % (error, traceback.format_exc()))
out['failures'] = failures
with open(REPORT, 'w') as handle:
    json.dump(out, handle, indent=1, default=str)
log('RESULT ' + ('OK' if not failures else 'FAIL %d' % len(failures)))
