"""Round 3b global ability icons (Icons B agent, 2026-11-05): guns, buildings, strikes, supplies and effects.

Same card, palette, relight and photographic finish as compose_ability_icons.py (imported, not copied); the cut-outs
are the R3B_* renders of round3b_render_parts.py plus the existing ones (Ju87_Dive, Ju390_Level, Para_Ger_*,
Shell_AP, Bomb_250, Flare_Hanging, T3485_Ground). Effects (beam, fire, flare light, tracers) are drawn, as before.

Usage: python round3b_compose_icons.py [icon names]
Writes Saved/GlobalAbilityIcons/out/T_GA_<Name>.png and Saved/AbilityIconsB/preview_b.png.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compose_ability_icons as C  # noqa: E402

SS, CANVAS, GROUND_Y, SKY = C.SS, C.CANVAS, C.GROUND_Y, C.SKY_GER
OUT = C.OUT
WORK = "D:/UE5Projects/RTS_Survival/Saved/AbilityIconsB/"
# The Bf 109's gear stays out in its dive pose (hide_bone has no effect in the editor, like the Ho 229's strut): both
# legs are erased here, in the 2048 R3B_Bf109_Dive render's pixels (checked on a gridded crop, 2026-11-05).
R3B_ERASE = {"R3B_Bf109_Dive": [[(995, 1300), (1065, 1232), (1098, 1238), (1084, 1300), (1084, 1392), (995, 1392)],
                                [(1153, 1298), (1217, 1293), (1224, 1330), (1207, 1370), (1163, 1370), (1153, 1330)]]}
_load_uncached = C.load_cutout_uncached


def _load_with_erase(name):
    pre, points = _load_uncached(name)
    if name in R3B_ERASE:
        mask = Image.new("L", (pre.shape[1], pre.shape[0]), 255)
        for poly in R3B_ERASE[name]:
            ImageDraw.Draw(mask).polygon(poly, fill=0)
        pre = pre * (np.asarray(mask).astype(np.float32) / 255)[..., None]
    return pre, points


C.load_cutout_uncached = _load_with_erase


# --- placement helpers ---------------------------------------------------------------------------------------------

def put(img, name, center, size, tall=False, rotate=0.0, gain=1.0, depth=0.0, tint=None, tint_amount=None):
    """Places a cut-out by its visible box centre; size is its width (or height when tall) in final pixels.
    Returns the image and the placed box (x0, y0, x1, y1) in final pixels (unrotated)."""
    cut, pts = C.load_cutout(name)
    x0, y0, x1, y1 = C.bbox(cut)
    scale = size / ((y1 - y0) if tall else (x1 - x0))
    layer, _ = C.place(cut, pts, ((x0 + x1) / 2, (y0 + y1) / 2), center, scale, rotate=rotate, gain=gain,
                       tint=tint if tint is not None else SKY,
                       tint_amount=tint_amount if tint_amount is not None else 0.42 * depth)
    half_w, half_h = (x1 - x0) * scale / 2, (y1 - y0) * scale / 2
    return C.over(img, layer), (center[0] - half_w, center[1] - half_h, center[0] + half_w, center[1] + half_h)


def line_layer(img, segments, width, color=(20, 20, 18), alpha=0.85):
    mask = C.drawn(lambda dr: [dr.line([(a[0] * SS, a[1] * SS), (b[0] * SS, b[1] * SS)], fill=255,
                                       width=max(1, int(width * SS))) for a, b in segments], blur=0.6)
    layer = np.zeros_like(img)
    layer[..., :3] = np.array(color, np.float32) / 255 * mask[..., None] * alpha
    layer[..., 3] = mask * alpha
    return C.over(img, layer)


def hang(img, payload, cx, top, width, canopies=1, canopy="R3B_Canopy_Ger", canopy_w=None, gap=None, depth=0.0,
         gain=1.0, canopy_gain=1.0, payload_tall=False):
    """A payload under one canopy or a cluster of three: the canopy's own lines meet at its harness, short straps run
    from there to the payload's top corners."""
    canopy_w = canopy_w or width * 0.95
    gap = gap if gap is not None else canopy_w * 0.18
    # the payload first so the straps lie over it
    cut, _ = C.load_cutout(payload)
    x0, y0, x1, y1 = C.bbox(cut)
    height = ((y1 - y0) / (x1 - x0)) * width if not payload_tall else width
    size = width if not payload_tall else width
    center = (cx, top + height / 2)
    layer_img, box = put(img, payload, center, size, tall=payload_tall, gain=gain, depth=depth)
    canopy_cut, _ = C.load_cutout(canopy)
    cx0, cy0, cx1, cy1 = C.bbox(canopy_cut)
    canopy_h = canopy_w * (cy1 - cy0) / (cx1 - cx0)
    if canopies == 1:
        spots = [(cx, 0.0)]
    else:
        spots = [(cx - canopy_w * 0.62, canopy_h * 0.12), (cx + canopy_w * 0.62, canopy_h * 0.12), (cx, -canopy_h * 0.1)]
    straps = []
    out = img
    for sx, lift in spots:
        harness = (sx, top - gap + lift * 0.0)
        out, _ = put(out, canopy, (sx, harness[1] - canopy_h / 2 + lift), canopy_w, gain=canopy_gain, depth=depth)
        bottom = harness[1] + lift
        straps += [((sx, bottom), (box[0] + (box[2] - box[0]) * 0.22, box[1] + 2)),
                   ((sx, bottom), (box[2] - (box[2] - box[0]) * 0.22, box[1] + 2))]
    out = line_layer(out, straps, 1.1 if depth < 0.5 else 0.8, alpha=0.75 - 0.3 * depth)
    # payload over the straps' ends
    out, _ = put(out, payload, center, size, tall=payload_tall, gain=gain, depth=depth)
    return out, box


