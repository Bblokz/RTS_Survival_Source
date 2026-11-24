"""Berlin furniture crush sounds, step 2: build the crush waves offline (plain Python 3 + numpy + ffmpeg).

  python Source/Tools/BerlinMap/furniture/sounds/furniture_sounds_build.py
Needs the exported project waves of step 1 (furniture_sounds_export.py) in Saved/Berlin/furniture/sounds/src/.
Writes (deterministic: fixed seeds, a re-run gives the same samples):
  Saved/Berlin/furniture/sounds/waves/S_BerlinFurn_Crush_<Cue>_<1..3>.wav   mono, 48 kHz, 16 bit (what step 3 imports)
  Saved/Berlin/furniture/sounds/preview/                                    the same files + all_in_a_row.wav,
      all_variations_in_a_row.wav, row_of_chairs_simulation.wav, reference_*.wav, spectrogram PNGs
  Saved/Berlin/furniture/sounds/build_stats.json                            durations, peaks, loudness, sources
Next step: furniture_sounds_import.py (headless Unreal). Doc: Source/Docs/Environment/BerlinFurniture.md.

Every wave is a mix of layers. A layer is a project wave (pitched by resampling, trimmed, filtered) or a small
synthesised tone. Each layer is first levelled to the same 100 ms loudness, so the dB numbers in the recipes are
real balance numbers. The finished wave is levelled to TARGET_LUFS (max 400 ms K-weighted loudness).
"""
import json
import os
import shutil
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import furn_audio as fa  # noqa: E402
from furniture_sound_table import CUES, CUE_VOLUME, MODULATOR, wave_name  # noqa: E402

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/sounds/'
SRC = ROOT + 'src/'
WAVES = ROOT + 'waves/'
PREVIEW = ROOT + 'preview/'
RATE = fa.RATE
TARGET_LUFS = -16.0      # wave level; the cue plays it at CUE_VOLUME (0.75 = -2.5 dB) like the other collapse cues
CEILING_DB = -1.0
LAYER_LUFS = -20.0       # every layer is levelled to this (100 ms window) before its recipe gain
MAX_SECONDS = 1.6        # 1.8 s asked; the lowest pitch roll (0.94) stretches a wave by 6 %

_cache = {}
_used = set()


def src(name):
    """A project source wave, mono, cut to its onset."""
    if name not in _cache:
        path = SRC + name + '.wav'
        if not os.path.isfile(path):
            raise RuntimeError(path + ' missing: run furniture_sounds_export.py first')
        _cache[name] = fa.trim_to_onset(fa.load(path))
    _used.add(name)
    return _cache[name]


def level(signal):
    return signal * fa.gain(LAYER_LUFS - fa.loudness(signal, 0.1))


def layer(name, pitch=1.0, start=0.0, length=None, lp=None, hp=None, release=0.06):
    """Source wave -> pitched (resample), cut, filtered, levelled layer."""
    signal = src(name)
    if pitch != 1.0:
        signal = fa.resample(signal, pitch)
    if length is None:
        length = len(signal) / RATE - start
    signal = fa.cut(signal, start, length, 0.001 if start == 0.0 else 0.004, release)
    if lp:
        signal = fa.lowpass(signal, lp)
    if hp:
        signal = fa.highpass(signal, hp)
    return level(signal)


# ---- synthesised layers (no source in the project) ---------------------------------------------------------------

def modes(seconds, f0, ratios, amps, t60s, detune=0.0, sag=0.0, seed=0):
    """Sum of decaying partials. detune: each partial doubled that far apart (beating). sag: pitch drop by the end."""
    rng = np.random.default_rng(seed)
    t = np.arange(int(seconds * RATE)) / RATE
    bend = 1.0 - sag * (t / max(seconds, 1e-6)) ** 2
    out = np.zeros(len(t))
    for ratio, amp, t60 in zip(ratios, amps, t60s):
        for spread in ((-detune, detune) if detune else (0.0,)):
            freq = f0 * ratio * (1.0 + spread) * bend
            phase = 2.0 * np.pi * np.cumsum(freq) / RATE + rng.uniform(0.0, 2.0 * np.pi)
            out += amp * np.sin(phase) * np.exp(-6.908 * t / t60)
    return fa.fade(out, 0.002, 0.03)


