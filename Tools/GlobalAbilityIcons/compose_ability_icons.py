"""Composes the global ability icons (512x512 cards with cut corners, no hexagon plate).

The aircraft, bombs, tanks and soldiers are the game's own meshes (render_ability_parts.py renders them to cut-outs);
sky, ground, tracers, muzzle flashes and impacts are drawn here so the icon stays free of the game's smoke effects.

Usage: python compose_ability_icons.py [icon names]      writes Saved/GlobalAbilityIcons/out/T_GA_<Name>.png + preview.png
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/"
RENDERS = ROOT + "renders/"
OUT = ROOT + "out/"
SIZE = 512
SS = 2                      # everything is drawn at twice the size and scaled down once at the end
CANVAS = SIZE * SS
CORNER = 34                 # size of the cut corners in final pixels
FRAME = 7                   # steel frame thickness in final pixels
# The button's hotkey label covers the bottom fifth of the card: the horizon and the impacts stay above it.
HORIZON = 0.72              # horizon height as a fraction of the card

# sky top, sky middle, horizon glow, far ground, near ground
PALETTES = {
    "ger": ((30, 45, 60), (84, 112, 128), (232, 196, 140), (86, 84, 62), (40, 40, 30)),
    "sov": ((52, 30, 30), (136, 84, 66), (244, 186, 120), (92, 74, 52), (46, 34, 26)),
}

# Render pixels removed from a cut-out: the Ho 229 keeps its nose gear strut in every pose the editor can show.
# These coordinates belong to the 2048 Ho229_Dive job; a new camera or resolution needs a new polygon.
ERASE = {"Ho229_Dive": [(1086, 1198), (1104, 1183), (1172, 1275), (1152, 1296)]}
# The barrage round render is a complete cartridge: a flying shell has left its brass case behind.
CROP_BELOW = {"Shell_AP": 412}

ys, xs = np.mgrid[0:CANVAS, 0:CANVAS].astype(np.float32)


def smooth(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0, 1)
    return t * t * (3 - 2 * t)


def over(base, layer):
    """Straight-alpha base, premultiplied layer."""
    a = layer[..., 3:4]
    out = base.copy()
    out[..., :3] = layer[..., :3] + base[..., :3] * (1 - a)
    out[..., 3:4] = a + base[..., 3:4] * (1 - a)
    return out


def add(base, layer, strength=1.0):
    """Additive light: flames, tracers, flashes."""
    out = base.copy()
    out[..., :3] = np.clip(base[..., :3] + layer[..., :3] * strength, 0, 1)
    return out


def noise(seed, scale_px, octaves=4):
    """Soft value noise in 0..1 for clouds and ground, made by scaling up random grids."""
    rng = np.random.default_rng(seed)
    total = np.zeros((CANVAS, CANVAS), np.float32)
    weight = 0.0
    for o in range(octaves):
        cells = max(2, int(CANVAS / (scale_px / 2 ** o)))
        grid = (rng.random((cells, cells)) * 255).astype(np.uint8)
        layer = np.asarray(Image.fromarray(grid).resize((CANVAS, CANVAS), Image.BICUBIC)).astype(np.float32) / 255
        total += layer * 0.5 ** o
        weight += 0.5 ** o
    return total / weight


def background(faction, seed, ground=True):
    """ground=False is open sky to the bottom of the card, for the aircraft that are shown in the air only."""
    top, mid, glow, far, near = [np.array(c, np.float32) / 255 for c in PALETTES[faction]]
    img = np.zeros((CANVAS, CANVAS, 4), np.float32)
    v = ys / CANVAS
    h = HORIZON if ground else 1.08
    sky = np.zeros((CANVAS, CANVAS, 3), np.float32)
    t1 = smooth(0.0, h * 0.72, v)[..., None]
    t2 = smooth(h * 0.55, h, v)[..., None]
    sky[...] = top * (1 - t1) + mid * t1
    sky = sky * (1 - t2) + glow * t2
    # thin high cloud bands stretched along the horizon, brighter towards the sun low on the left
    bands = noise(seed, CANVAS * 0.9, 5)
    bands = np.asarray(Image.fromarray((bands * 255).astype(np.uint8)).resize((CANVAS, CANVAS // 3)).resize(
        (CANVAS, CANVAS), Image.BICUBIC)).astype(np.float32) / 255
    cloud = smooth(0.52, 0.78, bands) * smooth(0.04, 0.5, v) * (1 - smooth(h * 0.9, h, v))
    sky = sky + (glow - sky) * (cloud[..., None] * 0.32)
    if not ground:
        img[..., :3] = sky
        img[..., 3] = 1
        return img
    # ground: darker towards the viewer, broken up by big soft field patches
    g = smooth(h, 1.0, v)[..., None]
    patches = noise(seed + 1, CANVAS * 0.35, 4)[..., None]
    ground = far * (1 - g) + near * g
    ground = ground * (0.82 + patches * 0.36)
    is_ground = smooth(h - 0.001, h + 0.002, v)[..., None]
    img[..., :3] = sky * (1 - is_ground) + ground * is_ground
    # haze where the ground meets the sky
    haze = np.exp(-((v - h) / 0.022) ** 2)[..., None] * 0.55
    img[..., :3] = img[..., :3] + (glow - img[..., :3]) * haze
    # a low tree line sitting on the horizon
    rng = np.random.default_rng(seed + 2)
    heights = np.interp(np.arange(CANVAS), np.arange(0, CANVAS + 9, 9), rng.random(CANVAS // 9 + 2)) * 0.012 + 0.003
    trees = (v > h - heights[None, :]) & (v < h + 0.004)
    img[..., :3][trees] = img[..., :3][trees] * 0.42 + far * 0.2
    img[..., 3] = 1
    return img


def load_cutout(name):
    """-> premultiplied RGBA float array, points dict. The HDR pass alpha is 1 - coverage."""
    color = np.asarray(Image.open(RENDERS + name + "_color.png").convert("RGB")).astype(np.float32) / 255
    alpha = 1 - np.asarray(Image.open(RENDERS + name + "_mask.png").convert("RGBA"))[..., 3].astype(np.float32) / 255
    pre = np.dstack([color * alpha[..., None], alpha])
    if name in ERASE:
        mask = Image.new("L", (alpha.shape[1], alpha.shape[0]), 255)
        ImageDraw.Draw(mask).polygon(ERASE[name], fill=0)
        pre = pre * (np.asarray(mask).astype(np.float32) / 255)[..., None]
    if name in CROP_BELOW:
        pre[CROP_BELOW[name]:] = 0
    with open(RENDERS + name + "_points.json") as fh:
        return pre, json.load(fh)


def bbox(pre):
    rows = np.nonzero(pre[..., 3].max(axis=1) > 0.3)[0]
    cols = np.nonzero(pre[..., 3].max(axis=0) > 0.3)[0]
    return cols[0], rows[0], cols[-1], rows[-1]


def place(pre, points, anchor, at, scale, rotate=0.0, gain=1.0, tint=None, tint_amount=0.0):
    """Puts a cut-out on the canvas with point `anchor` (name or x, y of the render) at `at` (final pixels).
    rotate is clockwise in degrees. Returns the premultiplied layer and a function mapping render pixels to canvas pixels."""
    src_anchor = np.array(points[anchor] if isinstance(anchor, str) else anchor, np.float64)
    dst_anchor = np.array(at, np.float64) * SS
    k = scale * SS
    ang = math.radians(rotate)
    cos_a, sin_a = math.cos(ang), math.sin(ang)

    def to_canvas(p):
        d = (np.array(p, np.float64) - src_anchor) * k
        return dst_anchor + np.array([d[0] * cos_a - d[1] * sin_a, d[0] * sin_a + d[1] * cos_a])

    # affine maps output pixels back to source pixels
    inv = 1.0 / k
    a, b = cos_a * inv, sin_a * inv
    d, e = -sin_a * inv, cos_a * inv
    c = src_anchor[0] - (a * dst_anchor[0] + b * dst_anchor[1])
    f = src_anchor[1] - (d * dst_anchor[0] + e * dst_anchor[1])
    src = pre
    # shrink first so the bicubic affine does not alias (the renders are much bigger than the card)
    shrink = max(1, int(1.0 / k))
    if shrink > 1:
        h, w = pre.shape[:2]
        channels = [np.asarray(Image.fromarray(pre[..., i]).resize((w // shrink, h // shrink), Image.BOX))
                    for i in range(4)]
        src = np.dstack(channels)
        a, b, c, d, e, f = a / shrink, b / shrink, c / shrink, d / shrink, e / shrink, f / shrink
    channels = [np.asarray(Image.fromarray(src[..., i]).transform((CANVAS, CANVAS), Image.AFFINE, (a, b, c, d, e, f),
                                                                  Image.BICUBIC)) for i in range(4)]
    layer = np.clip(np.dstack(channels), 0, 1)
    layer[..., :3] = np.minimum(layer[..., :3] * gain, layer[..., 3:4])
    if tint is not None:
        layer[..., :3] = layer[..., :3] * (1 - tint_amount) + np.array(tint, np.float32) / 255 * layer[..., 3:4] * tint_amount
    return layer, to_canvas


def rim(layer, color, width, strength):
    """Thin light edge around a cut-out so dark planes separate from the sky."""
    alpha = Image.fromarray((layer[..., 3] * 255).astype(np.uint8))
    grown = np.asarray(alpha.filter(ImageFilter.GaussianBlur(width))).astype(np.float32) / 255
    edge = np.clip(grown * 2.2 - layer[..., 3] * 2.2, 0, 1) * strength
    out = np.zeros_like(layer)
    out[..., :3] = np.array(color, np.float32) / 255 * edge[..., None]
    out[..., 3] = edge
    return out


def drawn(draw_fn, blur=0.0):
    """Draws on an L mask and returns it as 0..1 float."""
    big = Image.new("L", (CANVAS, CANVAS), 0)
    draw_fn(ImageDraw.Draw(big))
    if blur:
        big = big.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(big).astype(np.float32) / 255


def light_layer(mask, color):
    layer = np.zeros((CANVAS, CANVAS, 4), np.float32)
    layer[..., :3] = np.array(color, np.float32) / 255 * mask[..., None]
    layer[..., 3] = mask
    return layer


def taper(draw, start, end, w0, w1, fill=255):
    """A polygon streak from start (w0 wide) to end (w1 wide)."""
    start, end = np.array(start, float), np.array(end, float)
    d = end - start
    side = np.array([-d[1], d[0]]) / (np.linalg.norm(d) + 1e-6)
    draw.polygon([tuple(start + side * w0 / 2), tuple(end + side * w1 / 2),
                  tuple(end - side * w1 / 2), tuple(start - side * w0 / 2)], fill=fill)


def tracer(img, start, end, width, phase=0.0, dashes=4):
    """Bright tapered tracer rounds along one straight line from the muzzle: separate dashes, a glow and no smoke.
    phase (0..1 of a dash period) staggers guns firing side by side."""
    start, end = np.array(start, float), np.array(end, float)
    period = 1.0 / dashes
    dash = period * 0.62

    def dash_fn(draw, scale=1.0):
        for i in range(dashes):
            t1 = min(1.0, (i + phase) * period + dash)
            t0 = (i + phase) * period
            a, b = start + (end - start) * t0, start + (end - start) * t1
            taper(draw, b, a, width * scale, width * scale * 0.15)

    img = add(img, light_layer(drawn(lambda dr: dash_fn(dr, 2.6), blur=width * 1.1), (255, 120, 30)), 0.8)
    img = add(img, light_layer(drawn(lambda dr: dash_fn(dr, 1.0), blur=0.8), (255, 225, 150)), 1.0)
    return img


def star(img, at, size, angle_deg, seed=0):
    """A crisp muzzle flash: a long flame along the shot, short side spikes and a hot core."""
    at = np.array(at, float)
    ang = math.radians(angle_deg)
    fwd = np.array([math.cos(ang), math.sin(ang)])
    side = np.array([-fwd[1], fwd[0]])

    def flame(draw, k):
        for direction, length, width in ((fwd, 1.0, 0.34), (side, 0.42, 0.2), (-side, 0.42, 0.2), (-fwd, 0.2, 0.2)):
            tip = at + direction * size * length * k
            perp = np.array([-direction[1], direction[0]])
            base = size * width * k
            draw.polygon([tuple(at + perp * base), tuple(tip), tuple(at - perp * base)], fill=255)

    img = add(img, light_layer(drawn(lambda dr: flame(dr, 1.5), blur=size * 0.22), (255, 110, 25)), 0.9)
    img = add(img, light_layer(drawn(lambda dr: flame(dr, 1.0), blur=0.8), (255, 190, 80)), 1.0)
    img = add(img, light_layer(drawn(lambda dr: flame(dr, 0.5), blur=0.6), (255, 255, 235)), 1.0)
    return img


def impact(img, at, size, seed):
    """Sparks kicked up where a burst lands: a short hot fan on the ground, again without smoke."""
    rng = np.random.default_rng(seed)
    at = np.array(at, float)

    def fan(draw):
        for _ in range(9):
            ang = math.radians(rng.uniform(-155, -25))
            length = size * rng.uniform(0.45, 1.0)
            tip = at + np.array([math.cos(ang), math.sin(ang) * 0.9]) * length
            taper(draw, at, tip, size * 0.11, 0.5)

    img = add(img, light_layer(drawn(fan, blur=size * 0.12), (255, 130, 40)), 0.9)
    img = add(img, light_layer(drawn(fan, blur=0.6), (255, 225, 160)), 0.9)
    glow = np.exp(-(((xs - at[0]) / (size * 1.0)) ** 2 + ((ys - at[1]) / (size * 0.36)) ** 2))
    return add(img, light_layer(glow, (255, 150, 60)), 0.7)


def prop_disc(img, hub, right_axis, up_axis, radius):
    """A pale spinning-prop disc; the blades of the render are one frozen pose."""
    hub = np.array(hub, float)
    pts = [tuple(hub + right_axis * radius * math.cos(t) + up_axis * radius * math.sin(t))
           for t in np.linspace(0, 2 * math.pi, 64)]
    disc = drawn(lambda dr: dr.polygon(pts, fill=255), blur=1.2)
    return add(img, light_layer(disc, (255, 245, 225)), 0.09)


def frame_mask():
    """1 inside the card: a square with cut corners."""
    c = CORNER * SS

    def poly(inset):
        i = inset * SS
        k = (CORNER - inset * 0.59) * SS
        w = CANVAS - 1
        return [(k, i), (w - k, i), (w - i, k), (w - i, w - k), (w - k, w - i), (k, w - i), (i, w - k), (i, k)]

    return poly


def apply_frame(img):
    poly = frame_mask()
    outer = drawn(lambda dr: dr.polygon(poly(0), fill=255), blur=0.7)
    inner = drawn(lambda dr: dr.polygon(poly(FRAME), fill=255), blur=0.7)
    edge_in = drawn(lambda dr: dr.polygon(poly(FRAME + 2), fill=255), blur=0.7)
    # brushed steel: light from the top left, dark to the bottom right
    light = 1 - smooth(-0.1, 1.1, (xs + ys) / (2 * CANVAS))
    steel = np.dstack([0.20 + light * 0.34, 0.21 + light * 0.34, 0.22 + light * 0.34])
    out = img.copy()
    out[..., :3] = img[..., :3] * inner[..., None] + steel * (1 - inner[..., None])
    # a thin dark line between frame and picture
    line = np.clip(inner - edge_in, 0, 1)[..., None]
    out[..., :3] = out[..., :3] * (1 - line * 0.75)
    # soft inner shadow so the picture sits under the frame
    shade = 1 - np.asarray(Image.fromarray((edge_in * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(9 * SS))
                           ).astype(np.float32) / 255
    out[..., :3] = out[..., :3] * (1 - (shade * 0.55 * inner)[..., None])
    out[..., 3] = outer
    return out


def finish(img):
    img = apply_frame(img)
    rgba = (np.clip(img, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA").resize((SIZE, SIZE), Image.LANCZOS)


# --- shared picture elements -----------------------------------------------------------------------------------------

def blast(img, at, radius, seed):
    """A ground burst drawn as a hot spiked flash, a shock ring and thrown earth: nothing that hides the picture."""
    rng = np.random.default_rng(seed)
    at = np.array(at, float)
    spikes = 14

    def burst(draw, k):
        pts = []
        for i in range(spikes * 2):
            ang = math.pi * 2 * i / (spikes * 2)
            r = radius * k * (0.75 + 0.25 * math.sin(i * 7.3 + seed) ** 2 if i % 2 == 0 else 0.38)
            r_y = r * (1.0 if math.sin(ang) < 0 else 0.34)
            pts.append(tuple(at + np.array([math.cos(ang) * r, math.sin(ang) * r_y])))
        draw.polygon(pts, fill=255)

    ring = drawn(lambda dr: dr.ellipse([at[0] - radius * 1.5, at[1] - radius * 0.32, at[0] + radius * 1.5, at[1] + radius * 0.32],
                                       outline=255, width=max(2, int(radius * 0.05))), blur=radius * 0.03)
    img = add(img, light_layer(ring, (255, 170, 90)), 0.55)
    img = add(img, light_layer(drawn(lambda dr: burst(dr, 1.25), blur=radius * 0.16), (255, 90, 20)), 0.95)
    img = add(img, light_layer(drawn(lambda dr: burst(dr, 0.95), blur=1.0), (255, 170, 60)), 1.0)
    img = add(img, light_layer(drawn(lambda dr: burst(dr, 0.52), blur=1.0), (255, 245, 210)), 1.0)
    # clods of earth in dark flecks
    clods = [(at + np.array([rng.uniform(-1.2, 1.2) * radius, rng.uniform(-1.05, -0.2) * radius]),
              rng.uniform(0.02, 0.05) * radius) for _ in range(12)]

    def draw_clods(draw):
        for p, s in clods:
            draw.ellipse([p[0] - s, p[1] - s, p[0] + s, p[1] + s], fill=255)

    mask = drawn(draw_clods, blur=0.8)
    specks = np.zeros((CANVAS, CANVAS, 4), np.float32)
    specks[..., :3] = 0.07 * mask[..., None]
    specks[..., 3] = mask
    return over(img, specks)


def fall_lines(img, at, angle_deg, length, spread, alpha=0.35):
    """Thin speed lines trailing a falling bomb or shell."""
    at = np.array(at, float)
    ang = math.radians(angle_deg + 180)
    back = np.array([math.cos(ang), math.sin(ang)])
    side = np.array([-back[1], back[0]])

    def lines(draw):
        for off, k in ((-1.0, 0.7), (0.0, 1.0), (1.0, 0.8)):
            start = at + side * off * spread
            taper(draw, start, start + back * length * k, 2.6 * SS * 0.5 + 1, 0.4)

    return add(img, light_layer(drawn(lines, blur=1.0), (255, 235, 200)), alpha)


def prop_item(img, name, center, length, rotate, gain=1.0, tint=None, tint_amount=0.0, tall=False):
    """Places a bomb, shell, soldier or tank by its visible extent: `length` final pixels along the render's x axis
    (or its y axis when tall)."""
    cut, pts = load_cutout(name)
    x0, y0, x1, y1 = bbox(cut)
    extent = (y1 - y0) if tall else (x1 - x0)
    layer, _ = place(cut, pts, ((x0 + x1) / 2, (y0 + y1) / 2), center, length / extent, rotate=rotate, gain=gain,
                     tint=tint, tint_amount=tint_amount)
    return over(img, layer)


ROTORS = ("B_Rotor1", "B_Rotor2", "B_Rotor2_001", "B_Rotor2_002", "B_Rotor2_003", "B_Rotor2_004")


def plane(img, cut, pts, at, scale, depth, sky, gain=1.0, prop=None, guns=()):
    """Places one aircraft by its origin. depth 0 = foreground, 1 = far: far planes fade towards the sky colour."""
    layer, to_canvas = place(cut, pts, "origin", at, scale, gain=gain, tint=sky, tint_amount=0.42 * depth)
    img = over(img, layer)
    if prop:
        o = to_canvas(pts["origin"])
        r_axis = (to_canvas(pts["right"]) - o) / 300.0
        u_axis = (to_canvas(pts["up"]) - o) / 300.0
        for bone, radius in prop:
            if bone in pts:
                img = prop_disc(img, to_canvas(pts[bone]), r_axis, u_axis, radius)
    return img, [to_canvas(pts[g]) for g in guns], to_canvas


def angle_to(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def strafe_run(img, muzzles, direction, length, flash, width, dashes):
    """All guns of one aircraft fire parallel along its nose: same direction, same length, no target to converge on."""
    angle = math.degrees(math.atan2(direction[1], direction[0]))
    for j, m in enumerate(muzzles):
        img = tracer(img, m, m + direction * length, width, phase=j * 0.37, dashes=dashes)
        img = star(img, m, flash, angle)
    return img


SKY_GER = (112, 134, 146)
SKY_SOV = (150, 110, 96)
GROUND_Y = 400          # where things land, in final pixels


# --- the icons ---------------------------------------------------------------------------------------------------------

def forward_on_screen(pts):
    d = np.array(pts["forward"], float) - np.array(pts["origin"], float)
    return d / np.linalg.norm(d)


def ju87_strafe(count):
    """Ju 87s in the air with their wing MGs firing straight ahead."""
    cut, pts = load_cutout("Ju87_Dive")
    img = background("ger", 11, ground=False)
    direction = forward_on_screen(pts)
    # back to front, in a line abreast across the flight path; the lead plane is the big one
    formation = [((76, 284), 0.13, 0.9), ((372, 66), 0.15, 0.75), ((226, 220), 0.262, 0.0)][-count:]
    for at, scale, depth in formation:
        lead = depth == 0
        img, muzzles, _ = plane(img, cut, pts, at, scale, depth, SKY_GER, gain=1.45,
                                prop=[("B_Rotor1", 135.0)], guns=("W1", "W2"))
        img = strafe_run(img, muzzles, direction, (250 if lead else 150) * SS, (17 if lead else 10) * SS,
                         (3.6 if lead else 2.2) * SS, 4 if lead else 3)
    return finish(img)


def ho229_strafe():
    """Two Ho 229 jets in the air with the twin 30 mm firing straight ahead: fewer, heavier rounds than the Stuka's MGs."""
    cut, pts = load_cutout("Ho229_Dive")
    img = background("ger", 23, ground=False)
    direction = forward_on_screen(pts)
    for at, scale, depth in [((340, 74), 0.135, 0.8), ((196, 214), 0.25, 0.0)]:
        lead = depth == 0
        img, muzzles, _ = plane(img, cut, pts, at, scale, depth, SKY_GER, gain=1.15, guns=("B_Gun_L", "B_Gun_R"))
        img = strafe_run(img, muzzles, direction, (270 if lead else 170) * SS, (24 if lead else 13) * SS,
                         (5.0 if lead else 3.0) * SS, 3)
    return finish(img)


