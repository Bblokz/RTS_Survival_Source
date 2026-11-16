"""Throwaway-editor PIE test: UMegaphoneBroadcastComponent on the Rus megaphone BXPs.

    UnrealEditor.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>
    env MEGA_TEST_OUT = result json path

Editor world, before PIE (Python cannot spawn into PIE), three megaphones with short pauses:
  MEGA_BROADCAST  Rus_Bxp_Megaphone_Broadcast (Directorate lines by default)  -> killed with TriggerDestroyActor at ~45 s
  MEGA_SIREN      Rus_Bxp_Megaphone_Broadcast                                   -> PlaySiren at ~20 s
  MEGA_SILENT     Rus_Bxp_Megaphone (no line sets, the depot alarm setup)
Times are game seconds since PIE start. Checks: lines play, never the same line twice in a row, several distinct lines, the silent one stays silent, PlaySiren
and death stop the loop, and nothing more plays afterwards. The verbose "MEGAPHONE ..." log lines are the second
witness (grep the abslog).
"""
import json
import os

import unreal

OUT = os.environ.get('MEGA_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/MegaphoneTest/result.json')
RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/'
BROADCAST_BP = RUS + 'Rus_Bxp_Megaphone_Broadcast.Rus_Bxp_Megaphone_Broadcast_C'
PLAIN_BP = RUS + 'Rus_Bxp_Megaphone.Rus_Bxp_Megaphone_C'
SPOTS = {'MEGA_BROADCAST': (BROADCAST_BP, (0.0, 0.0)), 'MEGA_SIREN': (BROADCAST_BP, (-2500.0, 0.0)),
         'MEGA_SILENT': (PLAIN_BP, (2500.0, 0.0))}
SIREN_AT = 20.0
KILL_AT = 45.0
END_AT = 60.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'played': {}, 'last_audio': {},
     'after_stop': {}}


