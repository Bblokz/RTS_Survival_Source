"""Throwaway-editor PIE test of the counterattack's presentation (2026-11-22):
Dig In field manual -> dig-in markers on both ridges -> camera tour (+ announcer line) -> prepare countdown with the
dig-in positions (held markers, objective counter, reward) -> first wave -> skippable arrival cinematic (train shot,
then the landing field drop shot, slowed game).

    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

env PRESENT_MODE:
  cut       (default) the manager's Missions are cut to the counterattack without trigger; vehicles only; four Pz IV
            F1 dig in on the four positions late in the prepare phase (held markers, counter, reward).
  chain     Missions = [BaseBuilding, counterattack with its real "BaseBuilding completed" trigger]; the test completes
            BaseBuilding (OnMissionComplete) and the counterattack starts through the trigger; the unit pool keeps its
            squads (mixed landing-field batch); positions and reward as in cut.
  skipprep  as cut, but SkipPreparation is called 2 s into the tour: the waves start while the tour still shows and
            the arrival cinematic must wait for it; the cinematic is then skipped by holding the prompt.
env PRESENT_SKIP=1 holds the skip prompt 1 s into the drop shot (implied by skipprep).
Nothing is saved (the sequence asset is created once if missing).
"""
import json
import os
import shutil
import time

import unreal

MODE = os.environ.get('PRESENT_MODE', 'cut')
SKIP = os.environ.get('PRESENT_SKIP', '0') == '1' or MODE == 'skipprep'
TAG = MODE + ('_skip' if SKIP and MODE != 'skipprep' else '')
BASE = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackPresentationTest/'
OUT = os.environ.get('PRESENT_TEST_OUT', BASE + f'result_{TAG}.json')
SHOT_DIR = BASE + TAG
SCREENSHOTS = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + 'Screenshots/WindowsEditor/'
DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/'
BP = 'Mission_Intro_SovietCounterattack'
BASE_BUILDING = 'Mission_Intro_BaseBuilding_C'
SEQ = 'LS_Intro_FirstWaveArrival'
TANK = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1'
ENEMY_SIDE = (8400.0, -1500.0)
# 2026-11-23: the centre wave starts at the north map edge, not on route point 1 (8400, -1500).
CENTRE_SPAWN, OLD_CENTRE_SPAWN = (15500.0, 23900.0), (8400.0, -1500.0)
# Prepare: long enough for the old fog lift (tour + 15 s) to have expired before the marker check, and for four tanks
# to dig in (about 12 s) before the waves.
PREP = 45 if MODE == 'skipprep' else 70
UNITS = 6
MARKER_CHECK_AT, TANK_SPAWN_AT = 22.0, 30.0
LINE_NAME = 'IntroMission_DigInPositions'
FIRST_HELD, ALL_HELD, ARRIVAL_CUE = 'IntroMission_DigInFirstHeld', 'IntroMission_DigInAllHeld', 'IntroMission_ArrivalCue'
WATCHED_SOUNDS = (LINE_NAME, FIRST_HELD, ALL_HELD, ARRIVAL_CUE)
S = {'wall0': time.time(), 'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'pending': [],
     'shots': set(), 'arrival': []}


def wall():
    return time.time() - S['wall0']


