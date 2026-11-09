"""Lit preview of the impact-decal atlas over real project ground textures.

python preview.py --atlas <dir with T_LandscapeImpact_*.png> --out <dir>
python preview.py --set bomb --atlas <dir with T_BombImpact_*.png> --shell-atlas <shell dir> --out <dir>
python preview.py --old-vs-new <old shell dir> --atlas <new shell dir> --out <dir>

Decal box diameters per cell come from the atlas folder's atlas_meta.json (falls back to the old shell table).

Emulates M_LandscapeImpactDecal (DBuffer translucent colour + normal + roughness):
  base   = lerp(ground, decalAlbedo * Tint * lerp(0.88, 1.12, G) -> lerp(.., ScorchColor, scorch*ScorchStrength), a)
  normal = normalize(lerp(groundN, decalN(rotated with the decal), a))
  a      = texA * DecalColor.A * Opacity
Lighting: low sun (elevation 32 deg) + sky ambient + a small GGX-ish specular from roughness.

Sheets:
  sheet_close.png       8 bands x 2 variants on 4 grounds, each decal filling its tile (texture-res detail)
  sheet_rts.png         all 16 cells at true RTS scale (4.5 cm/px ~ 45 m camera, 1080p), 2x nearest-upscaled,
                        with a 3.3 x 6.6 m tank footprint for scale
  sheet_battlefield.png 40 x 24 m patches of grass and sand with ~70 randomly scattered, rotated, scaled decals
"""
import argparse
import json
import math
import os

import cv2
import numpy as np

GROUND_DIR = 'D:/UE5Projects/RTS_Survival/Saved/ImpactDecals/ground'
GROUNDS = [('grass', 'T_Ground_Grass_Near_01_BC.png'), ('dirt', 'T_Ground_Desert_Near_01_BC.png'),
           ('sand', 'T_Ground_Sand_Near_01_BC.png'), ('rock', 'T_Ground_Rock_Desert_Near_01_BC.png')]
GROUND_TILE_CM = 400.0
CELL = 512
DIAM = [40, 70, 110, 160, 210, 290, 400, 600]   # pre-2026-11-09 shell boxes (fallback when no meta)

# Material parameter defaults (keep in sync with ue_build_material.py).
TINT = np.array([1.0, 1.0, 1.0])
SCORCH_COLOR_LIN = np.array([0.010, 0.0085, 0.0075])   # linear, = material ScorchColor default
SCORCH_STRENGTH = 0.75
OPACITY = 1.0
NORMAL_STRENGTH = 1.0
ROUGH_MUL = 1.0

SUN_AZ = math.radians(215.0)   # light comes from upper-left-ish of the image
SUN_EL = math.radians(32.0)
L = np.array([math.cos(SUN_EL) * math.cos(SUN_AZ), math.cos(SUN_EL) * math.sin(SUN_AZ), math.sin(SUN_EL)])
VIEW = np.array([0.0, 0.55, 0.835])  # RTS camera ~57 deg pitch, looking 'up' the image
VIEW = VIEW / np.linalg.norm(VIEW)


