"""Records the run-over clip frames for page 2 of the crushing field manual (UW_WallBreachTakeover): a player Tiger H1
drives through an enemy Soviet rifle squad (Rus_Squad_Mosin, owner 2) that stands in its path.

Same recipe as ../WallBreach/capture_clip.py: runs inside a throwaway DebugGame editor on RTS_ToCopySmall (never the
shared editor, never a mission map), all from Python. Places the Tiger (owner 1) and the squad (owner 2, beside the lane
so its soldiers spawn clear of the hull) in the editor world, starts PIE, puts the soldiers in the lane with
MissionMapTestLibrary.map_test_place_squad_soldiers, holds the Tiger's fire every tick (map_test_hold_tank_fire: it must
crush them, not shoot them), and orders the Tiger through. URTSVehicleCrushSettings.DodgeChance is set to 0 on the
command line (-ini:Game override in capture_clip.sh, memory only), so every soldier in the path stays.
Every 1/24 s of game time it exports one picture per camera from a SceneCapture2D in the PIE world:
  Frames/      low, in front of the squad, looking back at the Tiger as it comes on
  FramesSide/  from the side of the lane, the hull passing over the squad and the blood left behind
  FramesWide/  high RTS-like view of the whole run
Nothing is saved. Launch: sh capture_clip.sh (log Saved/Logs/RunOverClip.log, grep RUN_OVER_CLIP)
Then: python ../VulturesAt/make_clip_atlas.py --clip RunOver --segments ...
"""
import json
import os

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/RunOver/'
TIGER_CLASS = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/'
               'Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C')
SQUAD_CLASS = '/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C'
LANE_X = float(os.environ.get('RUN_OVER_X', '0'))
LANE_Y = float(os.environ.get('RUN_OVER_Y', '0'))
# The Tiger starts this far before the squad (-Y) and drives along +Y; the sun on RTS_ToCopySmall lights the +Y faces.
START_Y = float(os.environ.get('RUN_OVER_START', '-1500'))
END_Y = 2600.0
# Where the soldiers stand in the lane: two ranks of three, all inside the Tiger's hull width.
SQUAD_Y = 400.0
SOLDIER_SPOTS = [(-75.0, 0.0), (10.0, 20.0), (90.0, -10.0), (-40.0, 120.0), (55.0, 135.0), (-110.0, 150.0),
                 (0.0, 240.0), (100.0, 230.0)]
# The squad spawns this far beside the lane; its soldiers are moved into it once they exist.
SQUAD_SPAWN_SIDE = 1800.0
TANK_OWNER = 1
SQUAD_OWNER = 2
FRAME_DT = 1.0 / 24.0
PICTURE_W, PICTURE_H = 1280, 720
FOV = 55.0
SETTLE_SECONDS = 2.0
# After the Tiger is this far past the squad (or MAX_RECORD seconds), record TAIL_SECONDS more for the blood decal.
CLEAR_DISTANCE = 900.0
TAIL_SECONDS = 2.5
MAX_RECORD_SECONDS = 25.0
# name: (look-at offset from the squad's middle on the ground, pitch, yaw, distance).
CAMERAS = {
    '': ((0.0, -100.0, 110.0), -16.0, -112.0, 1500.0),
    'Side': ((0.0, 120.0, 60.0), -28.0, 180.0, 1450.0),
    'Wide': ((0.0, 0.0, 0.0), -55.0, 150.0, 3000.0),
}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'frames': 0, 'next_frame': None, 'events': []}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('RUN_OVER_CLIP ' + message)


def finish():
    try:
        os.makedirs(ROOT, exist_ok=True)
        json.dump(dict(log=S['log'], events=S['events'], frames=S['frames']), open(ROOT + 'capture.json', 'w'),
                  indent=1, default=str)
    finally:
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def alive(actor):
    try:
        return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except Exception:  # noqa: BLE001
        return False


def nav_building(world):
    try:
        return bool(unreal.NavigationSystemV1.is_navigation_being_built(world))
    except Exception:  # noqa: BLE001
        return False


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def owner(actor):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    return comp.get_editor_property('owning_player') if comp is not None else -1


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def place():
    world = editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    z = ground_z(world, LANE_X, LANE_Y + SQUAD_Y)
    tz = ground_z(world, LANE_X, LANE_Y + START_Y)
    tank = actors.spawn_actor_from_class(unreal.load_class(None, TIGER_CLASS),
                                         unreal.Vector(LANE_X, LANE_Y + START_Y, tz + 50), unreal.Rotator(0, 0, 90))
    tank.set_actor_label('CLIP_TIGER')
    sz = ground_z(world, LANE_X + SQUAD_SPAWN_SIDE, LANE_Y + SQUAD_Y)
    squad = actors.spawn_actor_from_class(unreal.load_class(None, SQUAD_CLASS),
                                          unreal.Vector(LANE_X + SQUAD_SPAWN_SIDE, LANE_Y + SQUAD_Y, sz + 100),
                                          unreal.Rotator(0, 0, -90))
    squad.set_actor_label('CLIP_SQUAD')
    S['editor_ground'] = z
    place_cameras(world, z)
    log(f'placed Tiger owner {TANK_OWNER} {set_owner(tank, TANK_OWNER)}, squad owner {SQUAD_OWNER} '
        f'{set_owner(squad, SQUAD_OWNER)}, ground {z:.0f}')


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        if name.startswith('Rus_Squad_Mosin') and abs(loc.x - LANE_X - SQUAD_SPAWN_SIDE) < 400:
            S['squad'] = actor
        if name.startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        if abs(loc.x - LANE_X) < 300 and abs(loc.y - LANE_Y - START_Y) < 500:
            S['tank'] = tank
    log(f'runtime squad {alive(S.get("squad"))} owner {owner(S.get("squad"))}, tank {alive(S.get("tank"))}')


