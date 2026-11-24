"""Throwaway-editor run: give the Soviet BXPs that die with the looping NS_Fire_Tank an 8 s death fire, then prove it.

    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>
    env DEATHFIRE_OUT  = result json path
    env DEATHFIRE_SET  = 1 to write ABuildingExpansion::DeathVfxLifetime on the BPs (default 1)
    env DEATHFIRE_SAVE = 1 to also save them (default 1; 0 tests the in-memory value only)

1. Sets DeathVfxLifetime = 8 on the five BPs whose death graph passes NS_Fire_Tank as CollapseVfx, compiles, saves.
2. Spawns those five plus the two megaphone children (they inherit it) in the editor world, starts PIE, kills them all
   with TriggerDestroyActor and samples every live NS_Fire_Tank component per building (game time).
Checks: each building had a fire, the CDO value is 8, every fire is gone by kill + 8 s + 2 s fade + margin.
"""
import json
import os

import unreal

OUT = os.environ.get('DEATHFIRE_OUT', 'D:/UE5Projects/RTS_Survival/Saved/DeathFireLifetime/result.json')
DO_SET = os.environ.get('DEATHFIRE_SET', '1') == '1'
# 0 = set the value in memory only (test the C++ path while another editor holds the .uasset files).
DO_SAVE = os.environ.get('DEATHFIRE_SAVE', '1') == '1'
RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/'
LIFETIME = 8.0
FADE = 2.0
MARGIN = 3.0
SET_ON = ['Rus_Building_FuelStorageTank', 'Rus_Building_FuelStorageTankSmall', 'Rus_Bxp_Megaphone',
          'Rus_Bxp_Megaphone_Propaganda1', 'Rus_Bxp_ContainerHut']
SPAWN = SET_ON + ['Rus_Bxp_Megaphone_Broadcast', 'Rus_Bxp_Megaphone_Broadcast_FuelRation']
SPACING = 3500.0
KILL_AT = 4.0
END_AFTER_KILL = LIFETIME + FADE + MARGIN + 4.0
# The BPs reference NS_Fire_Tank, a redirector: the loaded system is NS_Fire_Tank_1.
FIRE_NAME = 'NS_Fire_Tank'
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'spots': {}, 'fire': {}}


def log(message):
    S['log'].append(f"{S['t']:7.1f} {message}")
    unreal.log_warning('DEATHFIRE ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(1 for c in S['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(S['checks']), 'checks': S['checks'], 'fire': S['fire'],
               'log': S['log']}, open(OUT, 'w'), indent=1)
    log(f'RESULT {passed}/{len(S["checks"])}')
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def bp_path(name):
    return RUS + name + '.' + name


def set_lifetimes():
    for name in SET_ON:
        bp = unreal.load_asset(bp_path(name))
        cdo = unreal.get_default_object(bp.generated_class())
        before = cdo.get_editor_property('death_vfx_lifetime')
        cdo.set_editor_property('death_vfx_lifetime', LIFETIME)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(bp.generated_class())
        if abs(cdo.get_editor_property('death_vfx_lifetime') - LIFETIME) > 0.01:
            cdo.set_editor_property('death_vfx_lifetime', LIFETIME)
        saved = unreal.EditorAssetLibrary.save_loaded_asset(bp, False) if DO_SAVE else None
        log(f'{name}: DeathVfxLifetime {before} -> {cdo.get_editor_property("death_vfx_lifetime")}, saved {saved}')
        if DO_SAVE:
            check(f'saved_{name}', saved)


