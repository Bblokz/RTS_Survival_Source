"""Colour-coded, battle-worn backgrounds behind the new tactical stencil icons (Set08_FlatStencil).

The original icons are only read, never written: every design produces a separate badge (background plate + icon) so the
plain icons stay available. Each design function takes a colour and a seed and returns a damaged plate; the damage is
procedural (seeded noise), so two plates of the same design never show the same chips.

Usage:
  python icon_backgrounds.py preview
      -> Saved/TacticalIconBackgrounds/: plates, belt badges, Pz II portraits per design + comparison sheet
  python icon_backgrounds.py badge <icon png> <design> <hex colour> <out png> [--seed N] [--size 256]
      -> one badge (RGBA) for any icon

Designs: enamel_chipped, brush_daub, roundel_worn, riveted_plate, overspray_stencil, lamp_lens
"""
import argparse
import math
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ICON_SET = Path("D:/UE_Assets/Icons/TrainingUI/BuildingIcons/Set08_FlatStencil")
WORN_ICON_SET = ICON_SET.parent / "Set08_FlatStencil_Worn"
CLEAN_PORTRAIT = Path("D:/UE_Assets/Icons/TrainingUI/GerTanks/Light/NoIcon/T_Ger_LightTank_PanzerII_F.png")
LIVE_PORTRAIT = Path("D:/UE_Assets/Icons/TrainingUI/GerTanks/Light/T_Ger_LightTank_PanzerII_F.png")
OUT = Path("D:/UE5Projects/RTS_Survival/Saved/TacticalIconBackgrounds")

RAPID_FIRE_ICON = "T_AutoCannon_Class_V2.png"
LIGHT_TANK_ICON = "T_LightTankClass_Icon_V2.png"

# Plates are drawn at this size and downsampled, so chips and cracks stay anti-aliased at icon size.
INTERNAL = 512
BADGE_SIZE = 256
PORTRAIT_SIZE = 256
# Badge size and corner inset on a 256 portrait; matches the area the hand-painted neon glyphs cover.
PORTRAIT_BADGE = 82
PORTRAIT_INSET = 4
# Training card size at 1080p (BP_W_TrainingItem) and a smaller HUD size, to judge legibility.
UI_SIZES = (75, 48)

GUNMETAL = np.array([48, 51, 55], np.float32)
BARE_STEEL = np.array([150, 150, 146], np.float32)
RUST = np.array([112, 62, 30], np.float32)

# Placeholder colour coding (role = rapid fire, class = light tank).
RAPID_FIRE_COLOUR = (186, 116, 30)
LIGHT_CLASS_COLOUR = (92, 112, 52)
PALETTE = {
    "red": (152, 44, 36),
    "amber": RAPID_FIRE_COLOUR,
    "ochre": (176, 148, 42),
    "olive": LIGHT_CLASS_COLOUR,
    "steel blue": (50, 92, 130),
    "violet": (98, 62, 122),
}


# ----------------------------------------------------------------------------------------------------------------- noise
def fbm(size, seed, base=8, octaves=4, persistence=0.5, aspect=(1, 1)):
    """Fractal value noise in [0, 1]; aspect (rows, cols) stretches cells, e.g. (8, 1) gives horizontal streaks."""
    rng = np.random.default_rng(seed)
    total = np.zeros((size, size), np.float32)
    amplitude, norm, cells = 1.0, 0.0, base
    for _ in range(octaves):
        rows = max(2, int(cells * aspect[0]))
        cols = max(2, int(cells * aspect[1]))
        grid = rng.random((rows + 1, cols + 1)).astype(np.float32)
        total += cv2.resize(grid, (size, size), interpolation=cv2.INTER_CUBIC) * amplitude
        norm += amplitude
        amplitude *= persistence
        cells *= 2
    total /= norm
    return (total - total.min()) / (np.ptp(total) + 1e-6)


def ramp(value, low, high):
    return np.clip((value - low) / (high - low), 0.0, 1.0)


