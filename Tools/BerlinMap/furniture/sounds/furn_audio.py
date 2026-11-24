"""Small offline audio helpers for the Berlin furniture crush sounds (numpy + ffmpeg only, no scipy).

All signals are mono float64 at RATE. Filters are zero-phase FFT filters (fine for one-shots this short).
"""
import os
import subprocess
import wave

import numpy as np

RATE = 48000


def load(path, rate=RATE):
    """Any audio file -> mono float64 at `rate`. Channels are averaged (ffmpeg -ac 1 adds 3 dB on dual mono)."""
    probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0', '-show_entries', 'stream=channels',
                            '-of', 'csv=p=0', path], capture_output=True, text=True).stdout.strip()
    channels = int(probe.split(',')[0]) if probe else 1
    pan = 'pan=mono|c0=' + '+'.join(f'{1.0 / channels:.6f}*c{i}' for i in range(channels))
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-af', pan, '-ar', str(rate), '-f', 'f64le', '-'],
                         capture_output=True).stdout
    return np.frombuffer(raw, dtype='<f8').copy()


def save(path, signal, rate=RATE):
    """Mono 16-bit PCM WAV with TPDF dither."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    rng = np.random.default_rng(len(signal))
    dither = (rng.random(len(signal)) - rng.random(len(signal))) / 32768.0
    data = np.clip(signal + dither, -1.0, 1.0)
    pcm = np.round(data * 32767.0).astype('<i2')
    with wave.open(path, 'wb') as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(rate)
        handle.writeframes(pcm.tobytes())


def db(value):
    return 20.0 * np.log10(max(float(value), 1e-12))


def gain(decibel):
    return 10.0 ** (decibel / 20.0)


def _spectrum(signal):
    size = 1 << int(np.ceil(np.log2(len(signal) + RATE // 4)))
    return np.fft.rfft(signal, size), np.fft.rfftfreq(size, 1.0 / RATE), size


def shape(signal, curve):
    """Zero-phase filter: `curve(freqs)` returns the linear gain per frequency."""
    spec, freqs, size = _spectrum(signal)
    return np.fft.irfft(spec * curve(freqs), size)[:len(signal)]


def highpass(signal, cutoff, order=2):
    return shape(signal, lambda f: 1.0 / np.sqrt(1.0 + (cutoff / np.maximum(f, 1e-3)) ** (2 * order)))


def lowpass(signal, cutoff, order=2):
    return shape(signal, lambda f: 1.0 / np.sqrt(1.0 + (f / cutoff) ** (2 * order)))


def bell(signal, centre, decibel, octaves=1.0):
    """Peaking EQ: gaussian bump of `decibel` at `centre`, `octaves` wide (log frequency)."""
    def curve(f):
        distance = np.log2(np.maximum(f, 1e-3) / centre) / (octaves / 2.0)
        return 10.0 ** (decibel * np.exp(-0.5 * distance * distance) / 20.0)
    return shape(signal, curve)


def shelf(signal, corner, decibel, high=True):
    def curve(f):
        x = np.log2(np.maximum(f, 1e-3) / corner)
        step = 1.0 / (1.0 + np.exp(-4.0 * x))
        return 10.0 ** (decibel * (step if high else 1.0 - step) / 20.0)
    return shape(signal, curve)


def resample(signal, ratio):
    """Pitch as resample (what a Sound Cue Modulator does): ratio 2.0 = an octave up and half as long."""
    count = int(len(signal) / ratio)
    position = np.arange(count, dtype=np.float64) * ratio
    return np.interp(position, np.arange(len(signal), dtype=np.float64), signal)


def fade(signal, attack=0.002, release=0.05):
    out = signal.copy()
    a, r = int(attack * RATE), int(release * RATE)
    if a > 0:
        out[:a] *= np.linspace(0.0, 1.0, a)
    if r > 0:
        out[-r:] *= np.cos(np.linspace(0.0, np.pi / 2.0, r)) ** 2
    return out


def cut(signal, start, length, attack=0.002, release=0.05):
    a = int(start * RATE)
    return fade(signal[a:a + int(length * RATE)], attack, release)


def onset(signal, threshold_db=-30.0):
    """Seconds of the first sample within threshold_db of the peak."""
    level = np.abs(signal)
    hits = np.nonzero(level > level.max() * gain(threshold_db))[0]
    return hits[0] / RATE if len(hits) else 0.0


def trim_to_onset(signal, pre=0.003):
    return signal[max(0, int((onset(signal) - pre) * RATE)):]


def decay(signal, seconds):
    """Exponential decay multiplier reaching -60 dB at `seconds`."""
    t = np.arange(len(signal)) / RATE
    return signal * np.exp(-6.908 * t / seconds)


def mix(length, layers):
    """layers: (signal, start_seconds, gain_db). Returns the sum, `length` seconds long."""
    out = np.zeros(int(length * RATE))
    for signal, start, decibel in layers:
        a = int(start * RATE)
        if a >= len(out):
            continue
        part = signal[:len(out) - a]
        out[a:a + len(part)] += part * gain(decibel)
    return out


def soft_clip(signal, ceiling=0.89):
    """Tanh knee above 70 % of the ceiling; leaves quiet material alone."""
    knee = 0.7 * ceiling
    out = signal.copy()
    over = np.abs(signal) > knee
    out[over] = np.sign(signal[over]) * (knee + (ceiling - knee) * np.tanh((np.abs(signal[over]) - knee) / (ceiling - knee)))
    return out


def k_weight(signal):
    """ITU-R BS.1770 K-weighting as a magnitude curve (shelf +4 dB above 1.5 kHz, high-pass 38 Hz)."""
    def curve(f):
        f = np.maximum(f, 1e-3)
        shelf_gain = 10.0 ** (4.0 / (1.0 + (1500.0 / f) ** 2) / 20.0)
        high = (f / 38.0) ** 2 / np.sqrt(1.0 + (f / 38.0) ** 4)
        return shelf_gain * high
    return shape(signal, curve)


def loudness(signal, window=0.4):
    """Max momentary loudness (LUFS-like, 400 ms window; shorter signals are zero-padded to the window)."""
    weighted = k_weight(signal)
    size = int(window * RATE)
    if len(weighted) < size:
        weighted = np.concatenate([weighted, np.zeros(size - len(weighted))])
    power = np.concatenate([[0.0], np.cumsum(weighted * weighted)])
    mean = (power[size:] - power[:-size]) / size
    return -0.691 + 10.0 * np.log10(max(float(mean.max()), 1e-12))


def stats(signal):
    peak = float(np.abs(signal).max())
    rms = float(np.sqrt(np.mean(signal * signal)))
    spec = np.abs(np.fft.rfft(signal)) ** 2
    freqs = np.fft.rfftfreq(len(signal), 1.0 / RATE)
    centroid = float((spec * freqs).sum() / max(spec.sum(), 1e-20))
    level = np.abs(signal)
    above = np.nonzero(level > peak * gain(-40.0))[0]
    active = (above[-1] - above[0]) / RATE if len(above) else 0.0
    return dict(seconds=round(len(signal) / RATE, 2), peak_db=round(db(peak), 1), rms_db=round(db(rms), 1),
                lufs_m=round(loudness(signal), 1), centroid_hz=round(centroid), active_s=round(active, 2),
                onset_s=round(onset(signal), 3))


def normalise(signal, target_lufs, ceiling_db=-1.0):
    """Gain to the target max-momentary loudness; soft-clips whatever would pass the ceiling."""
    out = signal * gain(target_lufs - loudness(signal))
    ceiling = gain(ceiling_db)
    if np.abs(out).max() > 0.7 * ceiling:
        out = soft_clip(out, ceiling)
        out = out * gain(target_lufs - loudness(out))
        out = np.clip(out, -ceiling, ceiling)
    return out


def noise(seconds, seed):
    return np.random.default_rng(seed).standard_normal(int(seconds * RATE))