def piano_string(f0, seconds, rng):
    """One out-of-tune string pair: stiff-string partials, the high ones die first."""
    t = np.arange(int(seconds * RATE)) / RATE
    stiffness = 0.0004 * (1.0 + 110.0 / f0)
    base = float(np.clip(2.4 * (220.0 / f0) ** 0.3, 0.9, 2.6))
    out = np.zeros(len(t))
    count = int(min(14, 6500.0 / f0))
    for cents in (rng.uniform(-22.0, -4.0), rng.uniform(4.0, 22.0)):
        f = f0 * 2.0 ** (cents / 1200.0)
        for n in range(1, count + 1):
            fn = n * f * np.sqrt(1.0 + stiffness * n * n)
            amp = rng.uniform(0.5, 1.0) / n ** 0.9
            out += amp * np.sin(2.0 * np.pi * fn * t + rng.uniform(0.0, 6.28)) * np.exp(-6.908 * t / (base / (1.0 + 0.22 * n)))
    return fa.fade(out, 0.003, 0.02)


def piano_jangle(seconds, seed):
    """The frame cracks and a handful of strings ring at once: clusters of neighbouring notes, most within 90 ms."""
    rng = np.random.default_rng(seed)
    notes = []
    for _ in range(3):
        root = int(rng.integers(43, 80))
        notes += [root + int(step) for step in rng.choice([0, 1, 2, 3, 6], size=int(rng.integers(2, 4)), replace=False)]
    layers = []
    for index, note in enumerate(notes):
        start = float(rng.uniform(0.0, 0.09)) if index < len(notes) - 3 else float(rng.uniform(0.12, 0.4))
        string = piano_string(440.0 * 2.0 ** ((note - 69) / 12.0), seconds - start, rng)
        layers.append((string / max(np.abs(string).max(), 1e-9), start, float(rng.uniform(-7.0, 0.0))))
    out = fa.mix(seconds, layers)
    out = fa.bell(fa.lowpass(out, 5200.0), 260.0, 3.0, 1.5)     # soundboard
    # a piano's ring drops fast after the strike; lift the tail (up to +10 dB at 0.8 s) so the ring carries the sound
    sustain = 1.0 + 2.2 * np.clip(np.arange(len(out)) / RATE / 0.8, 0.0, 1.0)
    return level(fa.decay(out * sustain, seconds * 2.2))


def string_snap(f0, seed):
    """A string that lets go: very stiff, short, falling."""
    rng = np.random.default_rng(seed)
    t = np.arange(int(0.35 * RATE)) / RATE
    out = np.zeros(len(t))
    glide = 1.0 - 0.22 * (1.0 - np.exp(-t / 0.05))
    for n in range(1, 9):
        fn = n * f0 * np.sqrt(1.0 + 0.012 * n * n) * glide
        out += np.sin(2.0 * np.pi * np.cumsum(fn) / RATE + rng.uniform(0.0, 6.28)) / n * np.exp(-6.908 * t / (0.3 / (1.0 + 0.3 * n)))
    return level(fa.fade(out, 0.001, 0.03))


def spring_twang(f0, seed):
    """Mattress / seat spring: a short wobbling tone. Kept low in the mix (it turns comic when loud)."""
    rng = np.random.default_rng(seed)
    t = np.arange(int(0.5 * RATE)) / RATE
    freq = f0 * (1.0 + 0.16 * np.exp(-t / 0.12) * np.sin(2.0 * np.pi * rng.uniform(26.0, 34.0) * t))
    phase = 2.0 * np.pi * np.cumsum(freq) / RATE
    out = sum(amp * np.sin(ratio * phase + rng.uniform(0.0, 6.28)) for ratio, amp in ((1.0, 1.0), (2.0, 0.5), (3.0, 0.3), (4.2, 0.2), (6.3, 0.12)))
    return level(fa.highpass(fa.fade(fa.decay(out, 0.45), 0.002, 0.05), 150.0))


def enamel_ring(f0, seed):
    """Dull ring of an enamelled cast-iron tub: a few inharmonic shell modes with slow beating, no top end."""
    out = modes(0.9, f0, (1.0, 2.32, 4.25, 6.63), (1.0, 0.6, 0.3, 0.14), (0.75, 0.42, 0.22, 0.12), detune=0.0035, seed=seed)
    return level(fa.lowpass(out, 3500.0))


def pipe_ping(f0, seed):
    """Hollow radiator pipe / rib knock."""
    out = modes(0.4, f0, (1.0, 2.05, 3.1, 4.3), (1.0, 0.5, 0.3, 0.15), (0.28, 0.2, 0.12, 0.08), detune=0.002, seed=seed)
    return level(out)


