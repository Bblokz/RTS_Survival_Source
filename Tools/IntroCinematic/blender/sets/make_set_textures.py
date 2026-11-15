"""Intro cinematic sets - procedural texture sets (numpy, written through Blender images).

Run:  blender.exe --factory-startup -b -P make_set_textures.py [-- only=Earth,Mould]
Writes T_Intro_Earth / _Wood / _Sandbag / _Mould (2048) and T_Intro_Steel (1024):
  *_BaseColor (sRGB), *_Normal (DirectX, green down), *_ORM (R=AO, G=roughness, B=metallic).
The crater textures are written by build_crater.py (they depend on the crack layout).
"""
import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sets_common as C
from sets_common import fft_noise, sstep, mixc, rgb, F

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
ONLY = [a.split('=', 1)[1].split(',') for a in ARGS if a.startswith('only=')]
ONLY = ONLY[0] if ONLY else None

# Earth atlas layout (2048): rows 0..511 = puddle strip (8 m x 2 m, not tiling),
# rows 520..2039 = soil, periodic in U (4 m) AND in V over its own 1520 rows (3 m); 8 px wrapped gutters.
EARTH_BAND0, EARTH_BAND1 = 520, 2040


def to_mean(alb, target):
    m = alb.reshape(-1, 3).mean(0)
    return np.clip(alb * (np.array(target, F) / 255.0 / m)[None, None, :], 0, 1)


def soil(h, w, seed, fine=1.0):
    """Returns (albedo, height m, ao, rough, wet mask). fine > 1 = features that many times smaller in px."""
    big = fft_noise(h, w, 2.2, seed)
    mid = fft_noise(h, w, 1.5, seed + 1, cut=0.08 * fine)
    clod = fft_noise(h, w, 1.0, seed + 2, cut=0.2 * fine)
    grit = fft_noise(h, w, 0.2, seed + 3)
    hm = 0.010 * mid + 0.006 * clod + 0.0007 * grit
    # clod ridging: sharpen the mid frequencies
    hm = hm + 0.006 * np.abs(fft_noise(h, w, 1.3, seed + 4, cut=0.1 * fine)) - 0.004
    wet = sstep(0.45 * big + 0.9 * mid, 0.5, 1.7)
    tone = 0.07 * big + 0.17 * mid + 0.10 * clod + 0.05 * grit
    alb = np.ones((h, w, 3), F) * rgb(64, 48, 34)[None, None, :] * (1.0 + tone[..., None])
    alb = mixc(alb, rgb(40, 29, 20), wet * 0.75)
    dry = sstep(clod + 0.5 * grit - 0.2 * big, 0.9, 2.0)
    alb = mixc(alb, rgb(96, 76, 55), dry * 0.6)
    # small stones
    st = fft_noise(h, w, 0.0, seed + 5, cut=0.06 * fine)
    stone = sstep(st, 2.25, 2.6)
    alb = mixc(alb, rgb(104, 97, 86), stone * 0.85)
    hm = hm + 0.006 * stone
    # root / straw fibres
    fib = fft_noise(h, w, 0.6, seed + 6, aniso=(9.0, 1.0), cut=0.3)
    fm = sstep(fib, 2.5, 3.0) * 0.5
    alb = mixc(alb, rgb(110, 92, 62), fm)
    lo = hm - 0.010 * mid
    ao = np.clip(1.0 + 45.0 * np.minimum(hm - 0.004, 0.0) + 20 * lo, 0.5, 1.0)
    rough = np.clip(0.88 - 0.42 * wet + 0.05 * grit, 0.3, 1.0)
    return alb, hm.astype(F), ao.astype(F), rough.astype(F), wet