def log(message):
    S['log'].append(f"{wall():7.1f} {message}")
    unreal.log_warning('PRESENTTEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(1 for c in S['checks'].values() if c['ok'])
    json.dump({'mode': TAG, 'passed': passed, 'total': len(S['checks']), 'checks': S['checks'], 'log': S['log'],
               'arrival': S['arrival'][-200:]}, open(OUT, 'w'), indent=1)
    log(f'RESULT {passed}/{len(S["checks"])}')
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def v(x):
    return (round(x.x), round(x.y), round(x.z))


def d2(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5


def mission_of(world, class_name):
    for obj in unreal.ObjectIterator(unreal.MissionBase):
        if obj.get_class().get_name() == class_name and obj.get_world() == world:
            return obj
    return None


def timer_text(world):
    for widget in unreal.ObjectIterator(unreal.W_MissionTimer):
        if widget.get_world() == world and widget.is_in_viewport():
            label = widget.get_editor_property('m_timer_text')
            clock = widget.get_editor_property('m_clock_text')
            return (str(label.get_text()) if label else '') + ' | ' + (str(clock.get_text()) if clock else '')
    return None


def widgets(world, name_part):
    return [w for w in unreal.ObjectIterator(unreal.UserWidget)
            if name_part in w.get_class().get_name() and w.get_world() == world and w.is_in_viewport()]


def other_takeovers(world):
    return [w for w in unreal.ObjectIterator(unreal.W_InfoTakeover)
            if w.get_world() == world and w.is_in_viewport() and 'DigIn' not in w.get_class().get_name()]


def shot(name):
    """`shot showui` (UI included); the file is renamed into SHOT_DIR once it appears."""
    if name in S['shots']:
        return
    S['shots'].add(name)
    os.makedirs(SHOT_DIR, exist_ok=True)
    before = set(os.listdir(SCREENSHOTS)) if os.path.isdir(SCREENSHOTS) else set()
    unreal.SystemLibrary.execute_console_command(game_world(), 'shot showui')
    S['pending'].append({'name': name, 'before': before, 'since': wall()})
    log('shot ' + name)


def collect_shots():
    if not S['pending'] or not os.path.isdir(SCREENSHOTS):
        return
    now = set(os.listdir(SCREENSHOTS))
    for item in list(S['pending']):
        new = sorted(now - item['before'])
        if new:
            src = SCREENSHOTS + new[0]
            if os.path.getsize(src) > 0 and wall() - item['since'] > 1.0:
                shutil.copyfile(src, f"{SHOT_DIR}/{item['name']}.png")
                S['pending'].remove(item)
                for other in S['pending']:
                    other['before'].add(new[0])
        elif wall() - item['since'] > 20:
            log('shot never appeared: ' + item['name'])
            S['pending'].remove(item)


def ensure_sequence():
    path = DIR + SEQ
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    seq = tools.create_asset(SEQ, DIR.rstrip('/'), unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(30, 1))
    seq.set_playback_start_seconds(0.0)
    seq.set_playback_end_seconds(20.0)
    log(f'sequence created + saved {unreal.EditorAssetLibrary.save_loaded_asset(seq, False)}')
    return seq


def shorten(da, keep_squads):
    for field in ('new_to_rts', 'normal', 'hard', 'brutal', 'ironman'):
        block = da.get_editor_property(field)
        block.set_editor_property('seconds_until_soviets_arrive', PREP)
        block.set_editor_property('wave_count', 1)
        block.set_editor_property('seconds_between_waves', 120)
        block.set_editor_property('min_units_first_wave', UNITS)
        block.set_editor_property('max_units_first_wave', UNITS)
        block.set_editor_property('units_added_per_wave', 0)
        if not keep_squads:
            block.set_editor_property('squad_types', [])
        da.set_editor_property(field, block)


def line_playing(world):
    for comp in unreal.ObjectIterator(unreal.AudioComponent):
        if comp.get_world() != world or not comp.is_playing():
            continue
        sound = comp.get_editor_property('sound')
        if sound is not None and sound.get_name() == LINE_NAME:
            return True
    return False


def note_sounds(world):
    """First wall time each watched voice line is heard playing."""
    heard = S.setdefault('heard', {})
    for comp in unreal.ObjectIterator(unreal.AudioComponent):
        if comp.get_world() != world or not comp.is_playing():
            continue
        sound = comp.get_editor_property('sound')
        name = sound.get_name() if sound is not None else ''
        if name in WATCHED_SOUNDS and name not in heard:
            heard[name] = wall()
            log(f'heard {name}')


def view_target_name(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    target = pc.get_view_target() if pc else None
    return target.get_class().get_name() if target else None


def camera_on(m, loc, test_id):
    """A one-shot test camera tour on loc (south ridge view of the tour's first shot), no bars."""
    m.call_method('PlayCameraTour', ([unreal.MissionCameraTourShot(
        focus_start=loc, focus_end=loc, yaw_start=20.0, yaw_end=20.0, pitch_start=18.0, pitch_end=18.0,
        arm_length_start=3000.0, arm_length_end=3000.0, seconds=6.0, blend_in_seconds=0.0)],
        unreal.MissionCameraTourSettings(show_letterbox=False, return_seconds=0.0), test_id))


def ghost_visible(marker):
    comps = marker.get_components_by_class(unreal.StaticMeshComponent)
    return any(c.is_visible() for c in comps)


# ------------------------------------------------------------------ phases

def phase_wait_load():
    editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    nav = unreal.NavigationSystemV1.get_navigation_system(editor_world)
    if nav is not None and nav.is_navigation_being_built(editor_world):
        return
    check('sequence_asset', ensure_sequence() is not None, DIR + SEQ)
    S['tank_class'] = unreal.EditorAssetLibrary.load_blueprint_class(TANK)
    manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(editor_world, unreal.Actor)
               if a.get_class().get_name() == 'BP_MissionManager_C'][0]
    tops = [t for t in manager.get_editor_property('missions') if t is not None]
    found = next((t for t in tops if t.get_class().get_name() == BP + '_C'), None)
    base_building = next((t for t in tops if t.get_class().get_name() == BASE_BUILDING), None)
    if found is None:
        found = unreal.new_object(unreal.load_asset(DIR + BP).generated_class(), outer=manager)
    da = unreal.load_asset(DIR + 'DA_Intro_SovietCounterattack')
    S['da'] = da
    found.set_editor_property('settings', da)
    shorten(da, keep_squads=(MODE == 'chain'))
    intro = da.get_editor_property('dig_in_intro')
    cine = da.get_editor_property('arrival_cinematic')
    check('data_asset_refs', intro.get_editor_property('marker_class') is not None
          and cine.get_editor_property('sequence') is not None
          and intro.get_editor_property('voice_line') is not None,
          f"marker {intro.get_editor_property('marker_class')} sequence {cine.get_editor_property('sequence')}")
    if MODE == 'chain':
        trigger = found.get_editor_property('mission_trigger')
        watched = trigger.get_editor_property('mission_class_to_watch') if trigger else None
        check('chain_trigger_watches_base_building', base_building is not None and watched is not None
              and watched.get_name() == BASE_BUILDING, f'{watched}')
        manager.set_editor_property('missions', [base_building, found])
    else:
        found.set_editor_property('mission_trigger', None)
        manager.set_editor_property('missions', [found])
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    S['phase'], S['mark'] = 'wait_pie', S['t']


def phase_wait_pie(world, elapsed):
    if world is not None and elapsed > 8.0:
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if pc is not None:
            pc.call_method('ClickedStartGameButton')
            log('PIE up, start clicked')
            S['phase'], S['mark'] = ('chain_bb' if MODE == 'chain' else 'wait_takeover'), S['t']
    elif elapsed > 120.0:
        check('pie_started', False)
        finish()


def phase_chain_bb(world, elapsed):
    for panel in other_takeovers(world):
        panel.call_method('HandleUnderstoodClicked')
    bb = mission_of(world, BASE_BUILDING)
    if bb is None:
        if elapsed > 90.0:
            check('base_building_ran', False, 'no BaseBuilding mission in PIE')
            finish()
        return
    S.setdefault('bb_seen', S['t'])
    if S['t'] - S['bb_seen'] > 12.0 and 'bb_done' not in S:
        S['bb_done'] = wall()
        bb.call_method('OnMissionComplete')
        log('BaseBuilding completed by the test')
        S['phase'], S['mark'] = 'wait_takeover', S['t']


def phase_wait_takeover(world, m, elapsed):
    for panel in other_takeovers(world):
        panel.call_method('HandleUnderstoodClicked')
        log('closed another takeover: ' + panel.get_class().get_name())
    panels = widgets(world, 'DigIn')
    if MODE == 'chain' and m is not None and 'ca_seen' not in S:
        S['ca_seen'] = wall()
        check('started_by_real_trigger', 'bb_done' in S and S['ca_seen'] - S['bb_done'] < 60.0,
              f"{S['ca_seen'] - S.get('bb_done', 0):.1f} s after BaseBuilding completed")
    if m is not None and panels:
        if 'takeover_seen' not in S:
            S['takeover_seen'] = S['t']
            check('digin_takeover_opens_first', m.call_method('GetDigInIntroStage') == 1,
                  f"stage {m.call_method('GetDigInIntroStage')}, class {panels[0].get_class().get_name()}")
            check('no_countdown_during_takeover', timer_text(world) is None, timer_text(world))
            markers = m.call_method('GetDigInMarkers') or []
            check('four_markers', len(markers) == 4, [v(a.get_actor_location()) for a in markers])
            check('markers_stay', all(a.get_life_span() == 0.0 for a in markers),
                  [a.get_life_span() for a in markers])
        if S['t'] - S['takeover_seen'] > 4.0:
            shot('1_digin_takeover')
        if S['t'] - S['takeover_seen'] > 7.0:
            panels[0].call_method('HandleUnderstoodClicked')
    elif m is not None and 'takeover_seen' in S and not panels:
        log('takeover closed')
        S['phase'], S['mark'] = 'tour', S['t']
    elif elapsed > 150.0:
        check('digin_takeover_opens_first', False, f'no Dig In takeover; mission {m}')
        finish()


def phase_tour(world, m, elapsed):
    index = m.call_method('GetCameraTourShotIndex')
    stage = m.call_method('GetDigInIntroStage')
    if 'tour_seen' not in S and m.call_method('GetIsCameraTourPlaying'):
        S['tour_seen'] = S['t']
        check('tour_after_takeover', stage == 2, f'stage {stage}')
    if index is not None and index >= 0:
        key = f'shot_{index}'
        S.setdefault(key, S['t'])
        S.setdefault('first_shot_wall', wall())
        if S['t'] - S[key] > 3.0:
            shot(f'2_tour_shot{index}')
        if index == 1 and 'tour_no_timer' not in S:
            S['tour_no_timer'] = True
            check('no_countdown_during_tour', timer_text(world) is None, timer_text(world))
    if 'line_wall' not in S and line_playing(world):
        S['line_wall'] = wall()
        S['line_shot'] = index
        log(f'dig-in line started, shot {index}')
    if MODE == 'skipprep' and 'shot_0' in S and S['t'] - S['shot_0'] > 2.0 and 'skip_called' not in S:
        S['skip_called'] = wall()
        m.call_method('SkipPreparation')
        log('SkipPreparation called during the tour')
        check('skipprep_waves_start_during_tour', m.call_method('GetPhase') == 1
              and m.call_method('GetIsCameraTourPlaying'),
              f"phase {m.call_method('GetPhase')}, tour {m.call_method('GetIsCameraTourPlaying')}")
        S['phase'], S['mark'], S['waves_start'] = 'arrival', S['t'], wall()
        return
    if stage == 3:
        text = timer_text(world)
        if text:
            check('countdown_after_tour', text.startswith('Prepare to defend') and 'tour_seen' in S, text)
            delay = S['line_wall'] - S['first_shot_wall'] if 'line_wall' in S and 'first_shot_wall' in S else None
            check('voice_line_over_tour', delay is not None and 0.3 < delay < 3.0 and S.get('line_shot', -1) >= 0,
                  f'started {delay if delay is None else round(delay, 2)} s after the first shot '
                  f'(shot {S.get("line_shot")})')
            S['phase'], S['mark'], S['prep_start'] = 'prep', S['t'], wall()
    elif elapsed > 90.0:
        check('countdown_after_tour', False, f'stage {stage} 90 s after the takeover closed')
        finish()


def spawn_tanks(world, m):
    tank_class = S['tank_class']        # loaded before PIE: EditorAssetLibrary refuses during play
    tanks = []
    for marker in m.call_method('GetDigInMarkers') or []:
        loc = marker.get_actor_location()
        yaw = unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(ENEMY_SIDE[0], ENEMY_SIDE[1], loc.z)).yaw
        tank = unreal.MissionMapTestLibrary.map_test_spawn_actor(world, tank_class, loc + unreal.Vector(0, 0, 120),
                                                                 unreal.Rotator(0.0, 0.0, yaw))
        tanks.append(tank)
    S['tanks'] = tanks
    log(f'spawned {sum(1 for t in tanks if t)} Pz IV F1 on the positions')


def phase_prep(world, m):
    pstart = wall() - S['prep_start']
    markers = m.call_method('GetDigInMarkers') or []
    if pstart > MARKER_CHECK_AT and 'marker_cam' not in S and markers:
        S['marker_cam'] = wall()
        camera_on(m, markers[0].get_actor_location(), 99)
    if 'marker_cam' in S and wall() - S['marker_cam'] > 3.0 and 'marker_rendered' not in S:
        S['marker_rendered'] = True
        rendered = [a.was_recently_rendered(0.5) for a in markers]
        check('markers_seen_in_prepare', len(markers) == 4 and sum(rendered) >= 2,
              f'{pstart:.0f} s into prepare, after the old fog lift; rendered {rendered}')
        shot('3_prepare_markers')
    if pstart > TANK_SPAWN_AT and 'tanks' not in S:
        spawn_tanks(world, m)
        S['tanks_wall'] = wall()
    if 'tanks' in S and wall() - S['tanks_wall'] > 3.0 and 'dig_ordered' not in S:
        S['dig_ordered'] = True
        results = [str(t.call_method('DigIn', (True,))) if t else 'none' for t in S['tanks']]
        log(f'DigIn ordered: {results}')
    held = m.call_method('GetDigInPositionsHeld')
    if 'held_logged' in S and S['held_logged'] == 4 and held < 4 and 'counter_back_checked' not in S:
        S['counter_back_wait'] = wall()
    if 'counter_back_wait' in S and wall() - S['counter_back_wait'] > 1.0 and 'counter_back_checked' not in S:
        S['counter_back_checked'] = True
        description = str(m.get_editor_property('mission_widget_state').get_editor_property('mission_description'))
        now = m.call_method('GetDigInPositionsHeld')
        check('counter_back_when_position_lost', now == 4 or f'{now}/4' in description, description[-80:])
    if held != S.get('held_logged'):
        S['held_logged'] = held
        alive = sum(1 for t in S.get('tanks', []) if t is not None and unreal.SystemLibrary.is_valid(t))
        log(f'held {held}/4 at {pstart:.1f} s into prepare; {alive} test tanks alive')
    if held >= 1 and 'first_held' not in S:
        S['first_held'] = wall()
        log(f'first position held {S["first_held"] - S.get("tanks_wall", 0):.1f} s after the spawn')
    if held == 4 and 'all_held' not in S:
        S['all_held'] = wall()
        check('positions_all_held', True, f'{S["all_held"] - S["tanks_wall"]:.1f} s after the spawn')
        description = str(m.get_editor_property('mission_widget_state').get_editor_property('mission_description'))
        # 2026-11-23 user: once every position is held the counter line goes.
        check('counter_gone_when_all_held', 'Vehicles dug in' not in description, description[-120:])
        check('held_markers_drop_ghost', markers and not any(ghost_visible(a) for a in markers),
              [ghost_visible(a) for a in markers])
        camera_on(m, markers[0].get_actor_location(), 98)
        S['held_cam'] = wall()
    if 'held_cam' in S and wall() - S['held_cam'] > 3.0:
        shot('3b_positions_held')
    if m.call_method('GetPhase') == 1:
        if 'all_held' not in S:
            check('positions_all_held', False, f'{held}/4 when the waves started')
        # Paid only when all four are still held as the waves start; in cut mode the ridge base is still alive and a
        # tank that loses its wall in that fight no longer holds its position.
        paid = m.call_method('GetAllPositionsRewardPaid')
        S['reward_paid'] = paid
        last = S.get('held_logged')
        check('reward_matches_positions', paid == (last == 4), f'paid {paid}, held {last}/4 at the wave start')
        check('markers_removed_when_waves_start', len(markers) == 0, len(markers))
        S['phase'], S['mark'], S['waves_start'] = 'arrival', S['t'], wall()
    elif pstart > PREP + 40:
        check('waves_started', False)
        finish()


def check_centre_spawn(m, since):
    """Centre units appear at the north edge (not on route point 1) and set off towards the route."""
    locations = [(p.x, p.y) for p in (m.call_method('GetLivingSovietUnitLocations') or [])]
    if since > 8.0 and 'centre_first' not in S and locations:
        near_new = [l for l in locations if d2(l, CENTRE_SPAWN) < 4000.0]
        near_old = [l for l in locations if d2(l, OLD_CENTRE_SPAWN) < 3000.0]
        S['centre_first'] = near_new
        check('centre_spawns_at_north_edge', len(near_new) >= 1 and not near_old,
              f'{len(near_new)} near {CENTRE_SPAWN}, {len(near_old)} near the old spawn; '
              + str([tuple(round(c) for c in l) for l in near_new]))
    if since > 70.0 and 'centre_moved_checked' not in S and S.get('centre_first'):
        S['centre_moved_checked'] = True
        still = [l for l in locations if d2(l, CENTRE_SPAWN) < 3000.0]
        check('centre_wave_drives_in', len(still) < len(S['centre_first']),
              f'{len(still)} of {len(S["centre_first"])} still within 30 m of the spawn after 70 s')


def phase_arrival(world, m):
    stage = m.call_method('GetArrivalCinematicStage')
    trains = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovietCargoTrain)
    planes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AircraftMaster)
    since = wall() - S['waves_start']
    check_centre_spawn(m, since)
    touring = m.call_method('GetIsCameraTourPlaying')
    rec = {'t': round(since, 1), 'stage': stage, 'trains': len(trains), 'planes': len(planes),
           'view': view_target_name(world), 'tour': touring,
           'dilation': round(unreal.GameplayStatics.get_global_time_dilation(world), 2)}
    if not S['arrival'] or S['arrival'][-1]['stage'] != stage or since - S['arrival'][-1]['t'] >= 1.0:
        S['arrival'].append(rec)
        log(f'arrival {rec}')
    if touring and stage in (2, 3):
        S['overlap'] = True
    if stage == 1 and 'waiting_checked' not in S and since > 3.0 and MODE != 'skipprep':
        S['waiting_checked'] = True
        check('train_held_while_waiting', not trains, f'{len(trains)} trains 3 s into the wave')
    if stage == 2:
        S.setdefault('train_shot_t', since)
        if since - S['train_shot_t'] > 0.6 and 'train_view' not in S:
            shot('4_cinematic_train')
            S['train_view'] = view_target_name(world)
            check('train_shot_camera', S['train_view'] == 'CameraActor' and trains,
                  f"view {S['train_view']}, trains {len(trains)}")
            check('skip_prompt_shown', len(widgets(world, 'HoldToSkip')) == 1,
                  [w.get_class().get_name() for w in widgets(world, 'HoldToSkip')])
            timers = [w for w in widgets(world, 'MissionTimer') if w.is_visible()]
            check('hud_hidden', not timers, f'{len(timers)} visible timer widgets')
    if stage == 3:
        drop_shot(world, m, since)
    if stage == 4:
        result = m.call_method('GetArrivalCinematicResult')
        expected = 2 if SKIP else 1     # EMissionCinematicTakeOverResult: 1 FinishedNaturally, 2 Skipped
        check('cinematic_result', result == expected, f'{result} (expected {expected})')
        check('cinematic_played', 'drop_shot_t' in S, S.get('drop_shot_t'))
        if MODE == 'skipprep':
            check('cinematic_waits_for_tour', not S.get('overlap'), 'tour and cinematic overlapped' if S.get('overlap') else '')
        if SKIP:
            check('skip_quick', 'skip_started' in S and since - S['skip_started'] < 4.0,
                  f"{since - S.get('skip_started', 0):.1f} s after the hold started")
        S['phase'], S['mark'], S['after'] = 'after', S['t'], wall()
    elif since > 180.0:
        check('cinematic_result', False, f'stage {stage} after 180 s')
        finish()


