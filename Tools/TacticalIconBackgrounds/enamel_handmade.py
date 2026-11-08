"""Six hand-made takes on the "enamel plate + worn icon" badge, built on the critique of icon_backgrounds.enamel_chipped.

What changed against the first enamel plate (each point fixes one tell that made it read as procedural):
  - the steel outline stays whole (at most a dent); only paint is lost
  - the CLEAN icon is painted into the plate's paint layer and one damage map cuts plate paint and icon alike, so
    both share one history (the speckled Set08_Worn icons are not used)
  - chips are angular cells (jittered-grid Voronoi), few and large, placed where plates get hit: corners, the lower
    edge, one impact point
  - a chip goes paint -> red-oxide primer ring -> bright steel -> dark pitting
  - muted period enamel colours, the cyan accent repainted as a flat pale blue-green
  - one height field and one top-left light for plate, paint ridges and icon; no stacked drop shadows
  - tight corners, icon slightly rotated and off-centre at ~2/3 of the plate, stencil edges wobble

Usage:
  python enamel_handmade.py      -> Saved/TacticalIconBackgrounds/handmade/ (badges, portraits, sheet)
"""
import math
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import icon_backgrounds as ib  # noqa: E402
from icon_backgrounds import (chamfered_rect, coverage, cropped_icon, fbm, metal_surface, ramp,  # noqa: E402
                              rounded_rect, signed_distance)

S = ib.INTERNAL
F32 = np.float32
OUT = ib.OUT / "handmade"

PRIMER = np.array([98, 52, 38], F32)        # red-oxide primer, RAL 8012-ish
STEEL = np.array([164, 162, 154], F32)
PIT = np.array([84, 58, 42], F32)            # rust-brown pitting at the bottom of a chip
CREAM = np.array([216, 204, 172], F32)
STENCIL_BLACK = np.array([30, 29, 27], F32)
PAINTED_TEAL = np.array([116, 152, 142], F32)
GROUND_COAT = np.array([214, 210, 198], F32)  # white ground enamel under the colour coat of a vitreous sign
SIGN_STEEL = np.array([34, 36, 42], F32)
RUST = np.array([118, 64, 32], F32)

# Muted period-enamel versions of the role / class colours (amber = rapid fire, olive = light tank class).
ROLE_COLOUR = (168, 122, 58)
CLASS_COLOUR = (84, 92, 56)

YS, XS = np.mgrid[0:S, 0:S].astype(F32)


# ---------------------------------------------------------------------------------------------------------- utilities
def blur(image, sigma):
    return cv2.GaussianBlur(image.astype(F32), (0, 0), sigma)


def mix(rgb, colour, mask):
    colour = np.asarray(colour, F32)
    return rgb * (1.0 - mask[..., None]) + colour * mask[..., None]


def flat(colour, seed, variation=0.03):
    """Enamel is smooth: only a few percent of tone change."""
    tone = 1.0 + variation * (2.0 * fbm(S, seed + 11, base=4, octaves=3) - 1.0)
    return np.asarray(colour, F32) * tone[..., None]


def lit(rgb, height, strength=0.45, sigma=1.6, gain=5.0):
    """One light from the top left over one height field, shared by plate, paint layers and icon."""
    smooth = blur(height, sigma)
    grad_y, grad_x = np.gradient(smooth)
    light = np.clip((grad_x + grad_y) * gain, -1.0, 1.0)
    return rgb * (1.0 + strength * light)[..., None]


# ------------------------------------------------------------------------------------------------------------- plate
def plate(seed, margin=0.045, radius=0.04, chamfer=None, dents=1):
    """Whole steel outline (no torn silhouette), optionally one or two smooth dents; returns alpha, distance, bevel."""
    mask = chamfered_rect(S, margin * S, chamfer * S) if chamfer else rounded_rect(S, margin * S, radius * S)
    distance = signed_distance(mask)
    rng = np.random.default_rng(seed + 21)
    for _ in range(dents):
        side = rng.integers(4)
        along = rng.uniform(0.25, 0.75) * S
        edge = margin * S
        x, y = [(along, edge), (S - edge, along), (along, S - edge), (edge, along)][side]
        distance -= 0.018 * S * np.exp(-((XS - x) ** 2 + (YS - y) ** 2) / (0.045 * S) ** 2)
    return coverage(distance), distance, ramp(distance, 0.0, 0.03 * S)