def make_earth():
    S = 2048
    bh = EARTH_BAND1 - EARTH_BAND0
    alb = np.zeros((S, S, 3), F)
    hm = np.zeros((S, S), F)
    ao = np.ones((S, S), F)
    ro = np.ones((S, S), F)
    a, h_, o, r, _ = soil(bh, S, 11)
    # puddle strip: 8 m x 2 m at 256 px/m
    ph = 512
    pa, phm, pao, pr, pwet = soil(ph, S, 31, fine=2.0)
    u = (np.arange(S, dtype=F)[None, :] + 0.5) / 256.0          # metres 0..8
    v = (np.arange(ph, dtype=F)[:, None] + 0.5) / 256.0         # metres 0..2
    env = sstep(u, 1.2, 2.6) * (1 - sstep(u, 5.4, 6.8)) * sstep(v, 0.05, 0.45) * (1 - sstep(v, 1.55, 1.95))
    blob = fft_noise(ph, S, 2.4, 41) * 0.9 + fft_noise(ph, S, 1.2, 42, cut=0.05) * 0.35
    lvl = blob + 2.6 * env - 1.9                                  # > 0 = standing water
    water = sstep(lvl, 0.0, 0.18)
    mud = sstep(lvl, -0.9, 0.0)
    pa = mixc(pa, rgb(38, 28, 19), mud * 0.8)
    pa = mixc(pa, rgb(34, 29, 24), water * 0.92)
    phm = phm * (1 - water) - 0.012 * mud * (1 - water) - 0.02 * water
    # tyre/boot churn ridges in the mud along the trench
    churn = fft_noise(ph, S, 1.4, 43, aniso=(1.0, 3.0), cut=0.06)
    phm = phm + 0.006 * churn * mud * (1 - water)
    pr = pr * (1 - mud * 0.55)
    pr = pr * (1 - water) + 0.05 * water
    pao = pao * (1 - water) + water
    # global mean of the soil band -> about 60,45,32
    m = a.reshape(-1, 3).mean(0)
    k = (np.array([60, 45, 32], F) / 255.0 / m)[None, None, :]
    a, pa = np.clip(a * k, 0, 1), np.clip(pa * k, 0, 1)
    alb[EARTH_BAND0:EARTH_BAND1] = a
    hm[EARTH_BAND0:EARTH_BAND1] = h_
    ao[EARTH_BAND0:EARTH_BAND1] = o
    ro[EARTH_BAND0:EARTH_BAND1] = r
    alb[:ph], hm[:ph], ao[:ph], ro[:ph] = pa, phm, pao, pr
    nrm = np.zeros((S, S, 3), F)
    nrm[EARTH_BAND0:EARTH_BAND1] = C.normal_from_height(h_, 512.0, True)
    nrm[:ph] = C.normal_from_height(phm, 256.0, False)
    for arr in (alb, ao, ro, nrm):
        arr[EARTH_BAND0 - 8:EARTH_BAND0] = arr[EARTH_BAND1 - 8:EARTH_BAND1]      # wrapped gutters
        arr[EARTH_BAND1:] = arr[EARTH_BAND0 + 8:EARTH_BAND0 + 16]
    m = C.save_set('T_Intro_Earth', alb, None, 512.0, ao, ro, nrm=nrm)
    band = C.load_img('T_Intro_Earth_BaseColor')[EARTH_BAND0:EARTH_BAND1].reshape(-1, 3).mean(0) * 255
    print('CHECK earth soil band mean sRGB %.0f,%.0f,%.0f (target 60,45,32)' % tuple(band))
    assert abs(band[0] - 60) < 5 and abs(band[1] - 45) < 5 and abs(band[2] - 32) < 5


def make_wood():
    S = 2048                       # 4 m x 4 m, grain along U, board rows 0.2 m (102.4 px)
    v = (np.arange(S, dtype=F)[:, None] + 0.5) / 512.0
    row = np.floor(v / 0.2)
    rr = np.random.default_rng(5)
    rowtone = rr.uniform(-1, 1, 20).astype(F)[row.astype(int) % 20]
    rowshift = rr.integers(0, S, 20)
    grain = fft_noise(S, S, 1.0, 51, aniso=(1.0, 22.0), cut=0.25)
    grain2 = fft_noise(S, S, 0.3, 52, aniso=(1.0, 40.0))
    big = fft_noise(S, S, 2.0, 53)
    # shift grain per board so rows do not continue each other
    g = np.empty_like(grain)
    g2 = np.empty_like(grain)
    for r_ in range(20):
        a, b = int(round(r_ * 102.4)), int(round((r_ + 1) * 102.4))
        g[a:b] = np.roll(grain[a:b], rowshift[r_], 1)
        g2[a:b] = np.roll(grain2[a:b], rowshift[r_] * 3, 1)
    dv = np.abs(np.mod(v + 0.1, 0.2) - 0.1) * np.ones((1, S), F)      # distance to a board edge (m)
    edge = np.exp(-(dv / 0.006) ** 2)
    wear = np.exp(-(dv / 0.03) ** 2)
    crack = sstep(g + 0.5 * g2, 1.7, 2.3)
    hm = 0.0016 * g + 0.0006 * g2 - 0.004 * edge - 0.003 * crack - 0.001 * wear
    tone = 0.16 * g + 0.07 * g2 + 0.10 * big + 0.12 * rowtone
    alb = np.ones((S, S, 3), F) * rgb(96, 86, 74)[None, None, :] * (1.0 + tone[..., None])
    alb = mixc(alb, rgb(84, 70, 54), sstep(big + 0.6 * rowtone, 0.3, 1.6) * 0.6)       # browner damp boards
    alb = mixc(alb, rgb(124, 119, 110), sstep(-big + 0.5 * g, 0.6, 2.0) * 0.45)         # silver weathering
    alb = mixc(alb, rgb(40, 33, 27), np.clip(crack + edge * 0.9, 0, 1) * 0.85)
    # mud splash staining
    mudm = sstep(fft_noise(S, S, 2.2, 54), 0.7, 1.8) * 0.5
    alb = mixc(alb, rgb(62, 47, 34), mudm)
    ao = np.clip(1.0 - 0.5 * edge - 0.35 * crack, 0.4, 1)
    rough = np.clip(0.86 + 0.04 * g - 0.12 * mudm, 0.5, 1)
    C.save_set('T_Intro_Wood', np.clip(alb, 0, 1), hm.astype(F), 512.0, ao.astype(F), rough.astype(F))


