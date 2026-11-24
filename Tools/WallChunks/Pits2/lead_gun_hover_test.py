"""Lead check (2026-11-12): a cursor (Visibility) trace aimed at a pit's gun must hit the pit (PitWeaponHitCollider),
not pass through to the ground. Enemy-owned pits, as the player sees them.

Throwaway DebugGame editor only:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
"""
import json
import math
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/gun_hover_result.json'
BX = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
SUBJECTS = [
    BX + 'Rus53KPit/Rus_BXP_53KPit.Rus_BXP_53KPit_C',
    BX + 'Rus53KPit/Rus_BXP_53KSectorPit.Rus_BXP_53KSectorPit_C',
    BX + 'GerPits/Ger_BXP_GerPit_Pak40.Ger_BXP_GerPit_Pak40_C',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerPit.Rus_BXP_B4HowitzerPit_C',
]
SPOT0 = unreal.Vector(11500, -7000, 0)
SPACING = 2600.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': []}


def log(message):
    S['log'].append(message)
    unreal.log('GUNHOVER ' + message)


def finish():
    try:
        json.dump(S['log'], open(OUT, 'w'), indent=1)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def ground_z(world, spot):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(spot.x, spot.y, 20000),
                                                 unreal.Vector(spot.x, spot.y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 100.0


def gun_point(actor):
    """Centre of the gun child actor's turret mesh bounds (falls back to 1 m above the pit)."""
    for child in actor.get_components_by_class(unreal.ChildActorComponent):
        gun = child.get_editor_property('child_actor')
        if gun is None:
            continue
        origin, extent = gun.get_actor_bounds(False)
        return origin, gun.get_name()
    return actor.get_actor_location() + unreal.Vector(0, 0, 100), 'none'


def check(world, actor):
    target, gun_name = gun_point(actor)
    hits = 0
    shots = 0
    for yaw in (0, 60, 120, 180, 240, 300):
        # RTS camera: about 55 degrees down, 18 m away.
        direction = unreal.Vector(math.cos(math.radians(yaw)) * 0.57, math.sin(math.radians(yaw)) * 0.57, -0.82)
        start = target - direction * 1800.0
        result = unreal.SystemLibrary.line_trace_single(world, start, target + direction * 400.0,
                                                        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                        unreal.DrawDebugTrace.NONE, True)
        shots += 1
        if not result:
            log(f'{actor.get_name()} yaw {yaw}: no hit')
            continue
        values = result.to_tuple()
        hit_actor, hit_comp = values[9], values[10]
        on_pit = hit_actor == actor
        hits += 1 if on_pit else 0
        log(f'{actor.get_name()} yaw {yaw}: hit {hit_actor.get_name() if hit_actor else None}.'
            f'{hit_comp.get_name() if hit_comp else None} pit={on_pit}')
    log(f'{actor.get_name()} RESULT gun {gun_name}: {hits}/{shots} cursor traces at the gun hit the pit')


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done':
        return
    try:
        if S['phase'] == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            for index, path in enumerate(SUBJECTS):
                spot = SPOT0 + unreal.Vector(index * SPACING, 0, 0)
                spot.z = ground_z(world, spot)
                placed = actors.spawn_actor_from_class(unreal.load_class(None, path), spot)
                placed.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', 2)
                log(f'placed {placed.get_name()} owner 2')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif S['phase'] == 'wait_pie':
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if world is not None and S['t'] - S['mark'] > 10.0:
                for path in SUBJECTS:
                    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, path)):
                        if actor.get_class().get_path_name() == path:
                            check(world, actor)
                finish()
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
    except Exception as error:
        log(f'error {error}')
        finish()


S['handle'] = unreal.register_slate_post_tick_callback(tick)
log('script loaded')
