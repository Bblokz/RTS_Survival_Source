"""Berlin furniture crush sounds, step 1: export the project source waves to WAV and dump the reference settings.

Headless, read-only (writes nothing into Content):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script=D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/furniture/sounds/furniture_sounds_export.py
      -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
Output: Saved/Berlin/furniture/sounds/src/<AssetName>.wav and Saved/Berlin/furniture/sounds/reference_dump.json
(cue trees of the reference cues, the Att_Collapse / SC_Collapse settings, the sound class of each exported wave).
Next step: furniture_sounds_build.py (offline, plain Python + numpy + ffmpeg).
"""
import json
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/sounds/'
SRC = OUT + 'src/'
W = '/Game/WARFARE_SOUNDS/'
R = '/Game/RTS_Survival/'
# (folder, substrings a wave name must contain one of; empty = every wave in the folder)
SOURCES = [
    (R + 'Sounds/DestructionAndCollapse', []),
    (R + 'Sounds/Construction/Woodwork', []),
    (R + 'Sounds/Resources', ['Stone']),
    (W + 'Miscellaneous_Sounds/Glass/Wavs', []),
    (W + 'Miscellaneous_Sounds/Metal/Wavs', []),
    (W + 'Miscellaneous_Sounds/Doors/Wavs', ['wooden', 'slam', 'creak_short', 'bell', 'manhole_metal_door_lid', 'gate_fence',
                                              'small_box', 'hatch_close']),
    (W + 'Weapons/Bullets/Wavs', ['impact_wood', 'impact_glass', 'impact_concrete', 'impact_body_thump', 'impact_rock']),
    (W + 'Weapons/Knife/Wavs', ['sword_hit']),
    (W + 'Foley/Wavs', ['glove_catch', 'bone_break', 'keys_belt']),
    (W + 'Footsteps/Wavs', ['wood_land', 'wood_wobble']),
    (W + 'Ambiences/Urban_Tones_Backgrounds/Wavs', ['chimes']),
    ('/Game/UVB80/Assets/UVB80/Environment/Sounds', []),
    (R + 'Blueprints/Environment/CrushableInstancedActor/FarmCrops/Sounds', []),
]
REFERENCE_CUES = [
    R + 'Sounds/DestructionAndCollapse/CollapsingTree/Cue_Collapse_Tree',
    R + 'Sounds/DestructionAndCollapse/CollapseSmallStoneObj/Cue_SmallStoneObj_Collapse',
    R + 'Sounds/DestructionAndCollapse/CollapsingMetal/MetalObject/Cue_MetalObj_Destruction',
    R + 'Sounds/DestructionAndCollapse/CollapsingMetal/MetalDebrisCollapse/Cue_MetalResource_SmallMetal_Collapse',
    R + 'Sounds/DestructionAndCollapse/Cactus/Cue_Cactus_Collapse',
    R + 'Blueprints/Environment/CrushableInstancedActor/FarmCrops/Sounds/Cue_FarmCrush_Wood',
    R + 'Blueprints/Environment/CrushableInstancedActor/FarmCrops/Sounds/Cue_FarmCrush_Pumpkin',
    R + 'StarterContent/Audio/Collapse_Cue',
    R + 'SpecialEffects/AddOnArmor/Sounds/Cue_AddOnArmorOff_Light',
]
SETTINGS = [R + 'Sounds/Settings/Collapse/Att_Collapse', R + 'Sounds/Settings/Collapse/SC_Collapse']


def log(message):
    unreal.log_warning('FURN_SND_EXPORT ' + message)


def name_of(obj):
    return obj.get_path_name().split('.')[0] if obj else None


def prop(obj, name):
    try:
        value = obj.get_editor_property(name)
    except Exception:  # noqa: BLE001
        return None
    if isinstance(value, (int, float, bool, str)) or value is None:
        return value
    if isinstance(value, unreal.Object):
        return name_of(value)
    try:
        return value.export_text()
    except Exception:  # noqa: BLE001
        return str(value)