def make_sandbag():
    S = 2048                       # 1 m x 1 m hessian, tiles both ways
    x = (np.arange(S, dtype=F)[None, :] + 0.5) / S
    y = (np.arange(S, dtype=F)[:, None] + 0.5) / S
    n_th = 128                     # threads per metre each way
    wob = 0.0016 * fft_noise(S, S, 1.6, 61)
    wx = np.sin(2 * np.pi * n_th * (x + wob))
    wy = np.sin(2 * np.pi * n_th * (y + np.roll(wob, 300, 0)))
    over = np.sin(np.pi * n_th * (x + wob)) * np.sin(np.pi * n_th * (y + np.roll(wob, 300, 0)))
    warp_up = (np.sign(over) * 0.5 + 0.5)
    weave = warp_up * (0.5 + 0.5 * wy) * (0.55 + 0.45 * np.abs(wx)) + (1 - warp_up) * (0.5 + 0.5 * wx) * (0.55 + 0.45 * np.abs(wy))
    fuzz = fft_noise(S, S, 0.3, 62)
    big = fft_noise(S, S, 2.2, 63)
    mid = fft_noise(S, S, 1.4, 64, cut=0.05)
    hm = 0.0012 * weave + 0.0002 * fuzz + 0.004 * mid * 0.3
    tone = 0.10 * big + 0.08 * mid + 0.06 * fuzz + 0.22 * (weave - 0.5)
    alb = np.ones((S, S, 3), F) * rgb(128, 108, 78)[None, None, :] * (1.0 + tone[..., None])
    dirt = sstep(big + 0.6 * mid, -0.3, 1.5)
    alb = mixc(alb, rgb(68, 53, 38), dirt * 0.75)
    damp = sstep(fft_noise(S, S, 2.4, 65), 0.6, 1.6)
    alb = mixc(alb, rgb(52, 42, 31), damp * 0.55)
    alb = mixc(alb, rgb(150, 136, 108), sstep(-big - mid, 1.2, 2.4) * 0.35)
    ao = np.clip(0.72 + 0.28 * weave, 0, 1)
    rough = np.clip(0.93 - 0.2 * damp - 0.05 * dirt, 0.5, 1)
    C.save_set('T_Intro_Sandbag', np.clip(alb, 0, 1), hm.astype(F), float(S), ao.astype(F), rough.astype(F))


