"""Masked-material textures for the Ore map SandStreak effect (NS_SandPush), replacing the pack's translucent ones.

    python Source/Tools/OreMountainsMap/ore_sandstreak_textures.py [--preview]

Inputs: the pack flipbooks exported by ore_sandstreak_survey.py / _survey2.py (Saved/OreMountains/sandstreak/survey).
Outputs (Saved/OreMountains/sandstreak/tex): linear RGBA PNGs, imported by ore_sandstreak_build.py.

Idea - stochastic coverage instead of blending. The material (M_SandStreak_Masked) computes the pack's own opacity
    c = pow(ParticleAlpha * D, DynParam.A) * pow(|N.V|, DotPower)        (D = the pack flipbook alpha, downsampled)
boosts it for the lower particle count,  c' = 1 - (1 - c)^R   (R = old live count / new live count, an MI scalar),
and draws the texel iff c' > n, where n is a clumped grain noise, uniform on 0..1, baked in the particle's own UV.
So the drawn fraction of an area is c', and overlapping sprites (unrelated noise: each has its own position, size
and rotation) cover 1 - prod(1 - c'_i): the same expected value translucent alpha accumulates to. Each screen pixel is
shaded once (masked = depth prepass clip + one base-pass shade), nothing is sorted or blended, and fades thin the
grains (c' drops below more of the noise) instead of popping. The noise travels with the sprite, so TSR reprojects
it with the particle's own velocity (no crawl from a separately panned pattern).

The noise is blurred white noise, rank-equalised to a uniform distribution: clumps of a few texels, about 2-4 screen
pixels at the RTS camera. Its mips use alpha-coverage scaling (threshold 0.5 on B) so far mips keep their contrast
instead of averaging to a flat 0.5.

Channel layout (sRGB off, BC7):
  T_SandStreak_Wave   512x256, 6x6 flipbook   R = D (pack T_Sandpush02C, mesh slot 0)  G = D (T_Sandpush01C, slot 1)
                                              B = noise                                 A = 1
  T_SandStreak_Puff   2048x2048, 6x6 flipbook R = D (pack T_C_Cam09, Smoke A)           G = D (T_C_Cam05, Smoke B)
                                              B = noise                                 A = 1
  T_SandStreak_Grain  32x32                   R = G = round grain  B = 0.5 (no grain noise)  A = 1
Shading is not stored: the material brightens the dense core from D itself.
"""
import os
import sys

os.environ['OPENCV_IO_ENABLE_OPENEXR'] = '1'
import cv2
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
SRC = os.path.join(ROOT, 'Saved', 'OreMountains', 'sandstreak', 'survey')
OUT = os.path.join(ROOT, 'Saved', 'OreMountains', 'sandstreak', 'tex')
os.makedirs(OUT, exist_ok=True)
RNG = np.random.default_rng(1945)

# Grain clump size (Gaussian sigma in texels before rank equalisation).
SIGMA_WAVE = 1.0      # wave frames: 85 x 42 texels over 1-3 m sheets (2-7 cm a texel)
SIGMA_PUFF = 1.1      # puff frames: 341 texels over 13-26 m sprites (4-8 cm a texel, 1-1.5 screen pixels)


def clumped_noise(h, w, sigma):
    n = RNG.random((h, w)).astype(np.float32)
    n = cv2.GaussianBlur(n, (0, 0), sigma, borderType=cv2.BORDER_REFLECT)
    order = np.argsort(n, axis=None)
    ranks = np.empty(order.size, np.float32)
    ranks[order] = (np.arange(order.size, dtype=np.float32) + 0.5) / order.size
    return ranks.reshape(h, w)


def load_png(name):
    a = np.asarray(Image.open(os.path.join(SRC, name + '.png')).convert('RGBA')).astype(np.float32) / 255.0
    return a[..., 3]


def load_exr(name):
    a = cv2.imread(os.path.join(SRC, name + '.exr'), cv2.IMREAD_UNCHANGED)   # BGRA float
    return np.clip(a[..., 3], 0, 1)


def build_pair(load, names, size, sigma):
    w, h = size
    dens = [np.clip(cv2.resize(load(n), (w, h), interpolation=cv2.INTER_AREA), 0, 1) for n in names]
    noise = clumped_noise(h, w, sigma)
    return np.stack([dens[0], dens[1], noise, np.ones_like(noise)], -1)


def save(rgba, name):
    Image.fromarray((np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, name + '.png'))


def grain():
    y, x = np.mgrid[0:32, 0:32].astype(np.float32)
    r = np.hypot(x - 15.5, y - 15.5) / 15.5
    d = np.clip((1.0 - r) * 3.0, 0, 1)                 # solid disc with a short soft rim for the mips
    half = np.full_like(d, 0.5)
    return np.stack([d, d, half, np.ones_like(d)], -1)


