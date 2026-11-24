"""Oder-Warthe direction-finding radio kit: synthesises every sound from scratch (no samples).

Outputs (48 kHz / 16-bit) into Source/Tools/OderWartheRadio/Wavs/:
  OW_Morse_<Type>_NN.wav       Morse bursts through a receiver (Type = Routine | Reply | AttackOrder)
  OW_Static_<Type>_NN.wav      static/crackle layer that plays with the burst of the same type
  OW_JammerBed_Loop.wav        seamless stereo jam loop (circular synthesis, every LFO whole-cycle)
  OW_StationLockOn_NN.wav      "aerial locks on" capture sting
Every wave is peak-normalised; the relative loudness lives in the cue volume (CUE_VOLUME below),
and the Drafts/ renders apply it so they sound as they will in game.

Usage: python ow_radio_synth.py            (writes Wavs/ and Drafts/)
"""
import json
import os
import numpy as np
import soundfile as sf
from scipy import signal

SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
WAV_DIR = os.path.join(HERE, "Wavs")
DRAFT_DIR = os.path.join(HERE, "Drafts")
PEAK_DBFS = -1.0
# Lead silence inside each Morse wave so the static layer's squelch-open crackle lands first.
MORSE_LEAD_S = 0.28
MORSE_TAIL_S = 0.30

# Relative loudness, applied as the SoundCue volume multiplier in Unreal (waves stay peak-normalised).
CUE_VOLUME = {
    "Burst_Routine": 0.30,
    "Burst_Reply": 0.48,
    "Burst_AttackOrder": 0.66,
    "Static_Routine": 0.24,
    "Static_Reply": 0.32,
    "Static_AttackOrder": 0.38,
    "JammerBed": 0.16,
    "StationLockOn": 0.70,
}

MORSE = {
    "A": ".-", "B": "-...", "C": "-.-.", "D": "-..", "E": ".", "F": "..-.", "G": "--.", "H": "....",
    "I": "..", "J": ".---", "K": "-.-", "L": ".-..", "M": "--", "N": "-.", "O": "---", "P": ".--.",
    "Q": "--.-", "R": ".-.", "S": "...", "T": "-", "U": "..-", "V": "...-", "W": ".--", "X": "-..-",
    "Y": "-.--", "Z": "--..", "0": "-----", "1": ".----", "2": "..---", "3": "...--", "4": "....-",
    "5": ".....", "6": "-....", "7": "--...", "8": "---..", "9": "----.",
}

# Per burst type: words-per-minute, carrier tone, hand-key timing jitter, signal-to-noise,
# fading depth (QSB) and target length window of the keyed text.
BURST_TYPES = {
    "Routine": dict(wpm=17.0, tone=610.0, jitter=0.10, snr_db=9.0, qsb=0.45, drive=1.0,
                    target=(1.7, 2.4), seed=101, chirp=35.0),
    "Reply": dict(wpm=21.0, tone=690.0, jitter=0.07, snr_db=15.0, qsb=0.22, drive=1.4,
                  target=(3.4, 4.4), seed=202, chirp=25.0),
    "AttackOrder": dict(wpm=27.0, tone=790.0, jitter=0.015, snr_db=22.0, qsb=0.08, drive=2.4,
                        target=(6.2, 7.4), seed=303, chirp=18.0),
}
VARIANTS_PER_TYPE = 4


# ----------------------------------------------------------------------------------------- helpers
def time_axis(sample_count):
    # float64 on purpose: int32 overflow on Windows numpy once multiplied by the rate.
    return np.arange(sample_count, dtype=np.float64) / SR