# The bomb sticks below are laid out on a taller page and squeezed to end above the ground line.
STICK_TOP = 190
STICK_SQUEEZE = 0.78


def level_bomber(faction, plane_name, seed, plane_at, scale, gain, bomb, bomb_length, stick, blasts, prop_r):
    """One bomber flying right with its load on the way down: count and size of the bombs say which ability it is.
    stick = list of (x, y, fall angle) in final pixels; blasts = list of (x, radius)."""
    cut, pts = load_cutout(plane_name)
    sky = SKY_GER if faction == "ger" else SKY_SOV
    img = background(faction, seed)
    for x, radius in blasts:
        img = blast(img, (x * SS, GROUND_Y * SS), radius * SS, seed + x)
    for x, y, angle in stick:
        y = STICK_TOP + (y - STICK_TOP) * STICK_SQUEEZE
        img = fall_lines(img, (x * SS, y * SS), angle, bomb_length * 0.9 * SS, bomb_length * 0.16 * SS, 0.32)
        img = prop_item(img, bomb, (x, y), bomb_length, angle)
    img, _, _ = plane(img, cut, pts, plane_at, scale, 0.0, sky, gain=gain, prop=[(r, prop_r) for r in ROTORS])
    return finish(img)


def tank_drop(faction, plane_name, tanks, formation, seed, ground=True, plane_gain=1.0, prop_r=100.0):
    """Transports each carrying a tank: tanks and formation (at, scale, depth) run from the far planes to the lead."""
    cut, pts = load_cutout(plane_name)
    sky = SKY_GER if faction == "ger" else SKY_SOV
    img = background(faction, seed, ground=ground)
    plane_px = np.linalg.norm(np.array(pts["forward"]) - np.array(pts["origin"]))
    for tank, (at, scale, depth) in zip(tanks, formation):
        tank_cut, tank_pts = load_cutout(tank)
        tank_px = np.linalg.norm(np.array(tank_pts["forward"]) - np.array(tank_pts["origin"]))
        tank_scale = scale * plane_px / tank_px * 1.1
        _, to_canvas = place(cut, pts, "origin", at, scale)
        o = to_canvas(pts["origin"])
        up = (to_canvas(pts["up"]) - o) / 300.0
        tank_at = (o - up * 395) / SS
        layer, tank_canvas = place(tank_cut, tank_pts, "origin", tank_at, tank_scale, gain=1.25, tint=sky,
                                   tint_amount=0.42 * depth)
        # the two lifting cables, from turret height to the belly
        lift = [o + up * -120 + np.array([d * scale * SS, 0]) for d in (-120, 150)]
        hook = [tank_canvas(tank_pts["origin"]) + up * 150 + np.array([d * scale * SS, 0]) for d in (-170, 190)]

        def draw_cables(draw):
            for a, b in zip(lift, hook):
                draw.line([tuple(a), tuple(b)], fill=255, width=max(2, int(5 * scale * SS)))

        cables = drawn(draw_cables, blur=0.7)
        cable_layer = np.zeros_like(img)
        cable_layer[..., :3] = 0.08 * cables[..., None]
        cable_layer[..., 3] = cables
        img = over(img, cable_layer)
        img = over(img, layer)
        img, _, _ = plane(img, cut, pts, at, scale, depth, sky, gain=plane_gain, prop=[(r, prop_r) for r in ROTORS])
    return finish(img)