def broken_chime(f0, seed):
    """Two strikes on clock chime rods (clamped-bar partials); the pitch sags and flutters, then it is choked."""
    rng = np.random.default_rng(seed)
    ratios, amps, t60s = (1.0, 2.76, 5.40, 8.93), (1.0, 0.5, 0.26, 0.1), (1.5, 0.7, 0.32, 0.16)
    first = modes(1.0, f0, ratios, amps, t60s, detune=0.0015, sag=0.035, seed=seed)
    second = modes(0.78, f0 * 0.84, ratios, amps, t60s, detune=0.0025, sag=0.06, seed=seed + 1)
    click = fa.decay(fa.highpass(fa.lowpass(fa.noise(0.02, seed + 2), 5000.0), 1800.0), 0.02)
    out = fa.mix(1.05, [(first, 0.0, 0.0), (second, 0.22, -3.0), (click, 0.0, -8.0), (click, 0.22, -11.0)])
    t = np.arange(len(out)) / RATE
    flutter = 1.0 - 0.45 * np.clip((t - 0.25) / 0.6, 0.0, 1.0) * (0.5 + 0.5 * np.sin(2.0 * np.pi * rng.uniform(11.0, 16.0) * t))
    return level(fa.fade(out * flutter, 0.001, 0.12))


def scatter(names, count, first, last, seed, pitch=(0.6, 0.95), gain=(-6.0, 0.0), **kw):
    """`count` small hits from `names` spread between `first` and `last` seconds, getting quieter. -> mix layers"""
    rng = np.random.default_rng(seed)
    out = []
    for index in range(count):
        start = first + (last - first) * (index + rng.uniform(0.0, 0.8)) / count
        hit = layer(names[int(rng.integers(len(names)))], pitch=float(rng.uniform(*pitch)), **kw)
        out.append((hit, float(start), float(rng.uniform(*gain)) - 5.0 * index / max(count - 1, 1)))
    return out


# ---- recipes: v = 0..2 picks other sources, offsets and seeds -----------------------------------------------------

def pick(v, *options):
    return options[v % len(options)]


def wood_small(v):
    """Thin boards and legs: two cracks, a sharp splinter snap, then a few bits clattering down."""
    snaps = ['bone_break_neck_snap_crack_02', 'bone_break_neck_snap_crack_03', 'bone_break_neck_snap_crack_04']
    lights = ['bullet_impact_wood_light_01', 'bullet_impact_wood_light_03', 'bullet_impact_wood_light_07']
    return 1.0, [
        (layer(pick(v, 'bullet_impact_wood_heavy_04', 'bullet_impact_wood_heavy_02', 'bullet_impact_wood_heavy_06'), 0.86), 0.0, 0.0),
        (layer(pick(v, snaps[0], snaps[2], snaps[1]), 0.7, lp=8000.0), 0.018, -5.0),
        (layer(pick(v, 'bullet_impact_wood_light_04', 'bullet_impact_wood_light_06', 'bullet_impact_wood_light_05'), 0.8),
         pick(v, 0.13, 0.17, 0.11), -3.0),
        (layer(pick(v, 'footstep_wood_land_06', 'footstep_wood_land_03', 'footstep_wood_land_09'), 0.9), pick(v, 0.3, 0.27, 0.33), -8.0),
        *scatter(snaps, 3, 0.22, 0.6, 100 + v, pitch=(0.55, 0.85), gain=(-11.0, -7.0), lp=7000.0),
        *scatter(lights, 3, 0.3, 0.75, 110 + v, pitch=(1.1, 1.4), gain=(-12.0, -8.0), length=0.18),
    ]


