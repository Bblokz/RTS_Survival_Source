"""Concept sheet: five dead-grass types for the Ger_IntroMission sand shader (user request after round 2: the yellow
straw cards and feral-wheat puffs "do not fit"). Drawn procedurally on a crop of the real in-game sand
(shots/r2_west_quadrant.png, open sand mean RGB 127/72/34), so the colours are judged against the shader.

    python grass_concepts.py   -> Saved/IntroFoliage/concepts/dead_grass_concepts.png

Per type: an in-game-scale view (RTS camera, ~24 px per metre, one clump group), a 4x close-up from above, a side
silhouette, and the palette. Light comes from the upper right like the map's sun (shadows fall to the lower left).
"""
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroFoliage/'
OUT = ROOT + 'concepts/'
os.makedirs(OUT, exist_ok=True)
SAND = Image.open(ROOT + 'shots/r2_west_quadrant.png').convert('RGB')
SHADOW_DIR = (-0.75, 0.6)          # screen: shadows go left/down


def font(size):
    for f in ('arialbd.ttf', 'arial.ttf', 'segoeui.ttf'):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def sand_tile(w, h, seed, scale=1.0):
    rnd = random.Random(seed)
    src = SAND.crop((60, 80, 760, 480))
    if scale != 1.0:
        src = src.resize((int(src.width * scale), int(src.height * scale)), Image.BICUBIC)
    x0 = rnd.randint(0, max(0, src.width - w))
    y0 = rnd.randint(0, max(0, src.height - h))
    return src.crop((x0, y0, x0 + w, y0 + h)).copy()


def jitter(col, rnd, amt=12):
    return tuple(max(0, min(255, int(c + rnd.uniform(-amt, amt)))) for c in col)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


# ---------------------------------------------------------------------------------------------------------------------
# blade primitives, drawn at SS x resolution then downsampled for anti-aliasing
def draw_blade(d, x, y, ang, length, curl, width, c0, c1, steps=8):
    pts = []
    a = ang
    px, py = x, y
    for k in range(steps + 1):
        pts.append((px, py))
        a += curl / steps
        px += math.cos(a) * length / steps
        py += math.sin(a) * length / steps
    for k in range(steps):
        t = k / steps
        fill = 255 if c0 is None else lerp(c0, c1, t) + (255,)
        d.line([pts[k], pts[k + 1]], fill=fill, width=max(1, int(width * (1 - 0.7 * t))))
    return pts[-1]


def shadow_layer(size, draw_fn, offset, blur, alpha):
    sh = Image.new('L', size, 0)
    ds = ImageDraw.Draw(sh)
    draw_fn(ds, shadow=True)
    sh = sh.filter(ImageFilter.GaussianBlur(blur))
    sh = sh.transform(size, Image.AFFINE, (1, 0, -offset[0], 0, 1, -offset[1]))
    return sh.point(lambda v: int(v * alpha))


def composite(base, draw_fn, offset, blur=3, alpha=0.55, ss=3):
    """Render draw_fn on an RGBA layer at ss x, add a soft drop shadow, downsample onto base."""
    W, H = base.size
    big = (W * ss, H * ss)
    sh = shadow_layer(big, draw_fn, (offset[0] * ss, offset[1] * ss), blur * ss, alpha)
    layer = Image.new('RGBA', big, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(layer), shadow=False)
    sh = sh.resize((W, H), Image.LANCZOS)
    layer = layer.resize((W, H), Image.LANCZOS)
    out = base.convert('RGBA')
    dark = Image.new('RGBA', (W, H), (25, 12, 5, 255))
    out = Image.composite(dark, out, sh)
    out.alpha_composite(layer)
    return out.convert('RGB')


