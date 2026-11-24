"""Aircraft strafe gun sounds, step 1: render every wave offline (numpy, 48 kHz stereo, PCM 16).

  python build_aircraft_gun_sounds.py [preview]

Writes D:/UE_Assets/Sounds/WeaponFire/AircraftGuns/<name>.wav, imported by import_aircraft_gun_sounds.py.
"preview" also renders Saved/AircraftGunAudio/preview_*.wav: a full strafe pass through the runtime logic
(fire loop + tail on the aircraft, impact churn loop + tail at the beaten zone, metal clusters).

Why loops: aircraft MGs fire RandomBurst volleys (every gun once per volley, many volleys a second), and every round
draws 4 spurts. One voice per round or per volley is hundreds of voices; one launch sound per volley sounded thin.
So each weapon holds one looping voice for its guns and one looping voice for its impact zone, both baked at the
real rate of fire, and ends each burst with a tail. All loops are built on a circular buffer: seamless by
construction, no crossfade.

Sets:
  MG (7.92 mm MG 15/17, ~1050 rpm a gun): FireLoop Pair (1-2 guns) / Quad (3+ guns), FireTail x3,
      ImpactLoop Soft (dirt, sand) / Hard (stone) x Pair / Quad, ImpactTail x3, MetalCluster x6 (unit hits).
  Cannon (20 mm MG 151/20, ~720 rpm a gun): FireLoop Pair, FireTail x3, Impact x8 (one per projectile hit:
      HE pop + dirt throw + thud).
"""
import os
import sys

import numpy as np

sys.path.insert(0, "D:/UE_Assets/Sounds/WeaponFire/_SfxBuilders")
import sfxlib as L  # noqa: E402

S = L.SOUNDS_ROOT
OUT_DIR = L.WEAPON_ROOT + "AircraftGuns/"
PREVIEW_DIR = "D:/UE5Projects/RTS_Survival/Saved/AircraftGunAudio/"
SR = L.SR

MG42_SINGLE = ["MachineGuns/MGSounds/MG42/S_MG42_Single%d.wav" % i for i in (1, 2, 3)]
PUNCH_127 = ["MachineGuns/MGSounds/Ba12.7mm/S_12_7MM_%d.wav" % i for i in (1, 2, 3, 4, 5)]
REVERBS = ["TankCannon/ReverbSounds/Reverb%d.wav" % i for i in (1, 2, 3, 4)]
SAND_COMB = [S + "Bullets/Impact/Comb_SandImpact/S_Bullet_SandImpact_%d.wav" % i for i in range(1, 14)]
GROUND = [S + "Bullets/Impact/GroundImpact/BulletGroundImpact_%d.wav" % i for i in range(1, 10)]
STONE = [S + "Bullets/Impact/StoneImpact/" + n for n in (
    "S_SToneBulletGroundImpact_2.wav", "S_SToneBulletGroundImpact_5.wav", "S_SToneBulletGroundImpact_8.wav",
    "S_StoneBulletGroundImpact_3.wav", "S_StoneBulletGroundImpact_4.wav", "S_StoneBulletGroundImpact_6.wav",
    "S_StoneBulletGroundImpact_9.wav")]
THWACK = [S + "Bullets/Impact/SniperSingleImpact/SniperSingleImpact0%s.wav" % i for i in
          ("1", "10", "2", "3", "4", "5", "6", "7", "8", "9")]
METAL = [S + "Bullets/Impact/MetalImpact/BulletMetalImpact%d.wav" % i for i in range(1, 10)]
METAL_SC = [S + "Explosions/SmallCaliber/S_SmallCaliber_ImpactMetal_%d.wav" % i for i in (1, 2, 3, 4)]
RICOCHET = ["TankCannon/SmallCalliber/ricochetSmallCalibre/SmallCalibrre_ricochet_%d.wav" % i for i in range(1, 7)]
SAND_BED = [S + "SandImpacts/SandImpact_Intense_%d.wav" % i for i in (1, 2, 3)]
HE20 = [S + "Explosions/20mmImpact/20MM_Impact%d.wav" % i for i in (1, 2, 3)]
SC_SAND = [S + "Explosions/SmallCaliber/S_SmallCaliber_ImpactSand_%d.wav" % i for i in (1, 2, 3)]