def paratroopers(faction, plane_name, seed, plane_at, scale, gain, chutes, prop_r):
    """The transport passing over with the first soldiers already under their canopies.
    chutes = (render, x, y, height in final pixels, depth), far ones first."""
    cut, pts = load_cutout(plane_name)
    sky = SKY_GER if faction == "ger" else SKY_SOV
    img = background(faction, seed)
    for name, x, y, height, depth in chutes:
        img = prop_item(img, name, (x, y), height, 0.0, tint=sky, tint_amount=0.4 * depth, tall=True)
    img, _, _ = plane(img, cut, pts, plane_at, scale, 0.0, sky, gain=gain, prop=[(r, prop_r) for r in ROTORS])
    return finish(img)


def barrage():
    """Heavy shells plunging into the target area: the rounds with a hot streak, and the first impacts."""
    img = background("ger", 37)
    for x, radius in ((296, 38), (176, 28), (398, 24)):
        img = blast(img, (x * SS, GROUND_Y * SS), radius * SS, x)
    fall = 104
    for x, y, length in [(150, 86, 60), (384, 104, 60), (270, 70, 66), (214, 232, 92), (332, 262, 108)]:
        ang = math.radians(fall + 180)
        back = np.array([math.cos(ang), math.sin(ang)])
        tail = np.array([x, y]) * SS + back * length * 0.5 * SS

        def streak(k, w):
            return drawn(lambda dr: taper(dr, tail, tail + back * length * k * SS, length * w * SS, 1.0),
                         blur=length * 0.05 * SS)

        img = add(img, light_layer(streak(1.0, 0.2), (255, 100, 30)), 0.55)
        img = add(img, light_layer(streak(0.5, 0.1), (255, 215, 150)), 0.5)
        # the shell render stands nose up
        img = prop_item(img, "Shell_AP", (x, y), length, fall + 90, gain=0.9, tall=True)
    return finish(img)


