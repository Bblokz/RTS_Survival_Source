"""Rebuilds the barrage aim ability card icons (180x180 hex plates) and their cancel versions from rendered rounds.

Rounds: (KADO-3D... ) BlenderTanks/AbilityIcons/AimAbililty/Rebuild/render_rounds.py renders round_<type>.png
(nose down, centred, PX_PER_UNIT px per model unit) into the work folder. The layouts keep the old icons' round
count, size and flight angle: each round is placed by its nose and tail point (measured on the old icons).
The plate is the old mortar icon with its picture painted over; crosshair and cancel cross are drawn here.

Usage: python compose_barrage_icons.py <work folder>   (needs <work>/ref/T_AimAbility_MortarBarrage.png)
Writes <work>/new/T_AimAbility_<Name>.png and T_Cancel_AimAbility_<Name>.png."""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

WORK = sys.argv[1].rstrip("/") + "/"
OUT = WORK + "new/"
SIZE = 180
SUPER = 4
PX_PER_UNIT = 512 / 18.0
LENGTH_UNITS = {"ap": 9.2, "he": 9.06, "huge": 10.46, "mortar": 9.24, "rocket": 14.4}
RNG = np.random.default_rng(7)

# name: (round type, trail type, [(nose xy, tail xy), ...]) -- drawn back to front in this order
ICONS = {
    "APBarrage": ("ap", "tracer", [((40, 82), (73, 35)), ((65, 112), (102, 47)), ((100, 137), (143, 65))]),
    "HEBarrage": ("he", "hot", [((58, 100), (37, 50)), ((143, 95), (122, 47)), ((110, 124), (78, 47))]),
    "HugeShellBarrage": ("huge", "hot", [((90, 133), (90, 23))]),
    "MortarBarrage": ("mortar", "smoke", [((36, 108), (50, 77)), ((56, 104), (70, 77)), ((75, 116), (92, 82)),
                                          ((97, 117), (112, 86)), ((117, 122), (132, 91))]),
    "SmallRocketBarrage": ("rocket", "exhaust", [((32, 88), (66, 32)), ((75, 91), (110, 35)),
                                                 ((43, 118), (79, 64)), ((109, 116), (144, 60)),
                                                 ((74, 128), (109, 72)), ((112, 155), (147, 99))]),
}

ys, xs = np.mgrid[0:SIZE, 0:SIZE]
cx = cy = (SIZE - 1) / 2.0
PLATE_RADIUS = 88.0


def hex_distance(x, y):
    dx, dy = np.abs(x - cx), np.abs(y - cy)
    return np.maximum(dy / (PLATE_RADIUS * 0.8660254), (1.7320508 * dx + dy) / (1.7320508 * PLATE_RADIUS))


inner = hex_distance(xs, ys) < 0.9
inner_soft = np.clip((0.9 - hex_distance(xs, ys)) / 0.02, 0, 1)


def over(base, layer):
    a = layer[..., 3:4] / 255.0
    out = base.copy()
    out[..., :3] = layer[..., :3] * a + base[..., :3] * (1 - a)
    out[..., 3:4] = base[..., 3:4] + layer[..., 3:4] * (1 - base[..., 3:4] / 255.0)
    return out


def add(base, layer):
    """Additive light (glows, flames): brightens what is under it."""
    out = base.copy()
    out[..., :3] = np.clip(base[..., :3] + layer[..., :3] * (layer[..., 3:4] / 255.0), 0, 255)
    return out


def masked(layer):
    layer = layer.copy()
    layer[..., 3] *= inner_soft
    return layer


def make_plate():
    mortar = np.asarray(Image.open(WORK + "ref/T_AimAbility_MortarBarrage.png").convert("RGBA")).astype(np.float32)
    plate = mortar.copy()
    # translucent dark red like the old plates, a touch lighter in the middle for depth
    d = np.sqrt((xs - 90) ** 2 + (ys - 110) ** 2) / 95.0
    shade = np.clip(1.0 - d, 0, 1)[..., None]
    fill = np.array([72, 5, 4, 175], np.float32) + shade * np.array([45, 8, 5, 30], np.float32)
    plate[inner] = fill[inner]
    return plate


def drawn(draw_fn, glow_radius, glow_strength=0.9):
    big = Image.new("RGBA", (SIZE * SUPER, SIZE * SUPER), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(big), SUPER)
    sharp = big.resize((SIZE, SIZE), Image.LANCZOS)
    glow = np.asarray(sharp.filter(ImageFilter.GaussianBlur(glow_radius))).astype(np.float32)
    glow[..., 3] *= glow_strength
    return glow, np.asarray(sharp).astype(np.float32)