# ---------------------------------------------------------------------------------------------------------------------
# the five types. Each returns a draw function for a clump centred at (cx, cy) with radius r (pixels at render scale)
def type_ash_tussock(rnd, cx, cy, r):
    """Aschgras: sun-bleached bunchgrass tussock, greyed ash-tan, fine blades bursting from a dark dead core."""
    blades = []
    for _ in range(int(70 * (r / 40) ** 1.2)):
        a = rnd.uniform(0, 2 * math.pi)
        blades.append((a, rnd.uniform(0.45, 1.0) * r, rnd.uniform(-0.6, 0.6), rnd.uniform(0.0, 0.25) * r))

    def fn(d, shadow):
        if not shadow:
            d.ellipse((cx - r * 0.28, cy - r * 0.24, cx + r * 0.28, cy + r * 0.24), fill=(58, 44, 34, 255))
        for a, L, c, o in blades:
            x0, y0 = cx + math.cos(a) * o, cy + math.sin(a) * o
            c0 = (96, 80, 62) if not shadow else 255
            c1 = jitter((176, 160, 136), rnd, 10) if not shadow else 255
            if shadow:
                draw_blade(d, x0, y0, a, L, c, 3, None, None)
            else:
                draw_blade(d, x0, y0, a, L, c, 3, c0, c1)
    return fn


def type_burnt_stubble(rnd, cx, cy, r):
    """Brandstoppel: wildfire-burnt stubble, charcoal stubs on an ash-grey/black scorch with rust edge."""
    patch = []
    for k in range(18):
        t = 2 * math.pi * k / 18
        rr = r * rnd.uniform(0.75, 1.15)
        patch.append((cx + math.cos(t) * rr, cy + math.sin(t) * rr * 0.8))
    stubs = [(cx + rnd.gauss(0, r * 0.45), cy + rnd.gauss(0, r * 0.4), rnd.uniform(0, 2 * math.pi), rnd.uniform(3, 9))
             for _ in range(int(160 * (r / 60) ** 2))]

    def fn(d, shadow):
        if not shadow:
            d.polygon(patch, fill=(66, 46, 34, 150))
            inner = [(cx + (x - cx) * 0.72, cy + (y - cy) * 0.72) for x, y in patch]
            d.polygon(inner, fill=(40, 34, 31, 200))
            for _ in range(60):
                x, y = cx + rnd.gauss(0, r * 0.35), cy + rnd.gauss(0, r * 0.3)
                d.ellipse((x - 2, y - 2, x + 2, y + 2), fill=jitter((150, 146, 140), rnd, 15) + (210,))
        for x, y, a, L in stubs:
            if shadow:
                d.line([(x, y), (x + math.cos(a) * L * 0.3, y + math.sin(a) * L * 0.3)], fill=255, width=3)
            else:
                d.line([(x, y), (x + math.cos(a) * L * 0.3, y + math.sin(a) * L * 0.3)],
                       fill=jitter((32, 26, 24), rnd, 8) + (255,), width=3)
                d.point((x, y), fill=(120, 72, 40, 255))
    return fn


def type_rust_mat(rnd, cx, cy, r):
    """Rostquecke: dead couch-grass mat lying flat, rust-umber runners hugging the sand; a darker tone of the sand."""
    runners = []
    for _ in range(int(26 * (r / 60))):
        a = rnd.uniform(0, 2 * math.pi)
        runners.append((cx + rnd.gauss(0, r * 0.25), cy + rnd.gauss(0, r * 0.2), a, rnd.uniform(0.6, 1.3) * r,
                        rnd.uniform(-1.2, 1.2)))

    def fn(d, shadow):
        for x, y, a, L, c in runners:
            pts = []
            px, py, aa = x, y, a
            for k in range(14):
                pts.append((px, py))
                aa += c / 14 + rnd.uniform(-0.15, 0.15)
                px += math.cos(aa) * L / 14
                py += math.sin(aa) * L / 14
            for k in range(13):
                col = 255 if shadow else jitter((98, 54, 30), rnd, 10) + (255,)
                d.line([pts[k], pts[k + 1]], fill=col, width=4)
                if k % 2 == 0:      # leaf blades off the runner
                    la = aa + rnd.choice((-1, 1)) * rnd.uniform(0.6, 1.2)
                    ll = rnd.uniform(6, 16)
                    col2 = 255 if shadow else jitter((150, 98, 62), rnd, 12) + (255,)
                    d.line([pts[k], (pts[k][0] + math.cos(la) * ll, pts[k][1] + math.sin(la) * ll)], fill=col2, width=2)
    return fn


