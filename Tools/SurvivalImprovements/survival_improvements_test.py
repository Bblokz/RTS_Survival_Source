"""Throwaway-editor PIE checks for the survival improvements (start content in the picker, waves). Never in the
shared editor. Launch (PowerShell, so /Game paths are not mangled):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01
    -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False
    -SurvivalDriveSetup [-SurvivalPickerHold | -SurvivalStartIndex=N -SurvivalGrace=5 -SurvivalStartMinute=25
    -SurvivalWaveProbe] -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate
    -NoLiveCoding
Mode comes from Saved/SurvivalImprovements/run.json: {"mode": "picker", "pick": 2} or {"mode": "waves",
"seconds": 300}. Results: Saved/SurvivalImprovements/<tag>_*.json/png and SURVIVALTEST lines in the log.
"""
import json
import os

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/'
OUT = ROOT + 'Saved/SurvivalImprovements/'
RUN = json.load(open(OUT + 'run.json'))
TAG = RUN.get('tag', RUN['mode'])
S = {'phase': 'wait_editor', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': 0}
CONTENT_RADIUS = 18500.0
RESOURCE_KEYS = ('Radixite', 'Metal', 'ScavObj', 'Resource_', 'Hangar', 'Airport', 'Capture', 'Cactus', 'Pit')


def log(message):
    S['log'].append(message)
    unreal.log_warning('SURVIVALTEST ' + message)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def finish():
    json.dump(S['log'], open(OUT + TAG + '_log.json', 'w'), indent=1)
    S['phase'] = 'done'
    try:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    except Exception:
        pass
    unreal.SystemLibrary.quit_editor()


def start_locations():
    found = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.SurvivalStartLocation)
    return sorted(found, key=lambda a: a.get_name())


def census(label):
    """Counts start-content-like actors by their nearest start location, with class names for radixite/metal."""
    actors = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor)
    starts = start_locations()
    centres = [s.get_actor_location() for s in starts]
    rows = [{'start': s.get_name(), 'counts': {}, 'classes': {}} for s in starts]
    for actor in actors:
        name = actor.get_class().get_name()
        key = next((k for k in RESOURCE_KEYS if k in name), None)
        if key is None:
            continue
        location = actor.get_actor_location()
        distances = [((location.x - c.x) ** 2 + (location.y - c.y) ** 2) ** 0.5 for c in centres]
        nearest = min(range(len(distances)), key=lambda i: distances[i])
        if distances[nearest] > CONTENT_RADIUS:
            continue
        row = rows[nearest]
        row['counts'][key] = row['counts'].get(key, 0) + 1
        if key in ('Radixite', 'Metal', 'Hangar', 'Capture', 'Airport'):
            row['classes'][name] = row['classes'].get(name, 0) + 1
    for row in rows:
        log('%s %s %s' % (label, row['start'], json.dumps(row['counts'], sort_keys=True)))
        log('%s %s classes %s' % (label, row['start'], json.dumps(row['classes'], sort_keys=True)))
    json.dump(rows, open(OUT + TAG + '_' + label + '.json', 'w'), indent=1)


def log_static_checks():
    """Kessel cactus settings as loaded from disk by this fresh editor, and the map's extra cacti in the PIE world."""
    try:
        kessel = unreal.load_asset('/Game/RTS_Survival/SurvivalMode/Data/DA_SurvivalStartContent_Kessel')
        desert = unreal.load_asset('/Game/RTS_Survival/SurvivalMode/Data/DA_SurvivalStartContent')
        for name, asset in (('kessel', kessel), ('desert', desert)):
            cacti = asset.get_editor_property('cacti')
            log('cacti settings %s clusters=%d actors=%d' % (
                name, cacti.get_editor_property('cluster_count'), len(cacti.get_editor_property('cactus_actors'))))
    except Exception as error:
        log('cacti settings FAILED %r' % (error,))
    extra = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor)
             if a.get_actor_label().startswith('SMD_CactusExtra_')]
    log('map extra cacti in PIE world: %d' % len(extra))


def picker():
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world(), unreal.W_SurvivalStartLocationPicker, False)
    return widgets[0] if widgets else None


def shot(name):
    # 'shot showui' keeps the HUD and widgets; the engine writes Saved/Screenshots/WindowsEditor/ScreenShotNNNNN.png.
    S['shots'] += 1
    unreal.SystemLibrary.execute_console_command(world(), 'shot showui')
    log('shot ' + name)


def tick_picker(now):
    phase = S['phase']
    if phase == 'pie' and now - S['mark'] > 45.0:
        census('preview')
        widget = picker()
        log('picker open %s' % (widget is not None))
        shot('picker_0')
        S['phase'], S['mark'] = 'next', now
    elif phase == 'next' and now - S['mark'] > 6.0:
        widget = picker()
        if widget:
            widget.select_location_by_index(RUN.get('pick', 2))
        S['phase'], S['mark'] = 'shot_pick', now
    elif phase == 'shot_pick' and now - S['mark'] > 8.0:
        shot('picker_pick')
        widget = picker()
        if widget:
            widget.confirm_selected_location()
            log('confirmed index %d' % RUN.get('pick', 2))
        S['phase'], S['mark'] = 'after', now
    elif phase == 'after' and now - S['mark'] > 40.0:
        census('after_confirm')
        shot('after_confirm')
        finish()


def tick_waves(now):
    if S['phase'] == 'pie' and now - S['mark'] > RUN.get('seconds', 300):
        census('waves_end')
        shot('waves_end')
        finish()
    elif S['phase'] == 'pie' and S['shots'] == 0 and now - S['mark'] > RUN.get('shot_at', 150):
        shot('waves_mid')


def tick(delta):
    if S.get('busy'):
        return
    S['busy'] = True
    try:
        tick_inner(delta)
    finally:
        S['busy'] = False


def tick_inner(delta):
    try:
        S['t'] += delta
        now = S['t']
        if S['phase'] == 'done':
            return
        if S['phase'] == 'wait_editor' and now > 20.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'starting', now
            return
        if S['phase'] == 'starting':
            if world() is not None and now - S['mark'] > 3.0:
                unreal.GameplayStatics.set_game_paused(world(), False)
                starts = start_locations()
                if starts:
                    starts[0].press_start_for_map_test()
                log('pie started mode=%s' % RUN['mode'])
                log_static_checks()
                S['phase'], S['mark'] = 'pie', now
            return
        for press_at in (12.0, 25.0):
            if S['phase'] != 'starting' and press_at not in S.setdefault('pressed', set()) and now - S['mark'] > press_at:
                S['pressed'].add(press_at)
                starts = start_locations()
                if starts:
                    starts[0].press_start_for_map_test()
        if RUN['mode'] == 'picker':
            tick_picker(now)
        else:
            tick_waves(now)
        if now > RUN.get('hard_limit', 900):
            log('hard limit reached')
            finish()
    except Exception as error:
        log('FAILED %r' % (error,))
        finish()


os.makedirs(OUT, exist_ok=True)
unreal.register_slate_post_tick_callback(tick)