def bandpass(x, low, high, order=4):
    sos = signal.butter(order, [low, high], btype="bandpass", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def lowpass(x, cutoff, order=4):
    sos = signal.butter(order, cutoff, btype="lowpass", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def highpass(x, cutoff, order=2):
    sos = signal.butter(order, cutoff, btype="highpass", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def circular_shape(x, gain_fn):
    """Frequency-domain filter: circular, so a loop stays seamless."""
    spectrum = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(len(x), 1.0 / SR)
    return np.fft.irfft(spectrum * gain_fn(freqs), n=len(x))


def pink(n, rng):
    white = rng.standard_normal(n)
    return circular_shape(white, lambda f: 1.0 / np.sqrt(np.maximum(f, 20.0)))


def rms(x):
    return float(np.sqrt(np.mean(np.square(x)) + 1e-20))


def normalise_peak(x, peak_dbfs=PEAK_DBFS):
    peak = np.max(np.abs(x))
    if peak <= 0:
        return x
    return x * (10 ** (peak_dbfs / 20.0) / peak)


def fade(x, fade_in_s, fade_out_s):
    out = x.copy()
    n_in = int(fade_in_s * SR)
    n_out = int(fade_out_s * SR)
    if n_in > 0:
        out[:n_in] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, n_in))
    if n_out > 0:
        out[-n_out:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, n_out))
    return out


def write(name, x, folder=WAV_DIR):
    os.makedirs(folder, exist_ok=True)
    data = np.clip(x, -1.0, 1.0)
    sf.write(os.path.join(folder, name + ".wav"), data, SR, subtype="PCM_16")


def crackle_track(n, rng, rate_hz, amp_spread, base_amp, band=(900.0, 5200.0)):
    """Sparse pops: Poisson impulses with heavy-tailed sizes, each a tiny ringing noise burst."""
    out = np.zeros(n)
    count = rng.poisson(rate_hz * n / SR)
    positions = rng.integers(0, n, size=count)
    for position in positions:
        length = int(rng.uniform(0.0008, 0.006) * SR)
        if position + length >= n:
            continue
        size = base_amp * (rng.pareto(amp_spread) + 1.0)
        burst = rng.standard_normal(length) * np.exp(-np.linspace(0, rng.uniform(3, 9), length))
        out[position:position + length] += burst * min(size, 6.0 * base_amp)
    return bandpass(out, band[0], band[1], order=2)


# ------------------------------------------------------------------------------------------ morse
def text_units(text):
    units = 0
    for word_index, word in enumerate(text.split(" ")):
        if word_index:
            units += 7
        for char_index, char in enumerate(word):
            if char_index:
                units += 3
            code = MORSE[char]
            units += sum(1 if s == "." else 3 for s in code) + (len(code) - 1)
    return units


def random_callsign(rng):
    letters = "ABDFGKLMNPRSUVWXZ"
    return "".join(rng.choice(list(letters), 2)) + str(rng.integers(1, 10))


def random_group(rng, length):
    if rng.random() < 0.6:
        return "".join(str(d) for d in rng.integers(0, 10, length))
    return "".join(rng.choice(list("ABDEGIKLMNORSTUW"), length))


def compose_burst_text(burst_type, rng, unit_s, target):
    """Soviet-net style traffic: routine = a callsign check, reply = callsign + short group,
    attack order = attention VVV, callsign, cipher groups, end-of-message AR."""
    for _ in range(500):
        call = random_callsign(rng)
        if burst_type == "Routine":
            text = call + (" K" if rng.random() < 0.5 else "")
        elif burst_type == "Reply":
            text = f"{call} {random_group(rng, int(rng.integers(3, 5)))} K"
        else:
            groups = " ".join(random_group(rng, int(rng.integers(3, 6))) for _ in range(int(rng.integers(2, 4))))
            text = f"VVV {call} {groups} AR"
        duration = text_units(text) * unit_s
        if target[0] <= duration <= target[1]:
            return text, duration
    raise RuntimeError(f"no {burst_type} text fits {target}")


def key_envelope(text, unit_s, jitter, rng):
    """Returns (envelope, key_down_onsets) with 5 ms raised-cosine edges (no key clicks)."""
    segments = []  # (is_on, seconds)

    def add(on, units):
        stretch = 1.0 + rng.normal(0.0, jitter)
        segments.append((on, max(0.25, units * stretch) * unit_s))

    for word_index, word in enumerate(text.split(" ")):
        if word_index:
            add(False, 7)
        for char_index, char in enumerate(word):
            if char_index:
                add(False, 3)
            for symbol_index, symbol in enumerate(MORSE[char]):
                if symbol_index:
                    add(False, 1)
                add(True, 1 if symbol == "." else 3)
    total = int(sum(s for _, s in segments) * SR) + 1
    gate = np.zeros(total)
    onsets = []
    cursor = 0.0
    for on, seconds in segments:
        start = int(cursor * SR)
        end = int((cursor + seconds) * SR)
        if on:
            gate[start:end] = 1.0
            onsets.append(start)
        cursor += seconds
    edge = int(0.005 * SR)
    kernel = np.hanning(2 * edge + 1)
    kernel /= kernel.sum()
    return np.convolve(gate, kernel, mode="same"), onsets


