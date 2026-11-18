"""Aircraft strafe gun sounds, step 2: import the waves and make the assets (headless, idempotent).

  bash Source/Tools/AircraftStrafeAudio/run_import.sh

Needs the C++ class UAircraftGunAudioDataAsset in the editor build that runs it (DebugGame-Cmd by default).
Reads D:/UE_Assets/Sounds/WeaponFire/AircraftGuns/*.wav (build_aircraft_gun_sounds.py) and writes into
/Game/RTS_Survival/Sounds/WeaponSounds/AircraftGuns/:
  S_Aircraft*                 SoundWaves, sound class RTS_SClass_Weapons; *Loop* waves loop. The SHA-1 of the source
                              WAV is kept as metadata, so an unchanged wave is skipped on a re-run.
  Cue_Aircraft20mm_Impact     Modulator -> Random -> the 8 cannon impacts (a duplicate of an 8-wave project cue,
                              retargeted: Python cannot create Sound Cue graph nodes). The projectile impact path plays cues.
  Att_AircraftGun_Fire        spatialized, linear: full volume to 30 m, silent at 150 m (aircraft fire from up high)
  Att_AircraftGun_Impact      spatialized, linear: full volume to 25 m, silent at 105 m
  SC_AircraftGun_Fire/_Impact concurrency 10 each, stop quietest
  DA_AircraftGunAudio         UAircraftGunAudioDataAsset with both sets (values below; a re-run resets them)
Report: Saved/AircraftGunAudio/import_report.json. Config/DefaultGame.ini points RTS Audio Settings at the data asset and
cooks the folder always.
"""
import hashlib
import json
import os

import unreal

WAVES = 'D:/UE_Assets/Sounds/WeaponFire/AircraftGuns/'
FOLDER = '/Game/RTS_Survival/Sounds/WeaponSounds/AircraftGuns'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/AircraftGunAudio/import_report.json'
CONTENT = 'D:/UE5Projects/RTS_Survival/Content/'
SOUND_CLASS = '/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_Weapons'
HASH_TAG = 'AircraftGunSourceSha1'
# 8-wave project cues shaped Modulator -> Random -> 8 wave players; the first that matches is duplicated.
TEMPLATE_CUES = [
    '/Game/RTS_Survival/Sounds/WeaponSounds/Casings/Flak88/Cue_Casing_Flak88_Hit',
    '/Game/RTS_Survival/Sounds/WeaponSounds/MediumCaliber/50MM_Flak41/Cue_PzIII_Flak41_50mm',
    '/Game/RTS_Survival/Sounds/WeaponSounds/MediumCaliber/50MM_KwK42_PzIIIJ/Cue_PzIII_J_KwK42_L60',
    '/Game/RTS_Survival/Sounds/WeaponSounds/SmallCaliber/Flakvierling_Recorded/SingleBarrel/Cue_Flak38_Barrel_Recorded',
    '/Game/RTS_Survival/Sounds/WeaponSounds/HeavyCaliber/DT100mm/T34_100/Cue_T34_100_Fire',
]
CANNON_IMPACT_CUE = 'Cue_Aircraft20mm_Impact'
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log_warning('AIRCRAFT_GUN_SND ' + message)


def disk_path(asset_path):
    return CONTENT + asset_path[len('/Game/'):] + '.uasset'


def save(asset):
    path = asset.get_path_name().split('.')[0]
    ok = bool(EAL.save_asset(path, only_if_is_dirty=False))
    if not (ok and os.path.isfile(disk_path(path))):
        raise RuntimeError('save failed: ' + path)


def import_wave(name, sound_class):
    wav = WAVES + name + '.wav'
    digest = hashlib.sha1(open(wav, 'rb').read()).hexdigest()
    path = f'{FOLDER}/{name}'
    looping = 'Loop' in name
    if EAL.does_asset_exist(path):
        wave = unreal.load_asset(path)
        if (EAL.get_metadata_tag(wave, HASH_TAG) == digest and wave.get_editor_property('sound_class_object') == sound_class
                and bool(wave.get_editor_property('looping')) == looping):
            return wave, 'unchanged'
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', wav)
    task.set_editor_property('destination_path', FOLDER)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    task.set_editor_property('factory', unreal.SoundFactory())
    TOOLS.import_asset_tasks([task])
    wave = unreal.load_asset(path)
    if wave is None:
        raise RuntimeError('import failed: ' + wav)
    # An import over an existing asset resets these.
    wave.set_editor_property('sound_class_object', sound_class)
    wave.set_editor_property('looping', looping)
    EAL.set_metadata_tag(wave, HASH_TAG, digest)
    save(wave)
    return wave, 'imported'


