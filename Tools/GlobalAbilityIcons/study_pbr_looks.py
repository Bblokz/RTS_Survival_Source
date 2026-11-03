"""Look study: six ways to make the global ability icons read as rendered instead of flat, on two abilities.
Every strategy keeps the shipped layout (compose_ability_icons) and changes one thing; the shipped tools are only
imported. Renders come from study_render.py (Saved/GlobalAbilityIcons/study/renders_*).

Usage: python study_pbr_looks.py      writes Saved/GlobalAbilityIcons/study/<strategy>_<icon>.png and study_sheet.png
"""
import json
import math
import os
import shutil

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

import compose_ability_icons as c

ROOT = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/"
STUDY = ROOT + "study/"
BASE_RENDERS = ROOT + "renders/"
JOBS = ["Ju87_Dive", "PE8_Level", "T3476", "T3485"]
FACTION_OF_JOB = {"Ju87_Dive": "ger", "PE8_Level": "sov", "T3476": "sov", "T3485": "sov"}

ICONS = {
    "3xJu87Strafing": lambda: c.ju87_strafe(3),
    "SovietT34Drop": lambda: c.tank_drop("sov", "PE8_Level", ("T3476", "T3476", "T3485"), c.PE8_THREE, 59),
}

# screen-space light per faction: x right, y up, z towards the viewer. The sun sits low behind the subject on the
# left, where the warm glow of the icon sky is; the sky colour fills from above, the ground bounces warm from below.
LIGHT = {
    "ger": dict(sun=(-0.55, 0.35, -0.45), sun_rgb=(1.0, 0.80, 0.56), sky_rgb=(0.52, 0.64, 0.78), ground_rgb=(0.55, 0.45, 0.32)),
    "sov": dict(sun=(-0.55, 0.30, -0.45), sun_rgb=(1.0, 0.70, 0.45), sky_rgb=(0.72, 0.52, 0.46), ground_rgb=(0.50, 0.38, 0.26)),
}


def norm(v):
    v = np.array(v, np.float32)
    return v / np.linalg.norm(v)


def read_rgb(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.float32) / 255


def blur(a, radius):
    """Gaussian blur of a float array (2D or RGB/RGBA), radius in pixels like PIL's."""
    a = np.ascontiguousarray(a, np.float32)
    return cv2.GaussianBlur(a, (0, 0), sigmaX=max(radius, 0.1), borderType=cv2.BORDER_REFLECT)


def write_renders(folder, job, color, src_folder=BASE_RENDERS):
    """A render folder the composer can read: new colour, the original mask and points."""
    os.makedirs(folder, exist_ok=True)
    Image.fromarray((np.clip(color, 0, 1) * 255).astype(np.uint8)).save(folder + job + "_color.png")
    for suffix in ("_mask.png", "_points.json"):
        shutil.copy(src_folder + job + suffix, folder + job + suffix)


# --- strategy 3: G-buffer relight ------------------------------------------------------------------------------------

def screen_normals(job):
    g = STUDY + "renders_gbuffer/"
    world = read_rgb(g + job + "_normal.png") * 2 - 1
    cam = json.load(open(g + job + "_camera.json"))
    right, up, fwd = (np.array(cam[k], np.float32) for k in ("right", "up", "forward"))
    n = np.dstack([world @ right, world @ up, -(world @ fwd)])
    return n / (np.linalg.norm(n, axis=2, keepdims=True) + 1e-6)