def type_dead_thistle(rnd, cx, cy, r):
    """Distelhorst: dead thistle / knapweed clump, grey-brown stems with spiky seed heads; tall, long shadows."""
    stems = [(rnd.uniform(0, 2 * math.pi), rnd.uniform(0.3, 1.0) * r) for _ in range(9)]

    def fn(d, shadow):
        if not shadow:
            for _ in range(10):        # dead rosette leaves at the foot
                a = rnd.uniform(0, 2 * math.pi)
                L = rnd.uniform(0.25, 0.5) * r
                d.polygon([(cx, cy), (cx + math.cos(a - 0.2) * L, cy + math.sin(a - 0.2) * L),
                           (cx + math.cos(a) * L * 1.2, cy + math.sin(a) * L * 1.2),
                           (cx + math.cos(a + 0.2) * L, cy + math.sin(a + 0.2) * L)], fill=jitter((84, 70, 58), rnd, 8) + (255,))
        for a, L in stems:
            ex, ey = cx + math.cos(a) * L, cy + math.sin(a) * L
            col = 255 if shadow else (112, 96, 82, 255)
            d.line([(cx, cy), (ex, ey)], fill=col, width=4)
            hr = rnd.uniform(5, 9)
            if shadow:
                d.ellipse((ex - hr, ey - hr, ex + hr, ey + hr), fill=255)
            else:
                d.ellipse((ex - hr, ey - hr, ex + hr, ey + hr), fill=jitter((92, 72, 60), rnd, 8) + (255,))
                for k in range(12):
                    t = 2 * math.pi * k / 12
                    d.line([(ex, ey), (ex + math.cos(t) * hr * 1.6, ey + math.sin(t) * hr * 1.6)],
                           fill=(178, 164, 140, 255), width=1)
                d.ellipse((ex - hr * 0.45, ey - hr * 0.45, ex + hr * 0.45, ey + hr * 0.45), fill=(196, 186, 168, 255))
    return fn


def type_drift_grass(rnd, cx, cy, r):
    """Flugsandhalme: thin dull-ochre grass combed flat by the wind and half buried, with a pale sand drift fan in its
    lee: it sits IN the ripple shader instead of on top of it."""
    wind = 0.35
    blades = [(cx + rnd.gauss(0, r * 0.35), cy + rnd.gauss(0, r * 0.12)) for _ in range(int(90 * (r / 60)))]

    def fn(d, shadow):
        if not shadow:
            fan = [(cx - r * 0.6, cy - r * 0.15), (cx + r * 1.4, cy - r * 0.35), (cx + r * 1.7, cy + r * 0.05),
                   (cx + r * 1.4, cy + r * 0.45), (cx - r * 0.6, cy + r * 0.2)]
            d.polygon(fan, fill=(170, 104, 58, 120))
        for x, y in blades:
            a = wind + rnd.uniform(-0.35, 0.35)
            L = rnd.uniform(0.35, 0.9) * r
            if shadow:
                draw_blade(d, x, y, a, L, rnd.uniform(-0.2, 0.2), 2, None, None)
            else:
                draw_blade(d, x, y, a, L, rnd.uniform(-0.2, 0.2), 2, (110, 74, 44), jitter((168, 132, 92), rnd, 10))
    return fn