def wood_cabinet(v):
    """Tall hollow carcass: boom, long crack and creak, then the glass / crockery inside."""
    snaps = ['bone_break_neck_snap_crack_02', 'bone_break_neck_snap_crack_03', 'bone_break_neck_snap_crack_04']
    boom = fa.bell(layer(pick(v, 'door_old_wooden_close_thud_02', 'door_old_wooden_close_thud_03', 'door_old_wooden_close_thud_05'), 0.8), 115.0, 4.0, 1.2)
    return 1.45, [
        (level(boom), 0.0, 1.0),
        (layer(pick(v, 'bullet_impact_wood_heavy_01', 'bullet_impact_wood_heavy_05', 'bullet_impact_wood_heavy_08'), 0.75), 0.03, 0.0),
        (layer(pick(v, 'door_creak_short_06', 'door_old_wooden_squeak_02', 'door_old_wooden_squeak_07'), pick(v, 0.9, 0.8, 0.85),
               start=pick(v, 0.0, 0.25, 0.2), length=0.38), 0.06, -8.0),
        (layer(pick(v, 'bullet_impact_wood_heavy_03', 'bullet_impact_wood_heavy_07', 'bullet_impact_wood_heavy_02'), 0.82),
         pick(v, 0.22, 0.26, 0.2), -3.0),
        (layer(pick(v, 'glass_smashable_small_break_01', 'glass_smashable_small_break_03', 'glass_smashable_small_break_02'), 0.9,
               length=0.75, hp=1400.0, release=0.3), pick(v, 0.12, 0.15, 0.1), -5.0),
        (layer(pick(v, 'glass_smashable_debris_fall_02', 'glass_smashable_debris_fall_04', 'glass_smashable_debris_fall_05'), 0.85,
               start=0.05, length=0.7, release=0.4), 0.5, -11.0),
        *scatter(snaps, 4, 0.3, 0.9, 200 + v, pitch=(0.5, 0.8), gain=(-11.0, -7.0), lp=7000.0),
    ]


def upholstery(v):
    """Stuffed seat: a soft whump, the frame cracking inside the padding, springs."""
    creaks = ['chair_frame_metal_creak_squeak_06', 'chair_frame_metal_creak_squeak_15', 'chair_frame_metal_creak_squeak_12']
    return 1.05, [
        (layer(pick(v, 'bullet_impact_body_thump_03', 'bullet_impact_body_thump_01', 'bullet_impact_body_thump_05'), 0.8, lp=2500.0), 0.0, 0.0),
        (layer(pick(v, 'foley_cloth_sports_glove_catch_02', 'foley_cloth_sports_glove_catch_05', 'foley_cloth_sports_glove_catch_01'), 0.7), 0.01, -3.0),
        (layer(pick(v, 'bullet_impact_wood_heavy_05', 'bullet_impact_wood_heavy_03', 'bullet_impact_wood_heavy_01'), 0.8, lp=1900.0), 0.04, -1.0),
        (layer(pick(v, 'bullet_impact_wood_light_05', 'bullet_impact_wood_light_08', 'bullet_impact_wood_light_02'), 0.78, lp=2600.0),
         pick(v, 0.2, 0.24, 0.18), -4.0),
        (layer(pick(v, *creaks), 0.8, release=0.2), 0.08, -6.0),
        (layer(pick(v, creaks[2], creaks[0], creaks[1]), 0.7, length=0.4, release=0.2), 0.42, -10.0),
        (spring_twang(pick(v, 215.0, 255.0, 185.0), 300 + v), 0.1, -10.0),
        (spring_twang(pick(v, 290.0, 205.0, 240.0), 310 + v), pick(v, 0.27, 0.31, 0.24), -13.0),
    ]


def iron_bed(v):
    """Tube frame and wire mesh: rattly clang, spring mesh rattle and squeaks, a muffled mattress."""
    creaks = ['chair_frame_metal_creak_squeak_05', 'chair_frame_metal_creak_squeak_14', 'chair_frame_metal_creak_squeak_07']
    return 1.3, [
        (layer(pick(v, 'metal_gate_fence_impact_03', 'metal_gate_fence_impact_02', 'metal_gate_fence_impact_04'), 0.85, length=0.9, release=0.4), 0.0, 0.0),
        (layer(pick(v, 'metal_med_impact_01', 'metal_tin_impacts_hit_hard_03', 'metal_tin_impacts_hit_hard_01'), 0.85), 0.0, -3.0),
        (layer(pick(v, 'bullet_impact_body_thump_05', 'bullet_impact_body_thump_08', 'bullet_impact_body_thump_01'), 0.8, lp=1500.0), 0.02, -6.0),
        (layer(pick(v, 'metal_rattle_spin_small_02', 'metal_rattle_spin_small_03', 'metal_rattle_spin_small_04'), 0.9, release=0.3), 0.1, -5.0),
        (layer(pick(v, *creaks), 0.85, release=0.15), 0.16, -7.0),
        (layer(pick(v, creaks[1], creaks[2], creaks[0]), 0.75, length=0.45, release=0.2), pick(v, 0.45, 0.5, 0.4), -9.0),
        (spring_twang(pick(v, 240.0, 200.0, 270.0), 400 + v), 0.07, -11.0),
        (spring_twang(pick(v, 180.0, 300.0, 225.0), 410 + v), 0.3, -13.0),
    ]


