"""Info takeover style options F, G, H (round 2, 2026-11-12): black / dark gunmetal in the Frontstahl family with no
neon cyan and no brass. Accents come from the Frontstahl skin's own tones (UISkins/_generator/themes_coh.py,
13_Frontstahl_Gunmetal): polish / paint bone (#c9c7bd, #d8d4c6), rim steel (#aab3b9), the dull signal red of
`danger` (#c8382a) and the olive of `tertiary` (#b9c86a), the last two desaturated.

  F  Schwarzstahl   blackened gunmetal, recessed screwed slot plates, chrome rims; steel labels, bone pilot lamps.
  G  Rotsignal      black-oxide plate, raised gunmetal placard with knurled end caps; dull red labels and lamps.
  H  Feldgrau       black-olive wrinkle paint, bakelite placard in a chrome bezel; olive labels and lamps.

Same layout contract as gen_style_art.py (1240 x 920 plate, 3-slice bands, placard 26..1214 x 16..102 with the page
lamps at its right end, footer rail in the last 100 rows). Writes T_TKO_<X>_Plate / _Button_<n|h|p> /
_PageLamp_<on|off> into Saved/InfoTakeover/Style/art/. Run: python gen_style_art_dark.py [F G H]
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_style_art as S  # noqa: E402

W, H = S.W, S.H
srgb, mix, sat, smoothstep = S.srgb, S.mix, S.sat, S.smoothstep
noise, fbm, grid_xy, blur, disc = S.noise, S.fbm, S.grid_xy, S.blur, S.disc
PLACARD = (26, 16, W - 26, 102)
FOOTER_RAIL = (26, S.FOOTER_TOP + 4, W - 26, H - 22)

# Frontstahl tones (themes_coh.py 13_Frontstahl_Gunmetal)
BONE = '#d8d4c6'
POLISH = '#c9c7bd'
STEEL = '#aab3b9'
RED = '#a8382c'          # danger #c8382a, a little darker and duller
OLIVE = '#9aa36e'        # tertiary #b9c86a, desaturated

LOOK = {
    'F': dict(lamp_on=BONE, lamp_off='#1d1f21', strip=BONE, strip_level=0.45),
    'G': dict(lamp_on=RED, lamp_off='#2a1512', strip=RED, strip_level=0.55),
    'H': dict(lamp_on=OLIVE, lamp_off='#1c2016', strip=OLIVE, strip_level=0.5),
}


def steel_bevel(rgb, d, width, strength=1.0, light='#8f979c', dark='#202325'):
    """A polished steel rim `width` texels wide along a rect's inside edge (d = signed distance, negative inside)."""
    h, w = rgb.shape[:2]
    band = sat(d + width + 0.5) * sat(0.5 - d)
    lit = S.CP.edge_light(w, h, inward=False)
    crest = np.exp(-((-d / max(width, 1.0) - 0.35) / 0.25) ** 2)
    metal = srgb(dark) + (srgb(light) - srgb(dark)) * ((0.25 + 0.75 * lit) * (0.4 + 0.6 * crest))[..., None]
    return mix(rgb, metal, (band * strength)[..., None])


def screws(rgb, rect, inset=11, radius=3.8):
    x0, y0, x1, y1 = rect
    pts = [(x0 + inset, y0 + inset), (x1 - inset, y0 + inset), (x0 + inset, y1 - inset), (x1 - inset, y1 - inset)]
    return S.bolts(rgb, pts, radius, slot=True, metal=('#3c4144', '#c9cdcf'))


def recessed_slot(rgb, rect, face, seed, radius=5.0, rim=3.0):
    """Frontstahl recessed screwed slot plate: a darker face sunk behind a thin steel bevel."""
    h, w = rgb.shape[:2]
    m, d = S.rect_mask(h, w, *rect, radius)
    xs, ys = grid_xy(h, w)
    face_rgb = srgb(face) * (1 + noise(h, w, 1.2, seed, 50)[..., None] * 0.02 + fbm(h, w, 90, seed + 1)[..., None] * 0.04)
    sink = np.exp(-np.clip(ys - rect[1], 0, None) / 10.0) * 0.45 + np.exp(-np.clip(xs - rect[0], 0, None) / 12.0) * 0.25
    face_rgb = face_rgb * (1 - np.clip(sink, 0, 0.6))[..., None]
    rgb = rgb * (1 - 0.4 * np.clip(blur(m, 2.5) - m, 0, 1)[..., None])
    rgb = mix(rgb, face_rgb, m[..., None])
    rgb = steel_bevel(rgb, d, rim, 0.9)
    return screws(rgb, rect)


