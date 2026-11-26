"""Headless one-shot editor job (2026-11-25): every Berlin response-force unit comes by train and goes straight to the HQ.

DA_Berlin_ResponseForce: Spawns.bAllUnitsByTrain = True (no road group at the map edge, next to where the train enters),
bTrainUsesOwnRoute = True with an empty TrainAttackRoute (the station's groups attack-move straight to the player HQ
instead of walking back north to AttackRoute[0], by the rail entry, where they stalled).
Needs the DebugGame DLL with these properties (SovietCounterattackSettings.h, 2026-11-25). Launch from PowerShell:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject -nullrhi -ExecutePythonScript=<this file>
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
Results: BERLIN_RESPONSE lines in the log; the editor quits when done.
"""
import unreal

DATA = '/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/DA_Berlin_ResponseForce'


def log(text):
    unreal.log_warning('BERLIN_RESPONSE ' + text)


def main():
    try:
        data = unreal.load_asset(DATA)
        spawns = data.get_editor_property('spawns')
        spawns.set_editor_property('all_units_by_train', True)
        data.set_editor_property('spawns', spawns)
        data.set_editor_property('train_uses_own_route', True)
        data.set_editor_property('train_attack_route', [])
        back = data.get_editor_property('spawns').get_editor_property('all_units_by_train')
        log(f'bAllUnitsByTrain={back} bTrainUsesOwnRoute={data.get_editor_property("train_uses_own_route")} '
            f'TrainAttackRoute={len(data.get_editor_property("train_attack_route"))} points')
        data.modify()
        log(f'saved={unreal.EditorAssetLibrary.save_loaded_asset(data, False)}')
    except Exception as error:  # noqa
        log(f'ERROR {error!r}')
    log('done')
    unreal.SystemLibrary.quit_editor()


main()