def bathtub(v):
    """Enamelled cast-iron vessel: deep dull boom, a hard cast-iron knock, the enamel ring, a scrape over rubble."""
    return 1.45, [
        (layer(pick(v, 'metal_drum_impact_thud_03', 'metal_drum_impact_thud_04', 'metal_drum_impact_thud_08'), 0.9, release=0.3), 0.0, 0.0),
        (layer(pick(v, 'manhole_metal_door_lid_impact_01', 'manhole_metal_door_lid_impact_05', 'manhole_metal_door_lid_impact_04'), 0.8, release=0.3), 0.012, -2.0),
        (enamel_ring(pick(v, 292.0, 331.0, 262.0), 500 + v), 0.012, -6.0),
        (layer(pick(v, 'bullet_impact_glass_01', 'bullet_impact_glass_04', 'bullet_impact_glass_02'), 0.7, lp=6000.0), 0.05, -12.0),
        (layer(pick(v, 'metal_scrape_deep_grind_squeak_01', 'metal_scrape_deep_grind_squeak_03', 'metal_scrape_deep_grind_squeak_02'), 0.9,
               start=pick(v, 0.1, 0.1, 0.2), length=0.6, release=0.3), 0.3, -7.0),
        (layer(pick(v, 'metal_drum_impact_thud_06', 'metal_drum_impact_thud_05', 'metal_drum_impact_thud_03'), 1.1, release=0.2),
         pick(v, 0.55, 0.6, 0.5), -9.0),
    ]


def radiator(v):
    """Cast-iron ribs: a hard clang, the ribs knocking each other one after another, a pipe tearing loose."""
    hits = ['metal_hit_small_04', 'metal_hit_small_05', 'metal_hit_small_06', 'metal_hit_small_08']
    rng = np.random.default_rng(600 + v)
    knocks, start = [], 0.09
    for index in range(6):
        knocks.append((layer(hits[int(rng.integers(len(hits)))], float(rng.uniform(0.65, 0.85)), length=0.22), start, -5.0 - 1.4 * index))
        knocks.append((pipe_ping(float(rng.uniform(480.0, 720.0)), 610 + 10 * v + index), start, -12.0 - 1.4 * index))
        start += float(rng.uniform(0.055, 0.1))
    return 1.3, [
        (layer(pick(v, 'manhole_metal_door_lid_impact_03', 'manhole_metal_door_lid_impact_02', 'manhole_metal_door_lid_impact_05'), 0.9, length=1.0, release=0.4), 0.0, 0.0),
        (layer(pick(v, 'metal_med_impact_02', 'metal_med_impact_03', 'metal_med_impact_02'), pick(v, 0.8, 0.8, 0.72), length=0.8, release=0.3), 0.02, -3.0),
        *knocks,
        (layer(pick(v, 'metal_scrape_deep_grind_squeak_04', 'metal_scrape_deep_grind_squeak_06', 'metal_scrape_deep_grind_squeak_02'), 1.0,
               start=pick(v, 0.1, 0.1, 0.3), length=0.5, release=0.3), 0.45, -9.0),
    ]


def tiled_stove(v):
    """Fire brick and glazed tiles: masonry crunch, tiles cracking, a duller-than-glass shatter, tiles sliding off."""
    rocks = ['bullet_impact_rock_02', 'bullet_impact_rock_05', 'bullet_impact_rock_08', 'bullet_impact_concrete_brick_05']
    return 1.55, [
        (layer(pick(v, 'S_SmallStoneObj_Collapse_2', 'S_SmallStoneObj_Collapse_3', 'S_SmallStoneObj_Collapse_4'), 1.0, length=1.3, release=0.5), 0.0, 0.0),
        (layer(pick(v, 'door_close_slam_slow_03', 'door_close_slam_slow_01', 'door_close_slam_slow_02'), 0.7, lp=320.0), 0.0, -4.0),
        (layer(pick(v, 'bullet_impact_concrete_brick_01', 'bullet_impact_concrete_brick_04', 'bullet_impact_concrete_brick_03'), 0.85), 0.0, -2.0),
        (layer(pick(v, 'bullet_impact_rock_01', 'bullet_impact_rock_04', 'bullet_impact_rock_07'), 0.8), pick(v, 0.09, 0.12, 0.08), -4.0),
        (layer(pick(v, 'glass_smashable_small_break_02', 'glass_smashable_small_break_01', 'glass_smashable_small_break_03'), 0.6,
               length=0.8, lp=5000.0, release=0.3), 0.06, -4.0),
        (layer(pick(v, 'glass_smashable_debris_fall_04', 'glass_smashable_debris_fall_01', 'glass_smashable_debris_fall_02'), 0.55,
               start=0.05, length=0.8, lp=6000.0, release=0.4), 0.42, -9.0),
        *scatter(rocks, 4, 0.3, 1.0, 700 + v, pitch=(0.6, 0.9), gain=(-10.0, -6.0)),
    ]


