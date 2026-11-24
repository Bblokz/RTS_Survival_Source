"""Info takeover panel plate art and UNDERSTOOD button art (plain python: numpy, cv2, Pillow).

The live look is option D (instrument panel), picked 2026-11-12; A, B, C and E are the other candidates of that round
(mockups in Saved/InfoTakeover/StyleOptions/). Run `python gen_style_art.py D`, then build_takeover_style.py.

Writes into Saved/InfoTakeover/Style/art/:
  T_TKO_<X>_Plate.png       1240 x 920 texels = slate units of the takeover panel (UW_InfoTakeover PanelWidth 1240,
                            PanelMaxHeight 920). M_TKO_GlassOverArt draws it with a vertical 3-slice: the top
                            TOP_BAND rows stay at the top, the bottom BOTTOM_BAND rows stay at the bottom, the middle is
                            cropped (shorter panels) with a short cross-fade, so nothing is ever stretched.
  T_TKO_<X>_Button_<n|h|p>.png   560 x 96 (2 texels per unit of the 280 x 48 UNDERSTOOD button).

Layout facts the art follows (slate units from the panel's top / bottom edge, the same for every takeover):
  - title text 30..~88, C++ divider line at y 94 (35 % cyan) from x 40 to 1200;
  - typewritten description from y ~112; body panes below it;
  - footer: checkbox + UNDERSTOOD in the last ~100 units, button 74..26 from the bottom.
The title and description sit straight on the plate, so the art keeps the area under them dark and quiet.

Reuses the Frontstahl part helpers (UISkins/_generator/variant_selection_parts.py, custom_parts.py).
"""
import math
import os
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..', 'UISkins', '_generator'))
sys.path.insert(0, GEN)
import custom_parts as CP  # noqa: E402
import variant_selection_parts as VS  # noqa: E402

OUT = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..', 'Saved', 'InfoTakeover', 'Style', 'art'))
W, H = 1240, 920
TOP_BAND = 150
BOTTOM_BAND = 124
DIVIDER_Y = 94
FOOTER_TOP = H - 100
BW, BH = 560, 96

srgb = CP.srgb


def mix(a, b, t):
    return a + (b - a) * t


def sat(x):
    return np.clip(x, 0, 1)


def smoothstep(x, lo, hi):
    t = sat((x - lo) / max(hi - lo, 1e-6))
    return t * t * (3 - 2 * t)


def noise(h, w, cell, seed, aspect=1.0):
    """Smooth value noise in -1..1 with features of `cell` texels (x stretched by aspect)."""
    rng = np.random.default_rng(seed)
    gh = max(int(h / cell) + 3, 3)
    gw = max(int(w / (cell * aspect)) + 3, 3)
    grid = rng.standard_normal((gh, gw)).astype(np.float32)
    up = cv2.resize(grid, (int(gw * cell * aspect), int(gh * cell)), interpolation=cv2.INTER_CUBIC)
    return np.clip(up[:h, :w] / 1.2, -1.5, 1.5).astype(float)


def fbm(h, w, cell, seed, octaves=4, aspect=1.0):
    total, amp, norm = np.zeros((h, w)), 1.0, 0.0
    for o in range(octaves):
        total += noise(h, w, max(cell / (2 ** o), 1.5), seed + o * 17, aspect) * amp
        norm += amp
        amp *= 0.5
    return total / norm


def grid_xy(h, w):
    ys, xs = np.mgrid[0:h, 0:w].astype(float) + 0.5
    return xs, ys


def blur(x, sigma):
    return cv2.GaussianBlur(x.astype(np.float32), (0, 0), sigma).astype(float)


def text_layer(h, w, items):
    """items: (text, x, y, size, font file, anchor). Returns a 0..1 coverage map."""
    img = Image.new('L', (w, h), 0)
    draw = ImageDraw.Draw(img)
    for text, x, y, size, font, anchor in items:
        f = ImageFont.truetype(os.path.join('C:/Windows/Fonts', font), size)
        draw.text((x, y), text, fill=255, font=f, anchor=anchor)
    return np.asarray(img).astype(float) / 255.0


def save_rgba(name, rgb, alpha):
    os.makedirs(OUT, exist_ok=True)
    rgb = VS.finish(rgb, alpha)
    data = np.dstack([sat(rgb), sat(alpha)])
    Image.fromarray((data * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, name + '.png'))
    print('wrote', name, data.shape[1], 'x', data.shape[0], flush=True)


def chrome_frame(dist, rim, light_hex='#aab3b9', dark_hex='#2b2f33'):
    chrome, crest = VS.chrome_rim(dist, rim, W if dist.shape[1] == W else dist.shape[1], dist.shape[0],
                                  dark_hex=dark_hex, light_hex=light_hex)
    rim_mask = np.clip(dist + rim + 0.5, 0, 1) * np.clip(0.5 - dist, 0, 1)
    return chrome, rim_mask, crest