# ------------------------------------------------------------------------------------------------------------- icon
def icon_masks(icon_path, seed, box=(0.13, 0.13, 0.87, 0.87), fill=0.7, rotate=0.0, offset=None, wobble=0.004):
    """Clean icon split into paintable masks (alpha, fill, cyan stripe, black outline), halo stripped, placed slightly
    off-centre, edges wobbled like a cut stencil. Icons stay upright (user's call): pass rotate=None for a random
    1-3 degree tilt."""
    rng = np.random.default_rng(seed + 31)
    icon = np.asarray(cropped_icon(icon_path), F32)
    alpha = ramp(icon[..., 3], 110, 170)
    rgb = icon[..., :3]
    cyan = ramp(rgb[..., 2] - rgb[..., 0], 30, 80) * alpha
    fill_mask = ramp(rgb.mean(axis=2), 70, 130) * alpha * (1.0 - cyan)
    outline = np.clip(alpha - fill_mask - cyan, 0.0, 1.0)
    stack = np.dstack([alpha, fill_mask, cyan, outline]).astype(F32)
    height, width = alpha.shape
    x0, y0, x1, y1 = (v * S for v in box)
    scale = fill * min((x1 - x0) / width, (y1 - y0) / height)
    rotate = rng.uniform(1.0, 3.0) * rng.choice([-1, 1]) if rotate is None else rotate
    offset = rng.uniform(-0.03, 0.03, 2) if offset is None else offset
    matrix = cv2.getRotationMatrix2D((width / 2.0, height / 2.0), rotate, scale)
    matrix[0, 2] += (x0 + x1) / 2.0 + offset[0] * S - width / 2.0
    matrix[1, 2] += (y0 + y1) / 2.0 + offset[1] * S - height / 2.0
    placed = cv2.warpAffine(stack, matrix, (S, S), flags=cv2.INTER_LINEAR, borderValue=0)
    if wobble:
        dx = (fbm(S, seed + 32, base=7, octaves=2) - 0.5) * 2.0 * wobble * S
        dy = (fbm(S, seed + 33, base=7, octaves=2) - 0.5) * 2.0 * wobble * S
        placed = cv2.remap(placed, XS + dx, YS + dy, cv2.INTER_LINEAR)
    return {name: placed[..., i] for i, name in enumerate(("alpha", "fill", "cyan", "outline"))}


def paint_icon(rgb, masks, fill_colour, outline_colour, stripe_colour):
    rgb = mix(rgb, outline_colour, masks["outline"])
    rgb = mix(rgb, fill_colour, masks["fill"])
    return mix(rgb, stripe_colour, masks["cyan"])


# ------------------------------------------------------------------------------------------------------------ damage
def cells(spacing, seed, warp=0.0):
    """Jittered-grid Voronoi: nearest-seed id, F1 and F2 per pixel, and the seed positions. Warping the lookup bends
    the straight cell walls a little so chips look shell-shaped rather than polygonal."""
    rng = np.random.default_rng(seed + 41)
    n = int(math.ceil(S / spacing)) + 4
    grid_x, grid_y = np.meshgrid(np.arange(n), np.arange(n))
    jitter = rng.uniform(-0.45, 0.45, (n, n, 2))
    seeds = np.dstack([(grid_x - 2 + 0.5 + jitter[..., 0]) * spacing, (grid_y - 2 + 0.5 + jitter[..., 1]) * spacing])
    px, py = XS, YS
    if warp:
        px = XS + (fbm(S, seed + 42, base=6, octaves=3) - 0.5) * 2.0 * warp
        py = YS + (fbm(S, seed + 43, base=6, octaves=3) - 0.5) * 2.0 * warp
    cell_x = np.floor(px / spacing).astype(np.int32) + 2
    cell_y = np.floor(py / spacing).astype(np.int32) + 2
    first = np.full((S, S), np.inf, F32)
    second = np.full((S, S), np.inf, F32)
    nearest = np.zeros((S, S), np.int32)
    for oy in range(-2, 3):
        for ox in range(-2, 3):
            cy = np.clip(cell_y + oy, 0, n - 1)
            cx = np.clip(cell_x + ox, 0, n - 1)
            d = np.hypot(px - seeds[cy, cx, 0], py - seeds[cy, cx, 1])
            closer = d < first
            second = np.where(closer, first, np.minimum(second, d))
            nearest = np.where(closer, cy * n + cx, nearest)
            first = np.where(closer, d, first)
    return nearest, first, second, seeds.reshape(-1, 2)


