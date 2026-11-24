"""Throwaway-editor PIE test: the advisor's TourVoiceLine plays over the "Secure the Line" camera tour (2026-11-16).

    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env TOUR_LINE_TEST_OUT = result json path

Imports IntroMission_SecureLineTour.wav once (sound class RTS_SClass_CinematicAndTransmissions, the new SoundWave is
saved), applies setup_secure_line_mission's defaults to the mission CDO and to the map's mission instance (neither is
saved), cuts the chain to [ClearSovietRig without trigger] like secure_line_headless_test and checks:
the line starts after the first tour shot shows (not before, the takeover would stop it), an audio component is
really playing it while the tour runs, at a non-zero volume, and it is not cut when the tour hands the screen back.
2026-11-18: the portrait shows on the tour overlay's copy of W_BP_Portrait (the HUD is hidden) with its word-timed
subtitle, a `shot showui` screenshot is taken mid-line, and portraits go back to the HUD widget after the tour.
"""
import importlib
import json
import os
import sys
import time

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import setup_secure_line_mission as setup  # noqa: E402

importlib.reload(setup)

OUT = os.environ.get('TOUR_LINE_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/TourVoiceLineTest/result.json')
CLEAR = 'Mission_Intro_ClearSovietRig_C'
LINE_NAME = 'IntroMission_SecureLineTour'
SCLASS = '/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions'
S = {'t': 0.0, 'phase': 'wait_load', 'mark': 0.0, 'log': [], 'checks': {}}


def wall():
    return time.time()


def log(message):
    S['log'].append(f'{round(S["t"], 1)} {message}')
    unreal.log_warning('[TourLineTest] ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': str(detail)}
    log(f'{"PASS" if ok else "FAIL"} {name}: {detail}')


def finish():
    S['phase'] = 'done'
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    passed = sum(c['ok'] for c in S['checks'].values())
    S['summary'] = f'{passed}/{len(S["checks"])} PASS'
    with open(OUT, 'w') as fh:
        json.dump(S, fh, indent=1, default=str)
    log(S['summary'])
    unreal.SystemLibrary.quit_editor()


def ensure_tour_line():
    """Imports the picked take once; returns the SoundWave."""
    if not unreal.EditorAssetLibrary.does_asset_exist(setup.TOUR_LINE):
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', setup.TOUR_LINE_SOURCE)
        task.set_editor_property('destination_path', setup.TOUR_LINE.rsplit('/', 1)[0])
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', False)
        task.set_editor_property('save', False)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        wave = unreal.load_asset(setup.TOUR_LINE)
        wave.set_editor_property('sound_class_object', unreal.load_asset(SCLASS))
        unreal.EditorAssetLibrary.save_loaded_asset(wave, False)
        log('imported + saved ' + setup.TOUR_LINE)
    return unreal.load_asset(setup.TOUR_LINE)


def game_world():
    worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    return worlds


def mission(world):
    for obj in unreal.ObjectIterator(unreal.SecureBaseMission):
        if obj.get_class().get_name() == CLEAR and obj.get_world() == world:
            return obj
    return None


def line_components(world):
    out = []
    for comp in unreal.ObjectIterator(unreal.AudioComponent):
        if comp.get_world() != world or not comp.is_playing():
            continue
        sound = comp.get_editor_property('sound')
        if sound is not None and sound.get_name() == LINE_NAME:
            out.append(comp)
    return out