def soldiers(world):
    """Enemy soldiers that are still standing (a crushed one dies at once and turns into a ragdoll)."""
    found = []
    for unit in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SquadUnit):
        if owner(unit) != SQUAD_OWNER:
            continue
        try:
            health = unit.get_component_by_class(unreal.HealthComponent)
            if health is not None and health.call_method('GetCurrentHealth') <= 0:
                continue
        except Exception:  # noqa: BLE001
            pass
        found.append(unit)
    return found


def camera_pose(name, ground):
    offset, pitch, yaw, distance = CAMERAS[name]
    rotation = unreal.Rotator(0.0, pitch, yaw)
    look = unreal.Vector(LANE_X, LANE_Y + SQUAD_Y, ground) + unreal.Vector(*offset)
    return look - rotation.get_forward_vector() * distance, rotation


def place_cameras(world, ground):
    """In the editor world, so PIE duplicates them; set up for capture once PIE runs."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for name in CAMERAS:
        location, rotation = camera_pose(name, ground)
        capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, location, rotation)
        capture.set_actor_label('CLIP_CAM_' + (name or 'Front'))


def make_cameras(world):
    S['cams'] = {}
    found = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)
    for name in CAMERAS:
        location, rotation = camera_pose(name, S['editor_ground'])
        capture = min(found, key=lambda c: (c.get_actor_location() - location).length())
        capture.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        capture.set_actor_location_and_rotation(location, rotation, False, True)
        comp = capture.capture_component2d
        target = unreal.RenderingLibrary.create_render_target2d(world, PICTURE_W, PICTURE_H,
                                                                unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        comp.set_editor_property('texture_target', target)
        comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property('fov_angle', FOV)
        comp.set_editor_property('capture_every_frame', True)
        comp.set_editor_property('capture_on_movement', False)
        comp.set_editor_property('always_persist_rendering_state', True)
        # The fog of war is a post-process material.
        comp.set_editor_property('show_flag_settings', [
            unreal.EngineShowFlagsSetting(show_flag_name='PostProcessMaterial', enabled=False)])
        folder = ROOT + 'Frames' + name
        os.makedirs(folder, exist_ok=True)
        S['cams'][name] = (capture, target, folder)
        log(f'camera {name or "Front"} at {capture.get_actor_location()} rot {rotation}')


def save_frames(world):
    name = f'Frame_{S["frames"]:04d}.png'
    for capture, target, folder in S['cams'].values():
        unreal.RenderingLibrary.export_render_target(world, target, folder, name)
    S['frames'] += 1


def hold_fire():
    tank = S.get('tank')
    return unreal.MissionMapTestLibrary.map_test_hold_tank_fire(tank) if alive(tank) else 0


def place_soldiers(world):
    spots = []
    for across, along in SOLDIER_SPOTS:
        x, y = LANE_X + across, LANE_Y + SQUAD_Y + along
        spots.append(unreal.Vector(x, y, ground_z(world, x, y)))
    return unreal.MissionMapTestLibrary.map_test_place_squad_soldiers(S['squad'], spots)


def track(world):
    tank = S.get('tank')
    loc = tank.get_actor_location() if alive(tank) else None
    # Distance still to go to the squad's front rank; negative once past it.
    dy = (LANE_Y + SQUAD_Y - loc.y) if loc else None
    standing = len(soldiers(world))
    frame = S['frames']
    if standing < S.get('standing', standing):
        S['events'].append(('soldier_down', frame, None if dy is None else round(dy), standing))
        log(f'soldier down at frame {frame}: {standing} standing, tank dy {dy:.0f}')
    S['standing'] = standing
    if dy is not None and dy < 300 and not S.get('contact_frame'):
        S['contact_frame'] = frame
        S['events'].append(('hull_at_squad', frame, round(dy)))
        log(f'hull reaches the squad at frame {frame} (dy {dy:.0f})')
    if frame % 24 == 0:
        speed = tank.get_velocity().length() if alive(tank) else 0
        log(f'frame {frame} tank dy {dy if dy is None else round(dy)} speed {speed:.0f} standing {standing}')
    return dy


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done' or S.get('busy'):
        return
    S['busy'] = True
    try:
        step()
    except Exception as error:  # noqa: BLE001
        log(f'ERROR {S["phase"]}: {error!r}')
        finish()
    finally:
        S['busy'] = False


def step():
    phase = S['phase']
    if phase == 'wait_load' and S['t'] > 15.0:
        # DodgeChance = 0 comes from capture_clip.sh's -ini:Game override (memory only; the settings class is not
        # exposed to Python, so set_editor_property cannot reach it).
        place()
        unreal.SystemLibrary.execute_console_command(editor_world(), 'RebuildNavigation')
        S['phase'], S['mark'] = 'editor_nav', S['t']
    elif phase == 'editor_nav' and S['t'] - S['mark'] > 3.0:
        if nav_building(editor_world()) and S['t'] - S['mark'] < 120.0:
            return
        log(f'editor navmesh rebuilt in {S["t"] - S["mark"]:.1f} s')
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        S['phase'], S['mark'] = 'wait_pie', S['t']
    elif phase == 'wait_pie':
        world = game_world()
        if world is not None and S['t'] - S['mark'] > 12.0:
            unreal.GameplayStatics.set_game_paused(world, False)
            for command in ('r.TextureStreaming 0', 'r.Streaming.PoolSize 6000'):
                unreal.SystemLibrary.execute_console_command(world, command)
            find_runtime(world)
            S['ground'] = ground_z(world, LANE_X, LANE_Y + SQUAD_Y)
            S['phase'], S['mark'] = 'wait_soldiers', S['t']
        elif S['t'] - S['mark'] > 150.0:
            log('pie never started')
            finish()
    elif phase == 'wait_soldiers':
        world = game_world()
        hold_fire()
        if not alive(S.get('squad')) or not alive(S.get('tank')):
            log('units missing')
            finish()
            return
        placed = place_soldiers(world)
        # The squad's soldiers spawn over a few frames; wait until the count holds still.
        if placed != S.get('placed'):
            S['placed'], S['placed_at'] = placed, S['t']
            return
        if placed == 0 or S['t'] - S['placed_at'] < 2.0:
            if S['t'] - S['mark'] > 60.0:
                log(f'soldiers never spawned ({placed})')
                finish()
            return
        log(f'{placed} soldiers placed in the lane; owners {[owner(u) for u in soldiers(world)]}')
        S['phase'], S['mark'] = 'settle', S['t']
    elif phase == 'settle':
        world = game_world()
        hold_fire()
        if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 1.0:
            return
        if 'cams' not in S:
            make_cameras(world)
            S['settle_game'] = unreal.GameplayStatics.get_time_seconds(world)
            return
        if unreal.GameplayStatics.get_time_seconds(world) - S['settle_game'] < SETTLE_SECONDS:
            return
        # A last placement: soldiers that wandered while the cameras settled stand in the lane again.
        place_soldiers(world)
        tank = S['tank']
        target = unreal.Vector(LANE_X, LANE_Y + END_Y, S['ground'] + 20)
        result = tank.move_to_location(target, True, unreal.Rotator(0, 0, 90), False)
        log(f'move order to {target}: {result}; weapons held {hold_fire()}; standing {len(soldiers(world))}')
        S['order_game'] = unreal.GameplayStatics.get_time_seconds(world)
        S['next_frame'] = S['order_game']
        S['last_game'] = S['order_game']
        S['standing'] = len(soldiers(world))
        S['events'].append(('move_order', 0, S['standing']))
        S['phase'] = 'record'
    elif phase == 'record':
        world = game_world()
        hold_fire()
        now = unreal.GameplayStatics.get_time_seconds(world)
        step_game = now - S['last_game']
        S['last_game'] = now
        S.setdefault('steps', []).append(round(step_game, 4))
        if len(S['steps']) == 48:
            log(f'game seconds per tick (first 48): min {min(S["steps"]):.4f} max {max(S["steps"]):.4f}')
        while now + 1e-4 >= S['next_frame']:
            save_frames(world)
            S['next_frame'] += FRAME_DT
        dy = track(world)
        elapsed = now - S['order_game']
        if dy is not None and dy < -CLEAR_DISTANCE and 'clear_game' not in S:
            S['clear_game'] = now
            log(f'Tiger clear of the squad at frame {S["frames"]}')
        if ('clear_game' in S and now - S['clear_game'] > TAIL_SECONDS) or elapsed > MAX_RECORD_SECONDS:
            log(f'recorded {S["frames"]} frames over {elapsed:.1f} game s; {len(soldiers(world))} soldiers standing; '
                f'repeated ticks {sum(1 for s in S["steps"] if s > FRAME_DT * 1.5)}')
            finish()
    if S['t'] > 600.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
