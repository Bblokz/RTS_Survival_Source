"""Procedural sounds for the Forge survey station (BerlinForgeStation.md, option C "Sedierung").

Writes 16-bit mono 48 kHz WAVs to D:/UE5Projects/RTS_Survival/Saved/BerlinForge/sounds:
    S_ForgeStation_CryoVent.wav        6.5 s pressurised frost vent: hiss with a hard attack, gas flutter, slow tail
    S_ForgeStation_CompressorRise.wav  3.0 s compressor spinning up before the vent: rising motor tone + rattle
    S_ForgeStation_PumpLoop.wav        4.0 s seamless loop: two-stroke cryo pump thump under a low hum
Deterministic (fixed seeds). numpy only.

Run:  python forge_sounds_gen.py
"""
import os
import wave

import numpy as np

OUT = "D:/UE5Projects/RTS_Survival/Saved/BerlinForge/sounds"
RATE = 48000


def one_pole_lowpass(signal, cutoff_hz):
    alpha = 1.0 - np.exp(-2.0 * np.pi * cutoff_hz / RATE)
    out = np.empty_like(signal)
    acc = 0.0
    for i, x in enumerate(signal):
        acc += alpha * (x - acc)
        out[i] = acc
    return out


def highpass(signal, cutoff_hz):
    return signal - one_pole_lowpass(signal, cutoff_hz)


def bandpass(signal, low_hz, high_hz):
    return one_pole_lowpass(highpass(signal, low_hz), high_hz)


def normalise(signal, peak=0.85):
    return signal * (peak / max(1e-9, np.max(np.abs(signal))))


def write_wav(name, signal):
    os.makedirs(OUT, exist_ok=True)
    data = (np.clip(signal, -1.0, 1.0) * 32767.0).astype(np.int16)
    path = os.path.join(OUT, name)
    with wave.open(path, "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        handle.writeframes(data.tobytes())
    print("wrote", path, "%.2f s" % (len(signal) / RATE))


def envelope(times, attack, hold, release):
    env = np.ones_like(times)
    env = np.where(times < attack, times / attack, env)
    tail_start = attack + hold
    env = np.where(times > tail_start, np.exp(-(times - tail_start) / release), env)
    return env


def cryo_vent():
    rng = np.random.default_rng(7)
    seconds = 6.5
    t = np.arange(int(seconds * RATE)) / RATE
    noise = rng.standard_normal(len(t))
    hiss = bandpass(noise, 1800.0, 9000.0)
    body = bandpass(noise, 300.0, 1600.0) * 0.45
    # gas flutter: slow random amplitude wobble
    flutter = 1.0 + 0.18 * one_pole_lowpass(rng.standard_normal(len(t)), 9.0) * 25.0
    env = envelope(t, 0.06, 3.2, 1.1)
    # the valve shuts: a short dip and a softer second puff
    env *= 1.0 - 0.35 * np.exp(-((t - 4.9) ** 2) / 0.01)
    write_wav("S_ForgeStation_CryoVent.wav", normalise((hiss + body) * flutter * env))


def compressor_rise():
    rng = np.random.default_rng(11)
    seconds = 3.0
    t = np.arange(int(seconds * RATE)) / RATE
    freq = 38.0 + 52.0 * (t / seconds) ** 1.6
    phase = 2.0 * np.pi * np.cumsum(freq) / RATE
    motor = np.sin(phase) + 0.5 * np.sin(2.0 * phase) + 0.25 * np.sign(np.sin(3.0 * phase)) * 0.4
    rattle = bandpass(rng.standard_normal(len(t)), 500.0, 3000.0) * (0.15 + 0.35 * t / seconds)
    rattle *= 0.5 + 0.5 * np.sign(np.sin(phase * 0.5))
    env = envelope(t, 0.4, 2.2, 0.25)
    write_wav("S_ForgeStation_CompressorRise.wav", normalise((motor * 0.6 + rattle) * env, 0.8))


def pump_loop():
    rng = np.random.default_rng(3)
    seconds = 4.0
    t = np.arange(int(seconds * RATE)) / RATE
    hum = 0.25 * np.sin(2.0 * np.pi * 50.0 * t) + 0.12 * np.sin(2.0 * np.pi * 100.0 * t)
    thumps = np.zeros_like(t)
    # two strokes per second, the second a little weaker; whole numbers of periods keep the loop seamless
    for start, gain in [(k * 0.5, 1.0 if k % 2 == 0 else 0.7) for k in range(8)]:
        local = t - start
        mask = local >= 0.0
        thumps += np.where(mask, gain * np.sin(2.0 * np.pi * 42.0 * local) * np.exp(-local / 0.07), 0.0)
    clank = bandpass(rng.standard_normal(len(t)), 900.0, 4000.0)
    clank *= sum(np.exp(-np.clip(t - (k * 0.5 + 0.03), 0, None) / 0.02) * (t >= k * 0.5 + 0.03) for k in range(8))
    write_wav("S_ForgeStation_PumpLoop.wav", normalise(hum + thumps + 0.12 * clank, 0.7))


if __name__ == "__main__":
    cryo_vent()
    compressor_rise()
    pump_loop()