def transport(img, at=(392, 58), scale=0.24, depth=0.55):
    cut, pts = C.load_cutout("Ju390_Level")
    img, _, _ = C.plane(img, cut, pts, at, scale, depth, SKY, gain=1.2, prop=[(r, 85.0) for r in C.ROTORS])
    return img


def plane_by_width(img, name, center, width, depth, gain=1.2, guns=(), prop=None):
    """An aircraft placed by its visible width; returns the image, the muzzle points and the forward direction."""
    cut, pts = C.load_cutout(name)
    x0, y0, x1, y1 = C.bbox(cut)
    scale = width / (x1 - x0)
    layer, to_canvas = C.place(cut, pts, ((x0 + x1) / 2, (y0 + y1) / 2), center, scale, gain=gain, tint=SKY,
                               tint_amount=0.42 * depth)
    img = C.over(img, layer)
    if prop:
        o = to_canvas(pts["origin"])
        r_axis = (to_canvas(pts["right"]) - o) / 300.0
        u_axis = (to_canvas(pts["up"]) - o) / 300.0
        for bone, radius in prop:
            if bone in pts:
                img = C.prop_disc(img, to_canvas(pts[bone]), r_axis, u_axis, radius * scale / 0.262 * 0.262)
    muzzles = [to_canvas(pts[g]) for g in guns if g in pts]
    return img, muzzles, C.forward_on_screen(pts)


# --- drawn effects -------------------------------------------------------------------------------------------------

