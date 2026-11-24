"""Procedural tileable base-colour textures for the Oder-Warthe kit (numpy inside Blender 5.2, background).

    Blender5.0.exe -b --factory-startup -P ow_textures.py
Writes D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Textures/T_OW_*.png (sRGB). All maps tile
(noise is filtered white noise in the frequency domain, so periodic). T_OW_PineBark_D is a 512 x 2048 vertical strip:
V 0 -> 1 = 0 -> 21 m up the trunk (charred base, grey-brown lower bark, reddish-grey "fox" upper bark); the top band
V 0.93..1 is silver dead wood for branches. U tiles.
"""
import os
import sys

import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ow_blender_lib import OW_TEX, write_mats_json  # noqa: E402

R = np.random.default_rng(1976)


def noise(h, w, beta=2.0, sx=1.0, sy=1.0, lo_cut=0.0):
    """Tileable fractal noise in 0..1. sx/sy > 1 stretch features along x (columns) / y (rows = V = up)."""
    white = R.standard_normal((h, w))
    fy = np.fft.fftfreq(h)[:, None] * h
    fx = np.fft.fftfreq(w)[None, :] * w
    k = np.sqrt((fx * sx) ** 2 + (fy * sy) ** 2)
    k[0, 0] = 1.0
    filt = 1.0 / np.power(k, beta / 1.0)
    filt[0, 0] = 0.0
    if lo_cut > 0:
        filt[k < lo_cut] = 0.0
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * filt))
    out -= out.min()
    out /= max(out.max(), 1e-9)
    return out