def gbuffer_relight(job):
    color = read_rgb(BASE_RENDERS + job + "_color.png")
    alpha = 1 - np.asarray(Image.open(BASE_RENDERS + job + "_mask.png").convert("RGBA"))[..., 3] / 255.0
    n = screen_normals(job)
    light = LIGHT[FACTION_OF_JOB[job]]
    sun = norm(light["sun"])
    sun_rgb, sky_rgb, ground_rgb = (np.array(light[k], np.float32) for k in ("sun_rgb", "sky_rgb", "ground_rgb"))
    n_dot_l = np.clip(n @ sun, 0, 1)[..., None]
    up = np.clip(n[..., 1], -1, 1)[..., None]
    facing = np.clip(n[..., 2], 0, 1)[..., None]
    # shading: the render keeps its own key; the hemisphere and the sun push form into it
    hemi = sky_rgb * np.clip(up, 0, 1) + ground_rgb * np.clip(-up, 0, 1) * 0.7 + 0.35
    relit = color * (0.62 + 0.55 * hemi * 0.8) + color * sun_rgb * n_dot_l * 0.55
    # warm specular on the paint from the sun, as on the lacquered metal of the real thing
    half = norm(sun + np.array([0, 0, 1], np.float32))
    spec = np.clip(n @ half, 0, 1) ** 36
    relit += sun_rgb * spec[..., None] * 0.45
    # fresnel: glancing edges pick up the sky behind
    fresnel = (1 - facing) ** 3
    relit += sky_rgb * fresnel * 0.35 * np.clip(up + 0.6, 0, 1)
    # cavity: creases where the normal turns fast go darker
    cavity = np.linalg.norm(n - blur(n, 3), axis=2)
    relit *= (1 - np.clip(cavity * 1.6, 0, 0.45))[..., None]
    return np.where(alpha[..., None] > 0.01, relit, color)


# --- strategy 1: the relit renders, brought to the shipped render's brightness so only the light direction differs

def matched_relight(job):
    relit = read_rgb(STUDY + "renders_relight/" + job + "_color.png")
    base = read_rgb(BASE_RENDERS + job + "_color.png")
    alpha = 1 - np.asarray(Image.open(BASE_RENDERS + job + "_mask.png").convert("RGBA"))[..., 3] / 255.0
    inside = alpha > 0.5
    gain = base[inside].mean() / max(relit[inside].mean(), 1e-3)
    return relit * min(gain, 2.5)


# --- strategy 4: photographic finishing ----------------------------------------------------------------------------

def clarity_and_ao(job):
    """Local contrast on the render, and contact shade where parts of the model meet (inner alpha distance)."""
    color = read_rgb(BASE_RENDERS + job + "_color.png")
    alpha = 1 - np.asarray(Image.open(BASE_RENDERS + job + "_mask.png").convert("RGBA"))[..., 3] / 255.0
    luma = color.mean(axis=2)
    local = luma - blur(luma, 12)
    out = color * (1 + np.clip(local * 1.8, -0.35, 0.35))[..., None]
    # occlusion: thin, crowded parts (gear, wheels, tracks) get darker than broad panels
    occlusion = 1 - blur(alpha, 18)
    out *= (1 - np.clip(occlusion * 0.55, 0, 0.35))[..., None]
    return out


def photographic_finish(img):
    """Applied to the composed card before the frame: tone curve, bloom on the hot parts, vignette, grain."""
    rgb = img[..., :3]
    # filmic S-curve with a little warmth in the highlights and cool in the shadows
    luma = rgb.mean(axis=2, keepdims=True)
    curved = rgb * rgb * (3 - 2 * rgb) * 0.65 + rgb * 0.35
    curved = curved + (np.array([0.03, 0.01, -0.02]) * luma) - np.array([0.0, 0.0, -0.015]) * (1 - luma)
    bright = np.clip(rgb.max(axis=2, keepdims=True) - 0.78, 0, 1) / 0.22 * rgb
    bloom = blur(bright, 18) * 0.8 + blur(bright, 5) * 0.5
    out = np.clip(curved + bloom, 0, 1)
    v = ((c.xs - c.CANVAS / 2) ** 2 + (c.ys - c.CANVAS / 2) ** 2) / (c.CANVAS / 2) ** 2
    out *= (1 - 0.22 * np.clip(v - 0.35, 0, 1))[..., None]
    grain = np.random.default_rng(3).normal(0, 0.012, out.shape[:2])[..., None]
    img = img.copy()
    img[..., :3] = np.clip(out + grain, 0, 1)
    return img


