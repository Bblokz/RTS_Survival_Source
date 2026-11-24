"""Two candidate sound sets for a 30 mm strafe gun (the Ho 229's MK 108 as a strafe gun instead of projectiles).

  python build_30mm_versions.py

Writes D:/UE_Assets/Sounds/WeaponFire/AircraftGuns/30mm_A|B/*.wav and Saved/AircraftGunAudio/preview_30mm_A|B.wav
(a strafe pass with the same rules and gain as the other previews). Nothing is imported; the user picks one first.

A "Hammer": the MK 108 as it was, a slow pneumatic-hammer thump (600 rpm), deep body and a long slap-back; every round
  lands as a heavy dirt-throwing HE burst over a low rumble.
B "Crack": a sharper, drier cannon (660 rpm) with a hard 37 mm-style crack; rounds land as bright high-explosive cracks
  with shrapnel and stone debris, less rumble.
Both: 2 guns, fire loop + 3 tails, impact churn loop + 3 tails, loops built on a circular buffer like the MG set.
"""
import os

import numpy as np

import build_aircraft_gun_sounds as B
import preview_strafe_pass as P

L = B.L
SR = B.SR
S = L.SOUNDS_ROOT
RNG = np.random.default_rng(1108)
B.RNG = RNG  # the shared helpers (impact pools, thuds) draw from this run's generator

CANNON30 = ["TankCannon/SmallCalliber/30mmAutoAA/SW_cannon02.WAV", "TankCannon/SmallCalliber/30mmAutoAA/SW_cannon03.WAV"]
FLAK37_REC = ["AAGuns/37mmFlak_Recorded/S_Flak37_Recorded_%d.wav" % i for i in range(1, 9)]
MED_SAND = [S + "Explosions/MediumCaliber/MediumCaliber_SandImpact_%d.wav" % i for i in (1, 2, 3)]
MED_STONE = [S + "Explosions/MediumCaliber/MediumCaliber_StoneImpact_%d.wav" % i for i in (1, 2, 3)]
DIRT_EXPL = [S + "Explosions/OnDirt/ExplosionOnDirt_0%d.wav" % i for i in (1, 2, 3)]
FAST_EXPL = [S + "Explosions/ShrapnellExpl/raw/Explosion_General_Fast_Large_Soft-0%d.WAV" % i for i in (1, 2, 3, 4)]
MED_BED = S + "Explosions/MediumCaliber/MediumImpacts.wav"
OUT = L.WEAPON_ROOT + "AircraftGuns/"
PANS = (-0.3, 0.3)


def norm(d):
    return d / (np.abs(d).max() + 1e-12)


def first_hit(rel, max_s, fout_s, ratio=1.0, lo=None, hi=None):
    d = B.one_hit(rel, max_s, fout_s, thresh=0.2)
    if lo or hi:
        d = L.fft_filter(d, lo=lo, hi=hi)
    return norm(L.pitch(d, ratio))


def low_thump(k, length_s=0.16, hi=130):
    d = L.trim_preroll(L.load(B.PUNCH_127[k % 5], 0.0, 0.5), thresh=0.3)[: int(length_s * SR)]
    return norm(L.fade(L.fft_filter(d, lo=30, hi=hi), fout_s=length_s * 0.6))


def mix(layers, length_s):
    out = np.zeros((int(length_s * SR), 2))
    for sig, gain, at in layers:
        L.add(out, sig * gain, at)
    return out


# ---------------------------------------------------------------- voices

def voice_a(k):
    """Hammer: dull 30 mm thump, deep 37 mm body, heavy low punch; little top end."""
    crack = first_hit(CANNON30[k % 2], 0.12, 0.06, ratio=0.86 + 0.04 * (k % 3), lo=70, hi=5500)
    crack = L.envelope(crack, 0.008, 0.03, -14.0)
    body = first_hit(L.AUTO37[k % 3], 0.3, 0.18, ratio=0.85, lo=35, hi=280)
    return mix([(crack, 1.0, 0.0), (body, L.db(-2), 0.002), (low_thump(k, 0.2), L.db(-1), 0.0)], 0.3)