def spawn_buildings(editor_world):
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for index, name in enumerate(SPAWN):
        x = (index % 4 - 1.5) * SPACING
        y = (index // 4 - 0.5) * SPACING
        cls = unreal.load_class(None, bp_path(name) + '_C')
        lifetime = unreal.get_default_object(cls).get_editor_property('death_vfx_lifetime')
        check(f'cdo_lifetime_{name}', abs(lifetime - LIFETIME) < 0.01, lifetime)
        hit = unreal.SystemLibrary.line_trace_single(editor_world, unreal.Vector(x, y, 20000),
                                                     unreal.Vector(x, y, -20000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        z = hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0
        actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(x, y, z))
        actor.set_actor_label('DF_' + name)
        S['spots'][name] = (x, y)
        S['fire'][name] = {'first': None, 'last': None, 'max': 0}
        log(f'spawned {name} at ({x:.0f}, {y:.0f}, {z:.0f})')


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def nearest_building(location):
    best, best_dist = None, 1e12
    for name, (x, y) in S['spots'].items():
        dist = (location.x - x) ** 2 + (location.y - y) ** 2
        if dist < best_dist:
            best, best_dist = name, dist
    return best if best_dist < (SPACING * 0.5) ** 2 else None


def sample_fires(world, since_kill):
    counts = {}
    for comp in unreal.ObjectIterator(unreal.NiagaraComponent):
        if not unreal.SystemLibrary.is_valid(comp) or comp.get_world() != world:
            continue
        asset = comp.get_asset()
        # Alive, active or not: the check is that the component is removed, not only that it stopped spawning.
        if asset is None or not asset.get_name().startswith(FIRE_NAME):
            continue
        name = nearest_building(comp.get_world_location())
        if name:
            counts[name] = counts.get(name, 0) + 1
    for name, count in counts.items():
        record = S['fire'][name]
        record['first'] = since_kill if record['first'] is None else record['first']
        record['last'] = since_kill
        record['max'] = max(record['max'], count)
    return counts


def dump_niagara(world):
    """Diagnostics: every Niagara component alive anywhere, and which test buildings still exist."""
    for comp in unreal.ObjectIterator(unreal.NiagaraComponent):
        if not unreal.SystemLibrary.is_valid(comp):
            continue
        asset = comp.get_asset()
        comp_world = comp.get_world()
        log(f'niagara {asset.get_name() if asset else None} owner {comp.get_owner().get_name() if comp.get_owner() else None}'
            f' world {comp_world.get_name() if comp_world else None} same {comp_world == world}'
            f' active {comp.is_active()} at {comp.get_world_location()}')
    alive = [a.get_actor_label() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
             if a.get_actor_label().startswith('DF_')]
    log(f'test buildings still alive: {alive}')


def kill_all(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_actor_label().startswith('DF_'):
            actor.call_method('TriggerDestroyActor', args=(unreal.RTSDeathType.KINETIC,))
    log('killed all test buildings')


def tick(delta):
    # Saving opens a slow-task dialog that ticks Slate, which calls this callback again mid-save (crashed the editor).
    if S.get('busy'):
        return
    S['t'] += delta
    elapsed = S['t'] - S['mark']
    try:
        world = game_world()
        if S['phase'] == 'wait_load' and S['t'] > 12.0:
            if DO_SET:
                S['busy'] = True
                try:
                    set_lifetimes()
                finally:
                    S['busy'] = False
            spawn_buildings(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world())
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
            return
        if S['phase'] == 'wait_pie':
            if world is not None and elapsed > 6.0:
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                try:
                    pc.call_method('ClickedStartGameButton')
                except Exception:  # noqa: BLE001  (unit-test map may have no start screen)
                    pass
                unreal.GameplayStatics.set_game_paused(world, False)
                S['game0'] = unreal.GameplayStatics.get_time_seconds(world)
                S['phase'], S['mark'] = 'run', S['t']
                log('PIE up')
            elif elapsed > 120.0:
                check('pie_started', False)
                finish()
            return
        if S['phase'] != 'run' or world is None:
            return
        game = unreal.GameplayStatics.get_time_seconds(world) - S['game0']
        if S['t'] - S['mark'] > 400.0:
            check('game_time_advanced', False, game)
            finish()
            return
        if game > KILL_AT and 'kill' not in S:
            S['kill'] = game
            kill_all(world)
        if 'kill' not in S:
            return
        since_kill = game - S['kill']
        if os.environ.get('DEATHFIRE_DUMP') == '1' and since_kill > 2.0 and 'dumped' not in S:
            S['dumped'] = True
            dump_niagara(world)
        counts = sample_fires(world, since_kill)
        if int(since_kill) != S.get('last_logged'):
            S['last_logged'] = int(since_kill)
            log(f'+{since_kill:5.1f}s live fires {counts}')
        if since_kill > END_AFTER_KILL:
            limit = LIFETIME + FADE + MARGIN
            for name, record in S['fire'].items():
                check(f'had_fire_{name}', record['max'] > 0, record)
                gone = record['last'] is not None and record['last'] - (record['first'] or 0.0) <= limit
                check(f'fire_gone_{name}', gone and counts.get(name, 0) == 0, record)
            finish()
    except Exception as error:  # report and stop instead of spinning
        check('exception', False, repr(error))
        finish()


unreal.register_slate_post_tick_callback(tick)
