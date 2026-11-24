"""Throwaway-editor test for the Soviet wall gates (never the shared editor).

UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
  -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
  -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/RusWallGates/gate_headless_test.py"
  -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

1. (Re)builds BP_RusWallGateA / BP_RusWallGateC (ue_build_gate_bps.py) and saves them; the map is never saved.
2. Spawns both gates with flanking walls, a Soviet T-34/76 (player 2) and a German Pz 38(t) (player 1) far away, PIE.
3. Shell holes on both closed leaves, then: Soviet tank to gate A (opens), to gate C (opens; A closes),
   German tank in C's doorway with the Soviet gone (C must stay open), German away (C closes),
   German in front of A (A must stay closed). Pictures at each step, gate state + open amount logged.
Results: Saved/RusWallGates/test/result.json + PNGs.
"""
import json
import sys

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/RusWallGates')
import ue_build_gate_bps  # noqa: E402

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/RusWallGates/'

OUT = 'D:/UE5Projects/RTS_Survival/Saved/RusWallGates/test/'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/'
WALL = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Rus_Wall_Concrete_Long_Holes.Rus_Wall_Concrete_Long_Holes_C'
SOVIET = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C'
GERMAN = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T.Ger_Vehicle_Panzer38T_C'
BASE = unreal.Vector(0, 0, 0)
GATE_C_Y = 4000.0
FAR_SOVIET = (-6000.0, -15000.0)
FAR_GERMAN = (15000.0, 15000.0)
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'step': 0}


def log(message):
    S['log'].append('%.1f %s' % (S['t'], message))
    unreal.log('GATETEST ' + message)


def finish():
    json.dump(S['log'], open(OUT + 'result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def of_class(world, name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if a.get_class().get_name() == name]


def gate(world, key):
    return of_class(world, 'BP_RusWallGate%s_C' % key)[0]


def gate_text(world):
    out = []
    for key in 'AC':
        g = gate(world, key)
        out.append('%s=%s open=%.2f passable=%s' % (key, str(g.get_gate_state()).split('.')[-1], g.get_gate_open_amount(),
                                                    g.get_is_gate_passable()))
    return ' | '.join(out)


def move(world, cls_name, xy):
    tank = of_class(world, cls_name)[0]
    tank.set_actor_location(unreal.Vector(xy[0], xy[1], BASE.z + 60), False, True)


def shoot_leaf(world, key, local, calibre):
    g = gate(world, key)
    leaf = g.get_editor_property('gate_leaf_mesh')
    comp = [c for c in g.get_components_by_class(unreal.WallHoleDamageComponent) if c.get_name() == 'WallHoleDamage_Leaf'][0]
    t = leaf.get_world_transform()
    start = t.transform_location(unreal.Vector(local[0], -400, local[2]))
    end = t.transform_location(unreal.Vector(local[0], 400, local[2]))
    hit = leaf.line_trace_component(start, end, False, False, False)
    if not hit:
        log('shot %s %s missed the leaf collision' % (key, local))
        return
    removed = comp.debug_fire_test_hit(hit[0], unreal.Vector(0, 1, 0), float(calibre))
    log('shot %s leaf at %s removed %.2f m2 holes=%d slot0=%s' % (key, local, removed, comp.get_hole_count(),
                                                                   leaf.get_material(0).get_name()))


def picture(world, name, key, eye_offset, look_offset=(0, 0, 0)):
    g = gate(world, key)
    centre = g.get_actor_location() + unreal.Vector(-500 + look_offset[0], look_offset[1], 300 + look_offset[2])
    eye = g.get_actor_location() + unreal.Vector(*eye_offset)
    cam = S['camera']
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.set_actor_location(eye, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, centre), True)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(cam, 0.0)
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT + name + '.png', camera=cam)
    finally:
        S['busy'] = False
    log('picture %s | %s' % (name, gate_text(world)))


def compiled_state():
    lib = unreal.NomadicDeployEditorLibrary
    for n in ('Materials/MI_RusWallGateA_Leaf_GateSteel', 'Holes/MI_RusWallGateA_Leaf_GateSteel_WallHoles',
              'Materials/MI_RusWallGateC_Leaf_Concrete', 'Holes/MI_RusWallGateC_Leaf_Concrete_WallHoles'):
        log('compiled %s %s' % (n.split('/')[-1], lib.describe_compiled_resource(unreal.load_asset(ROOT + n))))


