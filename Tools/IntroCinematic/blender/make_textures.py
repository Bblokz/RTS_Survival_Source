"""Ural Shield - procedural 4096 atlas (BaseColor / Normal DirectX / ORM), numpy only.

Run:  blender.exe --factory-startup -b -P make_textures.py
Layout (see us_common.py): full-width bands that tile in U (60 m per repeat) for concrete
and iron, plus a row of non-tiling patches (gate leaf plate, plain iron, dark, lens).
Array convention: row 0 = bottom of the image (V up), same as Blender's pixel order.
"""
import os
import sys
import numpy as np
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import us_common as C

F = np.float32


def fft_noise(h, w, beta, seed, cut=None, aniso=(1.0, 1.0)):
    """Periodic fractal noise, zero mean / unit std. aniso scales (ky, kx)."""
    r = np.random.default_rng(seed)
    f = np.fft.rfft2(r.standard_normal((h, w)).astype(F))
    ky = np.fft.fftfreq(h)[:, None] * aniso[0]
    kx = np.fft.rfftfreq(w)[None, :] * aniso[1]
    k = np.sqrt(ky * ky + kx * kx)
    k[0, 0] = 1.0
    amp = k ** (-beta)
    amp[0, 0] = 0.0
    if cut:
        amp *= np.exp(-(k / cut) ** 2)
    n = np.fft.irfft2(f * amp, s=(h, w))
    n -= n.mean()
    n /= (n.std() + 1e-9)
    return n.astype(F)


def noise1d(w, beta, seed, cut=None):
    return fft_noise(1, w, beta, seed, cut)[0]


