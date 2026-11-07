# Headless read of the Ger_Trains mission chain (classes, LoadToStartDelay, triggers) and a flatness survey around the
# base hint. Run: UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file>
import unreal

MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_Trains'
OUT = unreal.Paths.project_saved_dir() + 'NomadicAirDelivery/trains_chain.txt'
lines = []

def log(s):
    lines.append(str(s))
    unreal.log('NADREAD ' + str(s))

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
log('world ' + world.get_path_name())
actors = unreal.EditorLevelLibrary.get_all_level_actors()

def describe(m, depth):
    if m is None or depth > 12:
        return
    pad = '  ' * depth
    try:
        delay = m.get_editor_property('load_to_start_delay')
    except Exception as e:
        delay = 'ERR'
    try:
        trig = m.get_editor_property('mission_trigger')
    except Exception:
        trig = '?'
    log(f'{pad}{m.get_class().get_name()} delay={delay} trigger={trig}')
    try:
        for t in m.get_editor_property('triggerable_missions'):
            log(pad + ' [triggerable]')
            describe(t, depth + 1)
    except Exception:
        pass
    try:
        describe(m.get_editor_property('next_mission'), depth + 1)
    except Exception as e:
        log(pad + 'next err ' + str(e))

for a in actors:
    if a.get_class().get_name().startswith('BP_MissionManager'):
        log('manager ' + a.get_actor_label())
        for m in a.get_editor_property('missions'):
            describe(m, 1)

# Flatness survey around the expand-HQ hint.
HINT = unreal.Vector(-12591.95, 15335.04, -581.25)
STEP = 300.0
RADIUS = 7500.0
heights = {}
n = int(RADIUS / STEP)
for ix in range(-n, n + 1):
    for iy in range(-n, n + 1):
        x = HINT.x + ix * STEP
        y = HINT.y + iy * STEP
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            continue
        t = hit.to_tuple()
        blocking = t[0]
        if not blocking:
            continue
        loc = t[4]
        comp = t[10] if len(t) > 10 else None
        actor = t[9] if len(t) > 9 else None
        aname = actor.get_class().get_name() if actor else 'none'
        heights[(ix, iy)] = (loc.z, aname)

log(f'samples {len(heights)}')
for (ix, iy), (z, aname) in sorted(heights.items()):
    log(f'S {HINT.x + ix * STEP:.0f} {HINT.y + iy * STEP:.0f} {z:.1f} {aname}')

with open(OUT, 'w') as f:
    f.write('\n'.join(lines))
log('wrote ' + OUT)