RNG = np.random.default_rng(1944)


# ---------------------------------------------------------------- helpers

def circ_add(buf, sig, at_s):
    """Adds sig into buf at at_s, wrapping around the end: the loop seam is continuous by construction."""
    n = len(buf)
    a = int(at_s * SR) % n
    sig = sig[:n]
    first = min(len(sig), n - a)
    buf[a:a + first] += sig[:first]
    if first < len(sig):
        buf[:len(sig) - first] += sig[first:]


def slices(rel, rise_db=10, floor_db=-20, min_gap_ms=40, pre_s=0.003, max_s=0.22, fout_s=0.08):
    """Cuts a multi-hit source into single hits at its transient onsets."""
    d = L.load(rel)
    out = []
    for t_ms in L.hits_ms(d, rise_db=rise_db, floor_db=floor_db, min_gap_ms=min_gap_ms):
        a = max(0, int((t_ms / 1000 - pre_s) * SR))
        s = d[a:a + int(max_s * SR)]
        if len(s) > int(0.04 * SR) and np.abs(s).max() > 1e-3:
            out.append(L.fade(s, fin_s=0.001, fout_s=fout_s))
    return out


def one_hit(rel, max_s, fout_s, thresh=0.25):
    d = L.trim_preroll(L.load(rel), thresh=thresh)
    return L.fade(d[: int(max_s * SR)], fin_s=0.001, fout_s=fout_s)


def peak_db(d):
    return 20 * np.log10(np.abs(d).max() + 1e-12)


def finish_loop(buf, drive=1.35, peak=0.89):
    """Level and soft-limit a loop without fading it (a fade would break the seam)."""
    buf = buf - buf.mean(axis=0)
    buf = L.soft_limit(buf / (np.abs(buf).max() + 1e-12) * drive)
    return buf / np.abs(buf).max() * peak


def finish_shot(buf, drive=1.3, peak=0.89, fout_s=0.15):
    return L.finish(buf, drive_gain=drive, peak=peak, fout_s=fout_s)


def seam_jump_db(buf):
    """Sample step across the loop seam relative to the typical step: ~0 dB means no click."""
    step = np.abs(buf[0] - buf[-1]).max()
    typical = np.median(np.abs(np.diff(buf, axis=0)).max(axis=1)) + 1e-9
    return 20 * np.log10(step / typical + 1e-9)


# ---------------------------------------------------------------- MG voices

def mg_voice(k, ratio):
    """One 7.92 mm aircraft MG round: MG 42 crack (short, no echo), a little 12.7 mm punch for weight, low thump."""
    crack = L.trim_preroll(L.load(MG42_SINGLE[k % 3], 0.0, 0.5), thresh=0.3)
    crack = L.fft_filter(crack[: int(0.09 * SR)], lo=90, shelf_hi_db=2.0, shelf_hi_f=3500)
    crack = L.envelope(L.fade(crack, fout_s=0.05), 0.006, 0.018, -16.0)
    punch = L.trim_preroll(L.load(PUNCH_127[k % 5], 0.0, 0.4), thresh=0.3)[: int(0.07 * SR)]
    punch = L.fade(L.fft_filter(punch, lo=120, hi=2400), fout_s=0.05)
    thump = L.fade(L.fft_filter(L.trim_preroll(L.load(PUNCH_127[(k + 2) % 5], 0.0, 0.4), thresh=0.3)[: int(0.08 * SR)],
                                lo=40, hi=170), fout_s=0.06)
    out = np.zeros((int(0.1 * SR), 2))
    for layer, gain in ((L.match_crack(crack), 1.0), (punch / (np.abs(punch).max() + 1e-12), L.db(-9)),
                        (thump / (np.abs(thump).max() + 1e-12), L.db(-7))):
        out[: len(layer)] += layer[: len(out)] * gain
    return L.pitch(out, ratio)


def mg_voices(count=12):
    return [mg_voice(k, 1.0 + 0.11 * RNG.random()) for k in range(count)]