def chips(seed, spots, spacing, chance=1.0, warp=None):
    """Union of whole Voronoi cells near the damage spots: one connected angular flake per spot with a ragged rim
    (a jittered threshold, not a per-cell coin toss, which scatters cells like shattered glass).
    spots: (x, y, radius) in fractions of the plate."""
    nearest, _, _, seeds = cells(spacing * S, seed, warp if warp is not None else spacing * S * 0.25)
    rng = np.random.default_rng(seed + 51)
    probability = np.zeros(len(seeds))
    for x, y, radius in spots:
        d = np.hypot(seeds[:, 0] - x * S, seeds[:, 1] - y * S)
        probability = np.maximum(probability, np.exp(-(d / (radius * S)) ** 2))
    chosen = probability * chance > rng.uniform(0.35, 0.7, len(seeds))
    return chosen[nearest].astype(F32)


def chip_layers(chip, seed, primer_width=0.014, pit_depth=0.02):
    """paint / primer ring / bare steel / pitting coverages for one chip mask (they sum to 1)."""
    chip_mask = chip > 0.5
    outside = cv2.distanceTransform((~chip_mask).astype(np.uint8), cv2.DIST_L2, 5)
    inside = cv2.distanceTransform(chip_mask.astype(np.uint8), cv2.DIST_L2, 5)
    width = primer_width * S * (0.35 + 1.0 * fbm(S, seed + 61, base=9, octaves=2))
    paint = coverage(outside - width)
    steel = coverage(inside - 0.5)
    pit = coverage(inside - pit_depth * S * (0.6 + 0.8 * fbm(S, seed + 62, base=12, octaves=2)))
    pit *= ramp(fbm(S, seed + 63, base=26, octaves=2), 0.62, 0.78)
    primer = np.clip(1.0 - paint - steel, 0.0, 1.0)
    return paint, primer, np.clip(steel - pit, 0.0, 1.0), pit


def apply_chips(top_rgb, layers, seed, primer=PRIMER, steel=STEEL, pit=PIT):
    paint, primer_cov, steel_cov, pit_cov = layers
    steel_rgb = steel * (0.85 + 0.25 * fbm(S, seed + 71, base=20, octaves=3, aspect=(3, 0.3)))[..., None]
    rgb = top_rgb * paint[..., None] + primer * primer_cov[..., None] + steel_rgb * steel_cov[..., None]
    rgb = rgb + pit * pit_cov[..., None]
    height = 1.0 * paint + 0.6 * primer_cov + 0.25 * steel_cov
    return rgb, height


def corner_spots(rng, count, radius=(0.06, 0.1)):
    """Damage spots on the plate's corners, the lower ones more likely."""
    corners = [(0.07, 0.07, 0.6), (0.93, 0.07, 0.6), (0.07, 0.93, 1.0), (0.93, 0.93, 1.0)]
    weights = np.array([c[2] for c in corners])
    picks = rng.choice(4, size=count, replace=False, p=weights / weights.sum())
    return [(corners[i][0], corners[i][1], rng.uniform(*radius)) for i in picks]


def finish(rgb, alpha):
    return Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.clip(alpha, 0, 1) * 255]).astype(np.uint8), "RGBA")