def flames(img, x, y, width, height, seed, strength=1.0):
    """Ground fire: separate licking tongues, dark red at the tips and hot at the root, over a scorched patch with a
    low glow. No smoke (house rule)."""
    rng = np.random.default_rng(seed)
    scorch = C.drawn(lambda dr: dr.ellipse([(x - width * 0.62) * SS, (y - width * 0.07) * SS, (x + width * 0.62) * SS,
                                            (y + width * 0.11) * SS], fill=255), blur=4 * SS)
    layer = np.zeros_like(img)
    layer[..., :3] = 0.04 * scorch[..., None]
    layer[..., 3] = scorch * 0.7
    img = C.over(img, layer)
    img = C.light_pool(img, (x, y + 1), width * 0.7, width * 0.12, (255, 100, 25), 0.55 * strength, 8)
    count = max(4, int(width / 16))
    tongues = []
    for i in range(count):
        t = (i + 0.5) / count
        bx = x - width / 2 + width * t + rng.uniform(-3, 3)
        h = height * (0.35 + 0.65 * math.sin(math.pi * t) ** 0.8) * rng.uniform(0.6, 1.15)
        w = width / count * rng.uniform(1.3, 1.9)
        tongues.append((bx, h, w, rng.uniform(-0.3, 0.3), rng.uniform(0, math.tau)))

    def shape(k_w, k_h):
        def fn(draw):
            for bx, h, w, lean, phase in tongues:
                left, right = [], []
                for j in range(13):
                    t = j / 12
                    half = w * k_w / 2 * (1 - t) ** 0.85
                    cx = bx + lean * h * k_h * t ** 1.6 + math.sin(t * 5.5 + phase) * w * 0.18 * t
                    cy = y - h * k_h * t
                    left.append(((cx - half) * SS, cy * SS))
                    right.append(((cx + half) * SS, cy * SS))
                draw.polygon(left + right[::-1], fill=255)
        return fn

    img = C.add(img, C.light_layer(C.drawn(shape(1.0, 1.0), blur=1.6 * SS), (200, 45, 8)), 0.85 * strength)
    img = C.add(img, C.light_layer(C.drawn(shape(0.72, 0.7), blur=1.2 * SS), (255, 120, 20)), 0.8 * strength)
    img = C.add(img, C.light_layer(C.drawn(shape(0.42, 0.38), blur=1.0 * SS), (255, 220, 130)), 0.8 * strength)
    return img


def tracer_heavy(img, muzzles, direction, length, flash, width, dashes=3):
    angle = math.degrees(math.atan2(direction[1], direction[0]))
    for j, m in enumerate(muzzles):
        img = C.tracer(img, m, m + direction * length, width, phase=j * 0.37, dashes=dashes)
        img = C.star(img, m, flash, angle)
    return img


def ground_dark(img, box, strength=0.5):
    """Contact shadow for something standing on the ground; box in final pixels."""
    x0, y0, x1, y1 = box
    cx, w = (x0 + x1) / 2, (x1 - x0)
    mask = C.drawn(lambda dr: dr.ellipse([(cx - w * 0.5) * SS, (y1 - w * 0.06) * SS, (cx + w * 0.5) * SS,
                                          (y1 + w * 0.08) * SS], fill=255), blur=6 * SS)
    layer = np.zeros_like(img)
    layer[..., 3] = mask * strength
    return C.over(img, layer)


# --- the icons -----------------------------------------------------------------------------------------------------

def gun_drop(gun, seed, guns):
    """Guns coming down under cargo canopies, the transport passing over."""
    img = C.background("ger", seed)
    img = transport(img)
    for name, cx, top, width, canopies, depth in guns:
        img, _ = hang(img, gun, cx, top, width, canopies=canopies, canopy_w=width * (0.62 if canopies == 3 else 0.9),
                      depth=depth, gain=1.6)
    return C.finish(img)


def flak88_drop():
    img = C.background("ger", 151)
    img = transport(img, at=(400, 54), scale=0.22)
    # the crew jumping after the gun
    img = C.prop_item(img, "Para_Ger_B", (424, 196), 70, 0.0, tint=SKY, tint_amount=0.3, tall=True)
    img = C.prop_item(img, "Para_Ger_A", (470, 236), 60, 0.0, tint=SKY, tint_amount=0.35, tall=True)
    img, _ = hang(img, "R3B_Flak88_Gun", 222, 232, 290, canopies=1, canopy_w=168, gap=4, gain=2.0)
    return C.finish(img)