# ---------------------------------------------------------------------------------------------------------------- shapes
def signed_distance(mask):
    """Pixels inside the mask are positive, outside negative; perturbing it gives ragged but anti-aliased edges."""
    mask = mask.astype(np.uint8)
    inside = cv2.distanceTransform(mask, cv2.DIST_L2, 5)
    outside = cv2.distanceTransform(1 - mask, cv2.DIST_L2, 5)
    return inside - outside


def coverage(distance):
    return np.clip(distance + 0.5, 0.0, 1.0)


def rounded_rect(size, margin, radius):
    image = Image.new("L", (size, size), 0)
    ImageDraw.Draw(image).rounded_rectangle([margin, margin, size - 1 - margin, size - 1 - margin], radius, fill=255)
    return np.asarray(image) > 127


def chamfered_rect(size, margin, cut):
    lo, hi = margin, size - 1 - margin
    points = [(lo + cut, lo), (hi - cut, lo), (hi, lo + cut), (hi, hi - cut), (hi - cut, hi), (lo + cut, hi),
              (lo, hi - cut), (lo, lo + cut)]
    image = Image.new("L", (size, size), 0)
    ImageDraw.Draw(image).polygon(points, fill=255)
    return np.asarray(image) > 127


def radial(size):
    centre = (size - 1) / 2.0
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    return np.hypot(xs - centre, ys - centre), np.arctan2(ys - centre, xs - centre)


# -------------------------------------------------------------------------------------------------------------- surfaces
def paint_surface(colour, seed, size=INTERNAL):
    """Flat army enamel: low-frequency mottling plus a little grain."""
    mottle = fbm(size, seed + 101, base=5, octaves=5)
    grain = fbm(size, seed + 102, base=64, octaves=2)
    shade = 0.86 + 0.22 * mottle + 0.06 * (grain - 0.5)
    return np.asarray(colour, np.float32) * shade[..., None]


def metal_surface(seed, size=INTERNAL):
    """Dark gunmetal with faint horizontal brushing."""
    brushed = fbm(size, seed + 201, base=24, octaves=3, aspect=(4, 0.25))
    blotch = fbm(size, seed + 202, base=6, octaves=4)
    shade = 0.78 + 0.25 * blotch + 0.12 * (brushed - 0.5)
    return GUNMETAL * shade[..., None]


def bevel_light(alpha, blur):
    """Light from the top left over a soft height field built from the shape."""
    height = cv2.GaussianBlur(alpha.astype(np.float32), (0, 0), blur)
    grad_y, grad_x = np.gradient(height)
    light = grad_x + grad_y
    return light / (np.abs(light).max() + 1e-6)


def grime(seed, size=INTERNAL, strength=0.3):
    """Darker dirt that collects towards the bottom."""
    ys = np.linspace(0.0, 1.0, size, dtype=np.float32)[:, None]
    dirt = fbm(size, seed + 301, base=5, octaves=4) * (0.55 + 0.7 * ys)
    return 1.0 - strength * ramp(dirt, 0.45, 1.05)


def scratch_layer(seed, count, size=INTERNAL, length=(0.05, 0.18)):
    """Thin bright scratches through the paint."""
    rng = np.random.default_rng(seed + 401)
    layer = np.zeros((size, size), np.float32)
    for _ in range(count):
        x, y = rng.uniform(0.1, 0.9, 2) * size
        angle = rng.uniform(-0.6, 0.6) + (math.pi / 4 if rng.random() < 0.5 else -math.pi / 4)
        span = rng.uniform(*length) * size
        bend = rng.uniform(-0.25, 0.25)
        points = []
        for step in np.linspace(0.0, 1.0, 6):
            a = angle + bend * step
            points.append((x + math.cos(a) * span * step, y + math.sin(a) * span * step))
        cv2.polylines(layer, [np.int32(points)], False, float(rng.uniform(0.35, 0.9)),
                      thickness=int(rng.integers(1, 3)), lineType=cv2.LINE_AA)
    return layer