def make_steel():
    S = 1024
    # layout: U 0..0.5 = painted helmet steel; U 0.5..1, V 0.5..1 = chipped enamel; U 0.5..1, V 0..0.5 = bare rusty wire steel
    big = fft_noise(S, S, 2.0, 71)
    mid = fft_noise(S, S, 1.2, 72, cut=0.1)
    fine = fft_noise(S, S, 0.2, 73)
    scr = fft_noise(S, S, 0.4, 74, aniso=(1.0, 30.0))
    chips = sstep(mid + 0.5 * fine + 0.4 * big, 1.5, 1.9)
    paint = np.ones((S, S, 3), F) * rgb(62, 66, 58)[None, None, :] * (1 + (0.10 * big + 0.05 * fine)[..., None])
    paint = mixc(paint, rgb(86, 54, 36), chips)
    paint = mixc(paint, rgb(52, 42, 32), sstep(big, 0.4, 1.6) * 0.5)                 # mud film
    enamel = np.ones((S, S, 3), F) * rgb(176, 172, 158)[None, None, :] * (1 + (0.05 * big + 0.03 * fine)[..., None])
    enamel = mixc(enamel, rgb(40, 34, 30), chips)
    enamel = mixc(enamel, rgb(96, 76, 56), sstep(big + mid * 0.5, 0.2, 1.8) * 0.45)
    wire = np.ones((S, S, 3), F) * rgb(70, 60, 52)[None, None, :] * (1 + (0.15 * mid + 0.1 * fine)[..., None])
    wire = mixc(wire, rgb(104, 62, 38), sstep(mid + big, 0.0, 1.5) * 0.7)
    uu = (np.arange(S)[None, :] >= S // 2) * np.ones((S, 1), bool)
    vv = (np.arange(S)[:, None] >= S // 2) * np.ones((1, S), bool)
    alb = np.where(uu[..., None], np.where(vv[..., None], enamel, wire), paint)
    rough_p = np.clip(0.55 + 0.1 * fine + 0.3 * chips + 0.15 * sstep(big, 0.4, 1.6), 0.2, 1)
    rough_e = np.clip(0.28 + 0.05 * fine + 0.5 * chips, 0.1, 1)
    rough_w = np.clip(0.6 + 0.2 * sstep(mid + big, 0.0, 1.5), 0, 1)
    rough = np.where(uu, np.where(vv, rough_e, rough_w), rough_p)
    metal = np.where(uu, np.where(vv, chips * 0.8, 0.85 - 0.6 * sstep(mid + big, 0.0, 1.5)), chips * 0.5)
    hm = 0.0003 * fine + 0.0004 * scr * 0.3 - 0.0006 * chips
    ao = np.ones((S, S), F)
    C.save_set('T_Intro_Steel', np.clip(alb, 0, 1).astype(F), hm.astype(F), 1024.0, ao, rough.astype(F), metal.astype(F))


def make_mould():
    S = 2048                       # 1 m x 1 m, tiles both ways
    bulge = fft_noise(S, S, 2.6, 81, cut=0.012)                   # ~10-20 cm swellings
    mid = fft_noise(S, S, 1.6, 82, cut=0.05)
    wr = fft_noise(S, S, 1.4, 83, cut=0.06, aniso=(1.0, 2.2))
    wrinkle = 1.0 - np.abs(wr)                                    # ridged: sharp creases where wr ~ 0
    crease_line = sstep(wrinkle, 0.86, 0.99)
    pore_n = fft_noise(S, S, 0.0, 84, cut=0.10)
    pores = sstep(pore_n, 1.5, 2.1)
    pores_big = sstep(fft_noise(S, S, 0.0, 85, cut=0.035), 1.9, 2.4)
    vn = fft_noise(S, S, 2.0, 86, cut=0.03)
    vein = (1.0 - sstep(np.abs(vn), 0.0, 0.07)) * sstep(fft_noise(S, S, 2.2, 87), -0.6, 0.6)
    fine = fft_noise(S, S, 0.4, 88)
    hm = 0.010 * bulge + 0.003 * mid - 0.0022 * crease_line - 0.0016 * pores - 0.003 * pores_big + 0.0012 * vein + 0.00025 * fine
    b = sstep(bulge + 0.35 * mid, -1.4, 1.6)                      # 0 = crease, 1 = bulge top
    alb = np.ones((S, S, 3), F) * rgb(35, 12, 60)[None, None, :]
    alb = mixc(alb, rgb(72, 26, 114), sstep(b, 0.05, 0.6))
    alb = mixc(alb, rgb(124, 74, 178), sstep(b, 0.62, 1.0) * 0.8)
    alb = alb * (1.0 + (0.07 * fine + 0.06 * mid)[..., None])
    alb = mixc(alb, rgb(30, 9, 52), np.clip(crease_line * 0.7 + pores * 0.75 + pores_big * 0.85, 0, 1))
    alb = mixc(alb, rgb(40, 10, 72), vein * 0.6)
    alb = to_mean(alb, (70, 25, 110))
    # keep it purple, never magenta: red may not exceed 68 % of blue
    alb[..., 0] = np.minimum(alb[..., 0], alb[..., 2] * 0.68)
    alb[..., 1] = np.minimum(alb[..., 1], alb[..., 0] * 0.75)
    crease = np.clip((1 - sstep(b, 0.0, 0.5)) + crease_line * 0.6 + pores * 0.8 + pores_big, 0, 1)
    rough = np.clip(0.2 - 0.05 * sstep(b, 0.5, 1.0) + 0.4 * crease + 0.02 * fine, 0.12, 0.7)
    ao = np.clip(0.55 + 0.45 * sstep(b, 0.0, 0.5) - 0.3 * pores_big - 0.2 * crease_line, 0.3, 1)
    m = C.save_set('T_Intro_Mould', alb.astype(F), hm.astype(F), float(S), ao.astype(F), rough.astype(F))
    print('CHECK mould mean (target ~70,25,110); bulge rough %.2f crease rough %.2f' % (
        float(rough[b > 0.7].mean()), float(rough[b < 0.15].mean())))
    assert m[0] < m[2] * 0.75 and abs(m[2] - 110) < 12 and abs(m[0] - 70) < 12


JOBS = {'Earth': make_earth, 'Wood': make_wood, 'Sandbag': make_sandbag, 'Steel': make_steel, 'Mould': make_mould}
for name, fn in JOBS.items():
    if ONLY is None or name in ONLY:
        fn()
print('TEXTURES DONE')
