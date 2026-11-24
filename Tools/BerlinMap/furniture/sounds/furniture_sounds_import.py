"""Berlin furniture crush sounds, step 3: import the built waves and make the Sound Cues (headless, idempotent).

  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script=D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/furniture/sounds/furniture_sounds_import.py
      -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
Reads Saved/Berlin/furniture/sounds/waves/*.wav (furniture_sounds_build.py) and furniture_sound_table.py.
Writes into /Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Sounds/:
  S_BerlinFurn_Crush_<Cue>_<n>   SoundWave (mono, 48 kHz); the SHA-1 of its source WAV is kept as asset metadata, so an
                                 unchanged wave is skipped on a re-run
  Cue_BerlinFurn_Crush_<Cue>     SoundCue: Modulator (pitch / volume roll) -> Random without replacement -> wave players,
                                 volume 0.75, sound class RTS_SClass_Weapons. No attenuation or concurrency on the cue:
                                 the crush call passes Att_Collapse and SC_Collapse.
A new cue is a duplicate of Cue_Cactus_Collapse (same node shape, so the cue keeps a real editor graph: Python cannot
create Sound Cue graph nodes), retargeted to the furniture waves.
Report: Saved/Berlin/furniture/sounds/import_report.json ("on_disk_before" = what a fresh process found, the proof that
the previous run's cues survived a reload).
Do not re-run with changed waves while an editor has these assets loaded: the save fails ("Error moving file").
An unchanged re-run saves nothing and is safe.
"""
import hashlib
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import furniture_sound_table as table  # noqa: E402

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/sounds/'
WAVES = ROOT + 'waves/'
REPORT = ROOT + 'import_report.json'
CONTENT = 'D:/UE5Projects/RTS_Survival/Content/'
TEMPLATE_CUE = '/Game/RTS_Survival/Sounds/DestructionAndCollapse/Cactus/Cue_Cactus_Collapse'
SOUND_CLASS = '/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_Weapons'
HASH_TAG = 'BerlinFurnSourceSha1'
EAL = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning('FURN_SND_IMPORT ' + message)


def disk_path(asset_path):
    return CONTENT + asset_path[len('/Game/'):] + '.uasset'


def save(asset_path):
    ok = bool(EAL.save_asset(asset_path, only_if_is_dirty=False))
    return ok and os.path.isfile(disk_path(asset_path))