def srgb_to_lin(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def load_atlas(d, prefix='T_LandscapeImpact'):
    bco = cv2.cvtColor(cv2.imread(os.path.join(d, prefix + '_BCO.png'), cv2.IMREAD_UNCHANGED), cv2.COLOR_BGRA2RGBA).astype(np.float32) / 255
    nrs = cv2.cvtColor(cv2.imread(os.path.join(d, prefix + '_NRS.png'), cv2.IMREAD_UNCHANGED), cv2.COLOR_BGRA2RGBA).astype(np.float32) / 255
    return bco, nrs


def cell_diams(d):
    """Decal box diameter (cm) of each of the 16 cells, from the folder's atlas_meta.json."""
    p = os.path.join(d, 'atlas_meta.json')
    if os.path.exists(p):
        with open(p) as f:
            return [c['D'] for c in sorted(json.load(f)['cells'], key=lambda c: c['cell'])]
    return [DIAM[i // 2] for i in range(16)]


def cell_maps(bco, nrs, i):
    c, r = i % 4, i // 4
    sl = (slice(r * CELL, (r + 1) * CELL), slice(c * CELL, (c + 1) * CELL))
    return bco[sl], nrs[sl]


def ground_canvas(name_file, w, h, cm_per_px, rng):
    img = cv2.cvtColor(cv2.imread(os.path.join(GROUND_DIR, name_file), cv2.IMREAD_UNCHANGED), cv2.COLOR_BGRA2RGB).astype(np.float32) / 255
    tile_px = max(int(round(GROUND_TILE_CM / cm_per_px)), 4)
    t = cv2.resize(img, (tile_px, tile_px), interpolation=cv2.INTER_AREA)
    reps_y, reps_x = h // tile_px + 2, w // tile_px + 2
    big = np.tile(t, (reps_y, reps_x, 1))
    oy, ox = rng.integers(0, tile_px), rng.integers(0, tile_px)
    return big[oy:oy + h, ox:ox + w].copy()


class Canvas:
    def __init__(self, ground_rgb):
        h, w = ground_rgb.shape[:2]
        self.base = ground_rgb.copy()
        self.n = np.zeros((h, w, 3), np.float32)
        self.n[..., 2] = 1
        self.rough = np.full((h, w), 0.9, np.float32)

    def add_decal(self, bco_c, nrs_c, cx, cy, diam_px, angle, g=0.5, alpha_mul=1.0):
        """Stamp one decal centred at (cx, cy) px, box diameter diam_px, rotated by angle (rad, CCW in image)."""
        # Pre-filter the cell to roughly the target resolution (mip emulation), then affine-warp.
        tgt = max(int(round(diam_px)), 2)
        if tgt < CELL:
            lvl = max(1, 2 ** int(math.floor(math.log2(CELL / tgt))))
            sz = CELL // lvl
            b = cv2.resize(bco_c, (sz, sz), interpolation=cv2.INTER_AREA)
            n = cv2.resize(nrs_c, (sz, sz), interpolation=cv2.INTER_AREA)
        else:
            b, n, sz = bco_c, nrs_c, CELL
        scale = diam_px / sz
        ca, sa = math.cos(angle), math.sin(angle)
        # dst = R*S*(src - c_src) + c_dst
        M = np.array([[ca * scale, -sa * scale, 0], [sa * scale, ca * scale, 0]], np.float32)
        M[:, 2] = np.array([cx, cy]) - M[:, :2] @ np.array([sz / 2, sz / 2])
        H, W = self.base.shape[:2]
        bw = cv2.warpAffine(b, M, (W, H), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=(0, 0, 0, 0))
        nw = cv2.warpAffine(n, M, (W, H), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=(0.5, 0.5, 0.9, 0))
        a = np.clip(bw[..., 3] * alpha_mul * OPACITY, 0, 1)
        m = a > 1e-4
        if not m.any():
            return
        # Same maths as the material's ImpactBaseColor node (linear space).
        alb = srgb_to_lin(bw[..., :3]) * TINT * (0.82 + 0.36 * g)
        sc = np.clip(nw[..., 3] * SCORCH_STRENGTH * (0.8 + 0.4 * g), 0, 1)[..., None]
        alb = lin_to_srgb(alb * (1 - sc) + SCORCH_COLOR_LIN * sc)
        # Decode + rotate the tangent normal with the decal.
        nx = (nw[..., 0] * 2 - 1) * NORMAL_STRENGTH
        ny = (nw[..., 1] * 2 - 1) * NORMAL_STRENGTH
        rx = nx * ca - ny * sa
        ry = nx * sa + ny * ca
        nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0, 1))
        dn = np.stack([rx, ry, nz], -1)
        a3 = a[..., None]
        self.base = self.base * (1 - a3) + alb * a3
        nn = self.n * (1 - a3) + dn * a3
        self.n = nn / (np.linalg.norm(nn, axis=-1, keepdims=True) + 1e-6)
        self.rough = self.rough * (1 - a) + np.clip(nw[..., 2] * ROUGH_MUL, 0, 1) * a

    def shade(self):
        # Image coords: x right, y down. Normal stored as (x right, y down, up). Light vector in same frame.
        alb = srgb_to_lin(self.base)
        ndl = np.clip(self.n @ L, 0, 1)
        hv = (L + VIEW) / np.linalg.norm(L + VIEW)
        ndh = np.clip(self.n @ hv, 0, 1)
        a2 = np.clip(self.rough, 0.05, 1) ** 4
        d = ndh * ndh * (a2 - 1) + 1
        spec = a2 / (math.pi * d * d + 1e-6) * 0.04 * ndl
        sky = 0.28 + 0.12 * self.n[..., 2]
        lin = alb * (sky[..., None] + 2.4 * ndl[..., None]) + spec[..., None] * 2.4
        # Simple filmic-ish tonemap.
        lin = lin / (1 + lin * 0.35)
        return lin_to_srgb(lin * 1.05)


def to8(img):
    return (np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)


def label(img, text, x, y, scale=0.45, col=(255, 255, 255)):
    cv2.putText(img, text, (x + 1, y + 1), cv2.FONT_HERSHEY_SIMPLEX, scale, (0, 0, 0), 2, cv2.LINE_AA)
    cv2.putText(img, text, (x, y), cv2.FONT_HERSHEY_SIMPLEX, scale, col, 1, cv2.LINE_AA)


def sheet_close(bco, nrs, out, diam=DIAM):
    T = 256
    rng = np.random.default_rng(3)
    rows = []
    for gname, gfile in GROUNDS:
        for v in range(2):
            row = []
            for b in range(8):
                i = 2 * b + v
                cmpp = diam[b] / (T * 0.95)
                cv = Canvas(ground_canvas(gfile, T, T, cmpp, rng))
                bc, nc = cell_maps(bco, nrs, i)
                cv.add_decal(bc, nc, T / 2, T / 2, T * 0.95, 0.0, 0.5)
                img = to8(cv.shade())
                img = np.ascontiguousarray(img)
                label(img, '%s%s %dcm %s' % ('B%d' % b, 'AB'[v], diam[b], gname), 4, 14, 0.4)
                row.append(img)
            rows.append(np.concatenate(row, 1))
    sheet = np.concatenate(rows, 0)
    cv2.imwrite(os.path.join(out, 'sheet_close.png'), cv2.cvtColor(sheet, cv2.COLOR_RGB2BGR))


def draw_tank(img, cx, cy, cmpp, up=2):
    w, l = 330 / cmpp * up, 660 / cmpp * up
    p0 = (int(cx - w / 2), int(cy - l / 2))
    p1 = (int(cx + w / 2), int(cy + l / 2))
    cv2.rectangle(img, p0, p1, (230, 230, 60), 1, cv2.LINE_AA)
    label(img, 'tank 3.3x6.6m', p0[0] - 4, p0[1] - 4, 0.35, (230, 230, 60))


def sheet_rts(bco, nrs, out, cmpp=4.5, diam=DIAM):
    x_end = 80 + sum(d * (1.1 if d <= 290 else 2.15) + 40 for d in diam)
    W_cm, H_cm = max(4000, int(x_end + 500)), max(900, int(max(diam) * 1.08))
    W, H = int(W_cm / cmpp), int(H_cm / cmpp)
    rng = np.random.default_rng(5)
    panels = []
    for gname, gfile in GROUNDS:
        cv = Canvas(ground_canvas(gfile, W, H, cmpp, rng))
        x_cm = 80
        centers = []
        for b in range(8):
            d = diam[b]
            for v in range(2):
                if d <= 290:
                    cx_cm, cy_cm = x_cm + d * 0.55, H_cm * 0.5 + (v - 0.5) * d * 1.15
                else:
                    cx_cm, cy_cm = x_cm + d * (0.55 + 1.05 * v), H_cm * 0.5
                cx, cy = cx_cm / cmpp, cy_cm / cmpp
                cv.add_decal(*cell_maps(bco, nrs, 2 * b + v), cx, cy, d / cmpp, rng.uniform(0, 2 * math.pi), rng.uniform())
                centers.append((b, v, cx, cy))
            x_cm += d * (1.1 if d <= 290 else 2.15) + 40
        img = to8(cv.shade())
        img = cv2.resize(img, (W * 2, H * 2), interpolation=cv2.INTER_NEAREST)
        img = np.ascontiguousarray(img)
        done = set()
        for b, v, cx, cy in centers:
            if b in done:
                continue
            done.add(b)
            label(img, 'B%d' % b, int(cx * 2) - 8, 16, 0.45)
        draw_tank(img, W * 2 - 160, H, cmpp)
        label(img, '%s  (true RTS scale %.1f cm/px, shown 2x)' % (gname, cmpp), 6, H * 2 - 8, 0.45)
        panels.append(img)
    sheet = np.concatenate(panels, 0)
    cv2.imwrite(os.path.join(out, 'sheet_rts.png'), cv2.cvtColor(sheet, cv2.COLOR_RGB2BGR))


def sheet_battlefield(bco, nrs, out, cmpp=4.5, diam=DIAM):
    W_cm, H_cm = 4000, 2400
    W, H = int(W_cm / cmpp), int(H_cm / cmpp)
    panels = []
    for gname, gfile in [GROUNDS[0], GROUNDS[2]]:
        rng = np.random.default_rng(11)
        cv = Canvas(ground_canvas(gfile, W, H, cmpp, rng))
        # Weighted toward small calibres, like a real fight.
        weights = np.array([10, 10, 8, 7, 5, 3, 2, 1], float)
        weights /= weights.sum()
        for k in range(70):
            b = rng.choice(8, p=weights)
            v = rng.integers(0, 2)
            d = diam[b] * rng.uniform(0.8, 1.25)
            cx, cy = rng.uniform(0, W), rng.uniform(0, H)
            cv.add_decal(*cell_maps(bco, nrs, 2 * b + v), cx, cy, d / cmpp, rng.uniform(0, 2 * math.pi), rng.uniform())
        img = np.ascontiguousarray(to8(cv.shade()))
        draw_tank(img, W - 60, H - 100, cmpp, up=1)
        label(img, '%s 40x24 m, %.1f cm/px (1x)' % (gname, cmpp), 6, 16, 0.45)
        panels.append(img)
    sheet = np.concatenate(panels, 1)
    cv2.imwrite(os.path.join(out, 'sheet_battlefield.png'), cv2.cvtColor(sheet, cv2.COLOR_RGB2BGR))


def _rts_row(gfile, items, W_cm, H_cm, cmpp, rng, up):
    """items: (bco, nrs, cell, diam_cm, cx_cm, cy_cm, text). Returns the shaded (optionally upscaled) panel."""
    W, H = int(W_cm / cmpp), int(H_cm / cmpp)
    cv = Canvas(ground_canvas(gfile, W, H, cmpp, rng))
    for bc, nc, cell, d, cx, cy, _ in items:
        cv.add_decal(*cell_maps(bc, nc, cell), cx / cmpp, cy / cmpp, d / cmpp, rng.uniform(0, 2 * math.pi), rng.uniform())
    img = to8(cv.shade())
    if up != 1:
        img = cv2.resize(img, (W * up, H * up), interpolation=cv2.INTER_NEAREST)
    img = np.ascontiguousarray(img)
    for _, _, _, d, cx, cy, text in items:
        if text:
            label(img, text, int((cx - d * 0.3) / cmpp * up), int((cy - d * 0.5) / cmpp * up) + 14, 0.42)
    return img


def sheet_old_vs_new(old_dir, new_dir, out, cmpp=4.5, bands=(3, 4, 5, 6, 7)):
    """Shell bands old (top row) vs new (bottom row), both variants, at one common true RTS scale, 2x upscaled."""
    ob, on = load_atlas(old_dir)
    nb, nn = load_atlas(new_dir)
    od, nd = cell_diams(old_dir), cell_diams(new_dir)
    panels = []
    for gname, gfile in [GROUNDS[0], GROUNDS[2], GROUNDS[1]]:
        rng = np.random.default_rng(21)
        H_cm = 2 * max(nd[2 * b] for b in bands) + 250
        x = 60.0
        items = []
        for b in bands:
            dn, do = nd[2 * b], od[2 * b]
            for v in range(2):
                cx = x + dn * 0.5 + v * dn * 1.0
                items.append((ob, on, 2 * b + v, do, cx, 60 + dn * 0.5, 'old B%d%s %dcm' % (b, 'AB'[v], do)))
                items.append((nb, nn, 2 * b + v, dn, cx, 60 + dn * 1.5 + 80, 'new B%d%s %dcm' % (b, 'AB'[v], dn)))
            x += dn * 2.0 + 60
        img = _rts_row(gfile, items, x + 450, H_cm, cmpp, rng, 2)
        draw_tank(img, img.shape[1] - 130, img.shape[0] // 2, cmpp)
        label(img, '%s: old (top row) vs new (bottom row), true RTS scale %.1f cm/px shown 2x' % (gname, cmpp),
              6, img.shape[0] - 8, 0.5)
        panels.append(img)
    w = max(p.shape[1] for p in panels)
    panels = [np.pad(p, ((0, 0), (0, w - p.shape[1]), (0, 0))) for p in panels]
    cv2.imwrite(os.path.join(out, 'shell_b3_b7_old_vs_new.png'), cv2.cvtColor(np.concatenate(panels, 0), cv2.COLOR_RGB2BGR))


BOMB_PREFIX = 'T_BombImpact'


def sheet_bomb_close(bco, nrs, out, diam, T=256):
    """2 x 2 ground panels; in each, rows = bomb bands K0..K3, columns = the 4 variants, each decal filling its tile."""
    rng = np.random.default_rng(3)
    panels = []
    for gname, gfile in GROUNDS:
        rows = []
        for k in range(4):
            row = []
            for v in range(4):
                i = 4 * k + v
                cmpp = diam[i] / (T * 0.95)
                cv = Canvas(ground_canvas(gfile, T, T, cmpp, rng))
                cv.add_decal(*cell_maps(bco, nrs, i), T / 2, T / 2, T * 0.95, 0.0, 0.5)
                img = np.ascontiguousarray(to8(cv.shade()))
                label(img, 'K%d-%d %dm %s' % (k, v, round(diam[i] / 100), gname), 4, 14, 0.4)
                row.append(img)
            rows.append(np.concatenate(row, 1))
        panels.append(np.concatenate(rows, 0))
    sheet = np.concatenate([np.concatenate(panels[:2], 1), np.concatenate(panels[2:], 1)], 0)
    cv2.imwrite(os.path.join(out, 'contact_sheet_bombs.png'), cv2.cvtColor(sheet, cv2.COLOR_RGB2BGR))


def sheet_bomb_rts(bco, nrs, sb, sn, sdiam, out, diam, cmpp=4.5):
    """Shell B6A / B7A / B7B next to one cell of each bomb band, at one true RTS scale (1x), with a tank footprint."""
    panels = []
    for gname, gfile in [GROUNDS[0], GROUNDS[2], GROUNDS[1]]:
        rng = np.random.default_rng(31)
        H_cm = int(max(diam) * 1.05)
        x = 60.0
        items = []
        for cell, name in ((12, 'B6A 150mm'), (14, 'B7A 240mm'), (15, 'B7B 240mm')):
            d = sdiam[cell]
            items.append((sb, sn, cell, d, x + d * 0.5, H_cm * 0.5, 'shell %s' % name))
            x += d + 60
        for k in range(4):
            d = diam[4 * k + k]
            items.append((bco, nrs, 4 * k + k, d, x + d * 0.5, H_cm * 0.5, 'bomb K%d-%d %dcm' % (k, k, d)))
            x += d + 60
        img = _rts_row(gfile, items, x + 500, H_cm, cmpp, rng, 1)
        draw_tank(img, img.shape[1] - 70, img.shape[0] // 2, cmpp, up=1)
        label(img, '%s, true RTS scale %.1f cm/px (1x)' % (gname, cmpp), 6, img.shape[0] - 8, 0.5)
        panels.append(img)
    cv2.imwrite(os.path.join(out, 'sheet_bomb_rts.png'), cv2.cvtColor(np.concatenate(panels, 0), cv2.COLOR_RGB2BGR))


def sheet_bomb_battlefield(bco, nrs, sb, sn, sdiam, out, diam, cmpp=4.5):
    """60 x 36 m: a shell fight (80 craters at the shell box sizes) plus a stick of bombs, on grass and sand."""
    W_cm, H_cm = 6000, 3600
    W, H = int(W_cm / cmpp), int(H_cm / cmpp)
    panels = []
    for gname, gfile in [GROUNDS[0], GROUNDS[2]]:
        rng = np.random.default_rng(11)
        cv = Canvas(ground_canvas(gfile, W, H, cmpp, rng))
        weights = np.array([10, 10, 8, 7, 5, 3, 2, 1], float)
        weights /= weights.sum()
        for _ in range(80):
            b = rng.choice(8, p=weights)
            v = rng.integers(0, 2)
            d = sdiam[2 * b] * rng.uniform(0.92, 1.08)
            cv.add_decal(*cell_maps(sb, sn, 2 * b + v), rng.uniform(0, W), rng.uniform(0, H), d / cmpp,
                         rng.uniform(0, 2 * math.pi), rng.uniform())
        # A stick of 250/500 kg bombs across the field + one 1000 kg.
        for j, (k, kg_scale) in enumerate([(0, 1.0), (0, 0.9), (1, 1.0), (0, 1.1), (2, 1.0)]):
            v = int(rng.integers(0, 4))
            d = diam[4 * k + v] * kg_scale * rng.uniform(0.92, 1.08)
            cx = (0.12 + 0.19 * j) * W + rng.normal(0, 40)
            cy = (0.33 + 0.08 * j) * H + rng.normal(0, 40)
            cv.add_decal(*cell_maps(bco, nrs, 4 * k + v), cx, cy, d / cmpp, rng.uniform(0, 2 * math.pi), rng.uniform())
        img = np.ascontiguousarray(to8(cv.shade()))
        draw_tank(img, W - 60, H - 100, cmpp, up=1)
        label(img, '%s 60x36 m, %.1f cm/px (1x): shells + a bomb stick' % (gname, cmpp), 6, 16, 0.45)
        panels.append(img)
    cv2.imwrite(os.path.join(out, 'sheet_bomb_battlefield.png'), cv2.cvtColor(np.concatenate(panels, 1), cv2.COLOR_RGB2BGR))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--atlas', required=True)
    ap.add_argument('--out', default=None)
    ap.add_argument('--set', default='shell', choices=['shell', 'bomb'])
    ap.add_argument('--shell-atlas', default=None, help='bomb set: shell atlas folder for the comparison sheets')
    ap.add_argument('--old-vs-new', default=None, help='old shell atlas folder: write shell_b3_b7_old_vs_new.png only')
    a = ap.parse_args()
    out = a.out or a.atlas
    os.makedirs(out, exist_ok=True)
    if a.old_vs_new:
        sheet_old_vs_new(a.old_vs_new, a.atlas, out)
    elif a.set == 'bomb':
        bco, nrs = load_atlas(a.atlas, BOMB_PREFIX)
        diam = cell_diams(a.atlas)
        sheet_bomb_close(bco, nrs, out, diam)
        if a.shell_atlas:
            sb, sn = load_atlas(a.shell_atlas)
            sd = cell_diams(a.shell_atlas)
            sheet_bomb_rts(bco, nrs, sb, sn, sd, out, diam)
            sheet_bomb_battlefield(bco, nrs, sb, sn, sd, out, diam)
    else:
        bco, nrs = load_atlas(a.atlas)
        diam = cell_diams(a.atlas)[::2]
        sheet_close(bco, nrs, out, diam)
        sheet_rts(bco, nrs, out, diam=diam)
        sheet_battlefield(bco, nrs, out, diam=diam)
    print('previews in', out)