def sfh18_drop():
    img = C.background("ger", 157)
    img = transport(img, at=(410, 52), scale=0.2)
    img, _ = hang(img, "R3B_SFH18_Gun", 384, 186, 150, canopies=3, canopy_w=74, gap=10, depth=0.55, gain=2.0)
    img, _ = hang(img, "R3B_SFH18_Gun", 196, 262, 290, canopies=3, canopy_w=130, gap=18, gain=2.0)
    return C.finish(img)


def pit_drop(name, seed, spots, gain=1.4):
    img = C.background("ger", seed)
    img = transport(img, at=(404, 52), scale=0.2)
    for cx, top, width, canopies, canopy_w, depth in spots:
        img, _ = hang(img, name, cx, top, width, canopies=canopies, canopy_w=canopy_w, gap=4 if canopies == 1 else 12,
                      depth=depth, gain=gain)
    return C.finish(img)


def field_workshop_drop():
    img = C.background("ger", 163)
    img = transport(img, at=(410, 50), scale=0.2)
    img, _ = hang(img, "R3B_Pak40_Gun", 430, 210, 110, canopies=1, canopy_w=78, gap=8, depth=0.6, gain=2.0)
    img, _ = hang(img, "R3B_Pak40_Gun", 112, 170, 96, canopies=1, canopy_w=70, gap=8, depth=0.75, gain=2.0)
    img, _ = hang(img, "R3B_FieldWorkshop_Hang", 262, 246, 250, canopies=3, canopy_w=118, gap=16, gain=1.0)
    return C.finish(img)


def luftlande_sturm():
    """Ju 87s strafing ahead of the landing while the Vultures come down behind them."""
    img = C.background("ger", 167)
    img = C.prop_item(img, "Para_Ger_B", (408, 214), 104, 0.0, tint=SKY, tint_amount=0.35, tall=True)
    img = C.prop_item(img, "Para_Ger_A", (468, 286), 90, 0.0, tint=SKY, tint_amount=0.45, tall=True)
    img = C.prop_item(img, "Para_Ger_A", (330, 262), 130, 0.0, tall=True)
    cut, pts = C.load_cutout("Ju87_Dive")
    direction = C.forward_on_screen(pts)
    for at, scale, depth in [((86, 268), 0.12, 0.8), ((176, 150), 0.22, 0.0)]:
        lead = depth == 0
        img, muzzles, _ = C.plane(img, cut, pts, at, scale, depth, SKY, gain=1.45, prop=[("B_Rotor1", 135.0)],
                                  guns=("W1", "W2"))
        img = C.strafe_run(img, muzzles, direction, (200 if lead else 120) * SS, (15 if lead else 9) * SS,
                           (3.2 if lead else 2.0) * SS, 4 if lead else 3)
    return C.finish(img)


def supply_drop(payload, seed, payload_gain, landed_extra=None, width=118):
    """A transport's strip of supply bundles on their small chutes, the first one landed."""
    img = C.background("ger", seed)
    img = transport(img, at=(392, 56), scale=0.22)
    if landed_extra:
        img = landed_extra(img)
    img, box = put(img, payload, (122, 376), width * 1.05, gain=payload_gain)
    img = ground_dark(img, box, 0.35)
    img, box = put(img, payload, (122, 376), width * 1.05, gain=payload_gain)
    img, _ = hang(img, payload, 404, 236, width * 0.82, canopy="R3B_FlareChute", canopy_w=width * 0.92, gap=14,
                  depth=0.45, gain=payload_gain)
    img, _ = hang(img, payload, 268, 196, width, canopy="R3B_FlareChute", canopy_w=width * 1.05, gap=16,
                  gain=payload_gain)
    return C.finish(img)


def radixite_glint(img, at, size):
    return C.light_pool(C.light_pool(img, at, size, size * 0.6, (255, 170, 50), 0.6, 8), at, size * 0.35, size * 0.25,
                        (255, 230, 160), 0.6, 3)