def drop_shot(world, m, since):
    S.setdefault('drop_shot_t', since)
    S.setdefault('drop_wall', wall())
    dt = since - S['drop_shot_t']
    if dt > 2.5 and 'overlay_checked' not in S:
        S['overlay_checked'] = True
        cues = [w.call_method('GetSubtitleCueCount') for w in unreal.ObjectIterator(unreal.W_CinematicOverlay)
                if w.get_world() == world and w.is_in_viewport()]
        check('arrival_cue_subtitled', any(c >= 1 for c in cues), f'overlay subtitle cues {cues}')
    if 'train_shot_t' in S and 'train_len' not in S:
        S['train_len'] = S['drop_shot_t'] - S['train_shot_t']
        check('train_shot_about_2s', 1.0 <= S['train_len'] <= 4.5, f"{S['train_len']:.1f} s (wall, slowed game)")
    if dt > 0.5 and 'drop_checked' not in S:
        S['drop_checked'] = True
        dilation = unreal.GameplayStatics.get_global_time_dilation(world)
        check('game_slowed_during_cinematic', abs(dilation - 0.75) < 0.01, round(dilation, 3))
        pad_index = m.call_method('GetArrivalPadIndex')
        pads = S['da'].get_editor_property('spawns').get_editor_property('landing_pad_centers')
        cam = m.call_method('GetArrivalCinematicCamera')
        if cam is not None and 0 <= pad_index < len(pads):
            dist = d2(v(cam.get_actor_location()), v(pads[pad_index]))
            check('drop_camera_on_filmed_pad', abs(dist - 4200.0) < 600.0, f'pad {pad_index}, camera {dist:.0f} cm away')
    if dt > 1.5:
        shot('5_cinematic_drop_a')
    if SKIP and dt > 1.0 and 'skip_started' not in S:
        hold = widgets(world, 'HoldToSkip')
        S['skip_started'] = since
        if hold:
            hold[0].call_method('StartHold')
            log('holding the skip prompt')
        else:
            check('skip_prompt_present_for_skip', False)
    if not SKIP and dt > 6.0:
        shot('6_cinematic_drop_b')
    if not SKIP and dt > 12.0:
        shot('7_cinematic_drop_c')