def portrait_manager(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    return pc.call_method('GetPlayerPortraitManager') if pc is not None else None


def sample_tour_portrait(portraits):
    """Keeps what the active portrait shows while the line plays over the tour."""
    active = portraits.call_method('GetActivePortraitWidget')
    if active is None or active == S.get('hud_portrait'):
        return
    S['tour_portrait'] = active
    text = active.call_method('GetShownSubtitleText')
    if text:
        words = S.setdefault('tour_subtitle_samples', [])
        if not words or words[-1] != text:
            words.append(text)


def check_tour_portrait():
    copy = S.get('tour_portrait')
    hud = S.get('hud_portrait')
    vis = str(copy.get_visibility()) if copy is not None else None
    opacity = copy.get_render_opacity() if copy is not None else 0.0
    check('portrait_on_tour_overlay', copy is not None and hud is not None and copy != hud
          and copy.get_class() == hud.get_class(), f'active {copy}, hud {hud}')
    check('tour_portrait_visible', vis is not None and 'HIDDEN' not in vis.upper() and 'COLLAPSED' not in vis.upper()
          and opacity > 0.3, f'visibility {vis}, opacity {round(opacity, 2)}')
    samples = S.get('tour_subtitle_samples', [])
    check('tour_subtitle_words', len(samples) >= 3 and any(len(s.split()) >= 3 for s in samples),
          f'{len(samples)} distinct texts, last: {samples[-1] if samples else None!r}')


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    try:
        world = game_world() if phase not in ('wait_load', 'done') else None

        if phase == 'wait_load' and S['t'] > 15.0:
            S['phase'] = 'busy'     # the import ticks slate: keep this step from running again inside it
            wave = ensure_tour_line()
            check('line_imported', wave is not None and abs(wave.get_editor_property('duration') - 19.71) < 0.3,
                  f'{wave} {wave.get_editor_property("duration") if wave else None} s')
            sclass = wave.get_editor_property('sound_class_object') if wave else None
            check('line_sound_class', sclass is not None and sclass.get_name() == SCLASS.rsplit('/', 1)[1], sclass)
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(editor_world, unreal.Actor)
                       if a.get_class().get_name() == 'BP_MissionManager_C'][0]
            clear = None
            for top in manager.get_editor_property('missions'):
                while top is not None and clear is None:
                    if top.get_class().get_name() == CLEAR:
                        clear = top
                    top = top.get_editor_property('next_mission')
            check('map_instance_found', clear is not None, clear)
            if clear is None:
                finish()
                return
            S['instance_line_before'] = str(clear.get_editor_property('tour_voice_line'))
            # The setup script's own defaults, applied to the map instance (no Blueprint compile: that reinstances
            # the map's mission objects under this script). Nothing is saved.
            setup.apply_defaults(clear)
            check('setup_sets_tour_line', clear.get_editor_property('tour_voice_line') == wave
                  and abs(clear.get_editor_property('tour_voice_line_delay_seconds') - 0.8) < 1e-3,
                  clear.get_editor_property('tour_voice_line'))
            clear.set_editor_property('mission_trigger', None)
            clear.set_editor_property('next_mission', None)
            manager.set_editor_property('missions', [clear])
            log(f"map instance tour line before: {S['instance_line_before']}; chain cut to [ClearSovietRig]")
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
            return

        if phase == 'wait_pie':
            if world is not None and elapsed > 8.0:
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                if pc is not None:
                    pc.call_method('ClickedStartGameButton')
                    log('PIE up, start game clicked')
                    S['phase'], S['mark'] = 'wait_tour', S['t']
            elif elapsed > 120.0:
                check('pie_started', False)
                finish()
            return

        m = mission(world) if world is not None else None
        playing = line_components(world) if world is not None else []
        if playing and 'line_wall' not in S:
            S['line_wall'] = wall()
            S['line_shot'] = m.call_method('GetCameraTourShotIndex') if m else None
            S['line_volume'] = playing[0].get_editor_property('volume_multiplier')
            log(f"line audio started, shot {S['line_shot']}, volume_multiplier {S['line_volume']}")
        if 'line_wall' in S and playing:
            S['line_last_wall'] = wall()

        portraits = portrait_manager(world) if world is not None else None
        if phase == 'wait_tour' and portraits is not None and 'hud_portrait' not in S:
            S['hud_portrait'] = portraits.call_method('GetActivePortraitWidget')
        if phase == 'tour' and portraits is not None and 'line_wall' in S:
            sample_tour_portrait(portraits)

        if phase == 'wait_tour':
            if m is not None and m.call_method('GetIsCameraTourPlaying') and m.call_method('GetCameraTourShotIndex') >= 0:
                S['tour_wall'] = wall()
                check('line_not_before_first_shot', 'line_wall' not in S and not m.call_method('GetHasTourVoiceLinePlayed'),
                      f"line_wall {S.get('line_wall')}")
                try:
                    S['voicelines_volume_during_tour'] = m.get_editor_property('m_cinematic_take_over_voicelines_volume')
                except Exception as error:  # informative only
                    S['voicelines_volume_during_tour'] = repr(error)
                S['phase'], S['mark'] = 'tour', S['t']
            elif elapsed > 60.0:
                check('tour_started', False, 'no tour after 60 s')
                finish()
            return

        if phase == 'tour':
            if 'line_wall' in S and 'checked_audible' not in S and wall() - S['line_wall'] > 2.0:
                S['checked_audible'] = True
                still = bool(playing) and m.call_method('GetIsCameraTourPlaying')
                check('line_audible_during_tour', still and S['line_volume'] > 0.0,
                      f"playing {len(playing)} comps, tour playing {m.call_method('GetIsCameraTourPlaying')}, "
                      f"volume_multiplier {S['line_volume']}, takeover voicelines volume "
                      f"{S.get('voicelines_volume_during_tour')}")
                delay = S['line_wall'] - S['tour_wall']
                check('line_starts_after_first_shot', 0.5 < delay < 2.5 and S['line_shot'] is not None
                      and S['line_shot'] >= 0, f'{round(delay, 2)} s after the first shot (shot {S["line_shot"]})')
                check('played_flag', m.call_method('GetHasTourVoiceLinePlayed'))
            if 'line_wall' in S and 'checked_portrait' not in S and wall() - S['line_wall'] > 5.0:
                S['checked_portrait'] = True
                check_tour_portrait()
            # Own tick, after the timing checks: the screenshot stalls the game for seconds.
            if 'checked_portrait' in S and 'shot_wall' not in S and wall() - S['line_wall'] > 6.0:
                S['shot_wall'] = wall()
                unreal.SystemLibrary.execute_console_command(world, 'shot showui')
                log('shot showui (portrait + subtitle over the tour)')
            if m is not None and m.call_method('GetHasTourFinished'):
                S['tour_end_wall'] = wall()
                check('tour_length', 30.0 < S['tour_end_wall'] - S['tour_wall'] < 45.0,
                      f"{round(S['tour_end_wall'] - S['tour_wall'], 1)} s")
                S['phase'], S['mark'] = 'after', S['t']
            elif elapsed > 90.0:
                check('tour_finished', False, 'tour still running after 90 s')
                finish()
            return

        if phase == 'after':
            if 'checked_handback' not in S and portraits is not None:
                S['checked_handback'] = True
                active = portraits.call_method('GetActivePortraitWidget')
                copy = S.get('tour_portrait')
                copy_vis = str(copy.get_visibility()) if copy is not None else None
                check('portrait_back_on_hud', active is not None and active == S.get('hud_portrait')
                      and (copy is None or not copy.is_in_viewport() or 'HIDDEN' in copy_vis.upper()),
                      f'active {active}, hud {S.get("hud_portrait")}, tour copy {copy} visibility {copy_vis}, '
                      f'line still playing {portraits.call_method("GetIsPortraitPlaying")}')
            if elapsed > 3.0:
                heard = S.get('line_last_wall', 0) - S.get('line_wall', 0)
                check('line_played_through', heard > 18.5,
                      f'audio playing for {round(heard, 1)} s of 19.7 (ended '
                      f"{round(S.get('line_last_wall', 0) - S['tour_end_wall'], 1)} s after the tour)")
                finish()
            return
    except Exception as error:  # report and stop instead of spinning
        check('exception', False, repr(error))
        finish()


unreal.register_slate_post_tick_callback(tick)