PE8_THREE = [((120, 82), 0.165, 0.95), ((342, 72), 0.195, 0.75), ((222, 216), 0.40, 0.0)]
JU390_THREE = [((112, 74), 0.20, 0.95), ((356, 66), 0.24, 0.75), ((244, 222), 0.44, 0.0)]
JU390_TWO = [((356, 70), 0.24, 0.75), ((236, 218), 0.46, 0.0)]


def ju390_drop(tanks, seed):
    """The German vehicle drops: open sky, one Ju 390 per vehicle, the same aircraft as the paratrooper ability."""
    formation = JU390_THREE if len(tanks) == 3 else JU390_TWO
    return tank_drop("ger", "Ju390_Level", tanks, formation, seed, ground=False, plane_gain=1.2, prop_r=85.0)


def light_pool(img, centre, radius_x, radius_y, color, strength, blur):
    """A soft ellipse of light on the ground."""
    x, y = centre[0] * SS, centre[1] * SS
    pool = drawn(lambda dr: dr.ellipse([x - radius_x * SS, y - radius_y * SS, x + radius_x * SS, y + radius_y * SS],
                                       fill=255), blur=blur * SS)
    return add(img, light_layer(pool, color), strength)


def eye_mark(img, centre, half_width, color):
    """The recon symbol: an open eye, drawn like the target area of the ability."""
    cx, cy = centre[0] * SS, centre[1] * SS
    w = half_width * SS
    h = w * 0.52
    upper = [(cx + w * math.cos(a), cy - h * math.sin(a)) for a in np.linspace(0, math.pi, 40)]
    lower = [(cx + w * math.cos(a), cy - h * math.sin(a)) for a in np.linspace(math.pi, 2 * math.pi, 40)]

    def lids(draw):
        draw.line(upper + lower + upper[:1], fill=255, width=int(7 * SS))
        draw.ellipse([cx - h * 0.62, cy - h * 0.62, cx + h * 0.62, cy + h * 0.62], outline=255, width=int(6 * SS))
        draw.ellipse([cx - h * 0.24, cy - h * 0.24, cx + h * 0.24, cy + h * 0.24], fill=255)

    shadow = np.zeros((CANVAS, CANVAS, 4), np.float32)
    shadow[..., 3] = drawn(lids, blur=6 * SS) * 0.7
    img = over(img, shadow)
    return add(img, light_layer(drawn(lids, blur=0.6), color), 1.0)