def phase_after(world):
    if wall() - S['after'] > 2.0 and 'after_checked' not in S:
        S['after_checked'] = True
        check('view_back_on_pawn', view_target_name(world) != 'CameraActor', view_target_name(world))
        check('hud_back', bool(timer_text(world)), timer_text(world))
        check('skip_prompt_gone', not widgets(world, 'HoldToSkip'), '')
        dilation = unreal.GameplayStatics.get_global_time_dilation(world)
        check('game_speed_restored', abs(dilation - 1.0) < 0.01, round(dilation, 3))
        heard = S.get('heard', {})
        first = heard.get(FIRST_HELD)
        if 'first_held' in S:
            check('first_held_line', first is not None and -1.0 <= first - S['first_held'] < 6.0,
                  f'{None if first is None else round(first - S["first_held"], 1)} s after the first position was held')
        if S.get('reward_paid'):
            all_held = heard.get(ALL_HELD)
            check('all_held_line_with_reward', all_held is not None and -1.0 <= all_held - S['waves_start'] < 6.0,
                  f'{None if all_held is None else round(all_held - S["waves_start"], 1)} s after the waves started')
        cue = heard.get(ARRIVAL_CUE)
        if 'drop_wall' in S:
            check('arrival_cue_over_drop', cue is not None and -1.0 <= cue - S['drop_wall'] < 3.0,
                  f'{None if cue is None else round(cue - S["drop_wall"], 1)} s after the cut to the drop shot')
        shot('8_after_cinematic')
    since = wall() - S['waves_start']
    m = mission_of(world, BP + '_C')
    if m is not None:
        check_centre_spawn(m, since)
    if wall() - S['after'] > 6.0 and not S['pending'] and (since > 72.0 or MODE == 'skipprep'):
        finish()