def crosshair_marks(draw, k):
    c = (255, 175, 60, 255)
    x0, y0 = 90 * k, 137 * k
    draw.ellipse([x0 - 44 * k, y0 - 13 * k, x0 + 44 * k, y0 + 13 * k], outline=c, width=3 * k)
    draw.ellipse([x0 - 18 * k, y0 - 6 * k, x0 + 18 * k, y0 + 6 * k], outline=c, width=3 * k)
    draw.line([x0 - 58 * k, y0, x0 - 32 * k, y0], fill=c, width=3 * k)
    draw.line([x0 + 32 * k, y0, x0 + 58 * k, y0], fill=c, width=3 * k)
    draw.line([x0, y0 - 24 * k, x0, y0 - 9 * k], fill=c, width=3 * k)
    draw.line([x0, y0 + 9 * k, x0, y0 + 26 * k], fill=c, width=3 * k)


def cancel_cross(draw, k):
    c = (255, 35, 55, 255)
    draw.line([46 * k, 66 * k, 134 * k, 150 * k], fill=c, width=16 * k)
    draw.line([134 * k, 66 * k, 46 * k, 150 * k], fill=c, width=16 * k)


def place_round(kind, nose, tail):
    """Returns an RGBA float layer with the round's render scaled and rotated onto nose -> tail."""
    src = Image.open(WORK + "round_%s.png" % kind).convert("RGBA")
    nose, tail = np.array(nose, float), np.array(tail, float)
    length = np.linalg.norm(nose - tail)
    scale = length / (LENGTH_UNITS[kind] * PX_PER_UNIT)
    # supersample the placement for clean edges
    s = scale * SUPER
    img = src.resize((max(1, int(src.width * s)), max(1, int(src.height * s))), Image.LANCZOS)
    direction = nose - tail
    angle = math.degrees(math.atan2(direction[0], direction[1]))  # 0 = nose straight down, + = nose to the right
    img = img.rotate(angle, resample=Image.BICUBIC, expand=True)
    canvas = Image.new("RGBA", (SIZE * SUPER, SIZE * SUPER), (0, 0, 0, 0))
    centre = (nose + tail) / 2 * SUPER
    canvas.alpha_composite(img, (int(round(centre[0] - img.width / 2)), int(round(centre[1] - img.height / 2))))
    return np.asarray(canvas.resize((SIZE, SIZE), Image.LANCZOS)).astype(np.float32)


def rim_glow(round_layer, color=(255, 120, 40), radius=1.6, strength=1.6):
    """A warm glowing outline around a round so it reads on the dark plate (the old icons' hot rims, clean)."""
    alpha = Image.fromarray(round_layer[..., 3].astype(np.uint8), "L")
    grown = np.asarray(alpha.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(radius)))
    layer = np.zeros((SIZE, SIZE, 4), np.float32)
    layer[..., :3] = color
    layer[..., 3] = np.clip(grown.astype(np.float32) * strength, 0, 255)
    return layer


def streak(tail, back, length, width_start, width_end, color, alpha, blur):
    """A tapered streak from the tail backwards (unit vector back), as an RGBA layer."""
    big = Image.new("RGBA", (SIZE * SUPER, SIZE * SUPER), (0, 0, 0, 0))
    draw = ImageDraw.Draw(big)
    side = np.array([-back[1], back[0]])
    steps = 24
    for i in range(steps):
        t0, t1 = i / steps, (i + 1) / steps
        a = alpha * (1 - t0) ** 1.5
        w0 = width_start + (width_end - width_start) * t0
        w1 = width_start + (width_end - width_start) * t1
        p0 = np.array(tail) + back * length * t0
        p1 = np.array(tail) + back * length * t1
        poly = [tuple((p0 + side * w0 / 2) * SUPER), tuple((p1 + side * w1 / 2) * SUPER),
                tuple((p1 - side * w1 / 2) * SUPER), tuple((p0 - side * w0 / 2) * SUPER)]
        draw.polygon(poly, fill=(*color, int(255 * a)))
    img = big.resize((SIZE, SIZE), Image.LANCZOS).filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(img).astype(np.float32)