def inner_shadow(h, w, inset, falloff, strength):
    xs, ys = grid_xy(h, w)
    d = {'top': ys - inset, 'left': xs - inset, 'right': w - inset - xs, 'bottom': h - inset - ys}
    weight = {'top': 1.0, 'left': 0.8, 'right': 0.35, 'bottom': 0.25}
    s = np.zeros((h, w))
    for k, v in d.items():
        s = np.maximum(s, weight[k] * np.exp(-np.clip(v, 0, None) / falloff))
    return s * strength


def hairline(h, w, y, x0, x1, thick=1.0):
    xs, ys = grid_xy(h, w)
    reach = smoothstep(xs, x0, x0 + 20) * (1 - smoothstep(xs, x1 - 20, x1))
    return sat(1.0 - np.abs(ys - y) / thick) * reach


def groove(rgb, h, w, y, x0, x1, depth=0.45, light=0.12):
    """Engraved line: dark cut with a lit lip just below (light from the top)."""
    dark = hairline(h, w, y, x0, x1, 1.0)
    lit = hairline(h, w, y + 1.5, x0, x1, 0.8)
    rgb = rgb * (1 - depth * dark[..., None])
    return rgb + light * lit[..., None]


def bolts(rgb, points, radius, slot=True, metal=('#4f575e', '#dfe6ea')):
    h, w = rgb.shape[:2]
    xs, ys = grid_xy(h, w)
    for cx, cy in points:
        dx, dy = xs - cx, ys - cy
        r = np.hypot(dx, dy)
        seat = np.clip(radius + 2.0 - r, 0, 1)
        head = np.clip(radius - r, 0, 1)
        shade = np.clip(0.6 - (dx + dy) / (radius * 2.6), 0.12, 1.0)
        steel = srgb(metal[0]) + (srgb(metal[1]) - srgb(metal[0])) * shade[..., None]
        if slot:
            cut = (np.abs(dx - dy) < 1.0) & (r < radius - 1.2)
            steel = steel * (1 - 0.65 * cut[..., None])
        rgb = rgb * (1 - 0.55 * seat[..., None]) * (1 - head[..., None]) + steel * head[..., None]
    return rgb


def brackets(h, w, inset, length, thick, corners=('tl', 'tr', 'bl', 'br')):
    xs, ys = grid_xy(h, w)
    out = np.zeros((h, w))
    for c in corners:
        dx = (xs - inset) if 'l' in c else (w - inset - xs)
        dy = (ys - inset) if c[0] == 't' else (h - inset - ys)
        horiz = (dy >= 0) & (dy < thick) & (dx >= 0) & (dx < length)
        vert = (dx >= 0) & (dx < thick) & (dy >= 0) & (dy < length)
        out = np.maximum(out, (horiz | vert).astype(float))
    return out


def lamp(rgb, shape, colour, intensity, glow_sigma=4.0, glow_amount=0.5):
    accent = srgb(colour)
    core = mix(accent * 0.85, np.array([0.9, 1.0, 1.0]), 0.35)
    glow = blur(shape, glow_sigma)
    rgb = rgb + accent * (glow * glow_amount * intensity)[..., None]
    return mix(rgb, core * (0.35 + 0.65 * intensity), (shape * min(1.0, 0.4 + intensity))[..., None])


def disc(h, w, cx, cy, r):
    xs, ys = grid_xy(h, w)
    return sat(r - np.hypot(xs - cx, ys - cy) + 0.5)


def rect_mask(h, w, x0, y0, x1, y1, radius=0.0, soft=0.5):
    xs, ys = grid_xy(h, w)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    qx = np.abs(xs - cx) - ((x1 - x0) / 2 - radius)
    qy = np.abs(ys - cy) - ((y1 - y0) / 2 - radius)
    d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - radius
    return sat(soft - d), d


def hairline_scratches(h, w, seed, count, length, bright, region=None, angle=(-0.5, 0.5), thick=1):
    rng = np.random.default_rng(seed)
    layer = np.zeros((h, w), np.float32)
    for _ in range(count):
        x = rng.uniform(0, w)
        y = rng.uniform(0, h)
        if region is not None and region(x, y) < rng.random():
            continue
        a = rng.uniform(*angle) + (math.pi / 2 if rng.random() < 0.2 else 0)
        L = rng.uniform(*length)
        p0 = (int((x - math.cos(a) * L / 2) * 4), int((y - math.sin(a) * L / 2) * 4))
        p1 = (int((x + math.cos(a) * L / 2) * 4), int((y + math.sin(a) * L / 2) * 4))
        cv2.line(layer, p0, p1, float(rng.uniform(0.4, 1.0)), thick, cv2.LINE_AA, shift=2)
    return layer.astype(float) * bright