def voice_b(k):
    """Crack: hard recorded 37 mm crack with a bright snap, tighter body."""
    crack = first_hit(FLAK37_REC[k % 8], 0.1, 0.05, ratio=1.0 + 0.03 * (k % 3), lo=110)
    crack = L.envelope(crack, 0.006, 0.02, -16.0)
    snap = first_hit(L.SNAP37[k % 3], 0.05, 0.035, lo=2500)
    body = first_hit(L.FLAK20[k % 5], 0.2, 0.12, ratio=0.82, lo=45, hi=420)
    return mix([(crack, 1.0, 0.0), (snap, L.db(-9), 0.0), (body, L.db(-5), 0.002), (low_thump(k, 0.12, 160), L.db(-6), 0.0)],
               0.22)


# ---------------------------------------------------------------- impacts

def hit_a(pools, k):
    """Heavy HE burst in dirt: boom, dirt thrown up, thud, clods coming down."""
    boom = first_hit(MED_SAND[k % 3], 0.7, 0.45, ratio=RNG.uniform(0.86, 0.98))
    dirt = first_hit(DIRT_EXPL[k % 3], 0.6, 0.4, ratio=RNG.uniform(0.95, 1.08), lo=300)
    spray = norm(pools["soft"][RNG.integers(len(pools["soft"]))])
    return mix([(boom, 1.0, 0.0), (dirt, L.db(-6), 0.01), (spray, L.db(-7), 0.02), (low_thump(k, 0.22, 110), L.db(-2), 0.0)],
               0.8)


def hit_b(pools, k):
    """Sharp HE crack: bright explosion transient, shrapnel and stone debris, short."""
    crack = first_hit(FAST_EXPL[k % 4], 0.35, 0.22, ratio=RNG.uniform(1.0, 1.15), lo=180)
    pops = HE_POPS
    pop = norm(L.pitch(pops[k % len(pops)], RNG.uniform(0.8, 0.9)))
    grit = norm(pools["stone"][RNG.integers(len(pools["stone"]))])
    layers = [(crack, 1.0, 0.0), (pop, L.db(-3), 0.0), (grit, L.db(-8), 0.015), (low_thump(k, 0.12, 150), L.db(-7), 0.0)]
    if RNG.random() < 0.3:
        r = pools["rico"][RNG.integers(len(pools["rico"]))]
        layers.append((norm(r), L.db(-11), 0.03))
    return mix(layers, 0.6)


HE_POPS = B.he_pops()


def impact_loop(hits, rate_hz, loop_s, bed_db, bed_hi):
    n = int(loop_s * SR)
    out = np.zeros((n, 2))
    t = 0.0
    while t < loop_s:
        h = hits[RNG.integers(len(hits))]
        far = RNG.random()
        h = L.fft_filter(h, hi=3500) if far > 0.65 else h
        B.circ_add(out, L.pan(h, RNG.uniform(-0.6, 0.6)) * L.db(-9.0 * far), t)
        t += RNG.exponential(1.0 / rate_hz) * 0.6 + 0.4 / rate_hz
    bed_src = L.load(MED_BED)[int(0.6 * SR): int(0.6 * SR) + n + int(0.3 * SR)]
    bed = L.fade(L.fft_filter(bed_src, lo=40, hi=bed_hi), fin_s=0.15, fout_s=0.15)
    wrapped = np.zeros((n, 2))
    B.circ_add(wrapped, bed, 0.0)
    out += norm(wrapped) * L.db(bed_db)
    return B.finish_loop(out, drive=1.5)


def impact_tail(hits, i):
    out = np.zeros((int(1.6 * SR), 2))
    t, gap = 0.0, 0.06
    for k in range(4):
        L.add(out, L.pan(hits[(i * 3 + k) % len(hits)], RNG.uniform(-0.5, 0.5)) * L.db(-2.5 * k), t)
        t += gap
        gap *= 1.4
    patter = L.load(B.SAND_BED[i % 3])[int(2.0 * SR): int(3.3 * SR)]
    patter = L.fade(L.fft_filter(patter, lo=1500, hi=8000), fin_s=0.2, fout_s=0.6)
    L.add(out, norm(patter) * L.db(-13), 0.2)
    return B.finish_shot(out, fout_s=0.6)