def get_or_create(name, asset_class, factory):
    path = f'{FOLDER}/{name}'
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    asset = TOOLS.create_asset(name, FOLDER, asset_class, factory)
    if asset is None:
        raise RuntimeError('could not create ' + path)
    return asset


def attenuation(name, inner_cm, falloff_cm):
    asset = get_or_create(name, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = asset.get_editor_property('attenuation')
    settings.set_editor_property('attenuate', True)
    settings.set_editor_property('spatialize', True)
    settings.set_editor_property('attenuation_shape', unreal.AttenuationShape.SPHERE)
    settings.set_editor_property('attenuation_shape_extents', unreal.Vector(inner_cm, 0.0, 0.0))
    settings.set_editor_property('falloff_distance', falloff_cm)
    settings.set_editor_property('distance_algorithm', unreal.AttenuationDistanceModel.LINEAR)
    asset.set_editor_property('attenuation', settings)
    save(asset)
    return asset


def concurrency(name, max_count):
    asset = get_or_create(name, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    settings = asset.get_editor_property('concurrency')
    settings.set_editor_property('max_count', max_count)
    settings.set_editor_property('resolution_rule', unreal.MaxConcurrentResolutionRule.STOP_QUIETEST)
    asset.set_editor_property('concurrency', settings)
    save(asset)
    return asset


def cue_nodes(cue):
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


def cannon_impact_cue(waves, sound_class):
    path = f'{FOLDER}/{CANNON_IMPACT_CUE}'
    if not EAL.does_asset_exist(path):
        template = None
        for candidate in TEMPLATE_CUES:
            cue = unreal.load_asset(candidate) if EAL.does_asset_exist(candidate) else None
            nodes = cue_nodes(cue) if cue else None
            log(f'template {candidate}: {"Modulator -> Random -> %d players" % len(nodes[2]) if nodes else "other shape"}')
            if nodes and len(nodes[2]) == len(waves):
                template = candidate
                break
        if template is None:
            raise RuntimeError('no template cue with Modulator -> Random -> %d wave players' % len(waves))
        if EAL.duplicate_asset(template, path) is None:
            raise RuntimeError('could not duplicate ' + template)
    cue = unreal.load_asset(path)
    nodes = cue_nodes(cue)
    if nodes is None or len(nodes[2]) != len(waves):
        raise RuntimeError(f'{path}: not Modulator -> Random -> {len(waves)} wave players (delete it and re-run)')
    modulator, random, players = nodes
    for player, wave in zip(players, waves):
        player.set_editor_property('sound_wave', wave)
        player.set_editor_property('looping', False)
    random.set_editor_property('randomize_without_replacement', True)
    random.set_editor_property('weights', [1.0] * len(waves))
    modulator.set_editor_property('pitch_min', 0.94)
    modulator.set_editor_property('pitch_max', 1.06)
    modulator.set_editor_property('volume_min', 0.8)
    modulator.set_editor_property('volume_max', 1.0)
    # Quieter than a wave at 1.0: two cannons land ~24 of these a second.
    cue.set_editor_property('volume_multiplier', 0.55)
    cue.set_editor_property('pitch_multiplier', 1.0)
    cue.set_editor_property('sound_class_object', sound_class)
    cue.set_editor_property('attenuation_settings', None)
    cue.set_editor_property('override_attenuation', False)
    cue.modify()
    save(cue)
    return cue


def sound_set(waves, values):
    s = unreal.AircraftGunSoundSet()
    for key, value in values.items():
        if isinstance(value, str):
            value = waves[value]
        elif isinstance(value, list) and value and isinstance(value[0], str):
            value = [waves[v] for v in value]
        s.set_editor_property(key, value)
    return s


def run():
    report = {'waves': {}, 'errors': []}
    sound_class = unreal.load_asset(SOUND_CLASS)
    if sound_class is None:
        raise RuntimeError(SOUND_CLASS + ' missing')
    names = sorted(f[:-4] for f in os.listdir(WAVES) if f.endswith('.wav'))
    waves = {}
    for name in names:
        wave, action = import_wave(name, sound_class)
        waves[name] = wave
        report['waves'][name] = dict(action=action, seconds=round(wave.get_editor_property('duration'), 3),
                                     looping=bool(wave.get_editor_property('looping')))
    cue = cannon_impact_cue([waves[f'S_Aircraft20mm_Impact_{i}'] for i in range(1, 9)], sound_class)
    fire_att = attenuation('Att_AircraftGun_Fire', 3000.0, 12000.0)
    impact_att = attenuation('Att_AircraftGun_Impact', 2500.0, 8000.0)
    fire_conc = concurrency('SC_AircraftGun_Fire', 10)
    impact_conc = concurrency('SC_AircraftGun_Impact', 10)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.AircraftGunAudioDataAsset)
    data = get_or_create('DA_AircraftGunAudio', unreal.AircraftGunAudioDataAsset, factory)
    mg = sound_set(waves, dict(
        fire_loop_few_guns='S_AircraftMG_FireLoop_Pair', fire_loop_many_guns='S_AircraftMG_FireLoop_Quad',
        fire_tails=[f'S_AircraftMG_FireTail_{i}' for i in (1, 2, 3)], fire_volume=0.8,
        impact_loop_soft_few_guns='S_AircraftMG_ImpactLoop_Soft_Pair',
        impact_loop_soft_many_guns='S_AircraftMG_ImpactLoop_Soft_Quad',
        impact_loop_hard_few_guns='S_AircraftMG_ImpactLoop_Hard_Pair',
        impact_loop_hard_many_guns='S_AircraftMG_ImpactLoop_Hard_Quad',
        impact_tails=[f'S_AircraftMG_ImpactTail_{i}' for i in (1, 2, 3)],
        unit_hit_clusters=[f'S_AircraftMG_MetalCluster_{i}' for i in range(1, 7)], impact_volume=0.85))
    cannon = sound_set(waves, dict(
        fire_loop_few_guns='S_Aircraft20mm_FireLoop_Pair', fire_loop_many_guns='S_Aircraft20mm_FireLoop_Pair',
        fire_tails=[f'S_Aircraft20mm_FireTail_{i}' for i in (1, 2, 3)], fire_volume=0.7, impact_volume=0.8))
    cannon.set_editor_property('ground_impact_per_projectile', cue)
    # 30 mm strafe cannon (MK 108), version A "Hammer" from build_30mm_versions.py; 3.5 dB above the MG set's levels.
    heavy = sound_set(waves, dict(
        fire_loop_few_guns='S_Aircraft30mm_FireLoop', fire_loop_many_guns='S_Aircraft30mm_FireLoop',
        fire_tails=[f'S_Aircraft30mm_FireTail_{i}' for i in (1, 2, 3)], fire_volume=1.2,
        impact_loop_soft_few_guns='S_Aircraft30mm_ImpactLoop', impact_loop_soft_many_guns='S_Aircraft30mm_ImpactLoop',
        impact_tails=[f'S_Aircraft30mm_ImpactTail_{i}' for i in (1, 2, 3)],
        unit_hit_clusters=[f'S_AircraftMG_MetalCluster_{i}' for i in range(1, 7)], impact_volume=1.25))
    data.set_editor_property('machine_gun', mg)
    data.set_editor_property('cannon', cannon)
    data.set_editor_property('heavy_cannon', heavy)
    data.set_editor_property('heavy_cannon_from_calibre_mm', 28.0)
    data.set_editor_property('cannon_calibre_range_mm', unreal.Vector2D(15.0, 25.0))
    data.set_editor_property('fire_attenuation', fire_att)
    data.set_editor_property('fire_concurrency', fire_conc)
    data.set_editor_property('impact_attenuation', impact_att)
    data.set_editor_property('impact_concurrency', impact_conc)
    save(data)

    check = unreal.load_asset(data.get_path_name())
    report['data'] = dict(path=check.get_path_name(),
                          mg_fire_loop=str(check.get_editor_property('machine_gun').get_editor_property('fire_loop_many_guns')),
                          cannon_impact=str(check.get_editor_property('cannon').get_editor_property('ground_impact_per_projectile')))
    actions = [w['action'] for w in report['waves'].values()]
    report['summary'] = {a: actions.count(a) for a in sorted(set(actions))}
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    log(f'done: waves {report["summary"]}, data {report["data"]}')


try:
    run()
except Exception as error:  # noqa: BLE001
    log(f'ERROR {error!r}')
    raise