def log(message):
    S['log'].append(f"{S['t']:7.1f} {message}")
    unreal.log_warning('MEGATEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(1 for c in S['checks'].values() if c['ok'])
    json.dump({'passed': passed, 'total': len(S['checks']), 'checks': S['checks'], 'played': S['played'],
               'log': S['log']}, open(OUT, 'w'), indent=1)
    log(f'RESULT {passed}/{len(S["checks"])}')
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def by_label(world, label):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_actor_label() == label:
            return actor
    return None


def broadcast_comp(actor):
    comps = actor.get_components_by_class(unreal.MegaphoneBroadcastComponent) if actor else []
    return comps[0] if comps else None


def spawn_megaphones(editor_world):
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for label, (cls_path, (x, y)) in SPOTS.items():
        cls = unreal.load_class(None, cls_path)
        hit = unreal.SystemLibrary.line_trace_single(editor_world, unreal.Vector(x, y, 20000),
                                                     unreal.Vector(x, y, -20000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        z = hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0
        actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(x, y, z))
        actor.set_actor_label(label)
        comp = broadcast_comp(actor)
        sets = [s.get_name() for s in comp.line_sets] if comp else None
        log(f'spawned {label} {actor.get_class().get_name()} at z {z:.0f}, line sets {sets}')
        S[label + '_sets'] = sets


def sample(world):
    """Record each new line a megaphone starts: the newest audio component attached to its broadcast component."""
    for label in SPOTS:
        actor = by_label(world, label)
        comp = broadcast_comp(actor) if actor and unreal.SystemLibrary.is_valid(actor) else None
        if comp is None:
            continue
        audios = [c for c in comp.get_children_components(False) if isinstance(c, unreal.AudioComponent)]
        playing = [a for a in audios if a.is_playing() and a.sound is not None]
        for audio in playing:
            key = audio.get_name()
            if key in S['last_audio'].setdefault(label, []):
                continue
            S['last_audio'][label].append(key)
            S['played'].setdefault(label, []).append(audio.sound.get_name())
            if label in S['after_stop']:
                S['after_stop'][label].append(audio.sound.get_name())
            log(f'{label} plays {audio.sound.get_name()}')


def tick(delta):
    S['t'] += delta
    elapsed = S['t'] - S['mark']
    try:
        world = game_world()
        if S['phase'] == 'wait_load' and S['t'] > 12.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            spawn_megaphones(editor_world)
            check('broadcast_bp_has_directorate_set', S.get('MEGA_BROADCAST_sets') == ['DA_MegaphoneLines_Directorate'],
                  S.get('MEGA_BROADCAST_sets'))
            check('plain_bp_has_no_sets', S.get('MEGA_SILENT_sets') == [], S.get('MEGA_SILENT_sets'))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
            return
        if S['phase'] == 'wait_pie':
            if world is not None and elapsed > 6.0:
                unreal.SystemLibrary.execute_console_command(world, 'log LogMegaphoneBroadcast Verbose')
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                try:
                    pc.call_method('ClickedStartGameButton')
                except Exception:  # noqa: BLE001  (unit-test map may have no start screen)
                    pass
                # Short pauses for the test, set on the PIE copies (editor-world instance edits do not survive the
                # PIE duplication of SCS components), then restart so the first line comes quickly.
                for label in SPOTS:
                    comp = broadcast_comp(by_label(world, label))
                    comp.set_editor_property('first_line_delay_range', unreal.Vector2D(0.2, 0.5))
                    comp.set_editor_property('pause_between_lines_range', unreal.Vector2D(0.3, 0.6))
                    comp.call_method('StartBroadcast')
                log('PIE up, short pauses set')
                S['phase'], S['mark'] = 'run', S['t']
            elif elapsed > 120.0:
                check('pie_started', False)
                finish()
            return
        if S['phase'] != 'run' or world is None:
            return
        # Game time: an unattended editor runs at a few fps and clamps the frame delta, so game time lags real time.
        if 'game0' not in S:
            S['game0'] = unreal.GameplayStatics.get_time_seconds(world)
        elapsed = unreal.GameplayStatics.get_time_seconds(world) - S['game0']
        if S['t'] - S['mark'] > 400.0:
            check('game_time_advanced', False, elapsed)
            finish()
            return
        sample(world)
        if elapsed > SIREN_AT and 'siren' not in S:
            S['siren'] = True
            siren = by_label(world, 'MEGA_SIREN')
            siren.call_method('PlaySiren')
            comp = broadcast_comp(siren)
            check('siren_stops_broadcast', not comp.call_method('GetIsBroadcasting'))
            S['after_stop']['MEGA_SIREN'] = []
        if elapsed > KILL_AT and 'kill' not in S:
            S['kill'] = True
            target = by_label(world, 'MEGA_BROADCAST')
            comp = broadcast_comp(target)
            before = comp.call_method('GetIsBroadcasting')
            # TriggerDestroyActor runs UnitDies, the path every real kill takes (TakeFatalDamage can report
            # "No valid hp component" on a test-spawned BXP and then never kills it).
            target.call_method('TriggerDestroyActor', args=(unreal.RTSDeathType.KINETIC,))
            still = (unreal.SystemLibrary.is_valid(comp) and comp.call_method('GetIsBroadcasting'))
            check('broadcasting_before_death', before)
            check('death_stops_broadcast', not still)
            S['after_stop']['MEGA_BROADCAST'] = []
        if elapsed > END_AT:
            played = S['played'].get('MEGA_BROADCAST', [])
            check('broadcast_played_lines', len(played) >= 3, played)
            check('no_back_to_back_repeat', all(a != b for a, b in zip(played, played[1:])), played)
            check('several_distinct_lines', len(set(played)) >= min(3, len(played)), sorted(set(played)))
            check('siren_megaphone_played_before_siren', len(S['played'].get('MEGA_SIREN', [])) >= 1,
                  S['played'].get('MEGA_SIREN'))
            check('silent_megaphone_stays_silent', not S['played'].get('MEGA_SILENT'), S['played'].get('MEGA_SILENT'))
            check('nothing_after_siren', not S['after_stop'].get('MEGA_SIREN'), S['after_stop'].get('MEGA_SIREN'))
            check('nothing_after_death', not S['after_stop'].get('MEGA_BROADCAST'),
                  S['after_stop'].get('MEGA_BROADCAST'))
            finish()
    except Exception as error:  # report and stop instead of spinning
        check('exception', False, repr(error))
        finish()


unreal.register_slate_post_tick_callback(tick)