# ---------------------------------------------------------------- build + preview

def build(label, voice, rpm, verb_db, tail_verb_db, hit, rate_hz, bed_db, bed_hi):
    pools = B.impact_pools()
    voices = [voice(k) for k in range(8)]
    hits = [hit(pools, k) for k in range(12)]
    waves = {
        "FireLoop": B.fire_loop(voices, 2, rpm, 2.5, PANS, jitter_s=0.006, verb_db=verb_db),
        "ImpactLoop": impact_loop(hits, rate_hz, 3.0, bed_db, bed_hi),
    }
    for i in range(3):
        waves[f"FireTail_{i + 1}"] = B.fire_tail(voices, i, PANS, verb_db=tail_verb_db)
        waves[f"ImpactTail_{i + 1}"] = impact_tail(hits, i)
    out_dir = OUT + f"30mm_{label}/"
    for name, buf in waves.items():
        path = L.write(out_dir, f"S_Aircraft30mm{label}_{name}.wav", buf)
        print(f"{os.path.basename(path)}: {len(buf) / SR:.2f}s peak={B.peak_db(buf):.1f}dBFS"
              + (f" seam={B.seam_jump_db(buf):+.1f}dB" if "Loop" in name else ""))
    return waves


def render_pass(w, rng):
    """Same pass and rules as preview_strafe_pass.render_mg, with this set's loops and tails."""
    out = np.zeros((int(P.PASS_S * SR), 2))
    fire_gain = np.vectorize(lambda t: P.att(P.aircraft_dist(t), P.FIRE_ATT) * 0.8)
    zone_gain = P.att(2800.0, P.IMPACT_ATT) * 0.85
    for a, b in P.BURSTS:
        P.place(out, L.fade(P.loop_segment(w["FireLoop"], a, b - a + 0.05, rng), fin_s=0.004, fout_s=0.05), a, fire_gain)
        P.place(out, w[f"FireTail_{rng.integers(1, 4)}"], b, fire_gain)
        P.place(out, L.fade(P.loop_segment(w["ImpactLoop"], a, b - a + 0.05, rng), fin_s=0.02, fout_s=0.08) * zone_gain,
                a + P.FLIGHT_S)
        P.place(out, w[f"ImpactTail_{rng.integers(1, 4)}"] * zone_gain, b + P.FLIGHT_S)
    return out


if __name__ == "__main__":
    sets = {
        "A": build("A", voice_a, 600, -12.0, -6.0, hit_a, 13, -9, 220),
        "B": build("B", voice_b, 660, -18.0, -10.0, hit_b, 15, -16, 500),
    }
    # The same shared gain as the existing previews: the worst roll of the old MG/new MG/20 mm previews set it, and the
    # new MG quad preview's loudest roll is the reference, so these sit at their real loudness next to the others.
    mg = {n[:-4]: L.load(L.WEAPON_ROOT + "AircraftGuns/" + n) for n in os.listdir(L.WEAPON_ROOT + "AircraftGuns")
          if n.endswith(".wav")}
    ref_peak = max(np.abs(P.render_cannon(mg, np.random.default_rng(s))).max() for s in range(40))
    gain = L.db(-1.0) / ref_peak
    for label, w in sets.items():
        worst = max(np.abs(render_pass(w, np.random.default_rng(s))).max() for s in range(40)) * gain
        buf = render_pass(w, np.random.default_rng(7)) * gain
        if worst > 1.0:
            print(f"30mm {label}: worst roll {20 * np.log10(worst):+.1f} dBFS, pulled down to -1 dBFS")
            buf *= L.db(-1.0) / worst
        L.write(B.PREVIEW_DIR, f"preview_30mm_{label}.wav", buf)
        print(f"preview 30mm {label}: momentary max {P.momentary_max_db(buf):+.1f} dB")