# ----------------------------------------------------------------------------------------------------------- variants
def instrument_placard(icon_path, colour, seed):
    """1. Dark enamel instrument placard in a gunmetal bezel with two slotted screws, like the gauge kit. Cream
    glyph; the role colour is a painted band plus the glyph's accent stripe. Wear only around the screws."""
    rng = np.random.default_rng(seed)
    alpha, distance, bevel = plate(seed, chamfer=0.06, dents=0)
    bezel = 0.075 * S
    face = coverage(distance - bezel)
    groove = np.exp(-((distance - bezel) / (0.006 * S)) ** 2)
    rgb = metal_surface(seed) * 1.15
    face_rgb = flat((40, 41, 40), seed)
    band = coverage(np.minimum(YS - 0.72 * S, 0.84 * S - YS) + 0.5) * face
    face_rgb = mix(face_rgb, flat(colour, seed + 1), band)
    # Straight and centred in the face above the colour band (user's call for the placard).
    masks = icon_masks(icon_path, seed, box=(0.16, 0.12, 0.84, 0.7), fill=0.82, rotate=0.0, offset=(0.0, 0.0))
    face_rgb = paint_icon(face_rgb, masks, CREAM, (40, 41, 40), colour)
    screws = [(0.075, 0.5), (0.925, 0.5)]
    chip = chips(seed, [(x, y, 0.08) for x, y in screws] + [(rng.uniform(0.3, 0.7), 0.9, 0.06)], 0.035) * face
    layers = chip_layers(chip, seed, primer_width=0.008)
    painted, paint_height = apply_chips(face_rgb, layers, seed, primer=np.array([70, 66, 60], F32),
                                        steel=np.array([120, 120, 116], F32))
    rgb = rgb * (1.0 - face[..., None]) + painted * face[..., None]
    height = bevel * 1.2 + face * (paint_height - 1.2) + 0.25 * masks["alpha"] * layers[0] - 0.8 * groove
    for x, y in screws:
        d = np.hypot(XS - x * S, YS - y * S) / (0.032 * S)
        head = coverage((1.0 - d) * 0.032 * S)
        slot_angle = rng.uniform(0, math.pi)
        along = (XS - x * S) * math.sin(slot_angle) - (YS - y * S) * math.cos(slot_angle)
        slot = coverage(0.005 * S - np.abs(along)) * head
        rgb = mix(rgb, metal_surface(seed + 5) * 1.5, head)
        rgb = mix(rgb, PIT, slot * 0.9)
        height += np.sqrt(np.clip(1.0 - d ** 2, 0, 1)) * 1.5 - slot
    rgb = lit(rgb, height)
    glass = np.exp(-(((XS + YS) - 0.75 * S) / (0.09 * S)) ** 2) * face
    rgb = rgb + 24.0 * glass[..., None]
    return finish(rgb, alpha)


def field_stencil(icon_path, colour, seed):
    """2. Brushed-on role colour over red-oxide primer, glyph sprayed through a cut stencil in off-white:
    stencil bridges, overspray, drips, coverage fading toward one side."""
    rng = np.random.default_rng(seed)
    alpha, distance, bevel = plate(seed, radius=0.03)
    edge_thin = ramp(distance, 0.0, 0.05 * S) * (0.75 + 0.25 * fbm(S, seed + 2, base=10)) > 0.62
    top = mix(np.broadcast_to(PRIMER, (S, S, 3)).copy(), flat(colour, seed + 1), edge_thin.astype(F32))
    masks = icon_masks(icon_path, seed, fill=0.74, wobble=0.006)
    glyph = np.clip(masks["fill"] + masks["cyan"], 0, 1)
    ys, xs = np.where(glyph > 0.5)
    for _ in range(3):
        if rng.random() < 0.5:
            y = rng.uniform(ys.min(), ys.max())
            bridge = np.abs(YS - y - (XS - xs.mean()) * rng.uniform(-0.1, 0.1)) < 0.007 * S
        else:
            x = rng.uniform(xs.min(), xs.max())
            bridge = np.abs(XS - x - (YS - ys.mean()) * rng.uniform(-0.1, 0.1)) < 0.007 * S
        glyph = glyph * (1.0 - bridge)
    drips = np.zeros((S, S), np.uint8)
    for x in rng.uniform(xs.min() + 0.05 * S, xs.max() - 0.05 * S, int(rng.integers(2, 4))):
        column = np.where(glyph[:, int(x)] > 0.5)[0]
        if len(column):
            bottom, length = int(column.max()), int(rng.uniform(0.03, 0.09) * S)
            cv2.line(drips, (int(x), bottom), (int(x), bottom + length), 1, max(2, int(0.009 * S)))
            cv2.circle(drips, (int(x), bottom + length), max(2, int(0.008 * S)), 1, -1)
    glyph = np.maximum(glyph, blur(drips, 1.0))
    side = rng.choice([-1, 1])
    density = np.clip(0.62 + 0.38 * ramp(side * (XS - S / 2) / S, -0.5, 0.5) + 0.12 * (fbm(S, seed + 3) - 0.5), 0, 1)
    halo = blur(glyph, 0.035 * S)
    speckle = blur((rng.random((S, S)) < halo * 0.22).astype(F32), 0.8)
    spray = np.clip(glyph * density + blur(glyph, 0.01 * S) * 0.2 + speckle * 0.5, 0, 1)
    top = mix(top, CREAM, spray)
    chip = chips(seed, corner_spots(rng, 2) + [(rng.uniform(0.25, 0.75), 0.96, 0.08)], 0.06)
    rgb, height = apply_chips(top, chip_layers(chip, seed), seed)
    height += bevel + 0.15 * edge_thin + 0.1 * spray
    return finish(lit(rgb, height, strength=0.4), alpha)