def verb_bed_loop(n, shots_at, rel_db, hi=3500):
    """Slap-back reverb fed by a share of the shots, wrapped like the loop."""
    bed = np.zeros((n, 2))
    for i, t in enumerate(shots_at):
        if i % 6:
            continue
        v = L.crop(L.verb_tail(REVERBS[i % 4], 0.05, lo=90, hi=hi), 0.9, 0.6)
        circ_add(bed, v * L.db(rel_db), t + 0.02)
    return bed


def fire_loop(voices, guns, rpm, loop_s, pans, jitter_s=0.004, verb_db=-17.0):
    n = int(loop_s * SR)
    out = np.zeros((n, 2))
    period = 60.0 / rpm
    shots = []
    for g in range(guns):
        # Whole number of rounds per gun in the loop: the cadence carries across the seam.
        rounds = int(round(loop_s / period))
        p = loop_s / rounds
        phase = RNG.random() * p
        for r in range(rounds):
            t = phase + r * p + RNG.normal(0, jitter_s)
            v = voices[RNG.integers(len(voices))]
            circ_add(out, L.pan(v, pans[g]) * L.db(-RNG.random() * 2.0), t)
            shots.append(t)
    out += verb_bed_loop(n, sorted(shots), verb_db)
    return finish_loop(out)


def fire_tail(voices, i, pans, verb_db=-9.0):
    """Burst end: the last round (the loop fades out under it) and the slap of the shots off the ground."""
    out = np.zeros((int(1.6 * SR), 2))
    L.add(out, L.pan(voices[i % len(voices)], pans[i % len(pans)]), 0.004)
    v = L.crop(L.verb_tail(REVERBS[i % 4], 0.04, lo=80, hi=3200), 1.5, 1.0)
    L.add(out, v * L.db(verb_db), 0.03)
    return finish_shot(out, fout_s=0.6)


# ---------------------------------------------------------------- impacts

def impact_pools():
    soft = []
    for rel in SAND_COMB:
        soft += slices(rel, max_s=0.25, fout_s=0.12)
    for rel in GROUND:
        soft += slices(rel, max_s=0.25, fout_s=0.12)
    thwack = [one_hit(rel, 0.22, 0.12) for rel in THWACK]
    stone = []
    for rel in STONE:
        stone += slices(rel, max_s=0.25, fout_s=0.12)
    metal = [one_hit(rel, 0.4, 0.2) for rel in METAL] + [one_hit(rel, 0.5, 0.3) for rel in METAL_SC]
    rico = [one_hit(rel, 0.8, 0.4, thresh=0.1) for rel in RICOCHET]
    return dict(soft=soft, thwack=thwack, stone=stone, metal=metal, rico=rico)


def thud():
    """Low earth thud under a share of the hits: the weight of a round going into the ground."""
    rel = THWACK[RNG.integers(len(THWACK))]
    d = L.fft_filter(one_hit(rel, 0.12, 0.08), lo=35, hi=180)
    return d / (np.abs(d).max() + 1e-12)


def bed_loop(n, rel_db):
    """Dense far churn from the intense sand impacts, made seamless by wrapping its own tail."""
    src = L.load(SAND_BED[RNG.integers(3)])
    a = int(0.15 * SR)
    seg = L.fft_filter(src[a:a + n + int(0.3 * SR)], lo=150, hi=6000)
    seg = L.fade(seg, fin_s=0.15, fout_s=0.15)
    bed = np.zeros((n, 2))
    circ_add(bed, seg, 0.0)
    return bed / (np.abs(bed).max() + 1e-12) * L.db(rel_db)


def place_hit(out, pools, kind, t, writer):
    pan = RNG.uniform(-0.65, 0.65)
    far = RNG.random()
    gain = L.db(-11.0 * far)
    if kind == "hard":
        src = pools["stone"] if RNG.random() < 0.75 else pools["soft"]
    else:
        src = pools["soft"] if RNG.random() < 0.8 else pools["thwack"]
    hit = src[RNG.integers(len(src))]
    hit = L.pitch(hit, RNG.uniform(0.9, 1.1))
    if far > 0.6:
        hit = L.fft_filter(hit, hi=4500)
    writer(out, L.pan(hit / (np.abs(hit).max() + 1e-12), pan) * gain, t)
    if RNG.random() < (0.25 if kind == "hard" else 0.4):
        writer(out, L.pan(thud(), pan * 0.5) * gain * L.db(-4), t + 0.004)