def raised_plate(rgb, rect, face, seed, radius=4.0):
    """A raised gunmetal plate: lit top edge, dark bottom edge, a soft drop shadow."""
    h, w = rgb.shape[:2]
    m, d = S.rect_mask(h, w, *rect, radius)
    shadow = blur(np.roll(m, 3, axis=0), 4.0)
    rgb = rgb * (1 - 0.55 * np.clip(shadow - m, 0, 1)[..., None])
    xs, ys = grid_xy(h, w)
    face_rgb = srgb(face) * (1.06 - 0.12 * sat((ys - rect[1]) / max(rect[3] - rect[1], 1)))[..., None]
    face_rgb = face_rgb * (1 + noise(h, w, 1.2, seed, 60)[..., None] * 0.02 + fbm(h, w, 70, seed + 1)[..., None] * 0.05)
    rgb = mix(rgb, face_rgb, m[..., None])
    top = np.exp(-((ys - rect[1] - 1.0) / 1.0) ** 2) * m
    bottom = np.exp(-((rect[3] - ys - 1.0) / 1.0) ** 2) * m
    rgb = rgb + 0.10 * top[..., None]
    return rgb * (1 - 0.5 * bottom[..., None])


def knurl(rgb, rect, pitch=4.0, strength=0.5):
    """Diagonal knurl (crossed fine ridges) inside rect, the Frontstahl rail finish."""
    h, w = rgb.shape[:2]
    m, _ = S.rect_mask(h, w, *rect, 1.0)
    xs, ys = grid_xy(h, w)
    a = np.abs(((xs + ys) % pitch) / pitch - 0.5) * 2
    b = np.abs(((xs - ys) % pitch) / pitch - 0.5) * 2
    ridge = np.minimum(a, b)
    shade = 0.55 + 0.45 * ridge
    metal = srgb('#5c6266') * shade[..., None]
    return mix(rgb, metal, (m * strength)[..., None])


def nicks(rgb, dist, seed, amount=0.35):
    """Bright bare-metal chips along the outer rim, like the Frontstahl rims."""
    h, w = rgb.shape[:2]
    chips = smoothstep(fbm(h, w, 7, seed, octaves=3), 0.45, 0.6) * sat(dist + 6.5) * sat(0.5 - dist)
    return mix(rgb, srgb(POLISH), (chips * amount)[..., None])


def spatter(rgb, seed, amount=0.18):
    """Faint oil spatter: dark blots near the lower edge, kept out of the text zones."""
    h, w = rgb.shape[:2]
    xs, ys = grid_xy(h, w)
    blots = smoothstep(fbm(h, w, 22, seed, octaves=3), 0.5, 0.75)
    where = smoothstep(ys, h * 0.55, h) * (1 - smoothstep(xs, 120, 360) * (1 - smoothstep(xs, 880, 1120)))
    return rgb * (1 - amount * (blots * where)[..., None])


# ======================================================================== F  Schwarzstahl

def plate_f():
    rgb, cov, dist = S.gunmetal_plate(71, field_hex='#141618', rim=5.0, grain=0.008, mottle=0.03)
    rgb = nicks(rgb, dist, 72, 0.18)
    rgb = spatter(rgb, 73)
    rgb = recessed_slot(rgb, PLACARD, '#0d0f10', 74)
    rgb = recessed_slot(rgb, FOOTER_RAIL, '#0f1112', 75)
    rgb = S.bolts(rgb, [(12, 12), (W - 12, 12), (12, H - 12), (W - 12, H - 12)], 4.5)
    corner = lambda x, y: max(0.0, 1 - min(min(x, W - x), min(y, H - y)) / 80.0)
    rgb = rgb + S.hairline_scratches(H, W, 76, 240, (5, 22), 0.08, corner)[..., None] * srgb(POLISH)
    return rgb, cov


# ======================================================================== G  Rotsignal