def single_impact(icon_path, colour, seed):
    """3. Clean enamel with the icon painted flush; one hit took a big shell-shaped flake off a lower corner, through
    plate and icon, with hairline cracks running out of it. Nothing else is damaged."""
    rng = np.random.default_rng(seed)
    alpha, distance, bevel = plate(seed, radius=0.04, dents=1)
    masks = icon_masks(icon_path, seed)
    top = paint_icon(flat(colour, seed), masks, (204, 202, 194), STENCIL_BLACK, PAINTED_TEAL)
    corner_x = 0.21 if rng.random() < 0.5 else 0.79
    impact = (corner_x, 0.8)
    chip = np.maximum(chips(seed, [(impact[0], impact[1], 0.16)], 0.08, warp=0.02 * S),
                      chips(seed + 1, [(impact[0], impact[1], 0.19)], 0.03)) * alpha
    layers = chip_layers(chip, seed, primer_width=0.016)
    rgb, height = apply_chips(top, layers, seed)
    cracks = np.zeros((S, S), F32)
    centre = np.array(impact) * S
    for _ in range(int(rng.integers(4, 7))):
        heading = math.atan2(0.5 * S - centre[1], 0.5 * S - centre[0]) + rng.uniform(-1.2, 1.2)
        point = centre + np.array([math.cos(heading), math.sin(heading)]) * 0.1 * S
        points = [point.copy()]
        for _ in range(int(rng.integers(3, 7))):
            heading += rng.uniform(-0.4, 0.4)
            point = point + np.array([math.cos(heading), math.sin(heading)]) * rng.uniform(0.025, 0.05) * S
            points.append(point.copy())
        cv2.polylines(cracks, [np.int32(points)], False, 1.0, 2, cv2.LINE_AA)
    cracks *= layers[0]
    rgb = rgb * (1.0 - 0.45 * cracks[..., None])
    rgb = rgb + 30.0 * np.roll(np.roll(cracks, -1, 0), -1, 1)[..., None] * layers[0][..., None]
    height += bevel + 0.2 * masks["alpha"] * layers[0]
    return finish(lit(rgb, height), alpha)