def grain_noise(size=256, sigma=0.7):
    """Tileable clumped noise for the GrainTX sampler (2026-11-16 round 2): the flipbook's own B noise made grains of
    10-30 screen pixels on the big smoke sprites, which boiled from frame to frame. This one tiles at an MI-set rate
    (NoiseTiling) on the particle UV, so the grain size is set per renderer. Blurred with wrap-around (3x3 tiling)."""
    n = RNG.random((size, size)).astype(np.float32)
    big = cv2.GaussianBlur(np.tile(n, (3, 3)), (0, 0), sigma, borderType=cv2.BORDER_REFLECT)[size:2 * size, size:2 * size]
    order = np.argsort(big, axis=None)
    ranks = np.empty(order.size, np.float32)
    ranks[order] = (np.arange(order.size, dtype=np.float32) + 0.5) / order.size
    r = ranks.reshape(size, size)
    return np.stack([r, r, r, np.ones_like(r)], -1)


def main():
    save(grain_noise(), 'T_SandStreak_GrainNoise')
    wave = build_pair(load_png, ['T_Sandpush02C', 'T_Sandpush01C'], (512, 256), SIGMA_WAVE)
    puff = build_pair(load_exr, ['T_C_Cam09', 'T_C_Cam05'], (2048, 2048), SIGMA_PUFF)
    save(wave, 'T_SandStreak_Wave')
    save(puff, 'T_SandStreak_Puff')
    save(grain(), 'T_SandStreak_Grain')
    for tag, t in (('wave', wave), ('puff', puff)):
        print(tag, 'mean D %.3f / %.3f  noise mean %.3f' % (t[..., 0].mean(), t[..., 1].mean(), t[..., 2].mean()))
    if '--preview' in sys.argv:
        preview(puff)


# ---------------------------------------------------------------------------------------------------------------
# Offline A/B preview: one streak's smoke sprites over an in-game ground shot (7.8 cm a pixel), translucent pack
# look (old counts, original alpha) against the masked look (new counts, threshold field). Coverage only, flat colour.
def frame(img, idx, n=6):
    h, w = img.shape[:2]
    fh, fw = h // n, w // n
    r, c = divmod(idx, n)
    return img[r * fh:(r + 1) * fh, c * fw:(c + 1) * fw]


def preview(puff, k_alpha=0.3):
    """Translucent pack look (old counts) against the masked look (new counts + R boost), same random sprite set."""
    bg = np.asarray(Image.open(os.path.join(ROOT, 'Saved', 'OreMountains', 'fx', 'y_streak_valley.png')).convert('RGB')).astype(np.float32) / 255
    cm_px = 7.8
    colour = np.array([0.86, 0.80, 0.70], np.float32)
    cy, cx = 330, 640
    H, W = bg.shape[:2]

    def splat(canvas, fr, ch, size_cm, x_cm, y_cm, rot, mode, boost):
        px = max(4, int(size_cm / cm_px))
        img = cv2.resize(fr[..., [ch, 2]], (px, px), interpolation=cv2.INTER_AREA)
        m = cv2.getRotationMatrix2D((px / 2, px / 2), rot, 1.0)
        img = cv2.warpAffine(img, m, (px, px), flags=cv2.INTER_LINEAR)
        x0, y0 = int(cx + x_cm / cm_px - px / 2), int(cy + y_cm / cm_px - px / 2)
        sx0, sy0 = max(0, -x0), max(0, -y0)
        ex, ey = min(px, W - x0), min(px, H - y0)
        if ex <= sx0 or ey <= sy0:
            return
        sub = img[sy0:ey, sx0:ex]
        dst = canvas[y0 + sy0:y0 + ey, x0 + sx0:x0 + ex]
        c = np.clip(sub[..., 0] * k_alpha, 0, 1)
        if mode == 't':
            dst[:] = dst * (1 - c[..., None]) + colour * c[..., None]
        else:
            c2 = 1 - (1 - c) ** boost
            dst[c2 > sub[..., 1]] = colour

    out = []
    for mode, n_a, n_b in (('t', 87, 52), ('m', 25, 9)):
        canvas = bg.copy()
        rr = np.random.default_rng(7)
        for emitter, count, ch, boost in (('A', n_a, 0, 87 / 25), ('B', n_b, 1, 52 / 9)):
            for _ in range(count):
                idx = min(35, int(rr.random() * 36))
                fr = frame(puff, idx)
                size = rr.uniform(500, 1000) * 2.6
                splat(canvas, fr, ch, size, rr.uniform(-1200, 1200), rr.uniform(-250, 250),
                      rr.uniform(0, 360) if emitter == 'B' else 0.0, mode, boost)
        out.append(canvas)
    sheet = np.concatenate(out, 1)
    Image.fromarray((np.clip(sheet, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, 'preview_puffs.png'))
    crop = np.concatenate([o[200:460, 380:900] for o in out], 1)
    Image.fromarray((np.clip(crop, 0, 1) * 255).astype(np.uint8)).resize((crop.shape[1] * 2, crop.shape[0] * 2), Image.NEAREST).save(
        os.path.join(OUT, 'preview_puffs_zoom.png'))
    print('preview written')


if __name__ == '__main__':
    main()