def plate_g():
    rgb, cov, dist = S.gunmetal_plate(81, field_hex='#0f1112', rim=6.0, grain=0.007, mottle=0.05)
    rgb = nicks(rgb, dist, 82, 0.25)
    rgb = spatter(rgb, 83, 0.22)
    rgb = raised_plate(rgb, PLACARD, '#25282b', 84)
    x0, y0, x1, y1 = PLACARD
    rgb = knurl(rgb, (x1 - 9, y0 + 3, x1 - 3, y1 - 3), 3.0, 0.8)
    # one dull red index tab, the only colour on the plate (CoH-style header marker)
    tab, _ = S.rect_mask(H, W, x0 + 5, y0 + 14, x0 + 8, y1 - 14, 1.0)
    rgb = mix(rgb, srgb(RED), (tab * 0.9)[..., None])
    rgb = raised_plate(rgb, FOOTER_RAIL, '#1f2225', 85)
    rgb = knurl(rgb, (FOOTER_RAIL[0] + 3, FOOTER_RAIL[1] + 3, FOOTER_RAIL[0] + 9, FOOTER_RAIL[3] - 3), 3.0, 0.8)
    rgb = knurl(rgb, (FOOTER_RAIL[2] - 9, FOOTER_RAIL[1] + 3, FOOTER_RAIL[2] - 3, FOOTER_RAIL[3] - 3), 3.0, 0.8)
    rgb = S.bolts(rgb, [(13, 13), (W - 13, 13), (13, H - 13), (W - 13, H - 13)], 5.0)
    return rgb, cov


# ======================================================================== H  Feldgrau

def plate_h():
    h, w = H, W
    rim = 5.0
    dist = S.CP.rounded_rect_distance(w, h, 10.0, inset=1.0)
    cov = np.clip(0.5 - dist, 0, 1)
    chrome, rim_mask, _ = S.chrome_frame(dist, rim)
    xs, ys = grid_xy(h, w)
    paint = srgb('#1a1c1a') * (1 + (noise(h, w, 1.6, 91) * 0.5 + noise(h, w, 3.0, 92) * 0.5)[..., None] * 0.06
                               + fbm(h, w, 260, 93)[..., None] * 0.04)
    paint = paint * (1 - S.inner_shadow(h, w, rim + 1, 8.0, 0.45))[..., None]
    chips = smoothstep(fbm(h, w, 9, 94, octaves=3), 0.44, 0.56) * sat(dist + 16) * cov
    paint = mix(paint, srgb('#5e6460'), (chips * 0.55)[..., None])
    rgb = paint * (1 - rim_mask[..., None]) + chrome * rim_mask[..., None]
    # engraved double keyline around the body, quiet olive
    for inset in (14.0, 17.0):
        line = sat(1.0 - np.abs(-dist - inset)) * cov
        rgb = mix(rgb, srgb(OLIVE) * 0.55, (line * 0.35)[..., None])
    # bakelite placard and footer in thin chrome bezels
    for rect, seed in ((PLACARD, 95), (FOOTER_RAIL, 96)):
        m, d = S.rect_mask(h, w, *rect, 8.0)
        face = srgb('#0c0d0c') * (1 + noise(h, w, 2.0, seed, 1)[..., None] * 0.02)
        sheen = np.exp(-((ys - rect[1] - 10) / 18.0) ** 2) * 0.05
        rgb = rgb * (1 - 0.45 * np.clip(blur(m, 3.0) - m, 0, 1)[..., None])
        rgb = mix(rgb, face + sheen[..., None], m[..., None])
        rgb = steel_bevel(rgb, d, 2.5, 1.0, light='#b4bcc0', dark='#2a2e30')
        rgb = screws(rgb, rect, inset=12, radius=3.6)
    rgb = S.bolts(rgb, [(10, 40), (W - 10, 40), (10, H - 40), (W - 10, H - 40)], 3.4, slot=True,
                  metal=('#3b413b', '#b7beb6'))
    return rgb, cov


# ======================================================================== lamps and buttons