def synth_morse(burst_type, variant, params):
    rng = np.random.default_rng(params["seed"] + variant * 17)
    unit_s = 1.2 / params["wpm"]
    text, _ = compose_burst_text(burst_type, rng, unit_s, params["target"])
    envelope, onsets = key_envelope(text, unit_s, params["jitter"], rng)
    lead = int(MORSE_LEAD_S * SR)
    tail = int(MORSE_TAIL_S * SR)
    envelope = np.concatenate([np.zeros(lead), envelope, np.zeros(tail)])
    n = len(envelope)
    t = time_axis(n)

    # Carrier: per-variant pitch, slow drift, and a transmitter chirp that settles after each key-down.
    tone = params["tone"] * rng.uniform(0.96, 1.04)
    freq = tone + 6.0 * np.sin(2 * np.pi * rng.uniform(0.15, 0.35) * t + rng.uniform(0, 6.28))
    chirp = np.zeros(n)
    decay = np.exp(-np.arange(int(0.03 * SR)) / (0.008 * SR))
    for onset in onsets:
        start = onset + lead
        stop = min(n, start + len(decay))
        chirp[start:stop] = np.maximum(chirp[start:stop], decay[:stop - start])
    freq = freq + params["chirp"] * chirp
    phase = 2 * np.pi * np.cumsum(freq) / SR
    carrier = np.sin(phase) + 0.12 * np.sin(2 * phase) + 0.05 * np.sin(3 * phase)
    keyed = carrier * envelope

    # QSB fading (ionospheric) — strong on the distant routine traffic, almost none on the attack order.
    qsb_rate = rng.uniform(0.35, 0.8)
    qsb = 1.0 - params["qsb"] * (0.5 + 0.5 * np.sin(2 * np.pi * qsb_rate * t + rng.uniform(0, 6.28)))
    keyed *= qsb

    # Receiver: tight CW filter on the tone, then hiss at the type's signal-to-noise ratio.
    keyed = bandpass(keyed, tone * 0.55, tone * 2.8, order=2)
    hiss = bandpass(pink(n, rng), 300.0, 2900.0)
    on_rms = rms(keyed[envelope > 0.5])
    hiss *= on_rms / rms(hiss) * 10 ** (-params["snr_db"] / 20.0)
    hiss_gate = fade(np.ones(n), 0.12, 0.25)
    mix = keyed + hiss * hiss_gate

    # Drive: the attack order runs hot (soft clip = urgent, overdriven transmitter).
    mix = np.tanh(params["drive"] * mix / np.max(np.abs(mix))) / np.tanh(params["drive"])
    mix = bandpass(mix, 280.0, 3300.0, order=2)
    return normalise_peak(fade(mix, 0.01, 0.06)), text


# ----------------------------------------------------------------------------------------- static
STATIC_TYPES = {
    # length covers the longest Morse wave of that type; crackle density and noise level rise with urgency
    "Routine": dict(length=None, rate=14.0, frying=0.05, hiss=0.18, seed=411),
    "Reply": dict(length=None, rate=22.0, frying=0.08, hiss=0.22, seed=522),
    "AttackOrder": dict(length=None, rate=34.0, frying=0.11, hiss=0.26, seed=633),
}
STATIC_VARIANTS = 3


def squelch_burst(rng, seconds, colour_band):
    n = int(seconds * SR)
    noise = bandpass(rng.standard_normal(n), colour_band[0], colour_band[1], order=2)
    shape = np.exp(-np.linspace(0, rng.uniform(4, 7), n))
    shape[: int(0.004 * SR)] *= np.linspace(0, 1, int(0.004 * SR))
    return noise * shape