def hand_brushed(icon_path, colour, seed):
    """4. Topcoat brushed on by hand: ridges in one direction catch the light, coverage thins at the edges over
    primer; glyph brushed in a second pass, a little off register."""
    rng = np.random.default_rng(seed)
    alpha, distance, bevel = plate(seed, radius=0.035)
    angle = math.radians(rng.uniform(-8, 8))
    rotation = cv2.getRotationMatrix2D((S / 2, S / 2), math.degrees(angle), 1.15)
    bristles = cv2.warpAffine(fbm(S, seed + 1, base=5, octaves=5, aspect=(7, 0.2)), rotation, (S, S),
                              borderMode=cv2.BORDER_REFLECT)
    reach = 0.012 * S + 0.06 * S * (1.0 - bristles) * fbm(S, seed + 2, base=6)
    coat = coverage(distance - reach)
    top = mix(np.broadcast_to(PRIMER, (S, S, 3)).copy(), flat(colour, seed) * (0.93 + 0.12 * bristles)[..., None],
              coat)
    masks = icon_masks(icon_path, seed, wobble=0.007)
    shift = rng.integers(3, 7) * rng.choice([-1, 1]), rng.integers(2, 5) * rng.choice([-1, 1])
    fill = np.roll(masks["fill"], shift, axis=(0, 1))
    stripe = np.roll(masks["cyan"], shift, axis=(0, 1))
    second_pass = cv2.warpAffine(fbm(S, seed + 3, base=5, octaves=5, aspect=(7, 0.2)), rotation, (S, S),
                                 borderMode=cv2.BORDER_REFLECT)
    top = mix(top, STENCIL_BLACK, masks["outline"])
    top = mix(top, CREAM * (0.92 + 0.12 * second_pass)[..., None], fill * coat)
    top = mix(top, PAINTED_TEAL, stripe * coat)
    chip = chips(seed, corner_spots(rng, 1, radius=(0.05, 0.07)) + [(rng.uniform(0.2, 0.8), 0.96, 0.06)], 0.045)
    rgb, height = apply_chips(top, chip_layers(chip, seed, primer_width=0.006), seed)
    paint_body = np.clip(coat + masks["alpha"], 0, 1.4)
    height += bevel + 0.35 * paint_body * (0.6 + 0.8 * bristles) * 0.6
    return finish(lit(rgb, height, strength=0.5, sigma=1.2), alpha)


def waterslide_decal(icon_path, colour, seed):
    """5. Factory decal on painted steel: yellowed carrier film around the glyph, one corner lifted and folded back,
    a piece torn away; the plate itself only has edge wear."""
    rng = np.random.default_rng(seed)
    alpha, distance, bevel = plate(seed, radius=0.04)
    base = flat(colour, seed)
    masks = icon_masks(icon_path, seed, wobble=0.0)
    icon_distance = signed_distance(masks["alpha"] > 0.5)
    film = coverage((blur((icon_distance + 0.035 * S > 0).astype(F32), 0.02 * S) - 0.5) * 0.04 * S)
    ys, xs = np.where(film > 0.5)
    corner = np.array([xs.max(), ys.min()], F32)
    normal = np.array([1.0, -1.0], F32) / math.sqrt(2.0)
    fold = 0.16 * S
    side = (XS - corner[0]) * normal[0] + (YS - corner[1]) * normal[1] + fold
    lifted = film * (side > 0)
    mirror_x = XS - 2.0 * side * normal[0]
    mirror_y = YS - 2.0 * side * normal[1]
    flap = cv2.remap(lifted, mirror_x.astype(F32), mirror_y.astype(F32), cv2.INTER_LINEAR) * (side < 0)
    tear_x = xs.min() + rng.uniform(0.1, 0.4) * (xs.max() - xs.min())
    torn = chips(seed + 3, [(tear_x / S, ys.max() / S, 0.09)], 0.04, chance=0.9) * film
    intact = film * (1.0 - (side > 0)) * (1.0 - torn)
    decal = paint_icon(base * np.array([1.02, 1.0, 0.9], F32) + 6.0, masks, (214, 203, 168), (38, 34, 30),
                       (128, 164, 150))
    top = base * (1.0 - intact[..., None]) + decal * intact[..., None]
    top = top * (1.0 - 0.05 * lifted[..., None])
    outside_torn = cv2.distanceTransform((torn < 0.5).astype(np.uint8), cv2.DIST_L2, 5)
    fibres = coverage(0.006 * S * (0.4 + fbm(S, seed + 4, base=30)) - outside_torn) * intact
    top = mix(top, (226, 220, 204), fibres * 0.7)
    film_edge = np.exp(-(signed_distance(intact > 0.5) / (0.003 * S)) ** 2) * intact
    top = top + 18.0 * film_edge[..., None]
    shadow = blur(np.roll(np.roll(flap, 7, 0), -6, 1), 4.0)
    top = top * (1.0 - 0.45 * shadow[..., None])
    paper = np.array([198, 192, 174], F32) * (0.85 + 0.2 * ramp(-side, 0, fold))[..., None]
    top = mix(top, paper, flap)
    chip = chips(seed, corner_spots(rng, 2, radius=(0.05, 0.08)), 0.05)
    rgb, height = apply_chips(top, chip_layers(chip, seed), seed)
    height += bevel + 0.08 * intact + 0.6 * flap * ramp(-side, 0, fold)
    return finish(lit(rgb, height, strength=0.4), alpha)