def washstand(v):
    """Marble slab snapping, the basin and jug shattering, a light wooden stand."""
    return 1.25, [
        (layer(pick(v, 'bullet_impact_rock_01', 'bullet_impact_rock_04', 'bullet_impact_rock_05'), 0.7), 0.0, 0.0),
        (layer('Sound_Stone_Destroy', pick(v, 0.8, 0.72, 0.88)), 0.03, -2.0),
        (layer(pick(v, 'bullet_impact_glass_03', 'bullet_impact_glass_05', 'bullet_impact_glass_01'), 0.6, lp=7000.0), 0.02, -5.0),
        (layer(pick(v, 'glass_smashable_small_break_03', 'glass_smashable_small_break_02', 'glass_smashable_small_break_01'), 0.72,
               length=0.75, lp=6500.0, release=0.3), 0.05, -2.0),
        (layer(pick(v, 'bullet_impact_wood_light_02', 'bullet_impact_wood_light_04', 'bullet_impact_wood_light_06'), 0.85), pick(v, 0.1, 0.13, 0.09), -4.0),
        (layer(pick(v, 'glass_smashable_debris_fall_05', 'glass_smashable_debris_fall_02', 'glass_smashable_debris_fall_04'), 0.65,
               start=0.03, length=0.7, lp=7000.0, release=0.4), 0.4, -9.0),
    ]


def piano(v):
    """Upright piano: the case booms and cracks, every string rings at once out of tune, two strings let go."""
    lights = ['bullet_impact_wood_light_01', 'bullet_impact_wood_light_03', 'bullet_impact_wood_light_07']
    boom = fa.bell(layer(pick(v, 'door_old_wooden_close_thud_03', 'door_old_wooden_close_thud_02', 'door_old_wooden_close_thud_01'), 0.75), 100.0, 3.0, 1.2)
    return 1.6, [
        (level(boom), 0.0, 0.0),
        (layer(pick(v, 'bullet_impact_wood_heavy_02', 'bullet_impact_wood_heavy_04', 'bullet_impact_wood_heavy_07'), 0.75), 0.02, -1.0),
        (piano_jangle(1.55, 900 + v), 0.03, 1.0),
        (layer(pick(v, 'bullet_impact_wood_heavy_06', 'bullet_impact_wood_heavy_01', 'bullet_impact_wood_heavy_05'), 0.85), pick(v, 0.2, 0.24, 0.18), -4.0),
        (string_snap(pick(v, 880.0, 1170.0, 740.0), 910 + v), pick(v, 0.3, 0.26, 0.34), -9.0),
        (string_snap(pick(v, 1320.0, 660.0, 990.0), 920 + v), pick(v, 0.55, 0.5, 0.6), -11.0),
        *scatter(lights, 4, 0.12, 0.6, 930 + v, pitch=(1.2, 1.5), gain=(-12.0, -8.0), length=0.15),
    ]


def clock(v):
    """Grandfather clock: thin case cracking, the door glass, two sagging strikes of the chime, the movement running down."""
    snaps = ['bone_break_neck_snap_crack_02', 'bone_break_neck_snap_crack_03', 'bone_break_neck_snap_crack_04']
    return 1.5, [
        (layer(pick(v, 'bullet_impact_wood_heavy_08', 'bullet_impact_wood_heavy_03', 'bullet_impact_wood_heavy_04'), 0.88), 0.0, 0.0),
        (layer(pick(v, 'footstep_wood_land_v2_05', 'footstep_wood_land_v2_02', 'footstep_wood_land_v2_01'), 0.85), 0.01, -3.0),
        (layer(pick(v, 'bullet_impact_glass_01', 'bullet_impact_glass_02', 'bullet_impact_glass_04'), 0.9), 0.05, -6.0),
        (broken_chime(pick(v, 392.0, 349.2, 440.0), 1000 + 10 * v), 0.08, -3.0),
        (layer(pick(v, 'metal_rattle_spin_small_03', 'metal_rattle_spin_small_04', 'metal_rattle_spin_small_02'), 1.25, hp=700.0, release=0.4), 0.2, -11.0),
        (layer(pick(v, 'bullet_impact_wood_light_03', 'bullet_impact_wood_light_01', 'bullet_impact_wood_light_08'), 0.82), pick(v, 0.17, 0.2, 0.15), -4.0),
        *scatter(snaps, 3, 0.25, 0.7, 1010 + v, pitch=(0.55, 0.85), gain=(-11.0, -8.0), lp=7000.0),
    ]