def synth_static(burst_type, variant, params, length_s):
    rng = np.random.default_rng(params["seed"] + variant * 31)
    n = int(length_s * SR)
    t = time_axis(n)
    # Hiss with a wobbling "fry" texture, swelling in as the burst parts the jam.
    hiss = bandpass(pink(n, rng), 250.0, 4200.0)
    hiss /= rms(hiss)
    wobble = 1.0 + 0.25 * np.sin(2 * np.pi * rng.uniform(2.0, 5.0) * t) * np.sin(2 * np.pi * 0.31 * t)
    body = params["hiss"] * hiss * wobble
    fry = crackle_track(n, rng, rate_hz=900.0, amp_spread=3.5, base_amp=params["frying"])
    pops = crackle_track(n, rng, rate_hz=params["rate"], amp_spread=1.6, base_amp=0.55)
    layer = body + fry + pops
    # Squelch open (crackle cluster + noise swell) and squelch close ("kssh") at the window edges.
    open_burst = squelch_burst(rng, 0.22, (700.0, 4600.0)) * 1.6
    open_burst += crackle_track(len(open_burst), rng, rate_hz=140.0, amp_spread=1.4, base_amp=0.7)
    layer[: len(open_burst)] += open_burst
    close_burst = squelch_burst(rng, 0.18, (900.0, 5200.0)) * 1.2
    close_start = n - len(close_burst) - int(0.05 * SR)
    layer[close_start: close_start + len(close_burst)] += close_burst
    layer = highpass(layer, 180.0)
    return normalise_peak(fade(layer, 0.006, 0.05))


# ------------------------------------------------------------------------------------- jammer bed
JAM_LOOP_S = 32.0


def loop_lfo(t, cycles_per_loop, phase=0.0):
    """Sine LFO with a whole number of cycles over the loop, so the seam is invisible."""
    return np.sin(2 * np.pi * cycles_per_loop / JAM_LOOP_S * t + phase)


def synth_jammer_channel(rng, t, phase_offset):
    n = len(t)
    # 1) The warble: a buzzing pulse carrier whose pitch sweeps slowly and trills quickly
    #    (the classic bubbling Soviet jammer), heard through a soft receiver low-pass.
    base = 148.0 + 22.0 * loop_lfo(t, 3, phase_offset) + 9.0 * loop_lfo(t, 7, 1.3 + phase_offset)
    trill = 1.0 + 0.055 * loop_lfo(t, 176, phase_offset)  # 5.5 Hz trill
    freq = base * trill
    # Whole-loop phase must wrap to a multiple of 2*pi: rescale the instantaneous frequency.
    total_cycles = np.sum(freq) / SR
    freq *= np.round(total_cycles) / total_cycles
    phase = 2 * np.pi * np.cumsum(freq) / SR
    buzz = np.tanh(3.0 * np.sin(phase)) * 0.8 + 0.3 * np.sin(2 * phase + 0.4)
    buzz *= 0.55 + 0.45 * (0.5 + 0.5 * loop_lfo(t, 2, phase_offset * 0.5))
    buzz = circular_shape(buzz, lambda f: 1.0 / (1.0 + (f / 900.0) ** 4) * (f > 60))

    # 2) The noise roar under it: pink noise, dark, slowly breathing.
    roar = pink(n, rng)
    roar = circular_shape(roar, lambda f: (f > 80) / (1.0 + (f / 1400.0) ** 3))
    roar /= rms(roar)
    roar *= 0.8 + 0.2 * loop_lfo(t, 5, phase_offset)

    # 3) A faint woodpecker tap (10 Hz pulsed over-the-horizon signal), fading in and out
    #    across the loop so it is a colour, not a metronome.
    tap_rate = 320.0 / JAM_LOOP_S  # exactly 10 Hz, whole number per loop
    tap_phase = (t * tap_rate) % 1.0
    taps = np.exp(-tap_phase / 0.012) * rng.standard_normal(n)
    taps = circular_shape(taps, lambda f: np.exp(-((f - 650.0) / 380.0) ** 2))
    taps /= rms(taps) + 1e-9
    taps *= np.clip(loop_lfo(t, 1, 1.8 + phase_offset), 0.0, 1.0) ** 2

    buzz /= rms(buzz)
    mix = 0.55 * buzz + 0.55 * roar + 0.16 * taps
    # Tame the 2-4 kHz fatigue band.
    return circular_shape(mix, lambda f: 1.0 / (1.0 + (f / 2200.0) ** 6))