# ======================================================================== shared gunmetal plate

def gunmetal_plate(seed, field_hex='#15181b', rim=5.0, radius=10.0, grain=0.010, mottle=0.035):
    dist = CP.rounded_rect_distance(W, H, radius, inset=1.0)
    coverage = np.clip(0.5 - dist, 0, 1)
    chrome, rim_mask, _ = chrome_frame(dist, rim)
    field = srgb(field_hex) + np.zeros((H, W, 3))
    brushed = noise(H, W, 1.2, seed, aspect=60.0) * grain
    mott = fbm(H, W, 220, seed + 5) * mottle
    field = field * (1 + mott[..., None]) + brushed[..., None]
    xs, ys = grid_xy(H, W)
    field = field * (1.04 - 0.10 * (ys / H))[..., None]
    field = field * (1 - inner_shadow(H, W, rim + 1, 7.0, 0.45))[..., None]
    rgb = field * (1 - rim_mask[..., None]) + chrome * rim_mask[..., None]
    seam = np.exp(-((-dist - rim) / 0.8) ** 2) * coverage
    rgb = rgb * (1 - 0.5 * seam[..., None])
    outline = np.clip(1.0 - np.abs(dist + 0.5), 0, 1)
    rgb = rgb * (1 - 0.7 * outline[..., None])
    return rgb, coverage, dist


# ======================================================================== A  Frontstahl

def plate_a():
    rgb, cov, dist = gunmetal_plate(11)
    h, w = H, W
    xs, ys = grid_xy(h, w)
    # raised header strip behind the title, edge on the divider line
    strip = sat(DIVIDER_Y + 0.5 - ys) * sat(ys - 6) * sat(xs - 6) * sat(w - 6 - xs)
    rgb = mix(rgb, rgb * 1.32 + 0.010, (strip * 0.9)[..., None])
    rgb = groove(rgb, h, w, DIVIDER_Y + 1, 8, w - 8, depth=0.55, light=0.05)
    # recessed footer rail
    foot = sat(ys - FOOTER_TOP) * sat(h - 6 - ys)
    rgb = rgb * (1 - 0.22 * foot[..., None])
    rgb = groove(rgb, h, w, FOOTER_TOP, 8, w - 8, depth=0.5, light=0.07)
    # corner bolts and cyan brackets
    rgb = bolts(rgb, [(20, 20), (w - 20, 20)], 5.5)
    br = brackets(h, w, 12, 34, 2.5, ('bl', 'br'))
    rgb = lamp(rgb, br, VS.ACCENT, 0.55, 3.0, 0.3)
    # faint polish scratches near the corners only
    corner = lambda x, y: max(0.0, 1 - min(min(x, w - x), min(y, h - y)) / 90.0)
    sc = hairline_scratches(h, w, 3, 260, (6, 26), 0.10, corner)
    rgb = rgb + sc[..., None] * srgb('#c9c7bd')
    return rgb, cov


# ======================================================================== B  Rauchglas (smoked glass sheet)

