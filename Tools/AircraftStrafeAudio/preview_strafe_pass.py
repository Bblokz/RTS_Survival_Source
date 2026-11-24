"""Offline preview of one strafe pass with the runtime rules of UAircraftGunAudioPlayer (listen-only, not imported).

Pass: the aircraft dives past the camera (60 m -> 14 m -> 45 m) firing two bursts; the zone sits ~28 m from the camera.
- fire loop on the aircraft while volleys keep coming, cut with a short fade + a fire tail at each burst end
- impact churn loop at the zone from the first arrival (tracer flight ~0.3 s), cut + impact tail after the last
- metal clusters at most every 0.12 s while rounds land on a unit (0.9-1.5 s into the pass)
- 20 mm: cannon fire loop + one impact one-shot per projectile (2 guns, 720 rpm each)
Volume follows the attenuation the importer sets (linear falloff: fire inner 30 m / falloff 120 m, impacts 25 m / 80 m).
"old_*" is today's sound for comparison: one Puma MG shot per volley (~0.1 s) plus one sand impact per 0.12 s.

40 random rolls of the pass are rendered and the worst peak is printed: it must stay under full scale.
"""
import os

import numpy as np

import build_aircraft_gun_sounds as B

L = B.L
SR = B.SR
FIRE_ATT = (3000.0, 12000.0)
IMPACT_ATT = (2500.0, 8000.0)
BURSTS = ((0.0, 1.30), (1.75, 2.90))
FLIGHT_S = 0.3
PASS_S = 4.6


def att(dist_cm, inner_falloff):
    inner, falloff = inner_falloff
    return float(np.clip(1.0 - max(0.0, dist_cm - inner) / falloff, 0.0, 1.0))


def aircraft_dist(t):
    # Straight dive line past the camera, closest at 1.6 s.
    return float(np.hypot(1400.0, 2600.0 * (t - 1.6)))


def loop_segment(loop, start_s, dur_s, rng):
    n = int(dur_s * SR)
    offset = int(rng.random() * len(loop))
    idx = (np.arange(n) + offset) % len(loop)
    return loop[idx]


def place(out, sig, at_s, gain_curve=None):
    a = int(at_s * SR)
    n = min(len(sig), len(out) - a)
    if n <= 0:
        return
    g = gain_curve(at_s + np.arange(n) / SR) if gain_curve else 1.0
    out[a:a + n] += sig[:n] * (g[:, None] if gain_curve else g)


def render_mg(w, rng, quad=True, fire_volume=0.8, impact_volume=0.85):
    out = np.zeros((int(PASS_S * SR), 2))
    loop = w["S_AircraftMG_FireLoop_Quad" if quad else "S_AircraftMG_FireLoop_Pair"]
    churn = w["S_AircraftMG_ImpactLoop_Soft_Quad" if quad else "S_AircraftMG_ImpactLoop_Soft_Pair"]
    fire_gain = np.vectorize(lambda t: att(aircraft_dist(t), FIRE_ATT) * fire_volume)
    zone_gain = att(2800.0, IMPACT_ATT) * impact_volume
    for a, b in BURSTS:
        seg = L.fade(loop_segment(loop, a, b - a + 0.05, rng), fin_s=0.004, fout_s=0.05)
        place(out, seg, a, fire_gain)
        place(out, B.L.fade(w[f"S_AircraftMG_FireTail_{rng.integers(1, 4)}"], fout_s=0.01), b, fire_gain)
        seg = L.fade(loop_segment(churn, a, b - a + 0.05, rng), fin_s=0.02, fout_s=0.08)
        place(out, seg * zone_gain, a + FLIGHT_S)
        place(out, w[f"S_AircraftMG_ImpactTail_{rng.integers(1, 4)}"] * zone_gain, b + FLIGHT_S)
    t = 0.9 + FLIGHT_S
    while t < 1.5 + FLIGHT_S:
        place(out, w[f"S_AircraftMG_MetalCluster_{rng.integers(1, 7)}"] * zone_gain * 0.8, t)
        t += 0.12 + rng.random() * 0.1
    return out


def render_cannon(w, rng, fire_volume=0.7, impact_volume=0.45):
    out = np.zeros((int(PASS_S * SR), 2))
    fire_gain = np.vectorize(lambda t: att(aircraft_dist(t), FIRE_ATT) * fire_volume)
    for a, b in BURSTS:
        seg = L.fade(loop_segment(w["S_Aircraft20mm_FireLoop_Pair"], a, b - a + 0.05, rng), fin_s=0.004, fout_s=0.05)
        place(out, seg, a, fire_gain)
        place(out, w[f"S_Aircraft20mm_FireTail_{rng.integers(1, 4)}"], b, fire_gain)
        for gun in range(2):
            t = a + rng.random() * 0.083
            while t < b:
                dist = 2800.0 + rng.normal(0, 400)
                pan = rng.uniform(-0.6, 0.6)
                hit = w[f"S_Aircraft20mm_Impact_{rng.integers(1, 9)}"]
                place(out, L.pan(hit, pan) * att(dist, IMPACT_ATT) * impact_volume, t + FLIGHT_S)
                t += 60.0 / 720 + rng.normal(0, 0.005)
    return out


def render_old(rng):
    out = np.zeros((int(PASS_S * SR), 2))
    shots = [L.load(r) for r in B.MG42_SINGLE]
    sand = [L.load(r) for r in B.SAND_COMB]
    fire_gain = np.vectorize(lambda t: att(aircraft_dist(t), (1500.0, 6000.0)))
    for a, b in BURSTS:
        t = a
        while t < b:
            place(out, shots[rng.integers(3)] * 0.8, t, fire_gain)
            t += 0.1
        t = a + FLIGHT_S
        while t < b + FLIGHT_S:
            place(out, sand[rng.integers(len(sand))] * att(2800.0, (1000.0, 3000.0)), t)
            t += 0.12
    return out


def render(waves, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    # Engine mixing is floating point, so absolute peaks only matter relative to each other: every preview gets the
    # same gain (worst roll of all four at -1 dBFS) and keeps its loudness relative to the old sound.
    renders = (("old_mg", render_old), ("mg_quad", lambda r: render_mg(waves, r, True)),
               ("mg_pair", lambda r: render_mg(waves, r, False)), ("cannon_20mm", lambda r: render_cannon(waves, r)))
    worst = {label: max(np.abs(fn(np.random.default_rng(seed))).max() for seed in range(40)) for label, fn in renders}
    gain = L.db(-1.0) / max(worst.values())
    parts = []
    for label, fn in renders:
        buf = fn(np.random.default_rng(7)) * gain
        print(f"preview {label}: worst of 40 rolls {20 * np.log10(worst[label] * gain):+.1f} dBFS after the shared gain,"
              f" momentary max {momentary_max_db(buf):+.1f} dB")
        L.write(out_dir, f"preview_{label}.wav", buf)
        parts += [buf, np.zeros((SR, 2))]
    # One listening file: old, MG quad (Bf 110), MG pair (Ju 87 / Il-2), 20 mm (Me 410 / Fw 189), 1 s apart.
    L.write(out_dir, "preview_all_old_quad_pair_20mm.wav", np.concatenate(parts))


def momentary_max_db(buf):
    """Loudest 400 ms window of a roughly K-weighted mono fold."""
    m = L.fft_filter(buf, lo=60, shelf_hi_db=4, shelf_hi_f=1500).mean(axis=1)
    hop, win = int(0.1 * SR), int(0.4 * SR)
    return 20 * np.log10(max(np.sqrt((m[i:i + win] ** 2).mean()) for i in range(0, len(m) - win, hop)) + 1e-12)