def synth_jammer():
    rng = np.random.default_rng(777)
    n = int(JAM_LOOP_S * SR)
    t = time_axis(n)
    left = synth_jammer_channel(rng, t, 0.0)
    right = synth_jammer_channel(rng, t, 0.9)
    mid = 0.5 * (left + right)
    side = 0.5 * (left - right)
    stereo = np.stack([mid + 0.6 * side, mid - 0.6 * side], axis=1)
    return normalise_peak(stereo, -3.0)


# ----------------------------------------------------------------------------------- lock-on sting
LOCK_VARIANTS = 3


def synth_lock_on(variant):
    """DF receiver tunes in: a heterodyne whistle slides to zero beat through clearing static,
    a relay clunks, then a two-tone 'locked' pip."""
    rng = np.random.default_rng(900 + variant * 13)
    length = 1.9
    n = int(length * SR)
    t = time_axis(n)
    out = np.zeros(n)

    # Heterodyne slide (0.0-0.85 s): pitch glides down to the lock pitch with a small overshoot.
    slide_end = 0.85
    start_hz = rng.uniform(1900.0, 2400.0)
    lock_hz = rng.uniform(720.0, 800.0)
    slide_mask = t < slide_end
    progress = np.clip(t / slide_end, 0, 1)
    eased = 1 - (1 - progress) ** 3
    overshoot = -0.06 * np.sin(np.pi * np.clip((t - 0.62) / 0.23, 0, 1))
    whistle_hz = start_hz + (lock_hz - start_hz) * eased + lock_hz * overshoot
    whistle = np.sin(2 * np.pi * np.cumsum(whistle_hz) / SR)
    whistle_env = np.clip(t / 0.08, 0, 1) * np.where(slide_mask, 1.0, np.exp(-(t - slide_end) / 0.02))
    out += 0.35 * whistle * whistle_env * (0.8 + 0.2 * np.sin(2 * np.pi * 9.0 * t))

    # Static that clears as the receiver locks.
    static = bandpass(rng.standard_normal(n), 400.0, 4500.0, order=2)
    static += crackle_track(n, rng, rate_hz=60.0, amp_spread=1.6, base_amp=1.2)
    static_env = np.clip(1.0 - t / slide_end, 0, 1) ** 1.5
    out += 0.25 * static * static_env

    # Relay clunk at the lock.
    clunk_at = int(slide_end * SR)
    clunk_n = int(0.09 * SR)
    clunk_t = time_axis(clunk_n)
    clunk = (np.sin(2 * np.pi * 95.0 * clunk_t) * np.exp(-clunk_t / 0.018)
             + 0.6 * bandpass(rng.standard_normal(clunk_n), 1500.0, 6000.0, order=2) * np.exp(-clunk_t / 0.004))
    out[clunk_at:clunk_at + clunk_n] += 0.9 * clunk

    # Two-tone 'locked' pip: lock pitch then a fifth above, clean (the signal is now clear).
    for index, (start, hz, dur) in enumerate([(0.93, lock_hz, 0.13), (1.10, lock_hz * 1.5, 0.30)]):
        pip_n = int(dur * SR)
        pip_t = time_axis(pip_n)
        env = np.minimum(1.0, pip_t / 0.006) * np.exp(-pip_t / (0.09 if index == 0 else 0.16))
        pip = (np.sin(2 * np.pi * hz * pip_t) + 0.2 * np.sin(4 * np.pi * hz * pip_t)) * env
        s = int(start * SR)
        out[s:s + pip_n] += 0.55 * pip

    out = bandpass(out, 90.0, 6500.0, order=2)
    return normalise_peak(fade(out, 0.004, 0.08))