def plate_b():
    h, w = H, W
    dist = CP.rounded_rect_distance(w, h, 8.0, inset=1.0)
    cov = np.clip(0.5 - dist, 0, 1)
    frame_w = 12.0
    chrome, rim_mask, _ = chrome_frame(dist, 4.0)
    xs, ys = grid_xy(h, w)
    # steel frame band
    frame = np.clip(dist + frame_w + 0.5, 0, 1) * cov
    steel = srgb('#202427') * (1 + fbm(h, w, 40, 3)[..., None] * 0.05) + noise(h, w, 1.2, 4, 50)[..., None] * 0.012
    # smoked glass field: cool gradient, a backlight glow behind the title, a faint backlit grid
    t = sat(xs / w * 0.45 + ys / h * 0.55)
    glass = mix(srgb('#132129'), srgb('#080d10'), t[..., None])
    glow = np.exp(-(((xs - w * 0.32) / (w * 0.55)) ** 2 + ((ys - 40) / 300.0) ** 2))
    glass = glass + srgb('#1d6f86') * (glow * 0.10)[..., None]
    gx = sat(1.0 - np.abs(((xs + 0.0) % 40) - 0.5))
    gy = sat(1.0 - np.abs(((ys + 0.0) % 40) - 0.5))
    grid = np.maximum(gx, gy)
    quiet = 1 - 0.65 * smoothstep(ys, 20, 60) * (1 - smoothstep(ys, 220, 300))
    glass = glass + srgb('#38c8ff') * (grid * 0.022 * quiet)[..., None]
    glass = glass * (1 + fbm(h, w, 300, 9)[..., None] * 0.05)
    glass = glass * (1 - inner_shadow(h, w, frame_w + 1, 10.0, 0.55))[..., None]
    rgb = glass * (1 - frame[..., None]) + steel * frame[..., None]
    rgb = rgb * (1 - rim_mask[..., None]) + chrome * rim_mask[..., None]
    # glass edge where it meets the frame: a dark seat line and a lit bevel
    seat = np.exp(-((-dist - frame_w) / 0.9) ** 2)
    rgb = rgb * (1 - 0.6 * seat[..., None])
    bevel = np.exp(-((-dist - frame_w - 2.0) / 0.8) ** 2) * (0.4 + 0.6 * CP.edge_light(w, h, inward=True))
    rgb = rgb + 0.10 * bevel[..., None]
    # four machined corner clamps holding the sheet, two hex screws each
    clamp = np.zeros((h, w))
    for cx, cy, sx, sy in ((0, 0, 1, 1), (w, 0, -1, 1), (0, h, 1, -1), (w, h, -1, -1)):
        lx = (xs - cx) * sx
        ly = (ys - cy) * sy
        arm = ((lx >= 3) & (lx < 74) & (ly >= 3) & (ly < 22)) | ((lx >= 3) & (lx < 22) & (ly >= 3) & (ly < 74))
        clamp = np.maximum(clamp, arm.astype(float))
    clamp = blur(clamp, 0.7)
    cd = blur(clamp, 2.5)
    shade = 0.55 + 0.45 * CP.normal_from_height(cd * 6, 4.0)[..., 1]
    metal = mix(srgb('#3a4146'), srgb('#9aa4aa'), (shade ** 2)[..., None])
    metal = metal * (1 + noise(h, w, 1.2, 21, 30)[..., None] * 0.04)
    rgb = rgb * (1 - 0.5 * blur(clamp, 4.0)[..., None] * (1 - clamp[..., None]))
    rgb = mix(rgb, metal, clamp[..., None])
    pts = []
    for cx, cy, sx, sy in ((0, 0, 1, 1), (w, 0, -1, 1), (0, h, 1, -1), (w, h, -1, -1)):
        pts += [(cx + sx * 54, cy + sy * 12.5), (cx + sx * 12.5, cy + sy * 54)]
    rgb = bolts(rgb, pts, 4.5, slot=False, metal=('#2c3236', '#c7d0d5'))
    for px, py in pts:
        hexm = disc(h, w, px, py, 1.8)
        rgb = rgb * (1 - 0.7 * hexm[..., None])
    return rgb, cov


# ======================================================================== C  Kartentisch (map table)

