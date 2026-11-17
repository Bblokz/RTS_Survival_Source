"""Supply point trim sheet (wear pass, 2026-11-17): a copy of the Breakthrough trim layout (T_BF_Trim_*) whose timber
and drum rows are replaced by worn, weathered PBR content. The supply point base keeps its UVs; only its trim slot
changes material (M_SupplyPoint_Timber). Rows in Unreal V (image rows from the top), from generate_supply_point.py:
    void 0.0-0.1 | wood 0.1-0.3 (sleepers, planks, posts, cribs) | rail 0.3-0.5 (oil drums) | steel 0.5-1.0 (unused)
Inputs: the BF trim PNGs on disk, plus .hdr exports of project textures made by ue_export_wear_candidates.py and
ue_export_wear_maps.py (Saved/SupplyPointWear/tex):
    wood  = MilitaryAirport T_Planks_BC / _N / _DRO, one board turned so the grain runs along U
    drums = Megascans Rusty_Metal_vmghfhro D / N / ORDp, its blue paint islands repainted olive drab
Outputs: Textures/T_SupplyPoint_Trim_BaseColor.png (sRGB), _NormalDX.png, _ORM.png and a preview.
    python compose_trim.py
"""
import os

import cv2
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__)).replace('\\', '/')
TEX = 'D:/UE5Projects/RTS_Survival/Saved/SupplyPointWear/tex/'
BF = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/SharedParts/Textures/'
OUT = HERE + '/Textures/'
SIZE = 2048
WOOD = (int(0.1 * SIZE), int(0.3 * SIZE))
RAIL = (int(0.3 * SIZE), int(0.5 * SIZE))
# T_Planks board seams (columns, found from the occlusion channel); the widest clean board is used
BOARD = (1426 + 9, 1776 - 9)
RNG = np.random.default_rng(1117)


def hdr(name):
    image = cv2.imread(TEX + name + '.hdr', cv2.IMREAD_UNCHANGED)
    return cv2.cvtColor(image, cv2.COLOR_BGR2RGB).astype(np.float32)


def png(name):
    return np.asarray(Image.open(BF + name)).astype(np.float32) / 255.0


def to_srgb(linear):
    linear = np.clip(linear, 0.0, 1.0)
    return np.where(linear <= 0.0031308, linear * 12.92, 1.055 * np.power(linear, 1 / 2.4) - 0.055)


def to_linear(srgb):
    return np.where(srgb <= 0.04045, srgb / 12.92, np.power((srgb + 0.055) / 1.055, 2.4))


def periodic_noise(h, w, scale, seed, wrap_v=True, stretch_u=1.0):
    """Smooth noise in [0, 1] that tiles along U (and V when wrap_v); scale = feature size in px, stretch_u > 1 makes
    the features longer along U (the grain)."""
    rng = np.random.default_rng(seed)
    hh = h if wrap_v else h * 2
    spec = rng.normal(size=(hh, w)) + 1j * rng.normal(size=(hh, w))
    fy = np.fft.fftfreq(hh)[:, None]
    fx = np.fft.fftfreq(w)[None, :] * stretch_u
    f = np.sqrt(fx ** 2 + fy ** 2) * scale
    spec *= np.exp(-f ** 2) * (f > 0)
    n = np.real(np.fft.ifft2(spec))[:h]
    n = (n - n.mean()) / (n.std() + 1e-6)
    return np.clip(0.5 + 0.18 * n, 0.0, 1.0)


def fbm(h, w, scale, seed, wrap_v=True, stretch_u=1.0):
    return 0.55 * periodic_noise(h, w, scale, seed, wrap_v, stretch_u) \
        + 0.3 * periodic_noise(h, w, scale / 2.3, seed + 1, wrap_v, stretch_u) \
        + 0.15 * periodic_noise(h, w, scale / 5.1, seed + 2, wrap_v, stretch_u)


def turn_normal_ccw(normal):
    """np.rot90 (counter-clockwise) of a DirectX normal map: the XY vector turns with the picture."""
    turned = np.rot90(normal, 1).copy()
    x = turned[..., 0] * 2 - 1
    y = turned[..., 1] * 2 - 1
    turned[..., 0] = (y + 1) / 2
    turned[..., 1] = (-x + 1) / 2
    return turned