def tick(delta):
    S['t'] += delta
    collect_shots()
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    world = game_world()
    if phase == 'wait_load':
        if S['t'] > 15.0:
            phase_wait_load()
        return
    if phase == 'wait_pie':
        phase_wait_pie(world, elapsed)
        return
    if world is None:
        return
    if phase == 'chain_bb':
        phase_chain_bb(world, elapsed)
        return
    m = mission_of(world, BP + '_C')
    if phase == 'wait_takeover':
        phase_wait_takeover(world, m, elapsed)
        return
    if m is None:
        return
    note_sounds(world)
    if phase == 'tour':
        phase_tour(world, m, elapsed)
    elif phase == 'prep':
        phase_prep(world, m)
    elif phase == 'arrival':
        phase_arrival(world, m)
    elif phase == 'after':
        phase_after(world)


def guarded(delta):
    # Saving an asset pumps Slate, which would re-enter this callback mid-tick.
    if S.get('busy'):
        return
    S['busy'] = True
    try:
        if S['phase'] != 'done':
            tick(delta)
    except Exception as exc:  # noqa: BLE001
        import traceback
        log('EXCEPTION ' + traceback.format_exc())
        if S['phase'] != 'done':
            check('no_exception', False, str(exc))
            finish()
    finally:
        S['busy'] = False


unreal.register_slate_post_tick_callback(guarded)
log(f'presentation test loaded, mode {MODE}, skip {SKIP}')