def recon_flight(faction, plane_name, seed, plane_at, scale, gain, prop_r):
    """A lone unarmed aircraft with the ground lit under it and the eye of the target area."""
    cut, pts = load_cutout(plane_name)
    sky = SKY_GER if faction == "ger" else SKY_SOV
    img = background(faction, seed)
    img = light_pool(img, (256, GROUND_Y), 210, 56, (190, 235, 255), 0.5, 26)
    img = eye_mark(img, (256, 368), 112, (225, 245, 255))
    img, _, _ = plane(img, cut, pts, plane_at, scale, 0.0, sky, gain=gain, prop=[(r, prop_r) for r in ROTORS])
    return finish(img)


def flare_curtain():
    """A bomber over a row of burning parachute flares that light the ground under them."""
    cut, pts = load_cutout("Ju390_Level")
    img = background("ger", 101)
    row = [(96, 262, 84), (204, 276, 92), (312, 262, 84), (420, 276, 92)]
    for x, y, _ in row:
        img = light_pool(img, (x, GROUND_Y + 6), 76, 26, (255, 196, 110), 0.65, 16)
    for x, y, height in row:
        img = prop_item(img, "Flare_Hanging", (x, y), height, 0.0, gain=1.15, tall=True)
        burn = (x, y + height * 0.42)
        img = light_pool(img, burn, 34, 34, (255, 170, 70), 0.9, 12)
        img = light_pool(img, burn, 13, 13, (255, 250, 225), 1.0, 2)
    img, _, _ = plane(img, cut, pts, (250, 112), 0.44, 0.0, SKY_GER, gain=1.2, prop=[(r, 85.0) for r in ROTORS])
    return finish(img)