def smoke(tail, back, length, start_radius, end_radius, color, alpha, seed):
    """A puffy smoke trail: overlapping soft blobs that grow and fade away from the tail."""
    rng = np.random.default_rng(seed)
    layer = np.zeros((SIZE, SIZE, 4), np.float32)
    side = np.array([-back[1], back[0]])
    puffs = 14
    for i in range(puffs):
        t = (i + rng.uniform(0, 0.6)) / puffs
        r = start_radius + (end_radius - start_radius) * t
        p = np.array(tail) + back * length * t + side * rng.normal(0, 0.25) * r
        d = np.sqrt((xs - p[0]) ** 2 + (ys - p[1]) ** 2) / r
        a = np.clip(1 - d, 0, 1) ** 1.6 * alpha * (1 - t) ** 1.2 * rng.uniform(0.7, 1.0)
        shade = rng.uniform(0.85, 1.1)
        blob = np.zeros_like(layer)
        blob[..., :3] = np.array(color) * shade
        blob[..., 3] = a * 255
        layer = over(layer, blob)
    return layer


def glow_dot(p, radius, color, strength):
    d = np.sqrt((xs - p[0]) ** 2 + (ys - p[1]) ** 2) / radius
    layer = np.zeros((SIZE, SIZE, 4), np.float32)
    layer[..., :3] = color
    layer[..., 3] = np.clip(np.exp(-d * d * 2.5) * strength, 0, 1) * 255
    return layer


def trails(round_kind, kind, nose, tail):
    """(layers drawn normally, layers added as light) behind one round."""
    nose, tail = np.array(nose, float), np.array(tail, float)
    length = np.linalg.norm(nose - tail)
    back = (tail - nose) / length
    width = length / {"ap": 4.6, "he": 4.5, "huge": 5.2, "mortar": 4.6, "rocket": 7.2}[round_kind]
    seed = int(nose[0] * 7 + nose[1] * 13)
    normal, light = [], []
    if kind == "tracer":
        normal.append(smoke(tail, back, length * 1.1, width * 0.35, width * 0.9, (60, 50, 48), 0.55, seed))
        light.append(streak(tail, back, length * 0.9, width * 0.55, width * 0.1, (255, 70, 30), 0.9, 1.2))
        light.append(glow_dot(tail, width * 0.6, (255, 120, 60), 0.9))
    elif kind == "hot":
        # the hot air and glow of a shell in flight: an orange motion streak, no smoke
        light.append(streak(tail, back, length * 1.0, width * 1.05, width * 0.2, (255, 110, 30), 0.75, 1.6))
        light.append(streak(tail, back, length * 0.55, width * 0.6, width * 0.1, (255, 200, 120), 0.6, 0.8))
    elif kind == "smoke":
        normal.append(smoke(tail, back, length * 1.0, width * 0.3, width * 0.85, (55, 45, 42), 0.75, seed))
        light.append(streak(tail, back, length * 0.35, width * 0.5, width * 0.1, (255, 120, 40), 0.5, 1.0))
    elif kind == "exhaust":
        normal.append(smoke(tail, back, length * 0.9, width * 0.5, width * 1.3, (150, 140, 135), 0.55, seed))
        light.append(streak(tail, back, length * 0.45, width * 0.9, width * 0.1, (255, 150, 50), 1.0, 1.0))
        light.append(glow_dot(tail, width * 0.9, (255, 200, 120), 1.0))
    return normal, light


def compose(name, with_cross):
    kind, trail, rounds = ICONS[name]
    img = make_plate()
    ch_glow, ch_sharp = drawn(crosshair_marks, 3.0)
    img = add(img, masked(ch_glow))
    img = over(img, masked(ch_sharp))
    for nose, tail in rounds:
        normal, light = trails(kind, trail, nose, tail)
        for layer in normal:
            img = over(img, masked(layer))
        for layer in light:
            img = add(img, masked(layer))
        round_layer = place_round(kind, nose, tail)
        img = add(img, masked(rim_glow(round_layer)))
        img = over(img, masked(round_layer))
        # a little impact-side glint at the nose so the rounds read as flying towards the target
        img = add(img, masked(glow_dot(nose, 3.0, (255, 180, 110), 0.35)))
    if with_cross:
        cross_glow, cross_sharp = drawn(cancel_cross, 3.0, 0.8)
        img = add(img, masked(cross_glow))
        img = over(img, masked(cross_sharp))
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGBA")


os.makedirs(OUT, exist_ok=True)
for icon in ICONS:
    compose(icon, False).save(OUT + "T_AimAbility_%s.png" % icon)
    compose(icon, True).save(OUT + "T_Cancel_AimAbility_%s.png" % icon)
    print("ICON", icon)