def sstep(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def streaks(h, w, ppm, src_m, len_m, seed, thr=0.3, width=0.02, both=False):
    """Vertical run-off streaks hanging below a horizontal source line at height src_m."""
    col = noise1d(w, 0.6, seed, cut=width * 68.0 / ppm * 4)
    a = sstep(col, thr, thr + 0.9)
    ln = len_m * (0.12 + 0.88 * sstep(0.6 * noise1d(w, 1.2, seed + 1, cut=0.05)
                                      + 0.8 * noise1d(w, 0.2, seed + 3, cut=0.2), -1.3, 1.6) ** 1.5)
    a = a * (0.45 + 0.55 * sstep(noise1d(w, 0.0, seed + 4, cut=0.25), -1.0, 1.0))
    v = (np.arange(h, dtype=F)[:, None] + 0.5) / ppm
    t = (src_m - v) / ln[None, :]
    m = a[None, :] * np.clip(1 - t, 0, 1) ** 1.6 * (t >= 0)
    m *= 0.65 + 0.35 * sstep(fft_noise(h, w, 1.0, seed + 2, aniso=(6.0, 1.0)), -1.5, 1.0)
    return m.astype(F)


def normal_from_height(hm, ppm, wrap_u):
    """hm in metres. Returns tangent normal with +x = right (s), +y = up (t)."""
    if wrap_u:
        gx = (np.roll(hm, -1, 1) - np.roll(hm, 1, 1)) * 0.5
    else:
        gx = np.gradient(hm, axis=1)
    gy = np.gradient(hm, axis=0)
    gx *= ppm
    gy *= ppm
    n = np.stack([-gx, -gy, np.ones_like(hm)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n.astype(F)


def mix(a, b, m):
    b = np.asarray(b, F)
    if b.ndim == 1:
        b = b[None, None, :]
    return a * (1 - m[..., None]) + b * m[..., None]


def hash2(iy, ix, seed):
    x = (iy.astype(np.int64) * 73856093) ^ (ix.astype(np.int64) * 19349663) ^ (seed * 83492791)
    x = (x ^ (x >> 13)) * 1274126177
    x = x ^ (x >> 16)
    return ((x & 0xFFFF) / 65535.0).astype(F)


# ----------------------------------------------------------------------------------------
def concrete(h, w, ppm, seed, sources, rust_top=False, damp=False, lifts=None):
    """Board-formed concrete. sources: list of (height m, length m, strength) water stains."""
    vm = (np.arange(h, dtype=F)[:, None] + 0.5) / ppm
    um = (np.arange(w, dtype=F)[None, :] + 0.5) / ppm
    wm = w / ppm
    base = np.array([0.515, 0.485, 0.435], F)
    tone = (0.055 * fft_noise(h, w, 2.4, seed) + 0.035 * fft_noise(h, w, 1.3, seed + 1)
            + 0.022 * fft_noise(h, w, 0.2, seed + 2))
    hm = 0.0025 * fft_noise(h, w, 1.0, seed + 3) + 0.0012 * fft_noise(h, w, 0.0, seed + 4)
    # boards: 0.2 m tall, 4 m long, butt joints staggered per row
    bh, bl = 0.20, 6.0
    row = np.floor(vm / bh)
    off = hash2(row, np.zeros_like(row), seed + 5) * bl
    useg = np.mod(um + off, wm)
    seg = np.floor(useg / bl)
    tone = tone + (hash2(np.broadcast_to(row, (h, w)), seg, seed + 6) - 0.5) * 0.035
    grain = fft_noise(h, w, 0.8, seed + 7, aniso=(1.0, 14.0))
    tone = tone + 0.018 * grain
    hm = hm + 0.0012 * grain
    dv = np.abs(np.mod(vm + bh / 2, bh) - bh / 2)
    du = np.abs(np.mod(useg + bl / 2, bl) - bl / 2)
    gro = np.maximum(np.exp(-(dv / 0.011) ** 2) * np.ones((1, w), F), np.exp(-(du / 0.011) ** 2) * 0.35)
    hm = hm - 0.007 * gro
    board_step = (hash2(np.broadcast_to(row, (h, w)), seg, seed + 8) - 0.5) * 0.004   # boards not flush
    hm = hm + board_step
    ao = 1.0 - 0.45 * gro
    alb = np.ones((h, w, 3), F) * base[None, None, :] * (1.0 + tone[..., None] / 0.55)
    alb *= (1.0 - 0.28 * gro)[..., None]
    wet = np.zeros((h, w), F)
    # pour lifts (cold joints) with grout leak ridge
    for i, lv in enumerate(lifts or []):
        d = vm - lv
        ridge = np.exp(-(d / 0.035) ** 2) * (0.7 + 0.3 * noise1d(w, 0.8, seed + 20 + i)[None, :])
        hm = hm + 0.010 * ridge
        alb *= (1.0 - 0.22 * np.exp(-(d / 0.05) ** 2))[..., None]
        ao = ao * (1.0 - 0.25 * np.exp(-((d + 0.05) / 0.04) ** 2))
    # each lift is a slightly different batch of concrete
    if lifts:
        idx = np.zeros((h, 1), F)
        for lv in lifts:
            idx += (vm > lv)
        alb *= (1.0 + (hash2(idx, np.zeros_like(idx), seed + 30) - 0.5) * 0.10)[..., None]
    # vertical construction joints every 15 m (also sits on the tiling seam)
    dj = np.abs(np.mod(um + 7.5, 15.0) - 7.5)
    jg = np.exp(-(dj / 0.03) ** 2)
    hm = hm - 0.012 * jg
    alb *= (1.0 - 0.35 * jg)[..., None]
    ao = ao * (1.0 - 0.4 * jg)
    # form tie holes on a 1.5 m x 1.0 m grid
    tu = np.mod(um + 0.75, 1.5) - 0.75
    tv = np.mod(vm + 0.5, 1.0) - 0.5
    hole = np.exp(-((tu * tu + tv * tv) / 0.035 ** 2))
    hm = hm - 0.02 * hole
    alb *= (1.0 - 0.6 * hole)[..., None]
    ao = ao * (1.0 - 0.6 * hole)
    # bug holes / pitting
    pit = sstep(fft_noise(h, w, 0.0, seed + 9, cut=0.22), 2.3, 2.9)
    hm = hm - 0.006 * pit
    alb *= (1.0 - 0.35 * pit)[..., None]
    # water stains
    for i, (sv, ln, st) in enumerate(sources):
        m = streaks(h, w, ppm, sv, ln, seed + 40 + 7 * i, thr=0.15) * st
        alb = mix(alb, alb * np.array([0.50, 0.49, 0.47], F), np.clip(m, 0, 1))
        wet = np.maximum(wet, m)
        m2 = streaks(h, w, ppm, sv, ln * 0.5, seed + 400 + 7 * i, thr=1.1) * st   # lime leach, pale
        alb = mix(alb, [0.74, 0.73, 0.69], np.clip(m2 * 0.55, 0, 1))
    if rust_top:
        top = h / ppm
        m = streaks(h, w, ppm, top, 9.0, seed + 60, thr=0.55) * 0.75
        m += streaks(h, w, ppm, top, 3.0, seed + 61, thr=-0.3) * 0.35
        alb = mix(alb, [0.40, 0.23, 0.12], np.clip(m, 0, 0.8))
    if damp:
        d = np.clip(1.0 - vm / (2.2 + 0.8 * noise1d(w, 1.5, seed + 70)[None, :]), 0, 1) ** 1.5
        d *= 0.75 + 0.25 * fft_noise(h, w, 1.2, seed + 71)
        d = np.clip(d, 0, 1)
        alb = mix(alb, [0.20, 0.20, 0.16], d * 0.75)
        wet = np.maximum(wet, d * 0.6)
    rough = np.clip(0.90 + 0.03 * fft_noise(h, w, 0.8, seed + 80) - 0.12 * wet, 0, 1)
    return dict(alb=np.clip(alb, 0, 1), h=hm, ao=np.clip(ao, 0, 1), rough=rough,
                metal=np.zeros((h, w), F))


def stamp_rivets(arr_list, h, w, pts, rad_px, wrap):
    """Add hemispherical rivet heads. arr_list = [(array, amplitude, kind)], kind 'dome' or 'ring'."""
    r = int(np.ceil(rad_px * 2.2))
    yy, xx = np.mgrid[-r:r + 1, -r:r + 1].astype(F)
    for (py, px) in pts:
        iy, ix = int(round(py)), int(round(px))
        d = np.sqrt((yy - (py - iy)) ** 2 + (xx - (px - ix)) ** 2) / rad_px
        dome = np.sqrt(np.clip(1 - d * d, 0, 1))
        ring = np.exp(-((d - 1.25) / 0.45) ** 2)
        ys = np.arange(iy - r, iy + r + 1)
        xs = np.arange(ix - r, ix + r + 1)
        if wrap:
            xs = np.mod(xs, w)
            okx = np.ones(len(xs), bool)
        else:
            okx = (xs >= 0) & (xs < w)
        oky = (ys >= 0) & (ys < h)
        if not oky.any() or not okx.any():
            continue
        sel = np.ix_(ys[oky], xs[okx])
        for arr, amp, kind in arr_list:
            k = dome if kind == 'dome' else ring
            arr[sel] += amp * k[np.ix_(oky, okx)]


def iron(h, w, ppm, seed, plate_w, plate_h, wrap=True, rivet_pitch=0.25, rivet_r=0.05,
         streak=1.0, plates=True, bottom_dirt=False):
    """Dark riveted iron plate. plate_w must divide the width in metres when wrap=True."""
    vm = (np.arange(h, dtype=F)[:, None] + 0.5) / ppm
    um = (np.arange(w, dtype=F)[None, :] + 0.5) / ppm
    base = np.array([0.205, 0.200, 0.198], F)
    tone = 0.030 * fft_noise(h, w, 2.0, seed) + 0.020 * fft_noise(h, w, 1.0, seed + 1) \
        + 0.012 * fft_noise(h, w, 0.2, seed + 2)
    hm = 0.0010 * fft_noise(h, w, 1.2, seed + 3) + 0.0006 * fft_noise(h, w, 0.0, seed + 4)
    rust = sstep(fft_noise(h, w, 1.6, seed + 5) + 0.5 * fft_noise(h, w, 0.6, seed + 6), 0.9, 2.4) * 0.8
    ao = np.ones((h, w), F)
    riv = np.zeros((h, w), F)
    ring = np.zeros((h, w), F)
    seam_rows = []
    if plates:
        row = np.floor(vm / plate_h)
        uo = um + (np.mod(row, 2) * plate_w * 0.5)
        col = np.floor(uo / plate_w)
        if wrap:
            col = np.mod(col, round(w / ppm / plate_w))
        ph = hash2(np.broadcast_to(row, (h, w)), col, seed + 7)
        tone = tone + (ph - 0.5) * 0.05
        hm = hm + np.where(ph > 0.5, 0.012, 0.0)          # lapped plates
        dv = np.abs(np.mod(vm + plate_h / 2, plate_h) - plate_h / 2)
        du = np.abs(np.mod(uo + plate_w / 2, plate_w) - plate_w / 2)
        seam = np.maximum(np.exp(-(dv / 0.018) ** 2) * np.ones((1, w), F), np.exp(-(du / 0.018) ** 2))
        hm = hm - 0.010 * seam
        ao = ao * (1.0 - 0.55 * seam)
        rust = np.maximum(rust, 0.85 * np.maximum(np.exp(-(dv / 0.06) ** 2) * np.ones((1, w), F),
                                                   np.exp(-(du / 0.05) ** 2)) *
                          sstep(fft_noise(h, w, 1.0, seed + 8), -0.8, 1.0))
        # rivets along both sides of every seam
        pts = []
        nrows = int(np.ceil(h / ppm / plate_h))
        ncols = int(round(w / ppm / plate_w)) + 1
        inset = 0.10
        for r in range(nrows + 1):
            for sgn in (-1, 1):
                y = (r * plate_h + sgn * inset) * ppm
                if 0 < y < h:
                    for i in range(int(w / ppm / rivet_pitch)):
                        pts.append((y, (i + 0.5) * rivet_pitch * ppm))
            seam_rows.append(r * plate_h)
        for r in range(nrows):
            for c in range(-1, ncols + 1):
                x0 = (c * plate_w - (r % 2) * plate_w * 0.5)
                for sgn in (-1, 1):
                    x = (x0 + sgn * inset) * ppm
                    nv = int(plate_h / rivet_pitch)
                    for i in range(1, nv):
                        y = (r * plate_h + i * rivet_pitch) * ppm
                        if 0 < y < h and (wrap or 0 <= x < w):
                            pts.append((y, x))
        stamp_rivets([(riv, 1.0, 'dome'), (ring, 1.0, 'ring')], h, w, pts, rivet_r * ppm, wrap)
        riv = np.clip(riv, 0, 1)
        ring = np.clip(ring, 0, 1)
        hm = hm + 0.035 * riv
        ao = ao * (1.0 - 0.45 * ring * (1 - riv))
        rust = np.maximum(rust, 0.55 * ring * (1 - riv))
    # rust streaks from seams and the top edge
    top = h / ppm
    sm = streaks(h, w, ppm, top, 2.5, seed + 30, thr=0.2) * 0.6
    for i, sv in enumerate(seam_rows):
        if 0 < sv < top:
            sm = np.maximum(sm, streaks(h, w, ppm, sv - 0.08, 1.6, seed + 31 + i, thr=0.1) * 0.8)
    if not plates:
        sm = np.maximum(sm, streaks(h, w, ppm, top * 0.55, 4.0, seed + 35, thr=0.6) * 0.5)
    rust = np.clip(np.maximum(rust, sm * streak), 0, 1)
    alb = np.ones((h, w, 3), F) * base[None, None, :] * (1.0 + tone[..., None] / 0.2)
    alb = mix(alb, [0.235, 0.20, 0.175], sstep(fft_noise(h, w, 1.8, seed + 9), -0.5, 1.5) * 0.6)  # brown patina
    rc = mix(np.ones((h, w, 3), F) * np.array([0.36, 0.20, 0.10], F), [0.20, 0.11, 0.07],
             sstep(fft_noise(h, w, 0.8, seed + 10), -1, 1))
    alb = alb * (1 - rust[..., None]) + rc * rust[..., None]
    alb = alb + (riv * 0.05)[..., None]                    # worn rivet crowns
    alb *= ao[..., None] ** 0.5
    if bottom_dirt:
        d = np.clip(1.0 - vm / 1.8, 0, 1) ** 2 * (0.7 + 0.3 * fft_noise(h, w, 1.2, seed + 11))
        d = np.clip(d, 0, 1) * np.ones((1, w), F)
        alb = mix(alb, [0.30, 0.27, 0.22], d * 0.7)
        rust = np.maximum(rust, d * 0.8)
    hm = hm + 0.0015 * rust * fft_noise(h, w, 0.3, seed + 12)
    rough = np.clip(0.46 + 0.07 * fft_noise(h, w, 1.0, seed + 13) + 0.42 * rust, 0, 1)
    metal = np.clip(0.92 - 0.80 * rust, 0, 1)
    return dict(alb=np.clip(alb, 0, 1), h=hm, ao=np.clip(ao, 0, 1), rough=rough, metal=metal)


def flat(h, w, rgb, rough, metal):
    return dict(alb=np.ones((h, w, 3), F) * np.array(rgb, F), h=np.zeros((h, w), F),
                ao=np.ones((h, w), F), rough=np.full((h, w), rough, F), metal=np.full((h, w), metal, F))


def lens(n):
    yy, xx = np.mgrid[0:n, 0:n].astype(F)
    r = np.sqrt((yy - n / 2 + 0.5) ** 2 + (xx - n / 2 + 0.5) ** 2) / (n / 2)
    glass = (r < 0.82).astype(F)
    alb = mix(np.ones((n, n, 3), F) * np.array([0.16, 0.155, 0.15], F),
              [0.80, 0.78, 0.66], glass * (1.0 - 0.35 * r))
    hm = glass * (0.01 * np.cos(r * 40.0) + 0.05 * (1 - r * r)) / 8.0
    return dict(alb=alb, h=hm.astype(F), ao=1.0 - 0.5 * np.exp(-((r - 0.86) / 0.05) ** 2),
                rough=np.where(glass > 0, 0.18, 0.6).astype(F), metal=np.where(glass > 0, 0.0, 0.9).astype(F))


# ----------------------------------------------------------------------------------------
def finish(d, ppm, wrap_u):
    d['n'] = normal_from_height(d['h'], ppm, wrap_u)
    return d


def rot_leaf(d):
    """Upright (t up, s right) -> atlas with t along +U and s along -V."""
    out = {}
    for k, a in d.items():
        if a.ndim == 3:
            out[k] = np.ascontiguousarray(a.transpose(1, 0, 2)[::-1])
        else:
            out[k] = np.ascontiguousarray(a.T[::-1])
    n = out['n'].copy()
    out['n'][..., 0] = n[..., 1]      # atlas U component = old t component
    out['n'][..., 1] = -n[..., 0]     # atlas V component = -old s component
    return out


def main():
    S = C.S
    ppm = C.PPM
    A = dict(alb=np.zeros((S, S, 3), F), n=np.zeros((S, S, 3), F), ao=np.ones((S, S), F),
             rough=np.ones((S, S), F), metal=np.zeros((S, S), F))
    A['n'][..., 2] = 1.0

    def paste(d, reg, pad):
        for k in A:
            a = d[k]
            pw = ((pad, pad), (0 if reg.tile else pad, 0 if reg.tile else pad)) + (((0, 0),) if a.ndim == 3 else ())
            p = np.pad(a, pw, mode='edge')
            y0 = reg.y0 - pad
            x0 = reg.x0 - (0 if reg.tile else pad)
            ys, xs = max(y0, 0), max(x0, 0)
            ye, xe = min(y0 + p.shape[0], S), min(x0 + p.shape[1], S)
            A[k][ys:ye, xs:xe] = p[ys - y0:ye - y0, xs - x0:xe - x0]

    print('misc row filler / trim iron'); sys.stdout.flush()
    trim = finish(iron(C.MISC_ROW_H, S, 64.0, 11, 3.0, 2.0, wrap=True, plates=False, streak=0.9), 64.0, True)
    for k in A:
        A[k][0:C.MISC_ROW_H] = trim[k]
    print('gate leaf'); sys.stdout.flush()
    sm, tm = C.R_LEAF.size_m
    leaf = iron(int(round(tm * 100)), int(round(sm * 100)), 100.0, 21, 2.0, tm / 6.0, wrap=False,
                rivet_pitch=0.25, rivet_r=0.055, streak=1.0, bottom_dirt=True)
    paste(rot_leaf(finish(leaf, 100.0, False)), C.R_LEAF, 5)
    paste(finish(flat(128, 128, (0.018, 0.018, 0.02), 1.0, 0.0), 64.0, False), C.R_DARK, 4)
    paste(finish(lens(128), 64.0, False), C.R_LENS, 4)
    print('parapet iron'); sys.stdout.flush()
    paste(finish(iron(C.R_PARA.h, S, ppm, 31, 2.5, 2.4, streak=1.0), ppm, True), C.R_PARA, 4)
    print('iron band'); sys.stdout.flush()
    paste(finish(iron(C.R_IRON.h, S, ppm, 41, 3.0, 2.0, streak=1.0), ppm, True), C.R_IRON, 4)
    print('generic concrete'); sys.stdout.flush()
    gh = C.R_GEN.h / ppm
    paste(finish(concrete(C.R_GEN.h, S, ppm, 51, [(gh, 3.0, 0.7), (3.75, 2.0, 0.5), (1.4, 1.4, 0.8)],
                          lifts=[3.0]), ppm, True), C.R_GEN, 4)
    gen_d = concrete(C.R_HOOD.h, C.R_HOOD.w, ppm, 52, [(3.0, 1.2, 0.6)])
    yy, xx = np.mgrid[0:C.R_HOOD.h, 0:C.R_HOOD.w].astype(F)
    slot = (sstep(xx, 38, 42) * sstep(-xx, -218, -214) * sstep(yy, 42, 46) * sstep(-yy, -158, -154)).astype(F)
    gen_d['alb'] = mix(gen_d['alb'], [0.018, 0.018, 0.02], slot)
    gen_d['h'] = gen_d['h'] - 0.06 * slot
    gen_d['ao'] = gen_d['ao'] * (1 - 0.5 * slot)
    paste(finish(gen_d, 64.0, False), C.R_HOOD, 4)
    print('tall concrete'); sys.stdout.flush()
    srcs = [(30.0, 11.0, 0.85), (15.0, 7.0, 0.8)] + [(z, 2.5, 0.45) for z in (3, 6, 9, 12, 18, 21, 24, 27)]
    tall = finish(concrete(C.R_TALL.h, S, ppm, 61, srcs, rust_top=True, damp=True,
                           lifts=[3, 6, 9, 12, 15, 18, 21, 24, 27]), ppm, True)
    paste(tall, C.R_TALL, 4)
    top = C.R_TALL.y0 + C.R_TALL.h
    for k in A:
        A[k][top:] = A[k][top - 1:top]

    # ---- encode ----
    def save(name, rgb, cs):
        img = bpy.data.images.new(name, S, S, alpha=False)
        img.colorspace_settings.name = cs
        px = np.ones((S, S, 4), F)
        px[..., :3] = np.clip(rgb, 0, 1)
        img.pixels.foreach_set(px.ravel())
        img.filepath_raw = C.TEX[name]
        img.file_format = 'PNG'
        img.save()
        print('SAVED', C.TEX[name]); sys.stdout.flush()

    os.makedirs(C.OUT, exist_ok=True)
    save('BaseColor', A['alb'], 'sRGB')
    n = A['n']
    nrm = np.stack([n[..., 0] * 0.5 + 0.5, -n[..., 1] * 0.5 + 0.5, n[..., 2] * 0.5 + 0.5], -1)  # DirectX: green down
    save('Normal', nrm, 'Non-Color')
    save('ORM', np.stack([A['ao'], A['rough'], A['metal']], -1), 'Non-Color')

    # ---- self checks ----
    chk = bpy.data.images.load(C.TEX['Normal'])
    buf = np.empty(S * S * 4, F)
    chk.pixels.foreach_get(buf)
    buf = buf.reshape(S, S, 4)
    dk = buf[C.R_DARK.y0 + 64, C.R_DARK.x0 + 64, :3] * 255
    print('CHECK flat normal pixel (expect 128,128,255):', np.round(dk))
    assert abs(dk[0] - 128) < 2 and abs(dk[1] - 128) < 2 and dk[2] > 250
    # a ridge (pour lift at 15 m in the tall band): just above the crest the surface faces up,
    # which in a DirectX map means green BELOW 0.5
    yc = C.R_TALL.y0 + int(15.0 * ppm)
    above = buf[yc + 2:yc + 4, :, 1].mean()
    below = buf[yc - 4:yc - 2, :, 1].mean()
    print('CHECK green above/below a raised ridge (DirectX expects above<0.5<below): %.3f %.3f' % (above, below))
    assert above < 0.5 < below
    print('TEXTURES OK')


main()