def air_minefield(faction, plane_name, mine, seed, plane_at, scale, gain, prop_r):
    """The minelayer overhead, mines on the way down and the first ones lying in a strip on the ground."""
    cut, pts = load_cutout(plane_name)
    sky = SKY_GER if faction == "ger" else SKY_SOV
    img = background(faction, seed)
    for x, y, size in [(92, 408, 72), (214, 424, 82), (334, 402, 70), (432, 426, 78)]:
        img = light_pool(img, (x, y + 8), size * 0.62, size * 0.2, (255, 96, 40), 0.55, 8)
        img = prop_item(img, mine, (x, y), size, 0.0, gain=1.25)
    for x, y, size in [(152, 244, 62), (268, 306, 68), (384, 256, 60)]:
        img = fall_lines(img, (x * SS, (y - 30) * SS), 90, 70 * SS, 7 * SS, 0.4)
        img = prop_item(img, mine, (x, y), size, 0.0, gain=1.2)
    img, _, _ = plane(img, cut, pts, plane_at, scale, 0.0, sky, gain=gain, prop=[(r, prop_r) for r in ROTORS])
    return finish(img)


def radixite_nuke(faction, seed):
    """The rocket coming down into the green blast that is the whole ability."""
    img = background(faction, seed)
    glow = (70, 255, 110)
    img = light_pool(img, (256, GROUND_Y), 238, 72, glow, 0.8, 30)
    ring = drawn(lambda dr: dr.ellipse([18 * SS, (GROUND_Y - 52) * SS, 494 * SS, (GROUND_Y + 52) * SS],
                                       outline=255, width=int(7 * SS)), blur=1.5)
    img = add(img, light_layer(ring, (170, 255, 170)), 0.9)
    img = light_pool(img, (256, GROUND_Y - 118), 124, 96, (30, 190, 70), 0.75, 30)
    img = light_pool(img, (256, GROUND_Y - 118), 84, 60, (60, 235, 100), 0.6, 18)
    img = light_pool(img, (256, GROUND_Y - 44), 30, 76, (70, 235, 110), 0.7, 10)
    img = light_pool(img, (256, GROUND_Y - 118), 40, 30, (200, 255, 190), 0.75, 12)
    img = fall_lines(img, (256 * SS, 140 * SS), 90, 110 * SS, 12 * SS, 0.55)
    img = prop_item(img, "V2_Rocket", (256, 118), 186, 180.0, gain=1.15, tall=True)
    return finish(img)