def lamp_art(key, on):
    look = LOOK[key]

    def draw(base):
        h, w = base.shape[:2]
        c = S.LAMP_TEX / 2.0
        ring = disc(h, w, c, c, 20.0) - disc(h, w, c, c, 15.0)
        lit = S.CP.edge_light(w, h, inward=False)
        base = mix(base, srgb('#5d6468') + (srgb(STEEL) - srgb('#5d6468')) * lit[..., None], (ring * 0.95)[..., None])
        dome = disc(h, w, c, c, 15.0)
        if on:
            base = S.lamp(base, dome, look['lamp_on'], 0.85, 7.0, 0.45)
        else:
            base = mix(base, srgb(look['lamp_off']), dome[..., None])
        return base + 0.28 * disc(h, w, c - 5.0, c - 5.0, 4.4)[..., None]

    black = np.clip(draw(np.zeros((S.LAMP_TEX, S.LAMP_TEX, 3))), 0, 1)
    white = np.clip(draw(np.ones((S.LAMP_TEX, S.LAMP_TEX, 3))), 0, 1)
    alpha = np.clip(1.0 - (white - black).mean(-1), 0, 1)
    return np.clip(black / np.maximum(alpha, 1e-4)[..., None], 0, 1), alpha


def button_art(key, state):
    """Frontstahl confirm key: gunmetal face, chrome rim, the option's lamp strip and jewels; no cyan, no brass."""
    look = LOOK[key]
    w, h = S.BW, S.BH
    dist = S.CP.rounded_rect_distance(w, h, 14.0, inset=1.0)
    cov = np.clip(0.5 - dist, 0, 1)
    xs, ys = grid_xy(h, w)
    face = {'F': '#25282b', 'G': '#202326', 'H': '#1d1f1c'}[key]
    lift = {'n': 0.0, 'h': 0.22, 'p': -0.25}[state]
    field = srgb(face) * (1 + lift) * (1.10 - 0.22 * (ys / h))[..., None]
    field = field + noise(h, w, 1.2, 101, 40)[..., None] * 0.012
    field = field * (1 - S.inner_shadow(h, w, 9, 6.0, 0.5))[..., None]
    chrome, _ = S.VS.chrome_rim(dist, 8.0, w, h)
    if state == 'h':
        chrome = mix(chrome, srgb(POLISH) * (0.4 + 0.75 * chrome.mean(-1, keepdims=True)), 0.35)
    if state == 'p':
        chrome = chrome * 0.8
    rim_mask = np.clip(dist + 8.5, 0, 1) * cov
    rgb = field * (1 - rim_mask[..., None]) + chrome * rim_mask[..., None]
    rgb = rgb * (1 - 0.5 * (np.exp(-((-dist - 8.0) / 0.8) ** 2) * cov)[..., None])
    level = look['strip_level'] * {'n': 1.0, 'h': 1.5, 'p': 1.3}[state]
    reach = sat((xs - 44) / 16.0) * sat((w - 44 - xs) / 16.0)
    strip = (np.abs(ys - (h - 16.0)) < 1.6).astype(float) * reach
    rgb = S.lamp(rgb, strip, look['strip'], level, 3.0, 0.3)
    for jx in (32.0, w - 32.0):
        r = np.hypot(xs - jx, ys - h / 2.0)
        ring = np.clip(7.0 - r, 0, 1) * (1 - np.clip(5.0 - r, 0, 1))
        rgb = mix(rgb, srgb('#5d666c'), (ring * 0.9)[..., None])
        rgb = S.lamp(rgb, np.clip(5.0 - r, 0, 1), look['strip'], level, 3.0, 0.3)
    return rgb, cov


PLATES = {'F': plate_f, 'G': plate_g, 'H': plate_h}


def main(only=None):
    for key, fn in PLATES.items():
        if only and key not in only:
            continue
        rgb, cov = fn()
        S.save_rgba('T_TKO_%s_Plate' % key, rgb, cov)
        for state in ('n', 'h', 'p'):
            S.save_rgba('T_TKO_%s_Button_%s' % (key, state), *button_art(key, state))
        for state, on in (('on', True), ('off', False)):
            lrgb, lalpha = lamp_art(key, on)
            data = np.dstack([lrgb, lalpha])
            Image.fromarray((data * 255 + 0.5).astype(np.uint8), 'RGBA').save(
                os.path.join(S.OUT, 'T_TKO_%s_PageLamp_%s.png' % (key, state)))
        print('lamps', key, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or None)