def edge_chip_depth(seed, max_depth, density=0.5, base=14, size=INTERNAL):
    """How far paint has flaked back from an edge, per pixel; zero where the edge is intact."""
    noise = fbm(size, seed + 501, base=base, octaves=3)
    return ramp(noise, 1.0 - density, 1.0) ** 1.4 * max_depth


def spot_chips(seed, threshold=0.8, base=36, size=INTERNAL):
    """Small flakes off the middle of a painted face."""
    return ramp(fbm(size, seed + 601, base=base, octaves=2), threshold, threshold + 0.05)


def paint_over_metal(paint_mask, paint_rgb, metal_rgb, lip):
    """Bare metal where paint flaked off, with a dark lip where the paint edge stands proud of the metal."""
    lip_shadow = np.clip(cv2.GaussianBlur(paint_mask, (0, 0), lip) - paint_mask, 0.0, 1.0)
    metal = metal_rgb * (1.0 - 0.55 * lip_shadow)[..., None]
    return metal * (1.0 - paint_mask[..., None]) + paint_rgb * paint_mask[..., None]


def finish(rgb, alpha, inner_box):
    rgba = np.dstack([np.clip(rgb, 0, 255), np.clip(alpha, 0, 1) * 255.0]).astype(np.uint8)
    return rgba, inner_box


# --------------------------------------------------------------------------------------------------------------- designs
def enamel_chipped(colour, seed, size=INTERNAL):
    """Rounded enamel plate, flaked back to gunmetal along the edges, a few bites out of the silhouette."""
    distance = signed_distance(rounded_rect(size, 0.04 * size, 0.13 * size))
    bites = ramp(fbm(size, seed + 1, base=10, octaves=3), 0.7, 1.0) * 0.05 * size
    plate = distance - bites
    alpha = coverage(plate)
    rim = 0.014 * size
    paint = coverage(plate - rim - edge_chip_depth(seed, 0.075 * size, density=0.55))
    paint *= 1.0 - spot_chips(seed)
    rgb = paint_over_metal(paint, paint_surface(colour, seed), metal_surface(seed), 0.006 * size)
    scratches = scratch_layer(seed, 14) * paint
    rgb = rgb * (1 - scratches[..., None] * 0.6) + BARE_STEEL * scratches[..., None] * 0.6
    rgb *= (1.0 + 0.35 * bevel_light(alpha, 0.012 * size))[..., None]
    rgb *= grime(seed)[..., None]
    pad = 0.13 * size
    return finish(rgb, alpha, (pad, pad, size - pad, size - pad))


def brush_daub(colour, seed, size=INTERNAL):
    """A rough field-paint swipe: streaky bristle edges, dry-brushed ends where the paint runs out."""
    distance = signed_distance(rounded_rect(size, 0.08 * size, 0.06 * size))
    streaks = fbm(size, seed + 1, base=6, octaves=4, aspect=(6, 0.35))
    bristles = fbm(size, seed + 2, base=40, octaves=2, aspect=(4, 0.08))
    xs = np.abs(np.linspace(-1.0, 1.0, size, dtype=np.float32))[None, :]
    ends = xs ** 4
    swipe = distance + (streaks - 0.5) * 0.12 * size + (fbm(size, seed + 3, base=10) - 0.5) * 0.05 * size
    swipe -= ends * fbm(size, seed + 4, base=12, octaves=3) * 0.14 * size
    alpha = coverage(swipe)
    dry_gaps = ramp(0.42 - bristles, 0.0, 0.12) * np.clip(ends * 2.2 + 0.08, 0, 1)
    alpha *= 1.0 - dry_gaps
    rgb = paint_surface(colour, seed) * (0.82 + 0.3 * bristles)[..., None]
    # Paint pools thicker and darker along the stroke's lower edge.
    rgb *= (1.0 - 0.18 * ramp(np.linspace(0, 1, size, dtype=np.float32)[:, None], 0.55, 0.95))[..., None]
    rgb *= (1.0 + 0.18 * bevel_light(alpha, 0.008 * size))[..., None]
    pad = 0.15 * size
    return finish(rgb, alpha, (pad, pad, size - pad, size - pad))