def vitreous_sign(icon_path, colour, seed):
    """6. Baked vitreous enamel sign: glossy, glyph fired in flush, rolled steel rim, faint crazing, one specular
    streak, one round chip through white ground coat to black steel."""
    rng = np.random.default_rng(seed)
    alpha, distance, bevel = plate(seed, radius=0.07, dents=0)
    rim = 0.028 * S
    enamel = coverage(distance - rim)
    masks = icon_masks(icon_path, seed, wobble=0.002)
    face = paint_icon(np.broadcast_to(np.asarray(colour, F32) * 1.04, (S, S, 3)).copy(), masks, (228, 220, 196),
                      (32, 33, 38), (70, 118, 124))
    nearest, first, second, _ = cells(0.05 * S, seed + 2, warp=3.0)
    crazing = ramp(1.6 - (second - first), 0.0, 1.2) * ramp(fbm(S, seed + 3, base=4), 0.45, 0.75)
    face = face * (1.0 - 0.12 * crazing[..., None])
    edge_spot = [(rng.uniform(0.3, 0.7), 0.06, 0.07)] if rng.random() < 0.5 else [(0.94, rng.uniform(0.3, 0.7), 0.07)]
    chip = chips(seed, edge_spot, 0.05, warp=0.03 * S) * enamel
    layers = chip_layers(chip, seed, primer_width=0.012, pit_depth=0.015)
    rgb, chip_height = apply_chips(face, layers, seed, primer=GROUND_COAT, steel=SIGN_STEEL, pit=RUST)
    rim_rgb = SIGN_STEEL * (0.9 + 0.3 * fbm(S, seed + 5, base=16))[..., None]
    rgb = rgb * enamel[..., None] + rim_rgb * (1.0 - enamel[..., None])
    height = bevel * 1.5 + enamel * chip_height * 0.8
    rgb = lit(rgb, height, strength=0.55, sigma=1.4)
    streak = np.exp(-(((XS - YS) * 0.7 + (XS + YS) * 0.15 - 0.05 * S) / (0.035 * S)) ** 2)
    streak *= ramp(0.95 * S - (XS + YS), 0.0, 0.4 * S) * enamel * layers[0]
    rgb = rgb + 70.0 * streak[..., None]
    return finish(rgb, alpha)


VARIANTS = {
    "1_instrument_placard": instrument_placard,
    "2_field_stencil": field_stencil,
    "3_single_impact": single_impact,
    "4_hand_brushed": hand_brushed,
    "5_waterslide_decal": waterslide_decal,
    "6_vitreous_sign": vitreous_sign,
}


# ------------------------------------------------------------------------------------------------- portrait + sheet
def place_seated(portrait, badge, corner):
    """Badge seated on the portrait by a thin dark contact edge instead of a floating drop shadow."""
    size, inset = ib.PORTRAIT_BADGE, ib.PORTRAIT_INSET
    badge = badge.resize((size, size), Image.LANCZOS)
    x = inset if corner == "left" else portrait.width - inset - size
    y = portrait.height - inset - size
    badge_alpha = np.asarray(badge.getchannel("A"), F32) / 255.0
    contact = np.clip(blur(badge_alpha, 0.9) * 1.8, 0, 1) * 0.7
    edge = Image.fromarray(np.dstack([np.zeros((size, size, 3)), contact * 255]).astype(np.uint8), "RGBA")
    layer = Image.new("RGBA", portrait.size, (0, 0, 0, 0))
    layer.paste(edge, (x, y + 1))
    portrait = Image.alpha_composite(portrait, layer)
    layer = Image.new("RGBA", portrait.size, (0, 0, 0, 0))
    layer.paste(badge, (x, y))
    return Image.alpha_composite(portrait, layer)