def plate_c():
    h, w = H, W
    rim = 7.0
    dist = CP.rounded_rect_distance(w, h, 10.0, inset=1.0)
    cov = np.clip(0.5 - dist, 0, 1)
    chrome, rim_mask, _ = chrome_frame(dist, rim)
    xs, ys = grid_xy(h, w)
    band = 22.0
    # map field: dark slate teal, contours from a height field
    field = srgb('#0f181b') + np.zeros((h, w, 3))
    field = field * (1 + fbm(h, w, 260, 31)[..., None] * 0.06)
    terrain = fbm(h, w, 420, 77, octaves=5) + 0.25 * fbm(h, w, 900, 78, octaves=2)
    level = terrain / 0.05
    frac = np.abs(level - np.round(level))
    grad = np.maximum(np.hypot(*np.gradient(level)), 1e-3)
    line = sat(1.0 - frac / (grad * 0.9))
    major = (np.abs(np.round(level)) % 5 == 0).astype(float)
    quiet = 0.35 + 0.65 * smoothstep(ys, 300, 380)
    quiet = quiet * (1 - smoothstep(ys, h - 230, h - 150))
    quiet = np.maximum(quiet, 0.25 * (1 - smoothstep(ys, h - 230, h - 150)))
    contour = line * (0.10 + 0.11 * major) * quiet
    field = field + srgb('#38c8ff') * contour[..., None]
    # coordinate grid every 80 units, crosses at intersections
    gx = sat(1.0 - np.abs(((xs - band) % 80) - 0.5))
    gy = sat(1.0 - np.abs(((ys - band) % 80) - 0.5))
    field = field + srgb('#38c8ff') * (np.maximum(gx, gy) * 0.018)[..., None]
    cross = ((np.abs(((xs - band + 6) % 80) - 6) < 6.5) & (np.abs(((ys - band) % 80)) < 1.0)) | \
            ((np.abs(((ys - band + 6) % 80) - 6) < 6.5) & (np.abs(((xs - band) % 80)) < 1.0))
    field = field + srgb('#38c8ff') * (cross.astype(float) * 0.05 * quiet)[..., None]
    field = field * (1 - inner_shadow(h, w, band, 9.0, 0.5))[..., None]
    # frame band with ruler ticks and grid letters (top and bottom bands only; the sides crop)
    frame = sat(band + 0.5 + dist) * cov
    steel = srgb('#1c2124') * (1 + noise(h, w, 1.2, 8, 50)[..., None] * 0.015 + fbm(h, w, 60, 9)[..., None] * 0.04)
    rgb = field * (1 - frame[..., None]) + steel * frame[..., None]
    ticks = np.zeros((h, w))
    for x in range(int(band) + 10, w - int(band), 10):
        major_t = ((x - int(band)) % 80) == 0
        L = 9 if major_t else 4
        for y0, y1 in ((rim + 1, rim + 1 + L), (h - rim - 1 - L, h - rim - 1)):
            ticks[int(y0):int(y1), x:x + 1] = 1.0
    for y in list(range(int(band) + 10, TOP_BAND, 10)) + list(range(h - BOTTOM_BAND, h - int(band), 10)):
        major_t = ((y - int(band)) % 80) == 0
        L = 9 if major_t else 4
        ticks[y:y + 1, int(rim + 1):int(rim + 1 + L)] = 1.0
        ticks[y:y + 1, int(w - rim - 1 - L):int(w - rim - 1)] = 1.0
    labels = []
    letters = 'ABCDEFGHIJKLMNO'
    for i, x in enumerate(range(int(band) + 40, w - int(band), 80)):
        labels.append((letters[i % len(letters)], x, rim + 7.5, 9, 'bahnschrift.ttf', 'mm'))
        labels.append((letters[i % len(letters)], x, h - rim - 7.5, 9, 'bahnschrift.ttf', 'mm'))
    tl = text_layer(h, w, labels) * frame
    rgb = rgb + srgb('#7fb8c8') * (ticks * 0.35 * frame + tl * 0.55)[..., None]
    seat = np.exp(-((-dist - band) / 0.9) ** 2)
    rgb = rgb * (1 - 0.55 * seat[..., None])
    bevel = np.exp(-((-dist - band - 1.8) / 0.8) ** 2) * (0.3 + 0.7 * CP.edge_light(w, h, inward=True))
    rgb = rgb + 0.07 * bevel[..., None]
    rgb = rgb * (1 - rim_mask[..., None]) + chrome * rim_mask[..., None]
    # compass rose watermark lower right, quiet
    cx, cy, r = w - 150, h - 230, 70
    rr = np.hypot(xs - cx, ys - cy)
    ring = sat(1 - np.abs(rr - r)) + sat(1 - np.abs(rr - r * 0.78)) * 0.6
    ang = np.arctan2(ys - cy, xs - cx)
    star = sat(1 - np.abs(rr * np.abs(np.sin(2 * ang)) ) / 1.2) * (rr < r * 1.12)
    rose = np.maximum(ring, star)
    rgb = rgb + srgb('#38c8ff') * (rose * 0.05)[..., None]
    tln = text_layer(h, w, [('N', cx, cy - r - 12, 12, 'bahnschrift.ttf', 'mm')])
    rgb = rgb + srgb('#38c8ff') * (tln * 0.10)[..., None]
    rgb = bolts(rgb, [(15, 15), (w - 15, 15), (15, h - 15), (w - 15, h - 15)], 5.0)
    return rgb, cov


# ======================================================================== D  Geraeteplatte (instrument panel)