def roundel_worn(colour, seed, size=INTERNAL):
    """Round enamel disc in a gunmetal bezel, like the gauge rims; enamel flaked back from the bezel."""
    radius, angle = radial(size)
    outer = 0.47 * size
    ring = 0.065 * size
    bites = ramp(fbm(size, seed + 1, base=12, octaves=3), 0.75, 1.0) * 0.03 * size
    alpha = coverage(outer - radius - bites)
    inner_distance = (outer - ring) - radius
    paint = coverage(inner_distance - edge_chip_depth(seed, 0.07 * size, density=0.5))
    paint *= 1.0 - spot_chips(seed, threshold=0.83)
    metal = metal_surface(seed)
    # Bezel: lit along the top left, dark lower right, darker groove where it meets the enamel.
    bezel_light = 0.5 + 0.5 * np.cos(angle + 3 * math.pi / 4)
    in_ring = ramp(-inner_distance, 0.0, 2.0)
    metal *= (0.75 + 0.6 * bezel_light * in_ring)[..., None]
    groove = np.exp(-((inner_distance + 1.5) / (0.008 * size)) ** 2)
    metal *= (1.0 - 0.5 * groove)[..., None]
    enamel = paint_surface(colour, seed) * (1.05 - 0.25 * ramp(radius, 0.1 * size, outer - ring))[..., None]
    rgb = paint_over_metal(paint, enamel, metal, 0.006 * size)
    scratches = scratch_layer(seed, 10) * paint
    rgb = rgb * (1 - scratches[..., None] * 0.55) + BARE_STEEL * scratches[..., None] * 0.55
    rgb *= grime(seed, strength=0.22)[..., None]
    half = (outer - ring) * 0.74
    centre = size / 2.0
    return finish(rgb, alpha, (centre - half, centre - half, centre + half, centre + half))


def riveted_plate(colour, seed, size=INTERNAL):
    """Chamfered gunmetal plate with four rivets; the painted panel is worn through around its edge, rust weeps
    from the rivets."""
    plate = signed_distance(chamfered_rect(size, 0.035 * size, 0.08 * size))
    alpha = coverage(plate - ramp(fbm(size, seed + 1, base=14), 0.8, 1.0) * 0.02 * size)
    panel = signed_distance(rounded_rect(size, 0.15 * size, 0.035 * size))
    paint = coverage(panel - edge_chip_depth(seed, 0.09 * size, density=0.62, base=12))
    paint *= 1.0 - spot_chips(seed, threshold=0.78)
    metal = metal_surface(seed) * (1.0 + 0.45 * bevel_light(alpha, 0.01 * size))[..., None]
    rgb = paint_over_metal(paint, paint_surface(colour, seed), metal, 0.007 * size)
    rgb *= (1.0 + 0.15 * bevel_light(paint, 0.004 * size))[..., None]
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    rivet_radius = 0.03 * size
    # Inside the chamfered corners and clear of the painted panel.
    inset = 0.1 * size
    rust = np.zeros((size, size), np.float32)
    for cx in (inset, size - inset):
        for cy in (inset, size - inset):
            d = np.hypot(xs - cx, ys - cy) / rivet_radius
            dome = np.sqrt(np.clip(1.0 - d ** 2, 0, 1))
            highlight = np.clip(1.0 - np.hypot(xs - (cx - rivet_radius * 0.35), ys - (cy - rivet_radius * 0.35))
                                / (rivet_radius * 0.6), 0, 1)
            inside = d < 1.0
            rgb[inside] = (GUNMETAL * (0.7 + 0.6 * dome[inside, None]) + 110 * highlight[inside, None] ** 2)
            below = np.clip(ys - cy, 0, None)
            weep = np.exp(-((xs - cx) / (rivet_radius * 0.7)) ** 2) * np.exp(-below / (0.12 * size)) * (ys > cy)
            rust = np.maximum(rust, weep * fbm(size, seed + 9 + int(cx + cy), base=30, octaves=2, aspect=(0.3, 3)))
    rust = np.clip(rust * 0.85, 0, 0.7)
    rgb = rgb * (1 - rust[..., None]) + RUST * rust[..., None]
    rgb *= grime(seed, strength=0.25)[..., None]
    pad = 0.17 * size
    return finish(rgb, alpha, (pad, pad, size - pad, size - pad))