def radixite_metal_supply():
    def extra(img):
        img = C.light_pool(img, (212, 384), 40, 10, (255, 160, 50), 0.35, 6)
        img, _ = put(img, "R3B_RadixiteCore", (212, 366), 60, gain=1.2)
        img, _ = put(img, "R3B_RadixiteCore", (240, 376), 40, gain=1.2)
        return img
    return supply_drop("R3B_Crate_Big", 171, 1.7, extra, width=134)


def radixite_cache():
    img = C.background("ger", 173)
    img = transport(img, at=(400, 56), scale=0.22)
    img = C.light_pool(img, (170, GROUND_Y - 6), 120, 26, (255, 150, 40), 0.4, 14)
    img, box = put(img, "R3B_RadixiteDeposit", (164, 318), 150, tall=True, gain=1.05)
    img, _ = put(img, "R3B_RadixiteCore", (226, 372), 70, gain=1.1)
    img, _ = hang(img, "R3B_RadixiteCore", 342, 196, 112, canopy="R3B_FlareChute", canopy_w=126, gap=14, gain=1.25)
    return C.finish(img)


def sonnengewehr():
    """The sun beam from the sky sweeping a burnt line across the field towards a T-34 standing in its path."""
    img = C.background("ger", 181)
    hit = np.array([322.0, 384.0])
    # the burnt track the sweep leaves behind it, still glowing in its middle
    start = np.array([520.0, 420.0])
    scorch = C.drawn(lambda dr: C.taper(dr, start * SS, hit * SS, 12 * SS, 30 * SS), blur=3 * SS)
    layer = np.zeros_like(img)
    layer[..., :3] = np.array([0.05, 0.03, 0.02]) * scorch[..., None] * 0.85
    layer[..., 3] = scorch * 0.85
    img = C.over(img, layer)
    img = C.add(img, C.light_layer(C.drawn(lambda dr: C.taper(dr, start * SS, hit * SS, 2 * SS, 9 * SS), blur=2 * SS),
                                   (255, 110, 25)), 0.75)
    img = C.prop_item(img, "T3485_Ground", (170, 352), 190, 0.0, gain=1.6, tint=(92, 92, 80), tint_amount=0.15)
    # the beam: a yellow glow, a bright column and a white core, from above the card to the ground
    top = np.array([346.0, -20.0])

    def column(w0, w1):
        return lambda dr: C.taper(dr, top * SS, hit * SS, w0 * SS, w1 * SS)

    img = C.add(img, C.light_layer(C.drawn(column(64, 84), blur=18 * SS), (255, 200, 50)), 0.45)
    img = C.add(img, C.light_layer(C.drawn(column(30, 40), blur=5 * SS), (255, 220, 80)), 0.75)
    img = C.add(img, C.light_layer(C.drawn(column(10, 14), blur=1.5 * SS), (255, 250, 215)), 1.0)
    img = C.light_pool(img, tuple(hit), 92, 22, (255, 190, 60), 0.6, 12)
    img = C.light_pool(img, tuple(hit), 34, 9, (255, 250, 220), 0.9, 3)
    img = C.impact(img, tuple(hit * SS), 46 * SS, 181)
    return C.finish(img)


def incendiary_ju87():
    """A Ju 87 releasing its incendiary canisters over a field already burning."""
    img = C.background("ger", 191)
    for x, w, h in ((96, 92, 70), (214, 110, 92), (338, 96, 78), (446, 80, 62)):
        img = flames(img, x, GROUND_Y + 4, w, h, x)
    for x, y, ang in ((258, 228, 70), (300, 270, 74), (226, 288, 72)):
        img = C.fall_lines(img, (x * SS, y * SS), ang, 46 * SS, 6 * SS, 0.3)
        img = C.prop_item(img, "Bomb_250", (x, y), 44, ang)
    cut, pts = C.load_cutout("Ju87_Dive")
    img, _, _ = C.plane(img, cut, pts, (196, 140), 0.21, 0.0, SKY, gain=1.45, prop=[("B_Rotor1", 135.0)])
    return C.finish(img)