TYPES = [
    ('A  Aschgras (bleached bunchgrass)', type_ash_tussock,
     'Sun-bleached tussock, 0.5-1.2 m. Ash-tan blades greyer than the sand, dark dead core; reads as a clump from the '
     'RTS camera. Crushable: flattens to a grey mat.', [(58, 44, 34), (96, 80, 62), (150, 134, 110), (176, 160, 136)],
     (24, 0.42), 26),
    ('B  Brandstoppel (burnt stubble)', type_burnt_stubble,
     'Wildfire-burnt grass: charcoal stubs on an ash-grey scorch with a rust rim, 2-5 m patches. Ties into the burnt '
     'field; reads clearly darker than the sand.', [(32, 26, 24), (40, 34, 31), (66, 46, 34), (150, 146, 140)],
     (42, 0.25), 60),
    ('C  Rostquecke (dead couch-grass mat)', type_rust_mat,
     'Dead creeping grass lying flat, rust-umber runners. A darker, fibrous tone of the sand: ground texture rather '
     'than objects; cheapest, best for large areas. Decal or flat card.', [(98, 54, 30), (120, 70, 42), (150, 98, 62)],
     (44, 0.2), 55),
    ('D  Distelhorst (dead thistle clump)', type_dead_thistle,
     'Dead thistle / knapweed, 0.6-1.0 m, grey-brown stems with pale spiky seed heads and long shadows. Typical West '
     'German roadside and fallow weed; sparse accents at verges, fences, farm.', [(84, 70, 58), (112, 96, 82),
                                                                                  (178, 164, 140), (196, 186, 168)],
     (16, 0.5), 30),
    ('E  Flugsandhalme (wind-combed)', type_drift_grass,
     'Thin dull-ochre grass combed flat by the wind, half buried, with a pale drift fan in its lee. Sits IN the ripple '
     'shader; good on open sand where other types look placed.', [(110, 74, 44), (168, 132, 92), (170, 104, 58)],
     (40, 0.3), 50),
]


def side_view(fn_name, w, h, rnd):
    img = Image.new('RGB', (w, h), (232, 226, 214))
    d = ImageDraw.Draw(img)
    gy = h - 18
    d.rectangle((0, gy, w, h), fill=(150, 88, 46))
    cx = w // 2
    if fn_name == 'type_ash_tussock':
        for _ in range(60):
            a = -math.pi / 2 + rnd.uniform(-1.1, 1.1)
            draw_blade(d, cx + rnd.uniform(-8, 8), gy, a, rnd.uniform(40, 95), rnd.uniform(-0.8, 0.8), 2,
                       (96, 80, 62), jitter((176, 160, 136), rnd, 8))
    elif fn_name == 'type_burnt_stubble':
        d.rectangle((cx - 90, gy - 3, cx + 90, gy), fill=(40, 34, 31))
        for _ in range(45):
            x = cx + rnd.uniform(-85, 85)
            d.line([(x, gy), (x + rnd.uniform(-3, 3), gy - rnd.uniform(6, 20))], fill=(32, 26, 24), width=2)
    elif fn_name == 'type_rust_mat':
        for _ in range(40):
            x = cx + rnd.uniform(-90, 90)
            d.line([(x, gy), (x + rnd.uniform(-14, 14), gy - rnd.uniform(3, 10))], fill=jitter((98, 54, 30), rnd, 10), width=2)
    elif fn_name == 'type_dead_thistle':
        for _ in range(7):
            x2, y2 = cx + rnd.uniform(-40, 40), gy - rnd.uniform(70, 120)
            d.line([(cx + rnd.uniform(-5, 5), gy), (x2, y2)], fill=(112, 96, 82), width=3)
            d.ellipse((x2 - 7, y2 - 7, x2 + 7, y2 + 7), fill=(92, 72, 60))
            for k in range(10):
                t = 2 * math.pi * k / 10
                d.line([(x2, y2), (x2 + math.cos(t) * 11, y2 + math.sin(t) * 11)], fill=(178, 164, 140))
    else:
        d.polygon([(cx - 90, gy), (cx + 120, gy), (cx + 60, gy - 10), (cx - 70, gy - 6)], fill=(170, 104, 58))
        for _ in range(50):
            x = cx + rnd.uniform(-80, 40)
            draw_blade(d, x, gy - 4, -0.25 + rnd.uniform(-0.15, 0.1), rnd.uniform(30, 70), 0.25, 2, (110, 74, 44),
                       jitter((168, 132, 92), rnd, 8))
    d.text((6, 4), 'side view (1 m)', fill=(60, 50, 40), font=font(13))
    d.line([(w - 30, gy - 5), (w - 30, gy - 5 - 48)], fill=(60, 50, 40), width=2)
    return img