def plate_d():
    h, w = H, W
    rim = 4.0
    dist = CP.rounded_rect_distance(w, h, 12.0, inset=1.0)
    cov = np.clip(0.5 - dist, 0, 1)
    chrome, rim_mask, _ = chrome_frame(dist, rim, light_hex='#8e9890', dark_hex='#23271f')
    xs, ys = grid_xy(h, w)
    # field-grey crinkle paint
    paint = srgb('#262b27') + np.zeros((h, w, 3))
    crinkle = noise(h, w, 1.6, 41) * 0.5 + noise(h, w, 3.0, 42) * 0.5
    paint = paint * (1 + crinkle[..., None] * 0.05 + fbm(h, w, 240, 43)[..., None] * 0.05)
    # raised border band 18 units with a seam
    border = 18.0
    band = sat(border + 0.5 + dist) * cov
    paint = paint * (1 + 0.10 * band[..., None])
    seam = np.exp(-((-dist - border) / 0.9) ** 2)
    paint = paint * (1 - 0.55 * seam[..., None])
    lip = np.exp(-((-dist - border - 1.8) / 0.8) ** 2) * (0.3 + 0.7 * CP.edge_light(w, h, inward=True))
    paint = paint + 0.06 * lip[..., None]
    paint = paint * (1 - inner_shadow(h, w, border + 1, 8.0, 0.4))[..., None]
    # paint chips on the border band show bare steel
    chips = smoothstep(fbm(h, w, 9, 44, octaves=3), 0.42, 0.55) * band * (1 - smoothstep(-dist, 3, 14) * 0.6)
    paint = mix(paint, srgb('#6d736f'), (chips * 0.7)[..., None])
    rgb = paint * (1 - rim_mask[..., None]) + chrome * rim_mask[..., None]
    # black enamel placard behind the title (brass rim, 4 screws) and two indicator lamps
    px0, py0, px1, py1 = 26, 16, w - 26, 102
    pm, pd = rect_mask(h, w, px0, py0, px1, py1, 6.0)
    brass_rim = sat(pd + 3.5) * pm
    enamel = srgb('#101315') * (1 + noise(h, w, 2.0, 45)[..., None] * 0.03)
    enamel = enamel + srgb('#1d2a2e') * (sat(1 - (ys - py0) / 40.0) * 0.6)[..., None]
    shadow = blur(pm, 4.0)
    rgb = rgb * (1 - 0.5 * np.clip(shadow - pm, 0, 1)[..., None] - 0.0)
    brass = mix(srgb('#5d4a22'), srgb('#c9a456'), (0.35 + 0.65 * CP.edge_light(w, h, inward=False))[..., None])
    rgb = mix(rgb, enamel, pm[..., None])
    rgb = mix(rgb, brass, (brass_rim * 0.85)[..., None])
    rgb = bolts(rgb, [(px0 + 12, py0 + 12), (px1 - 12, py0 + 12), (px0 + 12, py1 - 12), (px1 - 12, py1 - 12)], 4.0,
                metal=('#5a4a26', '#e8cf8a'))
    # the page lamps on the placard are UMG widgets (page_lamp below), one per page
    # footer: recessed enamel rail with brass edge
    fm, fd = rect_mask(h, w, 26, FOOTER_TOP + 4, w - 26, h - 22, 6.0)
    rgb = rgb * (1 - 0.45 * np.clip(blur(fm, 3.0) - fm, 0, 1)[..., None])
    rgb = mix(rgb, srgb('#121517') * (1 + noise(h, w, 2.0, 46)[..., None] * 0.03), (fm * 0.92)[..., None])
    rgb = mix(rgb, brass, (sat(fd + 2.5) * fm * 0.6)[..., None])
    # rivets along top and bottom bands + corners only (the sides crop)
    pts = [(x, 9.5) for x in range(60, w - 40, 56)] + [(x, h - 9.5) for x in range(60, w - 40, 56)]
    pts += [(9.5, 40), (w - 9.5, 40), (9.5, h - 40), (w - 9.5, h - 40), (9.5, 80), (w - 9.5, 80)]
    rgb = bolts(rgb, pts, 3.4, slot=False, metal=('#3b413b', '#b7beb6'))
    # stencil on the border band, bottom left: device number
    st = text_layer(h, w, [('GER. 41  NR. 0723', 40, h - 9, 11, 'bahnschrift.ttf', 'lm')])
    st = st * (1 - smoothstep(fbm(h, w, 3, 47, octaves=2), 0.3, 0.6) * 0.6)
    
    return rgb, cov


# ======================================================================== page lamps (D)

LAMP_TEX = 48          # 2 texels per unit of the 24 unit lamp box in UW_InfoTakeover


def draw_page_lamp(rgb, on, scale=2.0):
    """The placard lamp as plate D drew it: grey ring, cyan lit dome with glow, or a dark amber dome."""
    h, w = rgb.shape[:2]
    c = LAMP_TEX / 2.0
    ring = disc(h, w, c, c, 10.0 * scale) - disc(h, w, c, c, 7.5 * scale)
    rgb = mix(rgb, srgb('#9aa29c'), (ring * 0.9)[..., None])
    dome = disc(h, w, c, c, 7.5 * scale)
    if on:
        rgb = lamp(rgb, dome, VS.ACCENT, 0.9, 4.0 * scale, 0.5)
    else:
        rgb = mix(rgb, srgb('#ffb347') * 0.22, dome[..., None])
    hl = disc(h, w, c - 2.5 * scale, c - 2.5 * scale, 2.2 * scale)
    return rgb + 0.35 * hl[..., None]


def page_lamp(on):
    """RGBA lamp: drawn over black and over white, alpha solved from the difference (keeps the soft glow)."""
    over_black = np.clip(draw_page_lamp(np.zeros((LAMP_TEX, LAMP_TEX, 3)), on), 0, 1)
    over_white = np.clip(draw_page_lamp(np.ones((LAMP_TEX, LAMP_TEX, 3)), on), 0, 1)
    alpha = np.clip(1.0 - (over_white - over_black).mean(-1), 0, 1)
    rgb = over_black / np.maximum(alpha, 1e-4)[..., None]
    return np.clip(rgb, 0, 1), alpha


