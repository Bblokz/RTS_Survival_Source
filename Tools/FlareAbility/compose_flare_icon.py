"""Composes the flare ability card icons (180x180, like the other aim ability icons):
  T_AimAbility_Flare.png         hex plate + flare glow + rendered parachute flare + the aim crosshair
  T_Cancel_AimAbility_Flare.png  the same with the red cancel cross
The plate is T_AimAbility_MortarBarrage (exported to <work>/ref) with its picture painted over; the crosshair and
the cancel cross are drawn here in the colours of the other aim icons; the flare comes from (KADO-3D)/Sdkfz-231/FlareAbility/render_flare_icon.py.
Usage: python compose_flare_icon.py <work folder>"""
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

WORK = sys.argv[1].rstrip("/") + "/"
SIZE = 180
mortar = np.asarray(Image.open(WORK + "ref/T_AimAbility_MortarBarrage.png").convert("RGBA")).astype(np.float32)

ys, xs = np.mgrid[0:SIZE, 0:SIZE]
cx = cy = (SIZE - 1) / 2.0


PLATE_RADIUS = 88.0     # flat-top hexagon, corners left and right


def hex_distance(x, y):
    """1 on the outline of the plate's flat-top hexagon, below 1 inside."""
    dx, dy = np.abs(x - cx), np.abs(y - cy)
    return np.maximum(dy / (PLATE_RADIUS * 0.8660254), (1.7320508 * dx + dy) / (1.7320508 * PLATE_RADIUS))


inner = hex_distance(xs, ys) < 0.9
# Plate: the mortar icon with its picture painted over by the plate's own translucent dark red.
plate = mortar.copy()
fill = np.array([88, 6, 5, 180], np.float32)
plate[inner] = fill

SUPER = 4


def drawn_layer(draw_fn, color, glow_radius):
    """Draws at 4x, adds a soft glow of the same colour and returns a float RGBA layer."""
    big = Image.new("RGBA", (SIZE * SUPER, SIZE * SUPER), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(big), SUPER)
    sharp = big.resize((SIZE, SIZE), Image.LANCZOS)
    glow = sharp.filter(ImageFilter.GaussianBlur(glow_radius))
    layer = np.asarray(glow).astype(np.float32)
    layer[..., 3] *= 0.8
    sharp_a = np.asarray(sharp).astype(np.float32)
    a = sharp_a[..., 3:4] / 255.0
    layer[..., :3] = sharp_a[..., :3] * a + layer[..., :3] * (1 - a)
    layer[..., 3] = np.maximum(layer[..., 3], sharp_a[..., 3])
    layer[..., :3] = color
    return layer


def crosshair_marks(draw, k):
    # the aim crosshair of the other aim icons: two ellipses and a cross, low on the plate
    c = (255, 150, 45, 255)
    x0, y0 = 90 * k, 136 * k
    draw.ellipse([x0 - 44 * k, y0 - 12 * k, x0 + 44 * k, y0 + 12 * k], outline=c, width=3 * k)
    draw.ellipse([x0 - 19 * k, y0 - 6 * k, x0 + 19 * k, y0 + 6 * k], outline=c, width=3 * k)
    draw.line([x0 - 59 * k, y0, x0 + 59 * k, y0], fill=c, width=3 * k)
    draw.line([x0, y0 - 22 * k, x0, y0 + 26 * k], fill=c, width=3 * k)


def cancel_cross(draw, k):
    c = (255, 30, 55, 255)
    draw.line([46 * k, 70 * k, 134 * k, 152 * k], fill=c, width=17 * k)
    draw.line([134 * k, 70 * k, 46 * k, 152 * k], fill=c, width=17 * k)


crosshair = drawn_layer(crosshair_marks, (255, 165, 60), 2.5)
cross = drawn_layer(cancel_cross, (255, 30, 55), 2.5)

FLARE_SIZE, FLARE_X, FLARE_Y = 132, 24, 16
flare = Image.open(WORK + "icon_flare_render.png").convert("RGBA").resize((FLARE_SIZE, FLARE_SIZE), Image.LANCZOS)
star_y = None
alpha = np.asarray(flare)[..., 3]
lum = np.asarray(flare.convert("L")).astype(np.float32)
bright = (lum > 245) & (alpha > 200)
if bright.any():
    by, bx = np.nonzero(bright)
    star_x, star_y = int(bx.mean()) + FLARE_X, int(by.mean()) + FLARE_Y


def over(base, layer_rgba):
    a = layer_rgba[..., 3:4] / 255.0
    out = base.copy()
    out[..., :3] = layer_rgba[..., :3] * a + base[..., :3] * (1 - a)
    out[..., 3:4] = np.maximum(base[..., 3:4], layer_rgba[..., 3:4])
    return out


def glow_layer(x, y, radius, color, strength):
    d = np.sqrt((xs - x) ** 2 + (ys - y) ** 2) / radius
    a = np.clip(np.exp(-d * d * 2.2) * strength, 0, 1) * inner
    layer = np.zeros((SIZE, SIZE, 4), np.float32)
    layer[..., :3] = color
    layer[..., 3] = a * 255
    return layer


def compose(with_cross):
    img = plate.copy()
    if star_y is not None:
        img = over(img, glow_layer(star_x, star_y, 60, (255, 120, 40), 0.35))
    img = over(img, crosshair * np.array([1, 1, 1, 1], np.float32) * inner[..., None])
    layer = np.zeros((SIZE, SIZE, 4), np.float32)
    layer[FLARE_Y:FLARE_Y + FLARE_SIZE, FLARE_X:FLARE_X + FLARE_SIZE] = np.asarray(flare).astype(np.float32)
    layer[..., 3] *= inner
    img = over(img, layer)
    if star_y is not None:
        # the burning star blooms over the cartridge
        img = over(img, glow_layer(star_x, star_y, 16, (255, 240, 200), 1.0))
    if with_cross:
        img = over(img, cross * inner[..., None])
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGBA")


compose(False).save(WORK + "T_AimAbility_Flare.png")
compose(True).save(WORK + "T_Cancel_AimAbility_Flare.png")
print("ICONS", WORK + "T_AimAbility_Flare.png", WORK + "T_Cancel_AimAbility_Flare.png", "star", star_y)