def import_wave(name, sound_class):
    wav = WAVES + name + '.wav'
    if not os.path.isfile(wav):
        raise RuntimeError(wav + ' missing: run furniture_sounds_build.py')
    digest = hashlib.sha1(open(wav, 'rb').read()).hexdigest()
    path = f'{table.SOUNDS_DIR}/{name}'
    if EAL.does_asset_exist(path):
        wave = unreal.load_asset(path)
        if EAL.get_metadata_tag(wave, HASH_TAG) == digest and wave.get_editor_property('sound_class_object') == sound_class:
            return wave, 'unchanged'
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', wav)
    task.set_editor_property('destination_path', table.SOUNDS_DIR)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    task.set_editor_property('factory', unreal.SoundFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    wave = unreal.load_asset(path)
    if wave is None:
        raise RuntimeError('import failed: ' + wav)
    wave.set_editor_property('sound_class_object', sound_class)   # an import over an asset resets it
    EAL.set_metadata_tag(wave, HASH_TAG, digest)
    if not save(path):
        raise RuntimeError('save failed: ' + path)
    return wave, 'imported'


def cue_nodes(cue):
    """-> (modulator, random, [wave players]) or None when the cue does not have that shape."""
    modulator = cue.get_editor_property('first_node')
    if not isinstance(modulator, unreal.SoundNodeModulator):
        return None
    children = list(modulator.get_editor_property('child_nodes'))
    if len(children) != 1 or not isinstance(children[0], unreal.SoundNodeRandom):
        return None
    players = list(children[0].get_editor_property('child_nodes'))
    if not players or not all(isinstance(p, unreal.SoundNodeWavePlayer) for p in players):
        return None
    return modulator, children[0], players


def cue_state(cue):
    nodes = cue_nodes(cue)
    if nodes is None:
        return None
    modulator, random, players = nodes
    waves = [p.get_editor_property('sound_wave') for p in players]
    sound_class = cue.get_editor_property('sound_class_object')
    return dict(volume=round(cue.get_editor_property('volume_multiplier'), 4),
                pitch=[round(modulator.get_editor_property('pitch_min'), 4), round(modulator.get_editor_property('pitch_max'), 4)],
                mod_volume=[round(modulator.get_editor_property('volume_min'), 4), round(modulator.get_editor_property('volume_max'), 4)],
                without_replacement=bool(random.get_editor_property('randomize_without_replacement')),
                waves=[w.get_name() if w else None for w in waves],
                sound_class=sound_class.get_name() if sound_class else None,
                attenuation=str(cue.get_editor_property('attenuation_settings')),
                duration=round(cue.get_editor_property('duration'), 3))


def wanted_state(cue_def):
    return dict(volume=table.CUE_VOLUME, pitch=[round(v, 4) for v in table.MODULATOR['pitch']],
                mod_volume=[round(v, 4) for v in table.MODULATOR['volume']], without_replacement=True,
                waves=[table.wave_name(cue_def['key'], v) for v in range(cue_def['variations'])],
                sound_class=SOUND_CLASS.split('/')[-1], attenuation='None')


def same(state, wanted):
    return state is not None and all(state.get(key) == value for key, value in wanted.items())


def build_cue(cue_def, waves, sound_class):
    path = table.cue_path(cue_def['key'])
    wanted = wanted_state(cue_def)
    created = False
    if not EAL.does_asset_exist(path):
        if EAL.duplicate_asset(TEMPLATE_CUE, path) is None:
            raise RuntimeError('could not duplicate the template cue to ' + path)
        created = True
    cue = unreal.load_asset(path)
    if not created and same(cue_state(cue), wanted):
        return 'unchanged', cue_state(cue)
    nodes = cue_nodes(cue)
    if nodes is None or len(nodes[2]) != len(waves):
        raise RuntimeError(f'{path}: not Modulator -> Random -> {len(waves)} wave players (delete the cue and re-run)')
    modulator, random, players = nodes
    for player, wave in zip(players, waves):
        player.set_editor_property('sound_wave', wave)
        player.set_editor_property('looping', False)
    random.set_editor_property('randomize_without_replacement', True)
    random.set_editor_property('weights', [1.0] * len(waves))
    modulator.set_editor_property('pitch_min', table.MODULATOR['pitch'][0])
    modulator.set_editor_property('pitch_max', table.MODULATOR['pitch'][1])
    modulator.set_editor_property('volume_min', table.MODULATOR['volume'][0])
    modulator.set_editor_property('volume_max', table.MODULATOR['volume'][1])
    cue.set_editor_property('volume_multiplier', table.CUE_VOLUME)
    cue.set_editor_property('pitch_multiplier', 1.0)
    cue.set_editor_property('sound_class_object', sound_class)
    cue.set_editor_property('attenuation_settings', None)
    cue.set_editor_property('override_attenuation', False)
    cue.modify()
    if not save(path):
        raise RuntimeError('save failed: ' + path)
    return ('created' if created else 'updated'), cue_state(cue)


def run():
    report = {'on_disk_before': {}, 'waves': {}, 'cues': {}, 'errors': []}
    sound_class = unreal.load_asset(SOUND_CLASS)
    if sound_class is None:
        raise RuntimeError(SOUND_CLASS + ' missing')
    for cue_def in table.CUES:
        path = table.cue_path(cue_def['key'])
        if EAL.does_asset_exist(path):
            state = cue_state(unreal.load_asset(path))
            report['on_disk_before'][table.cue_name(cue_def['key'])] = dict(state=state, as_wanted=same(state, wanted_state(cue_def)))
    for cue_def in table.CUES:
        try:
            waves = []
            for v in range(cue_def['variations']):
                name = table.wave_name(cue_def['key'], v)
                wave, action = import_wave(name, sound_class)
                waves.append(wave)
                report['waves'][name] = dict(action=action, seconds=round(wave.get_editor_property('duration'), 3),
                                             channels=wave.get_editor_property('num_channels'),
                                             on_disk=os.path.isfile(disk_path(f'{table.SOUNDS_DIR}/{name}')))
            action, state = build_cue(cue_def, waves, sound_class)
            report['cues'][table.cue_name(cue_def['key'])] = dict(action=action, state=state,
                                                                 on_disk=os.path.isfile(disk_path(table.cue_path(cue_def['key']))))
            log(f'{table.cue_name(cue_def["key"])}: {action} {json.dumps(state)}')
        except Exception as error:  # noqa: BLE001
            report['errors'].append(f'{cue_def["key"]}: {error!r}')
            log(f'{cue_def["key"]}: ERROR {error!r}')
    actions = [w['action'] for w in report['waves'].values()] + [c['action'] for c in report['cues'].values()]
    report['summary'] = {a: actions.count(a) for a in sorted(set(actions))}
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    log(f'done: {report["summary"]}, before-as-wanted '
        f'{sum(1 for c in report["on_disk_before"].values() if c["as_wanted"])}/{len(table.CUES)}, errors {report["errors"]}')


run()