def fire_barrage():
    """Heavy shells plunging into the target, everything they hit set burning."""
    img = C.background("ger", 193)
    for x, w, h in ((120, 104, 84), (290, 128, 104), (420, 92, 72)):
        img = flames(img, x, GROUND_Y + 4, w, h, x)
    fall = 104
    for x, y, length in [(160, 92, 60), (392, 110, 60), (256, 214, 96), (350, 252, 86)]:
        ang = math.radians(fall + 180)
        back = np.array([math.cos(ang), math.sin(ang)])
        tail = np.array([x, y]) * SS + back * length * 0.5 * SS
        streak = C.drawn(lambda dr: C.taper(dr, tail, tail + back * length * SS, length * 0.2 * SS, 1.0),
                         blur=length * 0.05 * SS)
        img = C.add(img, C.light_layer(streak, (255, 90, 20)), 0.7)
        img = C.prop_item(img, "Shell_AP", (x, y), length, fall + 90, gain=0.9, tall=True)
    return C.finish(img)


def fighter_strafe(name, guns, formation, flash, width, length, seed, gain=1.3):
    """Fighters in open sky, every gun firing straight along the nose (the strafing icon rule)."""
    img = C.background("ger", seed, ground=False)
    for center, plane_w, depth in formation:
        lead = depth == 0
        img, muzzles, direction = plane_by_width(img, name, center, plane_w, depth, gain=gain, guns=guns,
                                                 prop=[("B_Rotor1", 120.0), ("B_Rotor2", 120.0)])
        img = tracer_heavy(img, muzzles, direction, (length if lead else length * 0.6) * SS,
                           (flash if lead else flash * 0.6) * SS, (width if lead else width * 0.6) * SS,
                           4 if lead else 3)
    return C.finish(img)


def ju87g_tank_busters():
    """Two Ju 87 G with their 3.7 cm pods: fewer, much heavier rounds than the wing MGs."""
    img = C.background("ger", 199, ground=False)
    cut, pts = C.load_cutout("R3B_Ju87G_Dive")
    direction = C.forward_on_screen(pts)
    for at, scale, depth in [((356, 80), 0.14, 0.8), ((214, 214), 0.262, 0.0)]:
        lead = depth == 0
        img, muzzles, _ = C.plane(img, cut, pts, at, scale, depth, SKY, gain=1.45, prop=[("B_Rotor1", 135.0)],
                                  guns=("BK37_Muzzle_L", "BK37_Muzzle_R"))
        img = tracer_heavy(img, muzzles, direction, (250 if lead else 150) * SS, (28 if lead else 15) * SS,
                           (6.0 if lead else 3.4) * SS, 3)
    return C.finish(img)


def massive_flare():
    """One huge flare over the field lighting a wide circle: the curtain's single, bigger brother."""
    img = C.background("ger_dusk", 211)
    img = C.light_pool(img, (256, GROUND_Y), 236, 58, (255, 190, 110), 0.38, 26)
    img = C.light_pool(img, (256, GROUND_Y), 120, 28, (255, 220, 160), 0.22, 14)
    ring = C.drawn(lambda dr: dr.ellipse([20 * SS, (GROUND_Y - 58) * SS, 492 * SS, (GROUND_Y + 58) * SS], outline=255,
                                         width=int(4 * SS)), blur=1.5)
    img = C.add(img, C.light_layer(ring, (255, 220, 160)), 0.6)
    img = transport(img, at=(400, 54), scale=0.2, depth=0.6)
    x, y, height = 248, 196, 210
    burn = (x, y + height * 0.42)
    img = C.light_pool(img, burn, 54, 54, (255, 170, 70), 0.22, 22)
    img = C.prop_item(img, "Flare_Hanging", (x, y), height, 0.0, gain=1.15, tall=True)
    img = C.light_pool(img, burn, 20, 20, (255, 180, 80), 0.7, 7)
    img = C.light_pool(img, burn, 9, 9, (255, 252, 235), 1.0, 2)
    return C.finish(img)


