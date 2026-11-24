"""Records the Wall Breach takeover clip frames: a player Tiger H1 drives through a Soviet Rus_Wall_Concrete_Long.

Runs inside a throwaway DebugGame editor on RTS_ToCopySmall (never the shared editor, never a mission map), all from
Python: places the wall (owner 2) and the Tiger (owner 1) in the editor world, rebuilds the navmesh, starts PIE, and
after a settle orders the Tiger to a point past the wall. Every 1/24 s of game time it exports one picture per camera
from a SceneCapture2D in the PIE world (screenshots render the editor viewport or the RTS camera pawn instead):
  Frames/       exit side, three-quarter, the wall face the Tiger bursts out of
  FramesEntry/  entry side, three-quarter from behind the Tiger's left
  FramesWide/   high RTS-like view of the whole crossing
Nothing is saved: the map, walls and filters stay as they are on disk.
Launch: sh capture_clip.sh   (log Saved/Logs/WallBreachClip.log, grep WALL_BREACH_CLIP)
Then: python ../VulturesAt/make_clip_atlas.py --clip WallBreach --segments ...
"""
import json
import os

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/WallBreach/'
WALL_BP = os.environ.get('WALL_BREACH_WALL', 'Rus_Wall_Concrete_Long')
WALL_CLASS = f'/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/{WALL_BP}.{WALL_BP}_C'
# The long wall's actor origin is at one end; this offset puts its middle on the lane.
WALL_OFFSET_X = 1130.0
TIGER_CLASS = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/'
               'Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C')
LANE_X = float(os.environ.get('WALL_BREACH_X', '0'))
LANE_Y = float(os.environ.get('WALL_BREACH_Y', '0'))
# Signed: the Tiger starts this far from the wall on that side and drives to the other. On RTS_ToCopySmall the sun
# lights the wall's +Y face, so a start on -Y bursts out of the lit face towards the exit camera.
START_Y = float(os.environ.get('WALL_BREACH_START', '-1900'))
SIGN = 1.0 if START_Y > 0 else -1.0
END_Y = 2600.0
WALL_OWNER = 2
TANK_OWNER = 1
FRAME_DT = 1.0 / 24.0
PICTURE_W, PICTURE_H = 1280, 720
FOV = 55.0
SETTLE_SECONDS = 3.0
# After the Tiger is this far past the wall (or MAX_RECORD seconds), record TAIL_SECONDS more for the dust and rubble.
CLEAR_DISTANCE = 1300.0
TAIL_SECONDS = 2.0
MAX_RECORD_SECONDS = 30.0
# name: (look-at offset from the wall middle on the ground, pitch, yaw, distance), written for a start on +Y and
# mirrored across the wall for a start on -Y.
CAMERAS = {
    '': ((0.0, -150.0, 140.0), -24.0, 128.0, 1900.0),
    'Entry': ((0.0, 300.0, 140.0), -26.0, -128.0, 1900.0),
    'Wide': ((0.0, 0.0, 0.0), -55.0, 155.0, 3600.0),
}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'frames': 0, 'next_frame': None, 'events': []}


def log(message):
    S['log'].append(f'{S["t"]:.1f} {message}')
    unreal.log('WALL_BREACH_CLIP ' + message)


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


def set_owner(actor, player):
    comp = actor.get_component_by_class(unreal.RTSComponent) if actor else None
    if comp is not None:
        comp.set_editor_property('owning_player', player)
    return comp is not None


def place():
    world = editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # Traced beside the wall line: a trace on it would land on the wall's top.
    z = ground_z(world, LANE_X, LANE_Y + 1500 * SIGN)
    wall = actors.spawn_actor_from_class(unreal.load_class(None, WALL_CLASS),
                                         unreal.Vector(LANE_X + WALL_OFFSET_X, LANE_Y, z), unreal.Rotator(0, 0, 0))
    wall.set_actor_label('CLIP_WALL')
    tz = ground_z(world, LANE_X, LANE_Y + START_Y)
    tank = actors.spawn_actor_from_class(unreal.load_class(None, TIGER_CLASS),
                                         unreal.Vector(LANE_X, LANE_Y + START_Y, tz + 50), unreal.Rotator(0, 0, -90 * SIGN))
    tank.set_actor_label('CLIP_TIGER')
    S['editor_ground'] = z
    place_cameras(world, z)
    log(f'placed {WALL_BP} owner {WALL_OWNER} {set_owner(wall, WALL_OWNER)}, Tiger owner {TANK_OWNER} '
        f'{set_owner(tank, TANK_OWNER)}, ground {z:.0f}')


def find_runtime(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        name = actor.get_class().get_name()
        loc = actor.get_actor_location()
        if name == WALL_BP + '_C' and abs(loc.x - LANE_X - WALL_OFFSET_X) < 50 and abs(loc.y - LANE_Y) < 50:
            S['wall'] = actor
        if name.startswith('BP_FogOfWar_Manager'):
            for comp in actor.get_components_by_class(unreal.PostProcessComponent):
                comp.set_editor_property('enabled', False)
    for tank in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster):
        loc = tank.get_actor_location()
        if abs(loc.x - LANE_X) < 300 and abs(loc.y - LANE_Y - START_Y) < 500:
            S['tank'] = tank
    log(f'runtime wall {alive(S.get("wall"))} tank {alive(S.get("tank"))}')


def camera_pose(name, ground):
    offset, pitch, yaw, distance = CAMERAS[name]
    rotation = unreal.Rotator(0.0, pitch, yaw * SIGN)
    look = unreal.Vector(LANE_X, LANE_Y, ground) + unreal.Vector(offset[0], offset[1] * SIGN, offset[2])
    return look - rotation.get_forward_vector() * distance, rotation