def impact_loop(pools, kind, rate_hz, loop_s, rico_hz, bed_db):
    n = int(loop_s * SR)
    out = np.zeros((n, 2))
    t = 0.0
    while t < loop_s:
        place_hit(out, pools, kind, t, circ_add)
        t += RNG.exponential(1.0 / rate_hz) * 0.6 + 0.4 / rate_hz  # Poisson-ish, never a hole longer than ~3x
    count = max(1, int(round(rico_hz * loop_s)))
    for k in range(count):
        r = pools["rico"][RNG.integers(len(pools["rico"]))]
        circ_add(out, L.pan(r / np.abs(r).max(), RNG.uniform(-0.8, 0.8)) * L.db(-RNG.uniform(5, 13)),
                 (k + RNG.random()) * loop_s / count)
    out += bed_loop(n, bed_db)
    return finish_loop(out, drive=1.5)


def impact_tail(pools, i):
    """Burst end at the zone: the last few rounds thinning out, then dirt pattering down."""
    out = np.zeros((int(1.4 * SR), 2))
    t = 0.0
    gap = 0.03
    for k in range(7):
        place_hit(out, pools, "soft", t, L.add)
        t += gap
        gap *= 1.35
    out *= np.linspace(1.0, 0.35, len(out))[:, None]
    patter = L.load(SAND_BED[i % 3])[int(2.0 * SR): int(3.2 * SR)]
    patter = L.fade(L.fft_filter(patter, lo=1800, hi=9000), fin_s=0.2, fout_s=0.6)
    L.add(out, patter / (np.abs(patter).max() + 1e-12) * L.db(-14), 0.12)
    if i % 2 == 0:
        r = pools["rico"][i % len(pools["rico"])]
        L.add(out, L.pan(r / np.abs(r).max(), 0.5 - i * 0.3) * L.db(-8), 0.09)
    return finish_shot(out, fout_s=0.5)


def metal_cluster(pools, i):
    """Rounds hammering a unit: 3-6 clangs, a ricochet, ~0.4 s."""
    out = np.zeros((int(1.0 * SR), 2))
    hits = 3 + i % 4
    t = 0.0
    for k in range(hits):
        m = pools["metal"][RNG.integers(len(pools["metal"]))]
        m = L.pitch(m, RNG.uniform(0.92, 1.12))
        L.add(out, L.pan(m / np.abs(m).max(), RNG.uniform(-0.5, 0.5)) * L.db(-RNG.uniform(0, 7)), t)
        t += RNG.uniform(0.035, 0.09)
    r = pools["rico"][(i * 2) % len(pools["rico"])]
    L.add(out, L.pan(r / np.abs(r).max(), RNG.uniform(-0.7, 0.7)) * L.db(-6), RNG.uniform(0.02, 0.15))
    return finish_shot(out, fout_s=0.35)


# ---------------------------------------------------------------- 20 mm cannon

def cannon_voice(k):
    """MG 151/20: shaped 20 mm Flak crack, autocannon body, low pressure thump; heavier and slower than the MG."""
    shot = L.build_shot(L.FLAK20[k % 5], 1.0 + 0.04 * (k % 3), L.AUTO20[(k + 1) % 5], L.SNAP37[k % 3],
                        body_db=-1.0, body_s=0.2, body_hi=320.0, crack_floor_db=-15.0, crack_tau_s=0.018)
    shot = L.crop(shot, 0.2, 0.1)
    thump = L.fade(L.fft_filter(L.trim_preroll(L.load(PUNCH_127[k % 5], 0.0, 0.4), thresh=0.3)[: int(0.12 * SR)],
                                lo=35, hi=150), fout_s=0.08)
    out = np.zeros((len(shot), 2))
    out += shot
    out[: len(thump)] += thump / (np.abs(thump).max() + 1e-12) * L.db(-3)
    return out


def he_pops():
    pops = []
    for rel in HE20:
        pops += slices(rel, rise_db=10, floor_db=-20, min_gap_ms=40, max_s=0.5, fout_s=0.3)
    return pops


