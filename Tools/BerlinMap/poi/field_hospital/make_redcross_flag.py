"""Red Cross banner texture for the Berlin field hospital flag (T_FactionFlag_RedCross, 512 x 512).

The banner plane (SM_Rus_SovietBanner) is 1016 x 604 cm and maps the square texture over it, so the cloth is drawn at
that aspect and squeezed to 512 x 512: the cross reads square on the flag. Thirty years in the ruins: grey-yellow
cloth, faded red, soot along the edges and the bottom, faint stains.
Usage: python make_redcross_flag.py <out.png>
"""
import sys

import numpy as np
from PIL import Image, ImageFilter

W, H = 1016, 604
rng = np.random.default_rng(1976)


def noise(scale, octaves=4):
    out = np.zeros((H, W))
    for o in range(octaves):
        s = max(2, int(scale / 2 ** o))
        small = rng.random((H // s + 2, W // s + 2))
        img = Image.fromarray((small * 255).astype(np.uint8)).resize((W + 2 * s, H + 2 * s), Image.BICUBIC)
        out += np.asarray(img, dtype=float)[s:s + H, s:s + W] / 255 / 2 ** o
    return out / sum(1 / 2 ** o for o in range(octaves))


y, x = np.mgrid[0:H, 0:W]
cloth = np.array([214, 206, 184], float)
img = np.ones((H, W, 3)) * cloth
weave = (np.sin(x * 1.9) * np.sin(y * 1.9)) * 4
img += weave[..., None]
img *= (0.86 + 0.22 * noise(120))[..., None]

# The cross: arms one fifth of the height, a square cross centred on the cloth.
arm, span = H * 0.2, H * 0.62
cx, cy = W / 2, H / 2
cross = ((abs(x - cx) < arm / 2) & (abs(y - cy) < span / 2)) | ((abs(y - cy) < arm / 2) & (abs(x - cx) < span / 2))
mask = Image.fromarray((cross * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.6))
m = np.asarray(mask, float)[..., None] / 255
red = np.array([158, 28, 26], float) * (0.82 + 0.3 * noise(60))[..., None]
img = img * (1 - m) + red * m

# Grime: stains, soot rising from the bottom edge, darker fly end (x = W) and hoist.
stains = np.clip((noise(50) - 0.62) * 2.5, 0, 1)
edge = np.minimum(np.minimum(x, W - 1 - x), np.minimum(y, H - 1 - y)) / 40.0
soot = np.clip(((y / H) ** 4) * 0.8 + np.clip(1 - edge, 0, 1) * 0.5, 0, 1) * (0.6 + 0.4 * noise(30))
img *= (1 - 0.14 * stains - 0.4 * soot)[..., None]
img = np.clip(img, 0, 255)

out = Image.fromarray(img.astype(np.uint8)).resize((512, 512), Image.LANCZOS)
out.save(sys.argv[1])
print('wrote', sys.argv[1])