def place_cameras(world, ground):
    """In the editor world, so PIE duplicates them; set up for capture once PIE runs."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for name in CAMERAS:
        location, rotation = camera_pose(name, ground)
        capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, location, rotation)
        capture.set_actor_label('CLIP_CAM_' + (name or 'Exit'))


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
        # Rendering every frame with kept state lets auto exposure and temporal AA settle like the game view.
        comp.set_editor_property('capture_every_frame', True)
        comp.set_editor_property('capture_on_movement', False)
        comp.set_editor_property('always_persist_rendering_state', True)
        # The fog of war is a post-process material.
        comp.set_editor_property('show_flag_settings', [
            unreal.EngineShowFlagsSetting(show_flag_name='PostProcessMaterial', enabled=False)])
        folder = ROOT + 'Frames' + name
        os.makedirs(folder, exist_ok=True)
        S['cams'][name] = (capture, target, folder)
        log(f'camera {name or "Exit"} at {capture.get_actor_location()} rot {rotation}')


def save_frames(world):
    name = f'Frame_{S["frames"]:04d}.png'
    for capture, target, folder in S['cams'].values():
        unreal.RenderingLibrary.export_render_target(world, target, folder, name)
    S['frames'] += 1


def wall_state():
    wall = S.get('wall')
    if not alive(wall):
        return None
    holes = wall.get_component_by_class(unreal.WallHoleDamageComponent)
    return dict(breaches=holes.get_breach_count() if holes else None,
                removed=round(holes.get_removed_face_share(), 3) if holes else None,
                health=round(wall.get_editor_property('health')))


def track(world):
    tank = S.get('tank')
    state = wall_state()
    loc = tank.get_actor_location() if alive(tank) else None
    # Distance still to go to the wall line; negative once past it.
    dy = SIGN * (loc.y - LANE_Y) if loc else None
    frame = S['frames']
    if state and state['breaches'] and not S.get('breach_frame'):
        S['breach_frame'] = frame
        S['events'].append(('first_breach', frame, dy, state))
        log(f'first breach at frame {frame}, tank dy {dy:.0f}, {json.dumps(state)}')
    if dy is not None and dy < 150 and not S.get('contact_frame'):
        S['contact_frame'] = frame
        S['events'].append(('hull_at_wall', frame, round(dy)))
        log(f'hull reaches the wall line at frame {frame} (dy {dy:.0f})')
    if dy is not None and dy < -400 and not S.get('through_frame'):
        S['through_frame'] = frame
        S['events'].append(('through', frame, round(dy)))
        log(f'hull through at frame {frame} (dy {dy:.0f})')
    if frame % 24 == 0:
        log(f'frame {frame} tank dy {dy if dy is None else round(dy)} wall {json.dumps(state)}')
    return dy


def tick(delta):
    S['t'] += delta
    # Spawning Blueprints and exporting can re-enter the Slate tick; run one step at a time.
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
            S['ground'] = ground_z(world, LANE_X, LANE_Y + 1500 * SIGN)
            S['phase'], S['mark'] = 'settle', S['t']
        elif S['t'] - S['mark'] > 150.0:
            log('pie never started')
            finish()
    elif phase == 'settle':
        world = game_world()
        if (nav_building(world) and S['t'] - S['mark'] < 40.0) or S['t'] - S['mark'] < 2.0:
            return
        if 'cams' not in S:
            make_cameras(world)
            S['settle_game'] = unreal.GameplayStatics.get_time_seconds(world)
            return
        if unreal.GameplayStatics.get_time_seconds(world) - S['settle_game'] < SETTLE_SECONDS:
            return
        tank = S.get('tank')
        if not alive(tank) or not alive(S.get('wall')):
            log('units missing')
            finish()
            return
        target = unreal.Vector(LANE_X, LANE_Y - END_Y * SIGN, S['ground'] + 20)
        result = tank.move_to_location(target, True, unreal.Rotator(0, 0, -90 * SIGN), False)
        S['order_game'] = unreal.GameplayStatics.get_time_seconds(world)
        S['next_frame'] = S['order_game']
        S['last_game'] = S['order_game']
        S['events'].append(('move_order', 0))
        log(f'move order to {target}: {result}; wall {json.dumps(wall_state())}')
        S['phase'] = 'record'
    elif phase == 'record':
        world = game_world()
        now = unreal.GameplayStatics.get_time_seconds(world)
        step_game = now - S['last_game']
        S['last_game'] = now
        S.setdefault('steps', []).append(round(step_game, 4))
        if len(S['steps']) == 48:
            log(f'game seconds per tick (first 48): min {min(S["steps"]):.4f} max {max(S["steps"]):.4f}')
        # One picture per 1/24 s of game time, whatever the editor's tick rate; a slow tick repeats the frame.
        while now + 1e-4 >= S['next_frame']:
            save_frames(world)
            S['next_frame'] += FRAME_DT
        dy = track(world)
        elapsed = now - S['order_game']
        if dy is not None and dy < -CLEAR_DISTANCE and 'clear_game' not in S:
            S['clear_game'] = now
            log(f'Tiger clear at frame {S["frames"]}')
        if ('clear_game' in S and now - S['clear_game'] > TAIL_SECONDS) or elapsed > MAX_RECORD_SECONDS:
            log(f'recorded {S["frames"]} frames over {elapsed:.1f} game s; wall {json.dumps(wall_state())}; '
                f'repeated ticks {sum(1 for s in S["steps"] if s > FRAME_DT * 1.5)}')
            finish()
    if S['t'] > 600.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