def overspray_stencil(colour, seed, size=INTERNAL):
    """A spray-painted stencil square: soft speckled overspray halo, uneven density, a few drips, flaked patches."""
    rng = np.random.default_rng(seed + 1)
    core = rounded_rect(size, 0.1 * size, 0.035 * size).astype(np.uint8)
    for _ in range(int(rng.integers(3, 6))):
        x = int(rng.uniform(0.18, 0.82) * size)
        top = int(0.86 * size)
        length = int(rng.uniform(0.03, 0.11) * size)
        width = max(2, int(rng.uniform(0.008, 0.016) * size))
        cv2.line(core, (x, top), (x, top + length), 1, width)
        cv2.circle(core, (x, top + length), int(width * 0.9), 1, -1)
    distance = signed_distance(core) + (fbm(size, seed + 2, base=24, octaves=2) - 0.5) * 0.015 * size
    soft_core = np.clip(distance / (0.006 * size) + 0.5, 0, 1)
    halo = cv2.GaussianBlur(core.astype(np.float32), (0, 0), 0.03 * size)
    speckle = (rng.random((size, size)) < halo * 0.35).astype(np.float32)
    speckle = cv2.GaussianBlur(speckle, (0, 0), 0.7)
    density = 0.72 + 0.28 * fbm(size, seed + 3, base=4, octaves=4)
    flakes = ramp(fbm(size, seed + 4, base=9, octaves=4), 0.78, 0.84)
    alpha = np.maximum(soft_core * density * (1.0 - 0.85 * flakes), np.clip(speckle * 1.6, 0, 0.9) * (1 - soft_core))
    rgb = paint_surface(colour, seed) * (0.9 + 0.15 * density)[..., None]
    pad = 0.16 * size
    return finish(rgb, alpha, (pad, pad * 0.9, size - pad, size - pad * 1.1))


def lamp_lens(colour, seed, size=INTERNAL):
    """A backlit indicator lamp like the analog HUD kit: knurled bezel, glowing coloured glass, cracked by a hit."""
    rng = np.random.default_rng(seed + 1)
    radius, angle = radial(size)
    outer = 0.475 * size
    glass_radius = outer - 0.07 * size
    alpha = coverage(outer - radius)
    knurl = 0.5 + 0.5 * np.cos(angle * 44)
    bezel_light = 0.5 + 0.5 * np.cos(angle + 3 * math.pi / 4)
    metal = metal_surface(seed) * (0.6 + 0.35 * knurl + 0.45 * bezel_light)[..., None] * 0.9
    glass = coverage(glass_radius - radius)
    falloff = np.clip(radius / glass_radius, 0, 1)
    tint = np.asarray(colour, np.float32)
    lit = tint * (1.55 - 1.15 * falloff ** 1.6)[..., None] + 70 * np.exp(-(falloff / 0.35) ** 2)[..., None]
    lit *= (1.0 - 0.45 * np.exp(-((glass_radius - radius) / (0.02 * size)) ** 2))[..., None]
    cracks = np.zeros((size, size), np.float32)
    centre = size / 2.0
    # Hit near the glass edge so the cracks run across the visible ring around the icon.
    impact_angle = rng.uniform(0, 2 * math.pi)
    impact = np.array([centre, centre]) + np.array([math.cos(impact_angle), math.sin(impact_angle)]) * glass_radius * 0.8
    for _ in range(int(rng.integers(7, 11))):
        heading = rng.uniform(0, 2 * math.pi)
        point = impact.copy()
        points = [point.copy()]
        for _ in range(9):
            heading += rng.uniform(-0.35, 0.35)
            point = point + np.array([math.cos(heading), math.sin(heading)]) * rng.uniform(0.04, 0.08) * size
            points.append(point.copy())
        cv2.polylines(cracks, [np.int32(points)], False, 1.0, thickness=3, lineType=cv2.LINE_AA)
    for ring_radius in rng.uniform(0.05, 0.14, 2) * size:
        start = rng.uniform(0, 360)
        cv2.ellipse(cracks, tuple(int(v) for v in impact), (int(ring_radius), int(ring_radius)), 0, start,
                    start + rng.uniform(60, 160), 1.0, 1, cv2.LINE_AA)
    cracks *= glass
    crack_shadow = np.roll(np.roll(cracks, 2, axis=0), 2, axis=1)
    lit = lit * (1.0 - 0.5 * crack_shadow[..., None]) + 255 * cracks[..., None] * 0.55
    specular = np.zeros((size, size), np.float32)
    cv2.ellipse(specular, (int(centre - 0.14 * size), int(centre - 0.17 * size)),
                (int(0.17 * size), int(0.07 * size)), -35, 0, 360, 1.0, -1)
    specular = cv2.GaussianBlur(specular, (0, 0), 0.025 * size) * glass
    dirt = ramp(fbm(size, seed + 5, base=30, octaves=2), 0.75, 0.9) * glass
    lit = lit * (1.0 - 0.4 * dirt[..., None]) + 255 * specular[..., None] * 0.3
    rgb = metal * (1 - glass[..., None]) + lit * glass[..., None]
    half = glass_radius * 0.76
    return finish(rgb, alpha, (centre - half, centre - half, centre + half, centre + half))


