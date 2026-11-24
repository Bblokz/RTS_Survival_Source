"""Headless one-shot editor job (2026-11-24): Berlin alarm + Forge crew voice lines.

1. Reparents Mission_Berlin_StayUndetected to UMission_BerlinStayUndetected (C++; plays the Soviet rifleman alarm line
   per cause and sends the patrols) and sets its AlarmLines.
2. Sets CrewLines on Mission_Berlin_ForgeStation (the SovietHazmat crew radio).
Launch from PowerShell (no GPU needed):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject -nullrhi -ExecutePythonScript=<this file>
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
Results: BERLIN_LINES lines in Saved/Logs; the editor quits when done.
"""
import unreal

STAGES = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
LINES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/Berlin/'


def log(text):
    unreal.log_warning('BERLIN_LINES ' + text)


def sound(name):
    asset = unreal.load_asset(LINES + name)
    if asset is None:
        raise RuntimeError('missing sound ' + name)
    return asset


def cdo_of(bp):
    return unreal.get_default_object(unreal.load_object(None, bp.get_path_name() + '_C'))


def set_struct(cdo, prop, fields):
    value = cdo.get_editor_property(prop)
    for key, asset in fields.items():
        value.set_editor_property(key, asset)
    cdo.set_editor_property(prop, value)
    back = cdo.get_editor_property(prop)
    return {k: (back.get_editor_property(k).get_name() if back.get_editor_property(k) else None) for k in fields}


def stay_undetected():
    bp = unreal.load_asset(STAGES + 'Mission_Berlin_StayUndetected')
    parent = unreal.load_class(None, '/Script/RTS_Survival.Mission_BerlinStayUndetected')
    if parent is None:
        raise RuntimeError('C++ class Mission_BerlinStayUndetected not found (DLL not rebuilt?)')
    ok = unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # The new parent shows as the AlarmLines property on the class default (set_struct fails without it).
    log(f'StayUndetected reparented={ok}')
    got = set_struct(cdo_of(bp), 'AlarmLines', {
        'spotted_line': sound('Berlin_Alarm_Spotted'),
        'runner_arrived_line': sound('Berlin_Alarm_RunnerArrived'),
        'relay_bunker_line': sound('Berlin_Alarm_RelayBunker'),
    })
    log(f'StayUndetected AlarmLines {got}')
    cdo = cdo_of(bp)
    for name in ('ForecourtTriggerId', 'RelayBunkerTriggerId', 'BunkerTriggerId', 'ZoneTriggerIdBase',
                 'FirstZoneTriggerId', 'DetectionZoneTriggerIdBase', 'AlarmForecourtTriggerId',
                 'AlarmRelayBunkerTriggerId', 'AlarmFirstZoneTriggerId'):
        try:
            log(f'StayUndetected {name}={cdo.get_editor_property(name)}')
        except Exception:  # noqa
            pass
    bp.modify()
    log(f'StayUndetected saved={unreal.EditorAssetLibrary.save_loaded_asset(bp, False)}')


def forge_station():
    bp = unreal.load_asset(STAGES + 'Mission_Berlin_ForgeStation')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    got = set_struct(cdo_of(bp), 'CrewLines', {
        'sighted_line': sound('Berlin_ForgeCrew_Sighted'),
        'engaged_line': sound('Berlin_ForgeCrew_Engaged'),
        'first_target_lost_line': sound('Berlin_ForgeCrew_TargetLost'),
    })
    log(f'ForgeStation CrewLines {got}')
    bp.modify()
    log(f'ForgeStation saved={unreal.EditorAssetLibrary.save_loaded_asset(bp, False)}')


def main():
    # Both steps are idempotent; ForgeStation was saved on 2026-11-24 already.
    for step in (stay_undetected, forge_station):
        try:
            step()
        except Exception as error:  # noqa
            log(f'ERROR {step.__name__}: {error!r}')
    log('done')
    unreal.SystemLibrary.quit_editor()


main()