def large_barrage():
    """The barrage, wider and denser: more rounds in the air and a row of impacts across the whole field."""
    img = C.background("ger", 223)
    for x, radius in ((70, 22), (160, 30), (262, 40), (362, 30), (452, 24)):
        img = C.blast(img, (x * SS, C.GROUND_Y * SS), radius * SS, x + 7)
    fall = 104
    rounds = [(96, 70, 48), (206, 58, 50), (318, 76, 50), (430, 64, 48), (150, 172, 72), (270, 188, 78),
              (390, 178, 72), (210, 286, 92), (336, 302, 96)]
    for x, y, length in rounds:
        ang = math.radians(fall + 180)
        back = np.array([math.cos(ang), math.sin(ang)])
        tail = np.array([x, y]) * SS + back * length * 0.5 * SS
        img = C.add(img, C.light_layer(C.drawn(lambda dr: C.taper(dr, tail, tail + back * length * SS,
                                                                    length * 0.2 * SS, 1.0), blur=length * 0.05 * SS),
                                       (255, 100, 30)), 0.55)
        img = C.prop_item(img, "Shell_AP", (x, y), length, fall + 90, gain=0.9, tall=True)
    return C.finish(img)


def rail_bombardment():
    """A few rail slugs falling almost straight down from very high, each hit a hard narrow spike, no wide blast."""
    img = C.background("ger", 229)
    glow = (255, 120, 220)
    for x, y, length in [(146, 196, 92), (270, 268, 112), (392, 150, 84)]:
        top = np.array([x + 46.0, -40.0])
        here = np.array([x, y], float)
        heading = math.degrees(math.atan2(here[1] - top[1], here[0] - top[0]))
        back = (top - here) / np.linalg.norm(top - here)
        tail = here + back * length * 0.5
        img = C.add(img, C.light_layer(C.drawn(lambda dr: C.taper(dr, top * SS, tail * SS, 2 * SS, 12 * SS),
                                               blur=4 * SS), glow), 0.45)
        img = C.add(img, C.light_layer(C.drawn(lambda dr: C.taper(dr, top * SS, tail * SS, 0.6 * SS, 3 * SS),
                                               blur=1 * SS), (255, 235, 250)), 0.55)
        img = C.prop_item(img, "R3B_RailSlug", tuple(here), length, heading, gain=1.3)
    for x, size in ((124, 30), (240, 40), (366, 28)):
        img = C.light_pool(img, (x, GROUND_Y), size * 0.9, size * 0.2, glow, 0.45, 5)
        img = C.impact(img, (x * SS, GROUND_Y * SS), size * 1.3 * SS, x)
    return C.finish(img)


ICONS = {
    "GerFlak88Drop": flak88_drop,
    "GerSFH18Drop": sfh18_drop,
    "GerPanzerschreckPitDrop": lambda: pit_drop("R3B_Pit_Panzerschreck", 159,
                                                [(380, 196, 140, 1, 118, 0.45), (196, 240, 220, 1, 160, 0.0)]),
    "GerPakPitDrop": lambda: pit_drop("R3B_Pit_Pak40", 161, [(250, 236, 310, 1, 170, 0.0)]),
    "GerFieldBunkerDrop": lambda: pit_drop("R3B_Bunker_Flak88_Hang", 162, [(254, 254, 300, 3, 140, 0.0)], gain=1.6),
    "GerFieldWorkshopDrop": field_workshop_drop,
    "GerLuftlandeSturm": luftlande_sturm,
    "GerRadixiteMetalSupplyDrop": radixite_metal_supply,
    "GerConstructionSupplyDrop": lambda: supply_drop("R3B_MetalDump", 177, 1.5, width=146),
    "GerRadixiteCacheDrop": radixite_cache,
    "GerVehiclePartsSupplyDrop": lambda: supply_drop("R3B_VehicleParts", 179, 1.8, width=176),
    "GerSonnengewehr": sonnengewehr,
    "GerIncendiaryJu87": incendiary_ju87,
    "GerFireBarrage": fire_barrage,
    "Bf109Strafing": lambda: fighter_strafe("R3B_Bf109_Dive", ("W1", "W2", "W3", "W4"), [((236, 214), 330, 0.0)],
                                            15, 3.0, 250, 197),
    "2xBf110Strafing": lambda: fighter_strafe("R3B_Bf109_Dive", ("W1", "W2", "W3", "W4"),
                                              [((372, 84), 170, 0.8), ((214, 230), 320, 0.0)], 15, 3.0, 240, 201),
    "GerJu87GTankBusters": ju87g_tank_busters,
    "GerMassiveFlare": massive_flare,
    "Ju87Strafing": lambda: C.ju87_strafe(1),
    "Me410Airstrike": lambda: fighter_strafe("R3B_Me410_Dive", ("W1", "W4"),
                                             [((236, 214), 360, 0.0)], 15, 3.4, 250, 205),
    "LargeBarrage": large_barrage,
    "GerRailBombardment": rail_bombardment,
}