# --------------------------------------------------------------------------------------- drafts
def place(track, clip, start_s, gain):
    start = int(start_s * SR)
    if clip.ndim == 1 and track.ndim == 2:
        clip = np.stack([clip, clip], axis=1)
    end = min(len(track), start + len(clip))
    track[start:end] += gain * clip[: end - start]


def main():
    os.makedirs(WAV_DIR, exist_ok=True)
    os.makedirs(DRAFT_DIR, exist_ok=True)
    manifest = {"morse": {}, "static": {}, "lock_on": [], "jammer": "OW_JammerBed_Loop", "cue_volume": CUE_VOLUME}
    morse_clips = {}
    for burst_type, params in BURST_TYPES.items():
        morse_clips[burst_type] = []
        for variant in range(1, VARIANTS_PER_TYPE + 1):
            wave, text = synth_morse(burst_type, variant, params)
            name = f"OW_Morse_{burst_type}_{variant:02d}"
            write(name, wave)
            morse_clips[burst_type].append(wave)
            manifest["morse"].setdefault(burst_type, []).append({"name": name, "text": text,
                                                                 "seconds": round(len(wave) / SR, 2)})
        longest = max(len(w) for w in morse_clips[burst_type]) / SR
        STATIC_TYPES[burst_type]["length"] = longest + 0.15
    static_clips = {}
    for burst_type, params in STATIC_TYPES.items():
        static_clips[burst_type] = []
        for variant in range(1, STATIC_VARIANTS + 1):
            wave = synth_static(burst_type, variant, params, params["length"])
            name = f"OW_Static_{burst_type}_{variant:02d}"
            write(name, wave)
            static_clips[burst_type].append(wave)
            manifest["static"].setdefault(burst_type, []).append(name)
    jam = synth_jammer()
    write("OW_JammerBed_Loop", jam)
    lock_clips = []
    for variant in range(1, LOCK_VARIANTS + 1):
        wave = synth_lock_on(variant)
        write(f"OW_StationLockOn_{variant:02d}", wave)
        lock_clips.append(wave)
        manifest["lock_on"].append(f"OW_StationLockOn_{variant:02d}")

    # Draft renders at in-game relative level (cue volume applied).
    for burst_type in BURST_TYPES:
        gap = np.zeros(int(0.7 * SR))
        parts = []
        for morse, static in zip(morse_clips[burst_type], static_clips[burst_type] * 2):
            mix = np.zeros(max(len(morse), len(static)))
            mix[: len(morse)] += CUE_VOLUME[f"Burst_{burst_type}"] * morse
            mix[: len(static)] += CUE_VOLUME[f"Static_{burst_type}"] * static
            parts += [mix, gap]
        write(f"Draft_Bursts_{burst_type}", np.concatenate(parts), DRAFT_DIR)
    write("Draft_JammerBed_Loop_x2", CUE_VOLUME["JammerBed"] * np.concatenate([jam, jam]), DRAFT_DIR)
    write("Draft_StationLockOn", np.concatenate(
        [np.concatenate([CUE_VOLUME["StationLockOn"] * c, np.zeros(int(0.5 * SR))]) for c in lock_clips]), DRAFT_DIR)

    # In-context reel: the jam bed with one of each burst and the capture sting on top.
    reel = np.zeros((int(48 * SR), 2))
    place(reel, np.concatenate([jam, jam]), 0.0, CUE_VOLUME["JammerBed"])
    events = [(4.0, "lock", 0), (9.0, "Routine", 0), (17.0, "Reply", 1), (26.0, "AttackOrder", 2),
              (38.0, "Routine", 2)]
    for start_s, kind, index in events:
        if kind == "lock":
            place(reel, lock_clips[index], start_s, CUE_VOLUME["StationLockOn"])
            continue
        place(reel, morse_clips[kind][index], start_s, CUE_VOLUME[f"Burst_{kind}"])
        place(reel, static_clips[kind][index % STATIC_VARIANTS], start_s, CUE_VOLUME[f"Static_{kind}"])
    write("Draft_InContext_Reel", reel, DRAFT_DIR)

    with open(os.path.join(HERE, "manifest.json"), "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2)
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