def panel(idx, name, fn, text, palette, game, close_r):
    rnd = random.Random(100 + idx)
    W = 1500
    H = 330
    img = Image.new('RGB', (W, H), (24, 22, 20))
    d = ImageDraw.Draw(img)
    # in-game scale: 420x300 px of real sand (~17 x 12 m), clumps of the game radius (px per metre ~24)
    g = sand_tile(420, 300, idx)
    gr, gspread = game
    for k in range(rnd.randint(3, 5) if gr > 30 else rnd.randint(5, 9)):
        x, y = rnd.uniform(60, 360), rnd.uniform(60, 240)
        r = gr * rnd.uniform(0.6, 1.3)
        g = composite(g, fn(rnd, x * 3, y * 3, r * 3), (SHADOW_DIR[0] * 4, SHADOW_DIR[1] * 4), blur=1.5, alpha=0.5)
    img.paste(g, (10, 20))
    d.text((14, 22), 'in game scale (~17 m wide)', fill=(255, 255, 255), font=font(13))
    # close-up 4x
    c = sand_tile(420, 300, idx + 50, scale=2.6)
    c = composite(c, fn(rnd, 210 * 3, 150 * 3, close_r * 3 * (1.6 if close_r < 40 else 1.2)),
                  (SHADOW_DIR[0] * 10, SHADOW_DIR[1] * 10), blur=4, alpha=0.55)
    img.paste(c, (440, 20))
    d.text((444, 22), 'close-up from above', fill=(255, 255, 255), font=font(13))
    sv = side_view(fn.__name__, 250, 300, rnd)
    img.paste(sv, (870, 20))
    # text + palette
    d.text((1135, 20), name, fill=(255, 236, 200), font=font(19))
    words, line, y = text.split(), '', 52
    for w_ in words:
        if len(line + ' ' + w_) > 40:
            d.text((1135, y), line.strip(), fill=(220, 214, 204), font=font(14))
            y += 19
            line = ''
        line += ' ' + w_
    d.text((1135, y), line.strip(), fill=(220, 214, 204), font=font(14))
    for k, col in enumerate(palette):
        d.rectangle((1135 + k * 52, 268, 1135 + k * 52 + 44, 312), fill=col, outline=(0, 0, 0))
    d.text((1135, 250), 'palette', fill=(180, 170, 160), font=font(12))
    return img


def main():
    rows = [panel(i, *t) for i, t in enumerate(TYPES)]
    head = Image.new('RGB', (1500, 70), (24, 22, 20))
    hd = ImageDraw.Draw(head)
    hd.text((12, 10), 'Ger_IntroMission: five dead-grass types for the red sand shader', fill=(255, 236, 200), font=font(26))
    hd.text((12, 44), 'Drawn on real in-game sand (shader mean RGB 127/72/34). Pick one or more; they replace the yellow '
                      'straw cards and the feral-wheat puffs.', fill=(200, 190, 180), font=font(15))
    sheet = Image.new('RGB', (1500, 70 + 330 * len(rows)), (24, 22, 20))
    sheet.paste(head, (0, 0))
    for k, r in enumerate(rows):
        sheet.paste(r, (0, 70 + 330 * k))
    sheet.save(OUT + 'dead_grass_concepts.png')
    print(OUT + 'dead_grass_concepts.png', sheet.size)


if __name__ == '__main__':
    main()