DESIGNS = {
    "enamel_chipped": enamel_chipped,
    "brush_daub": brush_daub,
    "roundel_worn": roundel_worn,
    "riveted_plate": riveted_plate,
    "overspray_stencil": overspray_stencil,
    "lamp_lens": lamp_lens,
}


# ---------------------------------------------------------------------------------------------------------------- badges
def cropped_icon(icon_path):
    icon = Image.open(icon_path).convert("RGBA")
    return icon.crop(icon.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox())


def make_badge(icon_path, design, colour, seed, size=BADGE_SIZE):
    """Plate first, icon centred inside the plate's icon box on top (its own drop shadow falls onto the plate)."""
    plate_rgba, (x0, y0, x1, y1) = DESIGNS[design](colour, seed)
    badge = Image.fromarray(plate_rgba, "RGBA")
    if icon_path is not None:
        icon = cropped_icon(icon_path)
        scale = min((x1 - x0) / icon.width, (y1 - y0) / icon.height)
        icon = icon.resize((max(1, round(icon.width * scale)), max(1, round(icon.height * scale))), Image.LANCZOS)
        layer = Image.new("RGBA", badge.size, (0, 0, 0, 0))
        layer.paste(icon, (round((x0 + x1 - icon.width) / 2), round((y0 + y1 - icon.height) / 2)))
        badge = Image.alpha_composite(badge, layer)
    return badge.resize((size, size), Image.LANCZOS)