def resize(image, w, h):
    return cv2.resize(image, (w, h), interpolation=cv2.INTER_AREA if image.shape[1] > w else cv2.INTER_CUBIC)


def wrap_x(image, shift):
    return np.roll(image, shift, axis=1)


# =====================================================================================================================
def wood_band():
    h = WOOD[1] - WOOD[0]
    a, b = BOARD
    colour = np.rot90(hdr('T_Planks_BC')[:, a:b], 1)
    normal = turn_normal_ccw(hdr('T_Planks_N')[:, a:b])
    dro = np.rot90(hdr('T_Planks_DRO')[:, a:b], 1)
    colour, normal, dro = (resize(np.ascontiguousarray(x), SIZE, h) for x in (colour, normal, dro))
    colour = to_srgb(colour)
    rough = dro[..., 1]
    ao = dro[..., 2]
    height = dro[..., 0]

    # grey-out: sun-bleached, silvered timber (keep a little of the brown)
    lum = colour @ np.array([0.3, 0.59, 0.11], np.float32)
    silver = fbm(h, SIZE, 200, 11, stretch_u=4.0)
    bleach = np.clip((silver - 0.45) * 2.2, 0, 1)[..., None]
    colour = colour * (1 - 0.55 * bleach) + (lum[..., None] * np.array([1.08, 1.06, 1.0]) + 0.05) * 0.55 * bleach
    # long tone drift along the board (tiles every 2048 px = 1.6 m)
    drift = fbm(h, SIZE, 520, 12)
    colour *= (0.78 + 0.45 * drift)[..., None]
    # water stains and mildew: dark, slightly green blotches with crisp-ish tide lines
    stain_field = fbm(h, SIZE, 120, 13, stretch_u=3.0)
    stain = np.clip((stain_field - 0.56) * 5.0, 0, 1)
    tide = np.clip(1 - np.abs(stain_field - 0.56) * 45, 0, 1) * 0.5
    mildew = np.array([0.16, 0.17, 0.13], np.float32)
    colour = colour * (1 - 0.6 * stain[..., None]) + mildew * 0.6 * stain[..., None]
    colour *= (1 - 0.18 * tide)[..., None]
    rough = np.clip(rough + 0.12 + 0.05 * stain, 0, 1)

    # checks: dark splits along the grain, with a groove in the normal map
    crack = np.zeros((h, SIZE), np.float32)
    for _ in range(15):
        y0 = RNG.uniform(14, h - 14)
        x0 = RNG.uniform(0, SIZE)
        length = RNG.uniform(120, 900)
        width = RNG.uniform(1.0, 4.5)
        depth = RNG.uniform(0.35, 1.0)
        xs = np.arange(int(length))
        t = xs / max(length - 1, 1)
        ys = y0 + np.cumsum(RNG.normal(0, 0.18, xs.size))
        taper = np.sin(np.pi * t) ** 0.6
        for x, y, k in zip(xs, ys, taper):
            col = int(x0 + x) % SIZE
            r = max(width * k, 0.6)
            lo, hi = int(max(0, y - r - 1)), int(min(h, y + r + 2))
            yy = np.arange(lo, hi)
            crack[lo:hi, col] = np.maximum(crack[lo:hi, col], depth * np.clip(1 - np.abs(yy - y) / r, 0, 1))
    crack = cv2.GaussianBlur(crack, (3, 3), 0.6)
    colour *= (1 - 0.75 * crack)[..., None]
    ao *= 1 - 0.6 * crack
    rough = np.maximum(rough, crack * 0.95)
    grad_y = cv2.Sobel(crack, cv2.CV_32F, 0, 1, ksize=3) * 0.35
    normal[..., 1] = np.clip(normal[..., 1] + grad_y, 0, 1)

    # nails: dark heads with a rust bleed that runs along the grain
    rust = np.array([0.30, 0.17, 0.10], np.float32)
    breakup = fbm(h, SIZE, 18, 14)
    yy, xx = np.mgrid[0:h, 0:SIZE]
    for _ in range(7):
        cx, cy = RNG.uniform(0, SIZE), RNG.uniform(60, h - 60)
        dx = (xx - cx + SIZE / 2) % SIZE - SIZE / 2
        dy = yy - cy
        head = np.clip(1 - np.hypot(dx, dy) / 6.5, 0, 1)
        bleed = np.exp(-(dy / RNG.uniform(9, 16)) ** 2 - (np.abs(dx) / RNG.uniform(25, 70)) ** 1.4) * 0.5
        bleed *= np.clip((breakup - 0.3) * 2.5, 0, 1)
        colour = colour * (1 - bleed[..., None]) + rust * bleed[..., None]
        colour = colour * (1 - head[..., None]) + 0.05 * head[..., None]
        rough = np.where(head > 0.2, 0.55, rough)
    return np.clip(colour, 0, 1), np.clip(normal, 0, 1), np.stack([np.clip(ao, 0, 1), np.clip(rough, 0, 1),
                                                                   np.zeros_like(ao)], -1)