ICONS = {
    "3xJu87Strafing": lambda: ju87_strafe(3),
    "2xHo229Strafing": ho229_strafe,
    "Barrage": barrage,
    "Ju390Carpet250": lambda: level_bomber(
        "ger", "Ju390_Level", 41, (250, 150), 0.52, 1.2, "Bomb_250", 32,
        [(150, 214, 72), (188, 256, 74), (226, 298, 76), (262, 340, 78), (296, 382, 80),
         (92, 262, 74), (128, 304, 76), (166, 346, 78), (204, 388, 80)],
        [(300, 34), (216, 30), (130, 24)], 95.0),
    "PE8Carpet500": lambda: level_bomber(
        "sov", "PE8_Level", 43, (236, 158), 0.40, 1.0, "Bomb_500", 38,
        [(150, 232, 74), (196, 270, 76), (242, 308, 78), (286, 350, 80),
         (96, 286, 76), (142, 324, 78), (188, 366, 80), (330, 392, 82)],
        [(322, 36), (214, 30)], 100.0),
    "PE8Carpet1000": lambda: level_bomber(
        "sov", "PE8_Level", 47, (236, 158), 0.40, 1.0, "Bomb_2000", 76,
        [(150, 254, 74), (230, 312, 78), (306, 366, 82), (112, 352, 78)],
        [(330, 44)], 100.0),
    "PE8Carpet3000": lambda: level_bomber(
        "sov", "PE8_Level", 53, (236, 138), 0.40, 1.0, "Bomb_3000", 150,
        [(264, 318, 80)], [], 100.0),
    "SovietT34Drop": lambda: tank_drop("sov", "PE8_Level", ("T3476", "T3476", "T3485"), PE8_THREE, 59),
    "SovietT34DropVariation": lambda: tank_drop("sov", "PE8_Level", ("T34AA", "T34E", "T34100"), PE8_THREE, 61),
    "GerPzIIIFlak41Drop": lambda: ju390_drop(("Flak41", "Flak41"), 73),
    "GerSdkfz7FlakDrop": lambda: ju390_drop(("Sdkfz7Flak36", "Sdkfz7Flak"), 79),
    "GerRadixitePzIIMarderDrop": lambda: ju390_drop(("PzIIRadix", "PzIIRadix", "MarderDual"), 83),
    "GerFlakLuchsStugDrop": lambda: ju390_drop(("FlakLuchs", "FlakLuchs", "StugIII"), 89),
    "GerSpotterPzIIIDrop": lambda: ju390_drop(("PzIIIJ", "PzIVSpotter"), 97),
    "SovietParatroopers": lambda: paratroopers(
        "sov", "PE8_Level", 67, (250, 118), 0.36, 1.0,
        [("Para_Sov_B", 420, 232, 104, 0.7), ("Para_Sov_A", 92, 246, 116, 0.55),
         ("Para_Sov_B", 196, 284, 156, 0.0), ("Para_Sov_A", 330, 300, 166, 0.0)], 90.0),
    "GerParatroopers": lambda: paratroopers(
        "ger", "Ju390_Level", 71, (250, 112), 0.48, 1.2,
        [("Para_Ger_B", 420, 232, 104, 0.7), ("Para_Ger_A", 92, 246, 116, 0.55),
         ("Para_Ger_B", 196, 284, 156, 0.0), ("Para_Ger_A", 330, 300, 166, 0.0)], 85.0),
    "GerReconFlight": lambda: recon_flight("ger", "Fw189_Level", 103, (250, 158), 0.38, 1.05, 80.0),
    "SovietReconFlight": lambda: recon_flight("sov", "PE2_Level", 107, (262, 150), 0.36, 1.0, 80.0),
    "GerFlareCurtain": flare_curtain,
    "GerAirMinefield": lambda: air_minefield("ger", "Me410_Level", "Mine_Teller", 109, (256, 196), 0.46, 0.8, 80.0),
    "SovietAirMinefield": lambda: air_minefield("sov", "PE8_Level", "Mine_TM41", 113, (250, 112), 0.36, 1.0, 90.0),
    "GerRadixiteNuke": lambda: radixite_nuke("ger", 127),
    "SovietRadixiteNuke": lambda: radixite_nuke("sov", 131),
}


def main():
    os.makedirs(OUT, exist_ok=True)
    names = sys.argv[1:] or list(ICONS)
    made = []
    for name in names:
        icon = ICONS[name]()
        icon.save(OUT + "T_GA_%s.png" % name)
        made.append(icon)
        print("ICON", name)
    # review sheet: a big version with the sizes the button really uses under it
    columns = min(len(made), 4)
    rows = (len(made) + columns - 1) // columns
    sheet = Image.new("RGBA", (columns * 330 + 10, rows * 430 + 10), (28, 28, 30, 255))
    for i, icon in enumerate(made):
        x, y = 10 + (i % columns) * 330, 10 + (i // columns) * 430
        sheet.alpha_composite(icon.resize((320, 320), Image.LANCZOS), (x, y))
        sheet.alpha_composite(icon.resize((96, 96), Image.LANCZOS), (x, y + 326))
        sheet.alpha_composite(icon.resize((64, 64), Image.LANCZOS), (x + 104, y + 326))
        sheet.alpha_composite(icon.resize((48, 48), Image.LANCZOS), (x + 176, y + 326))
    sheet.save(OUT + "preview.png")


if __name__ == "__main__":
    main()
