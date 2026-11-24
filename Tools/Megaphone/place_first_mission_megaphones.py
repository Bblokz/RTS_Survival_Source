"""Places the two broadcast megaphones on Ger_FirstMission and saves the map (throwaway full editor, not the shared one).

    UnrealEditor.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>

M1 patrol base, inside the west gate: Rus_Bxp_Megaphone_Broadcast (Directorate lines).
M2 final-road camp, west of the tents: Rus_Bxp_Megaphone_Broadcast_FuelRation (Directorate + fuel ration cut, x3).
Re-runnable: actors labelled MEGA_* are replaced. Waits for the navmesh rebuild before saving (an early save stores an
empty navmesh), then quits. The depot alarm megaphone (Rus_Bxp_Megaphone, no line sets) is not touched.
"""
import math

import unreal

RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/'
SPOTS = [
    ('MEGA_PatrolBase', RUS + 'Rus_Bxp_Megaphone_Broadcast.Rus_Bxp_Megaphone_Broadcast_C', (500.0, -17700.0), 180.0),
    ('MEGA_FinalCamp', RUS + 'Rus_Bxp_Megaphone_Broadcast_FuelRation.Rus_Bxp_Megaphone_Broadcast_FuelRation_C',
     (12400.0, -30000.0), 0.0),
]
FOLDER = 'Mission/Megaphones'
MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission'
# A map argument on the command line was ignored once (the editor opened StartGame): load and verify explicitly.
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0}


def log(message):
    unreal.log_warning('MEGAPLACE ' + message)


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    return (t[4].z, t[9].get_actor_label() if t[9] else None) if t and t[0] else (None, None)


def place():
    world = editor_world()
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actor_sub.get_all_level_actors():
        if actor.get_actor_label().startswith('MEGA_'):
            log(f'removing old {actor.get_actor_label()}')
            actor_sub.destroy_actor(actor)
    for label, cls_path, (x, y), yaw in SPOTS:
        z, hit_actor = ground_z(world, x, y)
        if z is None:
            log(f'{label}: no ground at ({x}, {y}), skipped')
            continue
        cls = unreal.load_class(None, cls_path)
        actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label(label)
        actor.set_folder_path(FOLDER)
        near = []
        for other in actor_sub.get_all_level_actors():
            if other == actor:
                continue
            loc = other.get_actor_location()
            dist = math.hypot(loc.x - x, loc.y - y)
            if dist < 1200 and other.get_class().get_name() not in ('NavModifierVolume', 'DecalActor'):
                near.append((round(dist), other.get_class().get_name()))
        log(f'{label}: {actor.get_class().get_name()} at ({x:.0f}, {y:.0f}, {z:.0f}) on {hit_actor}; '
            f'within 12 m: {sorted(near)[:8]}')


def tick(delta):
    S['t'] += delta
    world = editor_world()
    if S['phase'] == 'wait_load' and S['t'] > 20.0:
        if world.get_name() != 'Ger_FirstMission':
            log(f'editor world is {world.get_name()}, loading {MAP}')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
            S['t'] = 5.0
            return
        log(f'editor world: {world.get_name()} ({world.get_path_name()})')
        place()
        S['phase'], S['mark'] = 'wait_nav', S['t']
        return
    if S['phase'] == 'wait_nav':
        building = unreal.NavigationSystemV1.is_navigation_being_built(world)
        navs = [a for a in unreal.EditorActorSubsystem().get_all_level_actors() if a.get_class().get_name() == 'RecastNavMesh']
        sized = any(n.get_actor_bounds(False)[1].x > 0 for n in navs)
        if (not building and sized and S['t'] - S['mark'] > 10.0) or S['t'] - S['mark'] > 900.0:
            log(f'navmesh built={not building} sized={sized} after {S["t"] - S["mark"]:.0f} s; saving')
            if world.get_name() != 'Ger_FirstMission':
                log(f'not saving: editor world is {world.get_name()}')
                S['phase'] = 'done'
                unreal.SystemLibrary.quit_editor()
                return
            ok = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
            log(f'saved: {ok}')
            S['phase'] = 'done'
            unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