def rail_band():
    """Drums: u = drum height (0.88 m = 0.55 units), v wraps the 1.82 m girth into the band, so the content is packed
    6x along V to read square on the drum."""
    h = RAIL[1] - RAIL[0]
    d = hdr('T_Rusty_Metal_vmghfhro_4K_D')
    n = hdr('T_Rusty_Metal_vmghfhro_4K_N')
    ordp = hdr('T_RustyMetal_vmghfhro_4K_ORDp')
    src_px_per_m = 4096 / 2.0                 # Megascans surface: 2 m tile
    w_src = 4096                              # the whole 2 m tile across one 1.6 m UV unit, so U still tiles
    h_src = int(1.82 * src_px_per_m)
    def take(image):
        tiled = np.concatenate([image, image], 0)[:h_src]
        tiled = np.concatenate([tiled, tiled], 1)[:, :w_src]
        return resize(np.ascontiguousarray(tiled), SIZE, h)
    colour, normal, packed = take(to_srgb(d)), take(n), take(ordp)
    # blue paint islands -> worn olive-drab drum paint; the rust stays
    blue = colour[..., 2] - 0.5 * (colour[..., 0] + colour[..., 1])
    paint = np.clip((blue + 0.01) * 14.0, 0, 1)[..., None]
    lum = (colour @ np.array([0.3, 0.59, 0.11], np.float32))[..., None]
    olive = np.array([0.29, 0.30, 0.24], np.float32) * (0.5 + 1.0 * lum)
    colour = colour * (1 - paint) + olive * paint
    ao, rough = packed[..., 0], packed[..., 1]
    return np.clip(colour, 0, 1), np.clip(normal, 0, 1), np.stack([np.clip(ao, 0, 1), np.clip(rough, 0, 1),
                                                                   np.zeros_like(ao)], -1)


def main():
    os.makedirs(OUT, exist_ok=True)
    base = png('T_BF_Trim_BaseColor.png')
    normal = png('T_BF_Trim_NormalDX.png')
    orm = png('T_BF_Trim_ORM.png')
    for (lo, hi), band in ((WOOD, wood_band), (RAIL, rail_band)):
        c, n, o = band()
        base[lo:hi], normal[lo:hi], orm[lo:hi] = c, n, o
    for name, image in (('BaseColor', base), ('NormalDX', normal), ('ORM', orm)):
        Image.fromarray((np.clip(image, 0, 1) * 255 + 0.5).astype(np.uint8)).save(OUT + f'T_SupplyPoint_Trim_{name}.png')
    preview = np.concatenate([base[:1024], np.concatenate([normal[WOOD[0]:RAIL[1]], orm[WOOD[0]:RAIL[1]]], 0)[:1024]], 1)
    Image.fromarray((np.clip(preview, 0, 1) * 255).astype(np.uint8)).resize((2048, 512)).save(OUT + 'T_SupplyPoint_Trim_Preview.jpg')
    print('trim written to', OUT)


if __name__ == '__main__':
    main()