def ss(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    t = t[..., None] if np.ndim(t) == 2 else t
    return a * (1 - t) + b * t


def col(rgb, h, w):
    return np.ones((h, w, 3)) * np.array(rgb)[None, None, :]


def save(name, rgb):
    h, w, _ = rgb.shape
    img = bpy.data.images.new(name, w, h, alpha=False)
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[..., :3] = np.clip(rgb, 0, 1)
    img.pixels.foreach_set(rgba.ravel())   # rows bottom to top, sRGB-encoded bytes
    os.makedirs(OW_TEX, exist_ok=True)
    img.filepath_raw = OW_TEX + name + '.png'
    img.file_format = 'PNG'
    img.save()
    print('saved', name, w, h, 'mean', rgb.reshape(-1, 3).mean(0).round(3))


def concrete(n=1024):
    # tile = 3 m: 1 px = 2.9 mm. Mottled weathered grey, aggregate, board marks, vertical rain streaks.
    base = 0.50 + 0.16 * (noise(n, n, 2.4) - 0.5) + 0.07 * (noise(n, n, 1.4) - 0.5)
    fine = noise(n, n, 0.6)
    base += 0.05 * (fine - 0.5)
    agg = noise(n, n, 0.2)
    base -= 0.10 * ss(0.72, 0.85, agg)
    base += 0.06 * ss(0.78, 0.9, 1 - agg)
    rows = np.arange(n)[:, None] * np.ones((1, n))
    board = (rows % (n // 15))
    jitter = 2 + 3 * noise(n, n, 2.0, sx=6.0)
    base -= 0.025 * (board < jitter)
    streak = noise(n, n, 1.6, sx=1.0, sy=12.0)  # stretched along y (rows = V = up)
    streak = ss(0.55, 0.9, streak)
    base -= 0.16 * streak * (0.6 + 0.4 * noise(n, n, 2.0))
    moss = ss(0.68, 0.85, noise(n, n, 2.2))
    rgb = col((1.0, 0.985, 0.95), n, n) * base[..., None]
    rgb = mix(rgb, rgb * np.array([0.82, 0.9, 0.72]), moss * 0.6)
    save('T_OW_Concrete_D', rgb)


def rust(n=1024):
    # tile = 2 m. Dark brown scale, orange rust blooms, black pits, faint paint remnants (field grey).
    a = noise(n, n, 2.2)
    b = noise(n, n, 1.2)
    rgb = col((0.26, 0.16, 0.10), n, n) * (0.8 + 0.4 * b[..., None])
    orange = ss(0.62, 0.82, a * 0.7 + b * 0.3)
    rgb = mix(rgb, col((0.50, 0.25, 0.10), n, n) * (0.8 + 0.4 * noise(n, n, 0.8)[..., None]), orange)
    black = ss(0.62, 0.8, noise(n, n, 1.8))
    rgb = mix(rgb, col((0.08, 0.07, 0.06), n, n), black * 0.8)
    paint = ss(0.70, 0.8, noise(n, n, 2.6))
    rgb = mix(rgb, col((0.30, 0.31, 0.28), n, n), paint * 0.7)
    pits = ss(0.8, 0.9, noise(n, n, 0.3))
    rgb *= (1 - 0.4 * pits)[..., None]
    streak = ss(0.6, 0.9, noise(n, n, 1.6, sx=1.0, sy=10.0))
    rgb = mix(rgb, col((0.42, 0.20, 0.08), n, n), streak * 0.4)
    save('T_OW_RustSteel_D', rgb)


def wood(n=1024):
    # tile = 2 m, vertical boards 20 cm (102 px). Weathered silver-brown, per-board tone, gaps.
    xs = np.arange(n)[None, :] * np.ones((n, 1))
    bw = n // 10
    board = np.minimum((xs // bw).astype(int), 9)
    tone = R.uniform(0.82, 1.15, 10)[board]
    grain = noise(n, n, 1.4, sx=1.0, sy=25.0)
    v = (0.40 + 0.18 * (grain - 0.5)) * tone
    v += 0.05 * (noise(n, n, 2.0) - 0.5)
    rgb = col((1.0, 0.92, 0.80), n, n) * v[..., None]
    gap = (xs % bw) < 3
    rgb[gap] *= 0.35
    knots = ss(0.86, 0.93, noise(n, n, 0.5))
    rgb *= (1 - 0.45 * knots)[..., None]
    dark = ss(0.55, 0.9, noise(n, n, 2.0, sx=1.0, sy=5.0))
    rgb *= (1 - 0.35 * dark)[..., None]
    save('T_OW_Wood_D', rgb)


def granite(n=1024):
    # tile = 2.5 m. Grey speckled granite, pale grey-green and ochre lichen, dark crust.
    base = 0.50 + 0.12 * (noise(n, n, 2.0) - 0.5)
    spk = noise(n, n, 0.1)
    base -= 0.18 * ss(0.62, 0.75, spk)
    base += 0.12 * ss(0.65, 0.78, 1 - spk)
    rgb = col((1.0, 0.98, 0.95), n, n) * base[..., None]
    lich = ss(0.52, 0.66, noise(n, n, 2.0) * 0.7 + noise(n, n, 1.0) * 0.3)
    rgb = mix(rgb, col((0.60, 0.64, 0.54), n, n) * (0.85 + 0.3 * noise(n, n, 0.7))[..., None], lich * 0.55)
    ochre = ss(0.75, 0.85, noise(n, n, 1.6))
    rgb = mix(rgb, col((0.58, 0.53, 0.34), n, n), ochre * 0.45)
    crust = ss(0.7, 0.85, noise(n, n, 1.8))
    rgb = mix(rgb, col((0.22, 0.22, 0.20), n, n), crust * 0.4)
    save('T_OW_Granite_D', rgb)


def earth(n=1024):
    # tile = 4 m. Sandy glacial soil, low-contrast dry heath, humus specks, straw, pebbles (no big blotches).
    rgb = col((0.47, 0.41, 0.31), n, n) * (0.92 + 0.16 * noise(n, n, 1.0)[..., None])
    rgb *= (0.94 + 0.12 * noise(n, n, 2.2)[..., None])
    heath = ss(0.45, 0.75, noise(n, n, 1.7)) * 0.35
    rgb = mix(rgb, col((0.37, 0.35, 0.25), n, n), heath)
    hum = ss(0.72, 0.84, noise(n, n, 0.5)) * 0.45
    rgb = mix(rgb, col((0.24, 0.21, 0.16), n, n), hum)
    straw = ss(0.78, 0.88, noise(n, n, 0.4, sx=1.0, sy=3.0)) * 0.35
    rgb = mix(rgb, col((0.62, 0.56, 0.40), n, n), straw)
    peb = ss(0.85, 0.9, noise(n, n, 0.15)) * 0.5
    rgb = mix(rgb, col((0.64, 0.61, 0.55), n, n), peb)
    save('T_OW_Earth_D', rgb)


def pine_bark(w=512, h=2048):
    # U: 512 px over ~60 cm of circumference; V: 2048 px over 21 m. Plates elongated along the trunk.
    v = (np.arange(h)[:, None] + 0.5) / h * np.ones((1, w))
    plates = noise(h, w, 1.5, sx=1.0, sy=8.0)
    fiss = ss(0.35, 0.18, noise(h, w, 1.1, sx=1.0, sy=12.0))
    flakes = noise(h, w, 1.0, sx=1.0, sy=3.0)
    # zones (V): char < ~0.11 with a ragged edge, grey-brown lower bark to ~0.33, fox bark above ~0.45
    edge = 0.11 + 0.045 * (noise(h, w, 2.0, sx=1.0, sy=4.0) - 0.5) * 2
    char = ss(edge + 0.012, edge - 0.012, v)
    fox = ss(0.30, 0.48, v + 0.05 * (noise(h, w, 2.0, sx=1.0, sy=4.0) - 0.5))
    lower = col((0.34, 0.28, 0.24), h, w) * (0.75 + 0.5 * plates[..., None])
    lower *= (1 - 0.55 * fiss)[..., None]
    upper = col((0.50, 0.36, 0.29), h, w) * (0.8 + 0.35 * flakes[..., None])
    upper = mix(upper, col((0.50, 0.46, 0.42), h, w), ss(0.5, 0.8, flakes) * 0.65)  # weathered grey flakes
    upper *= (1 - 0.25 * fiss)[..., None]
    rgb = mix(lower, upper, fox)
    charc = col((0.045, 0.040, 0.036), h, w) * (0.8 + 0.6 * ss(0.7, 0.9, plates)[..., None])
    charc *= (1 - 0.5 * fiss)[..., None]
    rgb = mix(rgb, charc, char)
    soot = ss(0.30, 0.12, v) * 0.5  # smoke-darkened just above the char line
    rgb *= (1 - soot * (1 - char))[..., None]
    # silver dead wood band for branches (V 0.93..1)
    band = v > 0.93
    grain = noise(h, w, 1.2, sx=1.0, sy=20.0)
    dead = col((0.36, 0.33, 0.30), h, w) * (0.75 + 0.45 * grain[..., None])
    rgb[band] = dead[band]
    save('T_OW_PineBark_D', rgb)


write_mats_json()
concrete()
rust()
wood()
granite()
earth()
pine_bark()
print('OW_TEXTURES_DONE')