RECIPES = dict(WoodSmall=wood_small, WoodCabinet=wood_cabinet, Upholstery=upholstery, IronBed=iron_bed, Bathtub=bathtub,
               Radiator=radiator, TiledStove=tiled_stove, Washstand=washstand, Piano=piano, Clock=clock)


def master(seconds, layers):
    seconds = min(seconds, MAX_SECONDS)
    out = fa.mix(seconds, layers)
    out = fa.highpass(out, 60.0)
    out = fa.bell(out, 2600.0, 2.0, 2.0)      # presence: this is what still reads under engines and guns
    out = fa.fade(out, 0.001, 0.12)
    return fa.normalise(out, TARGET_LUFS, CEILING_DB)


def energy_db(signal):
    """K-weighted energy of the whole one-shot re 1 s (a sound-exposure level: does not depend on how it is cut)."""
    weighted = fa.k_weight(signal)
    return round(-0.691 + 10.0 * np.log10(max(float((weighted * weighted).sum() / RATE), 1e-12)), 1)


def ffmpeg_ebur128(path):
    """ffmpeg's own meter, max momentary (a cross-check of fa.loudness)."""
    text = subprocess.run(['ffmpeg', '-nostats', '-i', path, '-af', 'apad=pad_dur=0.5,ebur128=framelog=info', '-f', 'null', '-'],
                          capture_output=True, text=True).stderr
    values = [float(line.split('M:')[1].split()[0]) for line in text.splitlines() if ' M:' in line and 'S:' in line]
    return round(max(values), 1) if values else None


def spectrogram(wav, png):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav, '-lavfi', 'showspectrumpic=s=640x320:legend=0:scale=log:fscale=log',
                    png], capture_output=True)


def reference(stats):
    """The cues these sounds are measured against, at their in-game level (wave x cue volume x modulator top)."""
    refs = {
        'Cue_FarmCrush_Wood': dict(waves=[f'bullet_impact_wood_heavy_0{i}' for i in range(1, 9)], volume=0.75, mod_top=1.0, mod_mid=0.925, pitch=0.81),
        'Cue_Collapse_Tree': dict(waves=[f'S_Tree_Destruction_{i}' for i in (1, 2, 3)], volume=0.75, mod_top=1.0, mod_mid=1.0, pitch=1.0),
        'Cue_SmallStoneObj_Collapse': dict(waves=[f'S_SmallStoneObj_Collapse_{i}' for i in (1, 2, 3, 4)], volume=0.75, mod_top=1.0, mod_mid=1.0, pitch=1.0),
        'Cue_MetalObj_Destruction': dict(waves=[f'S_MetalObj_Destruction_{i}' for i in (1, 2, 3)], volume=0.75, mod_top=1.0, mod_mid=1.0, pitch=1.0),
    }
    out = {}
    for name, ref in refs.items():
        rows = []
        for wave_id in ref['waves']:
            signal = fa.load(SRC + wave_id + '.wav')
            if ref['pitch'] != 1.0:
                signal = fa.resample(signal, ref['pitch'])
            rows.append((fa.loudness(signal), energy_db(signal), fa.db(np.abs(signal).max()), len(signal) / RATE))
        typical = fa.db(ref['volume'] * ref['mod_mid'])
        top = fa.db(ref['volume'] * ref['mod_top'])
        out[name] = dict(wave_lufs_m=round(float(np.mean([r[0] for r in rows])), 1),
                         in_game_lufs_m=round(float(np.mean([r[0] for r in rows])) + typical, 1),
                         in_game_lufs_m_loudest=round(max(r[0] for r in rows) + top, 1),
                         in_game_energy_db=round(float(np.mean([r[1] for r in rows])) + typical, 1),
                         in_game_peak_db=round(max(r[2] for r in rows) + top, 1),
                         seconds=[round(r[3], 2) for r in rows])
    stats['reference'] = out
    # one audible reference for the preview folder, at wave level
    for wave_id in ('bullet_impact_wood_heavy_04', 'S_Tree_Destruction_3'):
        fa.save(PREVIEW + 'reference_' + wave_id + '.wav', fa.load(SRC + wave_id + '.wav'))