def zoom_strip(picture75):
    """The badge row of the 75 px portrait at 3x, nearest neighbour, to judge it pixel by pixel."""
    strip = picture75.crop((0, 75 - 27, 75, 75))
    return strip.resize((strip.width * 3, strip.height * 3), Image.NEAREST)


def build_sheet(rows, path):
    gap, label_w = 18, 200
    widths = [180, 180, 256, 75, 48, 225]
    titles = ["rapid fire", "light tank class", "portrait 256", "75 px", "48 px", "75 px badges, 3x zoom"]
    row_h, header_h = 256 + gap, 92
    width = label_w + sum(widths) + gap * (len(widths) + 1)
    sheet = Image.new("RGB", (width, header_h + len(rows) * row_h), (22, 23, 25))
    draw = ImageDraw.Draw(sheet)
    draw.text((gap, 14), "Enamel + worn icons - six hand-made directions", fill=(235, 232, 220), font=ib.font(22))
    draw.text((gap, 42), "Shared damage through plate and icon, angular chips with primer + steel, muted enamel, "
                         "one light. Top row = the previous version.", fill=(160, 158, 150), font=ib.font(14))
    x = label_w + gap
    for title, w in zip(titles, widths):
        draw.text((x, header_h - 20), title, fill=(160, 158, 150), font=ib.font(13))
        x += w + gap
    y = header_h
    for label, cells_row in rows:
        draw.text((gap, y + 116), label, fill=(235, 232, 220), font=ib.font(16))
        x = label_w + gap
        for w, cell in zip(widths, cells_row):
            if w == 225:
                sheet.paste(cell, (x, y + (256 - cell.height) // 2))
            else:
                image = cell if cell.size == (w, w) else cell.resize((w, w), Image.LANCZOS)
                sheet.paste(ib.on_dark(image), (x, y + (256 - w) // 2))
            x += w + gap
        y += row_h
    sheet.save(path)


def portrait_row(label, left_badge, right_badge, seated=True):
    picture = ib.clean_portrait()
    place = place_seated if seated else ib.place_badge
    picture = place(place(picture, left_badge, "left"), right_badge, "right").convert("RGB")
    small = [picture.resize((s, s), Image.LANCZOS) for s in ib.UI_SIZES]
    return picture, (label, [right_badge, left_badge, picture] + small + [zoom_strip(small[0])])


def main():
    for sub in ("badges", "portraits"):
        (OUT / sub).mkdir(parents=True, exist_ok=True)
    belt = ib.ICON_SET / ib.RAPID_FIRE_ICON
    light = ib.ICON_SET / ib.LIGHT_TANK_ICON
    previous_belt = ib.make_badge(ib.WORN_ICON_SET / ib.RAPID_FIRE_ICON, "enamel_chipped", ib.RAPID_FIRE_COLOUR, 1000)
    previous_light = ib.make_badge(ib.WORN_ICON_SET / ib.LIGHT_TANK_ICON, "enamel_chipped", ib.LIGHT_CLASS_COLOUR,
                                   1500)
    _, row = portrait_row("previous: enamel + worn", previous_light, previous_belt, seated=False)
    rows = [row]
    for index, (name, build) in enumerate(VARIANTS.items()):
        seed = 3000 + index * 53
        belt_badge = build(belt, ROLE_COLOUR, seed).resize((ib.BADGE_SIZE, ib.BADGE_SIZE), Image.LANCZOS)
        light_badge = build(light, CLASS_COLOUR, seed + 700).resize((ib.BADGE_SIZE, ib.BADGE_SIZE), Image.LANCZOS)
        belt_badge.save(OUT / "badges" / f"AutoCannon_{name}.png")
        light_badge.save(OUT / "badges" / f"LightTank_{name}.png")
        picture, row = portrait_row(name.replace("_", " "), light_badge, belt_badge)
        picture.save(OUT / "portraits" / f"PzII_F_{name}.png")
        rows.append(row)
        print(f"variant {name}")
    build_sheet(rows, OUT / "EnamelHandmade_sheet.png")
    print(f"sheet {OUT / 'EnamelHandmade_sheet.png'}")


if __name__ == "__main__":
    main()