def node_tree(node, waves):
    if node is None:
        return None
    out = {'class': node.get_class().get_name()}
    for key in ('pitch_min', 'pitch_max', 'volume_min', 'volume_max', 'weights', 'randomize_without_replacement',
                'looping', 'input_volume', 'delay_min', 'delay_max', 'volume_multiplier', 'pitch_multiplier'):
        value = prop(node, key)
        if value is not None:
            out[key] = list(value) if isinstance(value, unreal.Array) else value
    if isinstance(node, unreal.SoundNodeWavePlayer):
        wave = node.get_editor_property('sound_wave')
        out['wave'] = name_of(wave)
        if wave is not None:
            waves.append(wave)
    children = [node_tree(child, waves) for child in node.get_editor_property('child_nodes')]
    if children:
        out['children'] = children
    return out


def wave_info(wave):
    return dict(path=name_of(wave), duration=round(wave.get_editor_property('duration'), 3),
                channels=wave.get_editor_property('num_channels'),
                rate=wave.get_editor_property('sample_rate') if hasattr(wave, 'sample_rate') else None,
                volume=prop(wave, 'volume'), pitch=prop(wave, 'pitch'), sound_class=prop(wave, 'sound_class_object'),
                attenuation=prop(wave, 'attenuation_settings'), concurrency=prop(wave, 'concurrency_set'),
                loading=prop(wave, 'loading_behavior'), compression=prop(wave, 'sound_asset_compression_type'))


def export_wave(wave):
    target = SRC + wave.get_name() + '.wav'
    if os.path.isfile(target) and os.path.getsize(target) > 1000:
        return True
    task = unreal.AssetExportTask()
    task.set_editor_property('object', wave)
    task.set_editor_property('filename', target)
    task.set_editor_property('automated', True)
    task.set_editor_property('prompt', False)
    task.set_editor_property('replace_identical', True)
    task.set_editor_property('exporter', unreal.SoundExporterWAV())
    return bool(unreal.Exporter.run_asset_export_task(task))


def run():
    os.makedirs(SRC, exist_ok=True)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    dump = {'waves': {}, 'cues': {}, 'settings': {}, 'failed': []}
    cue_waves = []
    for path in REFERENCE_CUES:
        cue = unreal.load_asset(path)
        if cue is None:
            dump['cues'][path] = None
            continue
        dump['cues'][path] = dict(volume_multiplier=prop(cue, 'volume_multiplier'),
                                  pitch_multiplier=prop(cue, 'pitch_multiplier'),
                                  sound_class=prop(cue, 'sound_class_object'),
                                  attenuation=prop(cue, 'attenuation_settings'),
                                  override_attenuation=prop(cue, 'override_attenuation'),
                                  concurrency=prop(cue, 'concurrency_set'),
                                  tree=node_tree(cue.get_editor_property('first_node'), cue_waves))
    for path in SETTINGS:
        asset = unreal.load_asset(path)
        key = 'attenuation' if isinstance(asset, unreal.SoundAttenuation) else 'concurrency'
        dump['settings'][path] = prop(asset, key)
    waves = {wave.get_path_name(): wave for wave in cue_waves}
    for folder, filters in SOURCES:
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) != 'SoundWave':
                continue
            name = str(data.asset_name)
            if filters and not any(f.lower() in name.lower() for f in filters):
                continue
            wave = data.get_asset()
            if wave is not None:
                waves[wave.get_path_name()] = wave
    for wave in waves.values():
        ok = export_wave(wave)
        info = wave_info(wave)
        info['exported'] = ok
        dump['waves'][wave.get_name()] = info
        if not ok:
            dump['failed'].append(info['path'])
    with open(OUT + 'reference_dump.json', 'w') as handle:
        json.dump(dump, handle, indent=1, default=str)
    log(f'done: {len(waves)} waves, {len(dump["failed"])} failed, dump {OUT}reference_dump.json')


run()