def run():
    for folder in (WAVES, PREVIEW):
        os.makedirs(folder, exist_ok=True)
    stats = {'target_lufs_m': TARGET_LUFS, 'cue_volume': CUE_VOLUME, 'modulator': MODULATOR, 'waves': {}, 'sources': {}}
    built = {}
    for cue in CUES:
        for v in range(cue['variations']):
            _used.clear()
            seconds, layers = RECIPES[cue['key']](v)
            signal = master(seconds, layers)
            name = wave_name(cue['key'], v)
            fa.save(WAVES + name + '.wav', signal)
            shutil.copyfile(WAVES + name + '.wav', PREVIEW + name + '.wav')
            spectrogram(WAVES + name + '.wav', PREVIEW + name + '.png')
            info = fa.stats(signal)
            info['energy_db'] = energy_db(signal)
            info['ebur128_m'] = ffmpeg_ebur128(WAVES + name + '.wav')
            info['in_game_lufs_m'] = round(info['lufs_m'] + fa.db(CUE_VOLUME * (MODULATOR['volume'][0] + MODULATOR['volume'][1]) / 2.0), 1)
            info['in_game_lufs_m_loudest'] = round(info['lufs_m'] + fa.db(CUE_VOLUME * MODULATOR['volume'][1]), 1)
            info['in_game_energy_db'] = round(info['energy_db'] + fa.db(CUE_VOLUME * (MODULATOR['volume'][0] + MODULATOR['volume'][1]) / 2.0), 1)
            info['seconds_at_lowest_pitch'] = round(info['seconds'] / MODULATOR['pitch'][0], 2)
            info['sources'] = sorted(_used)
            stats['waves'][name] = info
            built[name] = signal
            print(name, {k: info[k] for k in ('seconds', 'peak_db', 'lufs_m', 'ebur128_m', 'energy_db', 'centroid_hz', 'active_s')})
        stats['sources'][cue['key']] = sorted({s for v in range(cue['variations']) for s in stats['waves'][wave_name(cue['key'], v)]['sources']})
    reference(stats)

    # previews: at in-game cue level (x CUE_VOLUME), so they compare honestly with each other
    gap = np.zeros(int(0.7 * RATE))
    row = [np.concatenate([built[wave_name(cue['key'], 0)] * CUE_VOLUME, gap]) for cue in CUES]
    fa.save(PREVIEW + 'all_in_a_row.wav', np.concatenate(row))
    stats['all_in_a_row_order'] = [cue['key'] for cue in CUES]
    everything = [np.concatenate([built[wave_name(cue['key'], v)] * CUE_VOLUME, gap * (2 if v == cue['variations'] - 1 else 1)])
                  for cue in CUES for v in range(cue['variations'])]
    fa.save(PREVIEW + 'all_variations_in_a_row.wav', np.concatenate(everything))
    # ten chairs under a tank column: one every 0.45 s, random variation (no immediate repeat), cue pitch / volume rolls
    rng = np.random.default_rng(7)
    layers, last = [], -1
    for index in range(10):
        v = int(rng.choice([i for i in range(3) if i != last]))
        last = v
        hit = fa.resample(built[wave_name('WoodSmall', v)], float(rng.uniform(*MODULATOR['pitch'])))
        layers.append((hit * CUE_VOLUME * float(rng.uniform(*MODULATOR['volume'])), 0.3 + 0.45 * index, 0.0))
    chairs = fa.mix(0.3 + 0.45 * 10 + 1.5, layers)
    fa.save(PREVIEW + 'row_of_chairs_simulation.wav', chairs)
    stats['row_of_chairs'] = dict(peak_db=round(fa.db(np.abs(chairs).max()), 1), lufs_m=round(fa.loudness(chairs), 1),
                                  single_in_game_lufs_m=stats['waves'][wave_name('WoodSmall', 0)]['in_game_lufs_m'])
    # worst case of 400 random cue rolls against full scale (pitch does not change the peak; volume does)
    stats['loudest_roll_peak_db'] = round(max(i['peak_db'] for i in stats['waves'].values()) + fa.db(CUE_VOLUME * MODULATOR['volume'][1]), 1)
    with open(ROOT + 'build_stats.json', 'w') as handle:
        json.dump(stats, handle, indent=1)
    print(json.dumps(stats['reference'], indent=1))
    print('row of chairs', stats['row_of_chairs'], 'loudest roll peak', stats['loudest_roll_peak_db'])


if __name__ == '__main__':
    run()