# ======================================================================== E  Ruine (battle-scarred)

def plate_e():
    rgb, cov, dist = gunmetal_plate(53, field_hex='#16181a', rim=5.0)
    h, w = H, W
    xs, ys = grid_xy(h, w)
    edge_d = np.minimum(np.minimum(xs, w - xs), np.minimum(ys, h - ys))
    # rust bloom concentrated at the edges and corners, kept off the title / description zone
    edgeiness = np.exp(-edge_d / 90.0)
    corner_w = np.exp(-(np.minimum(xs, w - xs) * np.minimum(ys, h - ys)) / 9000.0)
    rust_n = fbm(h, w, 70, 61, octaves=5)
    keep_clear = 1 - 0.85 * (smoothstep(xs, 30, 80) * (1 - smoothstep(xs, 820, 1000))) * \
        (smoothstep(ys, 20, 40) * (1 - smoothstep(ys, 300, 360)))
    rust_amt = smoothstep(rust_n * 0.8 + edgeiness * 0.9 + corner_w * 0.5, 0.75, 1.05) * keep_clear * cov
    rust_col = mix(srgb('#2a150b'), srgb('#6a3a1c'), sat(fbm(h, w, 6, 62) * 0.5 + 0.5)[..., None])
    rust_col = rust_col * (1 + noise(h, w, 1.5, 63)[..., None] * 0.15)
    rgb = mix(rgb, rust_col * 0.75, (rust_amt * 0.8)[..., None])
    stain = smoothstep(rust_n, 0.2, 0.6) * edgeiness * keep_clear
    streaks = noise(h, w, 2.0, 64, aspect=0.02) * 0.5 + 0.5
    rgb = mix(rgb, srgb('#4a2614'), (stain * streaks * 0.25)[..., None])
    # soot from the bottom right
    soot = sat((xs / w) * 0.5 + (ys / h) * 0.8 - 0.55) * (0.6 + 0.4 * fbm(h, w, 120, 65))
    rgb = rgb * (1 - 0.45 * soot[..., None])
    # dents
    for (cx, cy, r) in ((1100, 420, 26), (1160, 640, 18), (90, 600, 22), (640, 880, 16)):
        d = np.hypot(xs - cx, ys - cy) / r
        dent = np.exp(-d ** 2 * 2.0)
        nx = -(xs - cx) / r * dent
        ny = -(ys - cy) / r * dent
        rgb = rgb * (1 + (nx * -0.6 + ny * -0.8)[..., None] * 0.25)
    # chipped chrome rim
    chips = smoothstep(fbm(h, w, 8, 66, octaves=3), 0.35, 0.5) * sat(dist + 6.5) * cov
    rgb = mix(rgb, srgb('#3a2414'), (chips * 0.75)[..., None])
    # shrapnel tear in the top right corner band
    hx, hy = w - 70, 54
    ang = np.arctan2(ys - hy, xs - hx)
    rr = np.hypot(xs - hx, ys - hy)
    jag = 13 + 7 * np.sin(ang * 5 + 1.3) + 4 * np.sin(ang * 11 + 0.4) + 3 * noise(h, w, 2, 67)
    hole = sat(jag - rr)
    lipr = sat(jag + 5 - rr) * (1 - hole)
    lit = sat(np.cos(ang + 2.2) * 0.5 + 0.5)
    rgb = mix(rgb, srgb('#9aa0a2') * (0.4 + 0.8 * lit[..., None]), (lipr * 0.8)[..., None])
    rgb = mix(rgb, srgb('#050505'), hole[..., None])
    petals = sat(jag + 12 - rr) * (1 - sat(jag + 5 - rr)) * (np.sin(ang * 5 + 1.3) > 0.4)
    rgb = rgb * (1 - 0.3 * petals[..., None])
    # bright hairline scratches everywhere but the text zones, stronger near the edges
    reg = lambda x, y: 0.25 + 0.75 * math.exp(-min(min(x, w - x), min(y, h - y)) / 120.0)
    sc = hairline_scratches(h, w, 68, 700, (5, 34), 0.12, lambda x, y: 0.1 + 0.9 * math.exp(-min(min(x, w - x), min(y, h - y)) / 70.0))
    rgb = rgb + sc[..., None] * srgb('#cfcac0') * keep_clear[..., None]
    rgb = bolts(rgb, [(20, 20), (20, h - 20), (w - 20, h - 20)], 5.5)
    # the fourth bolt is missing: an empty hole
    rgb = mix(rgb, srgb('#060606'), disc(h, w, w - 20, 20, 3.2)[..., None])
    rgb = rgb * (1 - 0.5 * (disc(h, w, w - 20, 20, 6.5) - disc(h, w, w - 20, 20, 3.2))[..., None])
    rgb = groove(rgb, h, w, DIVIDER_Y + 1, 8, w - 8, depth=0.4, light=0.04)
    rgb = groove(rgb, h, w, FOOTER_TOP, 8, w - 8, depth=0.4, light=0.05)
    return rgb, cov