FRONT_A = (300, -2600, 1700)
FRONT_C = (500, -3200, 2100)
BACK_A = (-1300, 2500, 1700)
# step: (delay since previous step, action)
STEPS = [
    (1.0, lambda w: (shoot_leaf(w, 'A', (-300, 0, 250), 105), shoot_leaf(w, 'A', (-640, 0, 330), 88),
                     shoot_leaf(w, 'C', (-420, 0, 260), 122), shoot_leaf(w, 'C', (-700, 0, 180), 88))),
    (120.0, lambda w: compiled_state()),
    (1.0, lambda w: picture(w, '01_closed_A', 'A', FRONT_A)),
    (0.5, lambda w: picture(w, '02_closed_C', 'C', FRONT_C)),
    (0.5, lambda w: (move(w, 'Rus_Vehicle_T3476_C', (-460, -900)), log('soviet -> A front'))),
    (1.3, lambda w: picture(w, '03_A_opening', 'A', FRONT_A)),
    (2.7, lambda w: picture(w, '04_A_open', 'A', FRONT_A)),
    (0.3, lambda w: picture(w, '05_A_open_inside', 'A', BACK_A)),
    (0.3, lambda w: (move(w, 'Rus_Vehicle_T3476_C', (-535, GATE_C_Y - 1000)), log('soviet -> C front'))),
    (2.0, lambda w: picture(w, '06_C_opening', 'C', FRONT_C)),
    (3.0, lambda w: picture(w, '07_C_open', 'C', FRONT_C)),
    (2.5, lambda w: picture(w, '08_A_after_soviet_left', 'A', FRONT_A)),
    (0.2, lambda w: (move(w, 'Ger_Vehicle_Panzer38T_C', (-535, GATE_C_Y + 50)), move(w, 'Rus_Vehicle_T3476_C', FAR_SOVIET),
                     log('german in C doorway, soviet far'))),
    (7.0, lambda w: picture(w, '09_C_german_in_doorway', 'C', FRONT_C)),
    (0.2, lambda w: (move(w, 'Ger_Vehicle_Panzer38T_C', FAR_GERMAN), log('german far'))),
    (8.0, lambda w: picture(w, '10_C_closed_again', 'C', FRONT_C)),
    (0.2, lambda w: (move(w, 'Ger_Vehicle_Panzer38T_C', (-460, -900)), log('german -> A front'))),
    (4.0, lambda w: picture(w, '11_A_german_in_front', 'A', FRONT_A)),
]


def setup_editor_world():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log('editor map %s' % world.get_path_name())
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(0, 0, 20000), unreal.Vector(0, 0, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit:
        BASE.z = hit.to_tuple()[4].z
    # material instances first (WallGateVAT switch + VAT params), then the Blueprints
    exec(compile(open(TOOLS + 'ue_gate_materials.py', encoding='utf-8').read(), 'ue_gate_materials.py', 'exec'),
         {'__name__': '__gatetest__'})
    bps = ue_build_gate_bps.build_all()
    log('built %s' % [b.get_name() for b in bps])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for key, y, width in (('A', 0.0, 920.0), ('C', GATE_C_Y, 1070.0)):
        g = actors.spawn_actor_from_class(unreal.load_class(None, ROOT + 'BP_RusWallGate%s.BP_RusWallGate%s_C' % (key, key)),
                                          unreal.Vector(0, y, BASE.z))
        g.set_actor_label('GateTest_%s' % key)
        # flanking walls: SM_Wall_14 runs -2260..0 from its actor origin
        actors.spawn_actor_from_class(unreal.load_class(None, WALL), unreal.Vector(2260, y, BASE.z))
        actors.spawn_actor_from_class(unreal.load_class(None, WALL), unreal.Vector(-width, y, BASE.z))
    for cls, owner, xy in ((SOVIET, 2, FAR_SOVIET), (GERMAN, 1, FAR_GERMAN)):
        tank = actors.spawn_actor_from_class(unreal.load_class(None, cls), unreal.Vector(xy[0], xy[1], BASE.z + 60))
        tank.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', owner)
    S['camera_spawn'] = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, -3000, BASE.z + 1500))


def tick(delta):
    S['t'] += delta
    # compiling, saving, class loads and screenshots tick Slate again: never re-enter (it crashed in a nested load)
    if S.get('busy') or S.get('in_tick'):
        return
    S['in_tick'] = True
    try:
        tick_body()
    finally:
        S['in_tick'] = False


def tick_body():
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            setup_editor_world()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                # fog of war would black out the pictures: switch the fog manager's post process off (as the
                # main-menu battle director does)
                classes = set()
                for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
                    classes.add(actor.get_class().get_name())
                    for pp in actor.get_components_by_class(unreal.PostProcessComponent):
                        log('post process %s on %s enabled=%s' % (pp.get_name(), actor.get_class().get_name(),
                                                                 pp.get_editor_property('enabled')))
                        pp.set_editor_property('enabled', False)
                    if isinstance(actor, unreal.PostProcessVolume):
                        log('pp volume %s unbound=%s' % (actor.get_name(), actor.get_editor_property('unbound')))
                log('classes %s' % sorted(classes))
                S['camera'] = min(of_class(world, 'CameraActor'),
                                  key=lambda c: (c.get_actor_location() - unreal.Vector(0, -3000, BASE.z + 1500)).length())
                log('pie | %s' % gate_text(world))
                S['phase'], S['mark'] = 'steps', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'steps':
            if S['step'] >= len(STEPS):
                finish()
                return
            delay, action = STEPS[S['step']]
            if S['t'] - S['mark'] >= delay:
                S['step'] += 1
                S['mark'] = S['t']
                action(game_world())
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log('ERROR %s step %d: %r' % (phase, S['step'], error))
        finish()


import os  # noqa: E402
os.makedirs(OUT, exist_ok=True)
unreal.register_slate_post_tick_callback(tick)
log('script loaded')