def cannon_impact(pools, pops, i):
    """One 20 mm round in the ground: Minengeschoss crack-pop, dirt thrown up, earth thud, debris pattering."""
    out = np.zeros((int(1.1 * SR), 2))
    pop = pops[i % len(pops)]
    pop = L.pitch(pop, RNG.uniform(0.95, 1.06))
    L.add(out, pop / np.abs(pop).max(), 0.0)
    for k in range(2):
        d = pools["soft"][RNG.integers(len(pools["soft"]))]
        L.add(out, L.pan(d / np.abs(d).max(), RNG.uniform(-0.4, 0.4)) * L.db(-4 - 4 * k), 0.006 + 0.02 * k)
    sc = one_hit(SC_SAND[i % 3], 0.9, 0.5)
    L.add(out, sc / np.abs(sc).max() * L.db(-7), 0.004)
    L.add(out, thud() * L.db(-2), 0.003)
    return finish_shot(out, drive=1.4, fout_s=0.4)


# ---------------------------------------------------------------- build

MG_PAIR_PANS = (-0.35, 0.35)
MG_QUAD_PANS = (-0.2, 0.2, -0.45, 0.45)
CANNON_PANS = (-0.25, 0.25)


def build_all():
    os.makedirs(OUT_DIR, exist_ok=True)
    waves = {}
    mg = mg_voices()
    waves["S_AircraftMG_FireLoop_Pair"] = fire_loop(mg, 2, 1050, 2.4, MG_PAIR_PANS)
    waves["S_AircraftMG_FireLoop_Quad"] = fire_loop(mg, 4, 1050, 2.4, MG_QUAD_PANS, verb_db=-19.0)
    for i in range(3):
        waves[f"S_AircraftMG_FireTail_{i + 1}"] = fire_tail(mg, i, MG_PAIR_PANS)
    pools = impact_pools()
    print(f"impact pools: soft={len(pools['soft'])} stone={len(pools['stone'])} metal={len(pools['metal'])}")
    waves["S_AircraftMG_ImpactLoop_Soft_Pair"] = impact_loop(pools, "soft", 26, 3.0, 1.3, -13)
    waves["S_AircraftMG_ImpactLoop_Soft_Quad"] = impact_loop(pools, "soft", 46, 3.0, 2.0, -10)
    waves["S_AircraftMG_ImpactLoop_Hard_Pair"] = impact_loop(pools, "hard", 26, 3.0, 3.0, -16)
    waves["S_AircraftMG_ImpactLoop_Hard_Quad"] = impact_loop(pools, "hard", 46, 3.0, 4.5, -13)
    for i in range(3):
        waves[f"S_AircraftMG_ImpactTail_{i + 1}"] = impact_tail(pools, i)
    for i in range(6):
        waves[f"S_AircraftMG_MetalCluster_{i + 1}"] = metal_cluster(pools, i)
    cannon = [cannon_voice(k) for k in range(8)]
    waves["S_Aircraft20mm_FireLoop_Pair"] = fire_loop(cannon, 2, 720, 2.5, CANNON_PANS, jitter_s=0.005, verb_db=-15.0)
    for i in range(3):
        waves[f"S_Aircraft20mm_FireTail_{i + 1}"] = fire_tail(cannon, i, CANNON_PANS, verb_db=-7.0)
    pops = he_pops()
    print(f"20 mm HE pops: {len(pops)}")
    for i in range(8):
        waves[f"S_Aircraft20mm_Impact_{i + 1}"] = cannon_impact(pools, pops, i)
    for name, buf in waves.items():
        path = L.write(OUT_DIR, name + ".wav", buf)
        loop = "Loop" in name
        extra = f" seam={seam_jump_db(buf):+.1f}dB" if loop else ""
        print(f"{os.path.basename(path)}: {len(buf) / SR:.2f}s peak={peak_db(buf):.1f}dBFS"
              f" rms={20 * np.log10(np.sqrt((buf ** 2).mean())):.1f}dBFS{extra}")
    return waves


if __name__ == "__main__":
    all_waves = build_all()
    if "preview" in sys.argv:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import preview_strafe_pass
        preview_strafe_pass.render(all_waves, PREVIEW_DIR)
