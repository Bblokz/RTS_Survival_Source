"""Throwaway-editor squad test of Squad_Ger_Burrowers (build_burrowers.py): spawns the squad Blueprint in PIE on
RTS_ToCopySmall, checks subtype, soldier count, meshes, weapons and owner, and takes pictures into Saved/Burrowers/squad/.
Never in the shared editor; the map is never saved. Launch: kumpel_look_test_run.sh 600 squad
"""
import json

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/Burrowers/squad/'
SQUAD = '/Game/RTS_Survival/Blueprints/Squads/Ger/Burrowers/Squad_Ger_Burrowers.Squad_Ger_Burrowers_C'
CAM_HOME = unreal.Vector(0, 6000, 2000)
SHOTS = [('squad_side', (-900, -500, 450), (0, 0, 90)), ('squad_close', (-450, -250, 220), (0, 0, 110)),
         ('squad_game', (-1600, 1600, 2400), (0, 0, 60))]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'result': {}, 'busy': False, 'queue': []}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('BURROWSQUAD ' + message)


def finish():
    try:
        json.dump(dict(log=S['log'], result=S['result']), open(OUT + 'result.json', 'w'), indent=1, default=str)
    finally:
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    S['centre'] = unreal.Vector(0, 0, ground_z(world, 0, 0))
    cam = actors.spawn_actor_from_class(unreal.CameraActor, CAM_HOME)
    cam.camera_component.set_editor_property('field_of_view', 50.0)
    # Placed in the level before PIE (Python cannot spawn into the PIE world): PIE duplicates it and its BeginPlay
    # spawns the soldiers, as for any squad placed on a map.
    squad = actors.spawn_actor_from_class(unreal.load_class(None, SQUAD), S['centre'] + unreal.Vector(0, 0, 120))
    log(f'placed {squad.get_name() if squad else None}')


def spawn_squad(world):
    cls = unreal.load_class(None, SQUAD)
    found = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    S['squad'] = found[0] if found else None
    log(f'runtime squads {len(found)}')


def check(world):
    sq = S['squad']
    res = {'squad_valid': bool(sq and unreal.SystemLibrary.is_valid(sq))}
    if res['squad_valid']:
        for prop in ('Squad Subtype', 'SquadSubtype'):
            try:
                res['subtype'] = str(sq.get_editor_property(prop))
                break
            except Exception:  # noqa: BLE001
                pass
    soldiers = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character):
        if (a.get_actor_location() - S['centre']).length() > 3000:
            continue
        mesh = a.get_component_by_class(unreal.SkeletalMeshComponent)
        sk = mesh.get_skeletal_mesh_asset() if mesh else None
        weapons = []
        for c in a.get_components_by_class(unreal.ChildActorComponent):
            child = c.get_editor_property('child_actor')
            weapons.append(child.get_class().get_name() if child else 'class:' + str(c.get_editor_property('child_actor_class')))
        anim = mesh.get_anim_instance() if mesh else None
        soldiers.append(dict(cls=a.get_class().get_name(), mesh=sk.get_name() if sk else None, weapons=weapons,
                             anim=anim.get_class().get_name() if anim else None,
                             mats=[mesh.get_material(i).get_name() for i in range(mesh.get_num_materials())] if mesh else []))
    res['soldiers'] = soldiers
    res['kumpels'] = len([s for s in soldiers if s['mesh'] == 'SK_BurrowerKumpel'])
    S['result'] = res
    log('check ' + json.dumps(res, default=str))
    if soldiers:
        xs = [a.get_actor_location() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character)
              if (a.get_actor_location() - S['centre']).length() <= 3000]
        S['centre'] = unreal.Vector(sum(v.x for v in xs) / len(xs), sum(v.y for v in xs) / len(xs), xs[0].z - 90)


def find_runtime(world):
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S['camera'] = min(cams, key=lambda c: (c.get_actor_location() - CAM_HOME).length())


def look(eye_off, look_off):
    cam, o = S['camera'], S['centre']
    eye, target = o + unreal.Vector(*eye_off), o + unreal.Vector(*look_off)
    cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    cam.set_actor_location(eye, False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(cam, 0.0)


def picture(name):
    S['busy'] = True
    try:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, f'{OUT}{name}.png', camera=S['camera'])
    finally:
        S['busy'] = False


def tick(delta):
    S['t'] += delta
    if S['busy'] or S['phase'] == 'done':
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            place_all()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.PostProcessMaterial 0')
                unreal.SystemLibrary.execute_console_command(world, 'r.Streaming.FullyLoadUsedTextures 1')
                find_runtime(world)
                spawn_squad(world)
                S['phase'], S['mark'] = 'wait_squad', S['t']
            elif S['t'] - S['mark'] > 150.0:
                log('pie never started')
                finish()
        elif phase == 'wait_squad' and S['t'] - S['mark'] > 10.0:
            check(game_world())
            S['queue'] = list(SHOTS)
            look(*SHOTS[0][1:])
            S['phase'], S['mark'] = 'warmup', S['t']
        elif phase == 'warmup' and S['t'] - S['mark'] > 6.0:
            picture('_warmup')
            S['phase'], S['mark'] = 'next', S['t']
        elif phase == 'next' and S['t'] - S['mark'] > 2.0:
            if not S['queue']:
                finish()
                return
            S['current'] = S['queue'].pop(0)
            look(*S['current'][1:])
            S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 3.0:
            picture(S['current'][0])
            log('picture ' + S['current'][0])
            S['phase'], S['mark'] = 'next', S['t']
        elif S['t'] > 400.0:
            log('timeout in ' + phase)
            finish()
    except Exception as exc:  # noqa: BLE001
        log(f'ERROR in {phase}: {exc!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