# The template each icon goes on (DA key as in EGlobalAbility).
TEMPLATES = {
    "GA_GerFlak88Drop": "GerFlak88Drop", "GA_GerSFH18Drop": "GerSFH18Drop",
    "GA_GerPanzerschreckPitDrop": "GerPanzerschreckPitDrop", "GA_GerPakPitDrop": "GerPakPitDrop",
    "GA_GerFieldBunkerDrop": "GerFieldBunkerDrop", "GA_GerFieldWorkshopDrop": "GerFieldWorkshopDrop",
    "GA_GerLuftlandeSturm": "GerLuftlandeSturm", "GA_GerRadixiteMetalSupplyDrop": "GerRadixiteMetalSupplyDrop",
    "GA_GerConstructionSupplyDrop": "GerConstructionSupplyDrop", "GA_GerRadixiteCacheDrop": "GerRadixiteCacheDrop",
    "GA_GerVehiclePartsSupplyDrop": "GerVehiclePartsSupplyDrop", "GA_GerSonnengewehr": "GerSonnengewehr",
    "GA_GerIncendiaryJu87": "GerIncendiaryJu87", "GA_GerFireBarrage": "GerFireBarrage",
    "GA_Bf109Strafing": "Bf109Strafing", "GA_2xBf110Strafing": "2xBf110Strafing",
    "GA_GerJu87GTankBusters": "GerJu87GTankBusters", "GA_GerMassiveFlare": "GerMassiveFlare",
    "GA_Ju87Strafing": "Ju87Strafing", "GA_Me410Airstrike": "Me410Airstrike", "GA_LargeBarrage": "LargeBarrage",
    "GA_GerRailBombardment": "GerRailBombardment",
}


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    names = sys.argv[1:] or list(ICONS)
    made = []
    for name in names:
        icon = ICONS[name]()
        icon.save(OUT + "T_GA_%s.png" % name)
        made.append((name, icon))
        print("ICON", name)
    columns = min(len(made), 4)
    rows = (len(made) + columns - 1) // columns
    sheet = Image.new("RGBA", (columns * 330 + 10, rows * 450 + 10), (28, 28, 30, 255))
    draw = ImageDraw.Draw(sheet)
    for i, (name, icon) in enumerate(made):
        x, y = 10 + (i % columns) * 330, 10 + (i // columns) * 450
        draw.text((x, y), name, fill=(220, 220, 220, 255))
        sheet.alpha_composite(icon.resize((320, 320), Image.LANCZOS), (x, y + 14))
        sheet.alpha_composite(icon.resize((96, 96), Image.LANCZOS), (x, y + 340))
        sheet.alpha_composite(icon.resize((64, 64), Image.LANCZOS), (x + 104, y + 340))
        sheet.alpha_composite(icon.resize((48, 48), Image.LANCZOS), (x + 176, y + 340))
    sheet.save(WORK + "preview_b.png")


if __name__ == "__main__":
    main()