def bare_icon_badge(icon_path, size=BADGE_SIZE):
    """The icon on its own, padded like a badge, to compare against the plated ones."""
    icon = cropped_icon(icon_path)
    box = INTERNAL * 0.84
    scale = min(box / icon.width, box / icon.height)
    icon = icon.resize((round(icon.width * scale), round(icon.height * scale)), Image.LANCZOS)
    layer = Image.new("RGBA", (INTERNAL, INTERNAL), (0, 0, 0, 0))
    layer.paste(icon, ((INTERNAL - icon.width) // 2, (INTERNAL - icon.height) // 2))
    return layer.resize((size, size), Image.LANCZOS)


def place_badge(portrait, badge, corner):
    """Badge in a bottom corner with a soft contact shadow so it lifts off busy scenery."""
    badge = badge.resize((PORTRAIT_BADGE, PORTRAIT_BADGE), Image.LANCZOS)
    x = PORTRAIT_INSET if corner == "left" else PORTRAIT_SIZE - PORTRAIT_INSET - PORTRAIT_BADGE
    y = PORTRAIT_SIZE - PORTRAIT_INSET - PORTRAIT_BADGE
    shadow_alpha = cv2.GaussianBlur(np.asarray(badge.getchannel("A"), np.float32), (0, 0), 2.0) * 0.6
    shadow = Image.new("RGBA", portrait.size, (0, 0, 0, 0))
    shadow.paste(Image.fromarray(np.dstack([np.zeros((PORTRAIT_BADGE, PORTRAIT_BADGE, 3)), shadow_alpha])
                                 .astype(np.uint8), "RGBA"), (x + 1, y + 2))
    portrait = Image.alpha_composite(portrait, shadow)
    layer = Image.new("RGBA", portrait.size, (0, 0, 0, 0))
    layer.paste(badge, (x, y))
    return Image.alpha_composite(portrait, layer)


def clean_portrait():
    return Image.open(CLEAN_PORTRAIT).convert("RGBA").resize((PORTRAIT_SIZE, PORTRAIT_SIZE), Image.LANCZOS)


def portrait_with(left_badge, right_badge):
    picture = clean_portrait()
    picture = place_badge(picture, left_badge, "left")
    return place_badge(picture, right_badge, "right").convert("RGB")


# ----------------------------------------------------------------------------------------------------------------- sheet
def font(size):
    try:
        return ImageFont.truetype("C:/Windows/Fonts/bahnschrift.ttf", size)
    except OSError:
        return ImageFont.load_default()


def on_dark(image, background=(30, 31, 34)):
    base = Image.new("RGBA", image.size, background + (255,))
    return Image.alpha_composite(base, image.convert("RGBA")).convert("RGB")


def build_sheet(rows, palette_rows, path):
    label_w, plate_w, badge_w, gap = 190, 180, 180, 18
    columns = [("background", plate_w), ("+ icon", badge_w), ("portrait 256", 256), ("75 px", 75), ("48 px", 48)]
    row_h = 256 + gap
    header_h = 92
    palette_cell = 64
    palette_h = 60 + len(palette_rows) * (palette_cell + 10)
    width = label_w + sum(w for _, w in columns) + gap * (len(columns) + 1)
    height = header_h + len(rows) * row_h + palette_h + gap
    sheet = Image.new("RGB", (width, height), (22, 23, 25))
    draw = ImageDraw.Draw(sheet)
    draw.text((gap, 14), "Tactical icon backgrounds - Pz II F (class: light tank, role: rapid fire)", fill=(235, 232, 220),
              font=font(22))
    draw.text((gap, 42), "Colours are placeholders: amber = rapid fire / belt fed, olive = light tank class",
              fill=(160, 158, 150), font=font(14))
    x = label_w + gap
    for title, w in columns:
        draw.text((x, header_h - 18), title, fill=(160, 158, 150), font=font(13))
        x += w + gap
    y = header_h
    for label, cells in rows:
        draw.text((gap, y + 110), label, fill=(235, 232, 220), font=font(16))
        x = label_w + gap
        for (_, w), cell in zip(columns, cells):
            if cell is not None:
                image = cell if cell.size == (w, w) else cell.resize((w, w), Image.LANCZOS)
                sheet.paste(on_dark(image), (x, y + (256 - w) // 2 if w < 256 else y))
            x += w + gap
        y += row_h
    draw.text((gap, y + 10), "Palette check (belt icon, 64 px)", fill=(235, 232, 220), font=font(16))
    names = list(PALETTE)
    for index, name in enumerate(names):
        draw.text((label_w + gap + index * (palette_cell + 24), y + 36), name, fill=(160, 158, 150), font=font(12))
    y += 56
    for label, badges in palette_rows:
        draw.text((gap, y + 22), label, fill=(200, 198, 190), font=font(14))
        for index, badge in enumerate(badges):
            sheet.paste(on_dark(badge.resize((palette_cell, palette_cell), Image.LANCZOS)),
                        (label_w + gap + index * (palette_cell + 24), y))
        y += palette_cell + 10
    sheet.save(path)


def preview():
    for sub in ("plates", "badges", "portraits"):
        (OUT / sub).mkdir(parents=True, exist_ok=True)
    belt = ICON_SET / RAPID_FIRE_ICON
    light = ICON_SET / LIGHT_TANK_ICON
    rows = []

    live = Image.open(LIVE_PORTRAIT).convert("RGB").resize((PORTRAIT_SIZE, PORTRAIT_SIZE), Image.LANCZOS)
    rows.append(("live (neon)", [None, None, live] + [live.resize((s, s), Image.LANCZOS) for s in UI_SIZES]))
    plain = portrait_with(bare_icon_badge(light), bare_icon_badge(belt))
    plain.save(OUT / "portraits" / "PzII_F_no_background.png")
    rows.append(("new icons, no bg", [None, bare_icon_badge(belt)] + [plain] +
                 [plain.resize((s, s), Image.LANCZOS) for s in UI_SIZES]))

    for index, design in enumerate(DESIGNS):
        seed = 1000 + index * 37
        plate = make_badge(None, design, RAPID_FIRE_COLOUR, seed)
        belt_badge = make_badge(belt, design, RAPID_FIRE_COLOUR, seed)
        light_badge = make_badge(light, design, LIGHT_CLASS_COLOUR, seed + 500)
        plate.save(OUT / "plates" / f"{design}.png")
        belt_badge.save(OUT / "badges" / f"AutoCannon_{design}.png")
        light_badge.save(OUT / "badges" / f"LightTank_{design}.png")
        picture = portrait_with(light_badge, belt_badge)
        picture.save(OUT / "portraits" / f"PzII_F_{design}.png")
        rows.append((design.replace("_", " "), [plate, belt_badge, picture] +
                     [picture.resize((s, s), Image.LANCZOS) for s in UI_SIZES]))
        print(f"design {design}")

    worn_belt = make_badge(WORN_ICON_SET / RAPID_FIRE_ICON, "enamel_chipped", RAPID_FIRE_COLOUR, 1000)
    worn_light = make_badge(WORN_ICON_SET / LIGHT_TANK_ICON, "enamel_chipped", LIGHT_CLASS_COLOUR, 1500)
    worn_picture = portrait_with(worn_light, worn_belt)
    worn_picture.save(OUT / "portraits" / "PzII_F_enamel_chipped_worn_icons.png")
    rows.append(("enamel + worn icons", [None, worn_belt, worn_picture] +
                 [worn_picture.resize((s, s), Image.LANCZOS) for s in UI_SIZES]))

    palette_rows = []
    for index, design in enumerate(DESIGNS):
        palette_rows.append((design.replace("_", " "),
                             [make_badge(belt, design, colour, 2000 + index * 11 + c, size=128)
                              for c, colour in enumerate(PALETTE.values())]))
    build_sheet(rows, palette_rows, OUT / "TacticalIconBackgrounds_sheet.png")
    print(f"sheet {OUT / 'TacticalIconBackgrounds_sheet.png'}")


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("preview")
    badge_parser = sub.add_parser("badge")
    badge_parser.add_argument("icon", type=Path)
    badge_parser.add_argument("design", choices=list(DESIGNS))
    badge_parser.add_argument("colour", help="hex, e.g. ba741e")
    badge_parser.add_argument("out", type=Path)
    badge_parser.add_argument("--seed", type=int, default=1000)
    badge_parser.add_argument("--size", type=int, default=BADGE_SIZE)
    args = parser.parse_args()
    if args.command == "preview":
        preview()
        return
    colour = tuple(int(args.colour.lstrip("#")[i:i + 2], 16) for i in (0, 2, 4))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    make_badge(args.icon, args.design, colour, args.seed, args.size).save(args.out)


if __name__ == "__main__":
    main()