# ======================================================================== UNDERSTOOD buttons

def button(option, state):
    w, h = BW, BH
    dist = CP.rounded_rect_distance(w, h, 14.0, inset=1.0)
    cov = np.clip(0.5 - dist, 0, 1)
    xs, ys = grid_xy(h, w)
    face = {'A': '#1d2023', 'B': '#0e1b21', 'C': '#132024', 'D': '#121517', 'E': '#1c1d1e'}[option]
    rim_light = {'A': '#aab3b9', 'B': '#b8c8d0', 'C': '#a7b8bd', 'D': '#d6b46a', 'E': '#9a9690'}[option]
    rim_dark = {'A': '#2b2f33', 'B': '#22303a', 'C': '#25313a', 'D': '#4a3a1c', 'E': '#2b2a28'}[option]
    lift = {'n': 0.0, 'h': 0.28, 'p': -0.22}[state]
    field = srgb(face) * (1 + lift) * (1.10 - 0.22 * (ys / h))[..., None]
    field = field + noise(h, w, 1.2, 81, 40)[..., None] * 0.012
    field = field * (1 - inner_shadow(h, w, 9, 6.0, 0.5))[..., None]
    chrome, crest = VS.chrome_rim(dist, 8.0, w, h, dark_hex=rim_dark, light_hex=rim_light)
    if state == 'h':
        chrome = mix(chrome, srgb(VS.TAB_SELECTED) * (0.35 + 0.75 * chrome.mean(-1, keepdims=True)), 0.5)
    if state == 'p':
        chrome = mix(chrome, srgb(VS.ACCENT) * (0.35 + 0.75 * chrome.mean(-1, keepdims=True)), 0.6)
    rim_mask = np.clip(dist + 8.5, 0, 1) * cov
    rgb = field * (1 - rim_mask[..., None]) + chrome * rim_mask[..., None]
    seam = np.exp(-((-dist - 8.0) / 0.8) ** 2) * cov
    rgb = rgb * (1 - 0.5 * seam[..., None])
    intensity = {'n': 0.6, 'h': 1.0, 'p': 0.85}[state]
    reach = sat((xs - 44) / 16.0) * sat((w - 44 - xs) / 16.0)
    strip = (np.abs(ys - (h - 16.0)) < 1.7).astype(float) * reach
    colour = '#ffb347' if option == 'D' else VS.ACCENT
    rgb = lamp(rgb, strip, colour, intensity, 3.0, 0.35)
    for jx in (32.0, w - 32.0):
        r = np.hypot(xs - jx, ys - h / 2.0)
        ring = np.clip(7.0 - r, 0, 1) * (1 - np.clip(5.0 - r, 0, 1))
        rgb = mix(rgb, srgb('#5d666c'), (ring * 0.9)[..., None])
        rgb = lamp(rgb, np.clip(5.0 - r, 0, 1), colour, intensity, 3.0, 0.4)
    if option == 'E':
        rust = smoothstep(fbm(h, w, 14, 91, octaves=4) + np.exp(-np.minimum(xs, w - xs) / 50.0) * 0.5, 0.45, 0.8)
        rgb = mix(rgb, srgb('#5a2e16'), (rust * rim_mask * 0.7)[..., None])
    return rgb, cov


PLATES = {'A': plate_a, 'B': plate_b, 'C': plate_c, 'D': plate_d, 'E': plate_e}


def main(only=None):
    for key, fn in PLATES.items():
        if only and key not in only:
            continue
        rgb, cov = fn()
        save_rgba('T_TKO_%s_Plate' % key, rgb, cov)
        for state in ('n', 'h', 'p'):
            brgb, bcov = button(key, state)
            save_rgba('T_TKO_%s_Button_%s' % (key, state), brgb, bcov)
        if key == 'D':
            for state, on in (('on', True), ('off', False)):
                lrgb, lalpha = page_lamp(on)
                data = np.dstack([lrgb, lalpha])
                Image.fromarray((data * 255 + 0.5).astype(np.uint8), 'RGBA').save(
                    os.path.join(OUT, 'T_TKO_D_PageLamp_%s.png' % state))
                print('wrote page lamp', state, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or None)