# --- strategy 5: lit backdrop ---------------------------------------------------------------------------------------

BASE_BACKGROUND = c.background
SUN_SCREEN = (0.16, 0.66)          # where the low sun sits on the card, as a fraction of its size


def lit_backdrop(faction, seed, ground=True):
    """The shipped gradient with lit cumulus in front of a low sun: bright sun-side rims, shaded bellies, rays."""
    img = BASE_BACKGROUND(faction, seed, ground)
    top, mid, glow, far, near = [np.array(col, np.float32) / 255 for col in c.PALETTES[faction]]
    size = c.CANVAS
    sun = np.array(SUN_SCREEN) * size
    rng = np.random.default_rng(seed + 9)
    clouds = np.zeros((size, size), np.float32)
    lit = np.zeros((size, size), np.float32)
    v = c.ys / size
    horizon = c.HORIZON if ground else 1.0
    for layer in range(3):
        height = c.noise(seed + 20 + layer, size * (0.55 - 0.12 * layer), 5)
        squash = np.asarray(Image.fromarray((height * 255).astype(np.uint8)).resize((size, size // 2)).resize(
            (size, size), Image.BICUBIC)).astype(np.float32) / 255
        band = c.smooth(0.15 + 0.18 * layer, 0.45 + 0.15 * layer, v) * (1 - c.smooth(horizon - 0.08, horizon, v))
        mask = c.smooth(0.55, 0.68, squash) * band
        # light from the sun: compare the cloud height with itself shifted towards the sun
        shift = (sun - np.array([size / 2, size / 2])) / size * 18
        towards = np.roll(np.roll(squash, int(shift[1]), axis=0), int(shift[0]), axis=1)
        lit_side = np.clip((squash - towards) * 9 + 0.5, 0, 1)
        clouds = np.maximum(clouds, mask * (0.55 - 0.12 * layer))
        lit = np.maximum(lit, mask * lit_side)
    cloud_dark = mid * 0.7 + top * 0.3
    cloud_lit = glow * 0.85 + 0.15
    cloud_rgb = cloud_dark[None, None] * (1 - lit[..., None]) + cloud_rgb_lit(cloud_lit) * lit[..., None]
    img[..., :3] = img[..., :3] * (1 - clouds[..., None]) + cloud_rgb * clouds[..., None]
    # the sun and its glow, and rays where the clouds part
    d = np.sqrt((c.xs - sun[0]) ** 2 + (c.ys - sun[1]) ** 2) / size
    img[..., :3] += glow * (np.exp(-(d / 0.035) ** 2) * 0.9 + np.exp(-(d / 0.22) ** 2) * 0.28)[..., None]
    angle = np.arctan2(c.ys - sun[1], c.xs - sun[0])
    rays = (0.5 + 0.5 * np.sin(angle * 23 + seed)) ** 6 * np.exp(-d / 0.45) * (1 - clouds)
    img[..., :3] += glow * (rays * 0.12)[..., None]
    img[..., :3] = np.clip(img[..., :3], 0, 1)
    return img


def cloud_rgb_lit(col):
    return np.array(col, np.float32)[None, None]


def sun_rim(job):
    """Rim light along the sun-side edges of the cut-out, from the alpha gradient: integration with the lit sky."""
    color = read_rgb(BASE_RENDERS + job + "_color.png")
    alpha = 1 - np.asarray(Image.open(BASE_RENDERS + job + "_mask.png").convert("RGBA"))[..., 3] / 255.0
    soft = blur(alpha, 4)
    gx = np.gradient(soft, axis=1)
    gy = np.gradient(soft, axis=0)
    # edges whose outside faces the sun (up and to the left)
    towards_sun = np.clip(gx * 0.8 + gy * 0.6, 0, None) * 45
    rim = np.clip(towards_sun, 0, 1) * alpha
    light = LIGHT[FACTION_OF_JOB[job]]
    return color + np.array(light["sun_rgb"], np.float32) * rim[..., None] * 0.75


# --- strategy 6: lens and depth -------------------------------------------------------------------------------------

BASE_PLACE = c.place


def place_with_focus(pre, points, anchor, at, scale, rotate=0.0, gain=1.0, tint=None, tint_amount=0.0):
    """Planes and tanks further back (tinted towards the sky) are out of focus; the lead stays sharp."""
    layer, to_canvas = BASE_PLACE(pre, points, anchor, at, scale, rotate, gain, tint, tint_amount)
    if tint_amount > 0.05:
        layer = blur(layer, 1.0 + tint_amount * 6.0)
    return layer, to_canvas


def lens_finish(img):
    rgb = img[..., :3]
    bright = np.clip(rgb.max(axis=2, keepdims=True) - 0.8, 0, 1) / 0.2 * rgb
    rgb = np.clip(rgb + blur(bright, 14) * 0.7, 0, 1)
    # a little lateral colour towards the corners, as a real lens has
    shifted = rgb.copy()
    shifted[..., 0] = np.asarray(Image.fromarray((rgb[..., 0] * 255).astype(np.uint8)).resize(
        (int(c.CANVAS * 1.004),) * 2, Image.BICUBIC).crop((2, 2, c.CANVAS + 2, c.CANVAS + 2))) / 255
    img = img.copy()
    img[..., :3] = shifted
    return img


# --- strategy 2: the whole picture rendered in engine -------------------------------------------------------------

def scene_icon(name):
    folder = STUDY + "renders_scene/"
    scene = "Scene_Ju87" if name == "3xJu87Strafing" else "Scene_T34Drop"
    shot = read_rgb(folder + scene + "_color.png")
    rows = json.load(open(folder + scene + "_points.json"))
    zoom = 1.0 if name == "3xJu87Strafing" else 1.06
    size = c.CANVAS
    src = Image.fromarray((shot * 255).astype(np.uint8)).resize((int(size * zoom),) * 2, Image.LANCZOS)
    off = (src.width - size) // 2
    img = np.zeros((size, size, 4), np.float32)
    img[..., :3] = np.asarray(src.crop((off, off, off + size, off + size))).astype(np.float32) / 255
    img[..., 3] = 1
    k = size * zoom / shot.shape[0]

    def to_canvas(p):
        return np.array(p) * k - off

    if name == "3xJu87Strafing":
        for i, row in enumerate(rows):
            lead = i == len(rows) - 1
            o, f = to_canvas(row["origin"]), to_canvas(row["forward"])
            direction = (f - o) / np.linalg.norm(f - o)
            muzzles = [to_canvas(row[g]) for g in ("W1", "W2") if g in row]
            img = c.strafe_run(img, muzzles, direction, (220 if lead else 140) * c.SS, (14 if lead else 10) * c.SS,
                               (3.0 if lead else 2.2) * c.SS, 4 if lead else 3)
    else:
        for plane_row, tank_row in zip(rows[0::2], rows[1::2]):
            top, bottom = to_canvas(plane_row["origin"]), to_canvas(tank_row["origin"])
            gap = bottom - top
            span = abs(gap[1]) * 0.55

            def draw_cables(draw):
                for d in (-1, 1):
                    draw.line([tuple(top + gap * 0.22 + [d * span * 0.5, 0]), tuple(bottom - gap * 0.18 + [d * span, 0])],
                              fill=255, width=3)

            cables = c.drawn(draw_cables, blur=0.7)
            layer = np.zeros_like(img)
            layer[..., :3] = 0.08 * cables[..., None]
            layer[..., 3] = cables * 0.8
            img = c.over(img, layer)
    return c.finish(img)


# --- running the strategies ------------------------------------------------------------------------------------------

def preprocessed(folder, fn):
    for job in JOBS:
        write_renders(STUDY + folder + "/", job, fn(job))
    return STUDY + folder + "/"


def compose(name, renders=BASE_RENDERS, background=None, finish=None, place=None):
    saved = c.RENDERS, c.background, c.finish, c.place
    base_finish = c.finish
    try:
        c.RENDERS = renders
        if background:
            c.background = background
        if finish:
            c.finish = lambda img: base_finish(finish(img))
        if place:
            c.place = place
        return ICONS[name]()
    finally:
        c.RENDERS, c.background, c.finish, c.place = saved


STRATEGIES = [
    ("baseline", "Shipped icon", "flat studio renders on a painted gradient",
     lambda name: Image.open(ROOT + "out/T_GA_%s.png" % name).convert("RGBA")),
    ("relight", "1  Sun and sky relight", "re-rendered under the map's real sun, sky light and a warm rim",
     lambda name: compose(name, RELIT_1)),
    ("scene", "2  Whole scene in engine", "sky, clouds, sun and Lumen rendered together, tracers drawn; layout is the camera's, not the icon's",
     scene_icon),
    ("gbuffer", "3  G-buffer relight", "normal pass: sun spec, fresnel sky edges, hemisphere fill, cavity",
     lambda name: compose(name, RELIT_3)),
    ("photo", "4  Photographic finish", "clarity, contact shade, filmic curve, bloom, vignette, grain",
     lambda name: compose(name, RELIT_4, finish=photographic_finish)),
    ("backdrop", "5  Lit backdrop", "lit cumulus, low sun, rays; sun-side rim on the models",
     lambda name: compose(name, RELIT_5, background=lit_backdrop)),
    ("lens", "6  Lens and depth", "wide lens close up (perspective), focus falloff, bloom",
     lambda name: compose(name, STUDY + "renders_lens/", finish=lens_finish, place=place_with_focus)),
]


def label_font(size):
    for path in ("C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/arialbd.ttf"):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def main():
    global RELIT_1, RELIT_3, RELIT_4, RELIT_5
    RELIT_1 = preprocessed("renders_relit1", matched_relight)
    RELIT_3 = preprocessed("renders_relit3", gbuffer_relight)
    RELIT_4 = preprocessed("renders_relit4", clarity_and_ao)
    RELIT_5 = preprocessed("renders_relit5", sun_rim)
    results = {}
    for key, _, _, fn in STRATEGIES:
        for name in ICONS:
            icon = fn(name)
            icon.save(STUDY + "%s_%s.png" % (key, name))
            results[(key, name)] = icon
        print("STRATEGY", key)
    col, big, small = 330, 320, 96
    head = 80
    sheet = Image.new("RGBA", (len(STRATEGIES) * col + 10, head + 2 * (big + 10) + small + 30), (26, 26, 28, 255))
    draw = ImageDraw.Draw(sheet)
    title, sub = label_font(17), label_font(12)
    for i, (key, label, line, _) in enumerate(STRATEGIES):
        x = 10 + i * col
        draw.text((x, 10), label, fill=(240, 236, 228), font=title)
        words, rows = line.split(), [""]
        for word in words:
            if draw.textlength(rows[-1] + " " + word, font=sub) > col - 14:
                rows.append(word)
            else:
                rows[-1] = (rows[-1] + " " + word).strip()
        for r, text in enumerate(rows[:3]):
            draw.text((x, 34 + r * 14), text, fill=(170, 170, 170), font=sub)
        for j, name in enumerate(ICONS):
            icon = results[(key, name)]
            sheet.alpha_composite(icon.resize((big, big), Image.LANCZOS), (x, head + j * (big + 10)))
            sheet.alpha_composite(icon.resize((small, small), Image.LANCZOS),
                                  (x + j * (small + 12), head + 2 * (big + 10) + 10))
    sheet.save(STUDY + "study_sheet.png")
    print("SHEET", STUDY + "study_sheet.png")


if __name__ == "__main__":
    main()
