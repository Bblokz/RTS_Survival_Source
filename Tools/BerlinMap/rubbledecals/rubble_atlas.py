"""Builds the Berlin rubble decal atlas from the Blender cell renders (rubble_blender.py).

Run:  python rubble_atlas.py --src <render dir> --out <atlas dir>

Writes (UE import settings in rubble_ue_build.py):
  T_BerlinRubbleDecal_BCO.png  RGB = albedo (sRGB), A = opacity                      -> TC_Default (BC3), sRGB on
  T_BerlinRubbleDecal_NRS.png  R = normal X (image right), G = normal Y (+G = image down, DirectX), B = roughness,
                               A = piece mask (1 = rubble piece, 0 = dust bed)        -> TC_Masks (BC3), sRGB off
  preview_sheet.png            all cells lit from the top left over the Berlin ground colour, with names
Cell layout and texel density: rubble_table.py.

Per cell: the renders are premultiplied by coverage, so albedo/normal/height/roughness are divided by alpha. The dust
bed is the blurred piece coverage broken up by noise (ragged outline, never a square), with grit specks; contact
occlusion comes from the height map (blurred height minus height at two radii) and darkens both the pieces' crevices
and the bed next to them. Alpha is forced to zero in a gutter at the cell border so mips do not bleed.
"""
import argparse
import os

import cv2
import numpy as np

import rubble_table as T

GROUND = np.array([0.55, 0.52, 0.51])  # lit Berlin ground in the critic shots (sRGB)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def fbm(rng, size, base, octaves=5, gain=0.55):
    out = np.zeros((size, size), np.float32)
    amp, tot = 1.0, 0.0
    cells = base
    for _ in range(octaves):
        g = rng.random((cells + 1, cells + 1)).astype(np.float32)
        out += amp * cv2.resize(g, (size, size), interpolation=cv2.INTER_CUBIC)
        tot += amp
        amp *= gain
        cells *= 2
    out /= tot
    lo, hi = np.percentile(out, 1), np.percentile(out, 99)
    return np.clip((out - lo) / (hi - lo), 0, 1)


def blur(img, sigma):
    return cv2.GaussianBlur(img, (0, 0), sigma)


def build_cell(ci, src):
    cell = T.CELLS[ci]
    d = np.load(os.path.join(src, "cell%02d.npy" % ci))
    R = d.shape[0]
    px_m = (T.CELL_CM / 100.0) / R
    rng = np.random.default_rng(500 + ci)

    a = np.clip(d[..., 8], 0, 1)
    inv = np.where(a > 1e-3, 1.0 / np.maximum(a, 1e-3), 0.0)
    alb = np.clip(d[..., 0:3] * inv[..., None], 0, 1)
    rough = np.clip(d[..., 3] * inv, 0, 1)
    h = np.maximum(d[..., 4] * inv, 0.0)
    n = d[..., 5:8] * inv[..., None]
    n[a <= 1e-3] = (0, 0, 1)
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)

    bed_col, bed_op, spread_m, grit = cell["bed"]
    bed_col = np.array(bed_col, np.float32)

    # ---- bed: blurred coverage broken up by noise
    sigma = max(2.0, spread_m / px_m * 0.42)
    dens = blur(a, sigma)
    dens /= max(dens.max(), 1e-6)
    n1 = fbm(rng, R, 6)
    n2 = fbm(rng, R, 14)
    field = dens ** 0.55 * (0.62 + 0.62 * n1) + 0.10 * (n2 - 0.5)
    bed_a = smoothstep(0.16, 0.50, field) * bed_op * (0.70 + 0.30 * n2)
    # thinner towards the outer edge, so the outline frays instead of ending on a contour line
    bed_a *= smoothstep(0.05, 0.35, dens + 0.15 * (n1 - 0.5))

    mott = fbm(rng, R, 24, 4)
    bed_rgb = bed_col[None, None, :] * (0.86 + 0.26 * mott)[..., None]

    # grit specks: small dots of lighter/darker dust and broken bits of the cell's own pieces
    piece_mean = (alb * a[..., None]).reshape(-1, 3).sum(0) / max(a.sum(), 1.0)
    p = np.clip(grit * 0.055 * bed_a / max(bed_op, 1e-3), 0, 0.2)
    seeds = rng.random((R, R)) < p
    sizes = rng.random((R, R))
    speck = np.zeros((R, R), np.float32)
    speck[seeds] = 0.4 + 0.6 * sizes[seeds]
    big = cv2.dilate(speck * (sizes > 0.7), np.ones((3, 3), np.uint8))
    speck = np.maximum(speck, big * 0.8)
    speck = blur(speck, 0.7)
    spk_mask = np.clip(speck * 2.2, 0, 1)
    tone = rng.random((R, R)).astype(np.float32)
    tone = cv2.dilate(tone, np.ones((3, 3), np.uint8))
    spk_col = np.where((tone > 0.6)[..., None], piece_mean[None, None, :] * (0.8 + 0.4 * tone[..., None]),
                       bed_col[None, None, :] * (0.65 + 0.7 * tone[..., None]))
    bed_rgb = bed_rgb * (1 - spk_mask[..., None]) + spk_col * spk_mask[..., None]
    bed_h = speck * 0.008 + (mott - 0.5) * 0.004

    # ---- combine
    H = a * h + (1 - a) * np.maximum(bed_h, 0) * (bed_a > 0.02)
    occ1 = np.clip(blur(H, 0.015 / px_m) - H, 0, None)
    occ2 = np.clip(blur(H, 0.06 / px_m) - H, 0, None)
    ao = 1.0 - np.clip(occ1 * 7.0 + occ2 * 2.2, 0, 0.6)

    gy, gx = np.gradient(bed_h / px_m)
    n_bed = np.dstack([-gx, gy, np.ones_like(gx)])  # world: -dz/dX, -dz/dY (row down = -Y)
    n_bed /= np.linalg.norm(n_bed, axis=-1, keepdims=True)
    nn = n_bed * (1 - a[..., None]) + n * a[..., None]
    nn /= np.maximum(np.linalg.norm(nn, axis=-1, keepdims=True), 1e-6)

    col = (bed_rgb * (1 - a[..., None]) + alb * a[..., None]) * ao[..., None]
    alpha = np.maximum(a, bed_a)
    rgh = 0.93 * (1 - a) + rough * a

    # gutter
    yy, xx = np.mgrid[0:R, 0:R]
    edge = np.minimum(np.minimum(xx, R - 1 - xx), np.minimum(yy, R - 1 - yy)).astype(np.float32)
    alpha *= smoothstep(0.012 * R, 0.028 * R, edge)
    dead = alpha < 0.004
    col[dead] = bed_col
    nn[dead] = (0, 0, 1)
    rgh[dead] = 0.93

    # ---- downsample to the atlas cell
    o = T.OUT_CELL_PX
    col_s = cv2.resize(col, (o, o), interpolation=cv2.INTER_AREA)
    al_s = cv2.resize(alpha, (o, o), interpolation=cv2.INTER_AREA)
    n_s = cv2.resize(nn, (o, o), interpolation=cv2.INTER_AREA)
    n_s /= np.maximum(np.linalg.norm(n_s, axis=-1, keepdims=True), 1e-6)
    r_s = cv2.resize(rgh, (o, o), interpolation=cv2.INTER_AREA)
    m_s = cv2.resize(a, (o, o), interpolation=cv2.INTER_AREA)
    bco = np.dstack([col_s, al_s])
    nrs = np.dstack([n_s[..., 0] * 0.5 + 0.5, -n_s[..., 1] * 0.5 + 0.5, r_s, m_s])
    return bco, nrs, n_s


def lit_preview(bco, n_s, ground):
    L = np.array([-0.45, 0.55, 0.70])
    L /= np.linalg.norm(L)
    lam = np.clip((n_s * L).sum(-1), 0, 1)
    lin = np.power(np.clip(bco[..., :3], 0, 1), 2.2) * (0.30 + 0.95 * lam)[..., None]
    g_lin = np.power(ground, 2.2) * (0.30 + 0.95 * L[2])
    a = bco[..., 3:4]
    out = lin * a + g_lin * (1 - a)
    return np.power(np.clip(out, 0, 1), 1 / 2.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cells", default=None)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    o = T.OUT_CELL_PX
    S = o * T.GRID
    bco_atlas = np.zeros((S, S, 4), np.float32)
    nrs_atlas = np.zeros((S, S, 4), np.float32)
    bco_atlas[...] = (0.5, 0.47, 0.43, 0.0)
    nrs_atlas[...] = (0.5, 0.5, 0.93, 0.0)
    sheet = np.zeros((S, S * 2, 3), np.float32)
    cells = range(16) if not args.cells else [int(c) for c in args.cells.split(",")]
    for ci in cells:
        if not os.path.exists(os.path.join(args.src, "cell%02d.npy" % ci)):
            print("missing cell", ci)
            continue
        bco, nrs, n_s = build_cell(ci, args.src)
        r, c = ci // T.GRID, ci % T.GRID
        bco_atlas[r * o:(r + 1) * o, c * o:(c + 1) * o] = bco
        nrs_atlas[r * o:(r + 1) * o, c * o:(c + 1) * o] = nrs
        sheet[r * o:(r + 1) * o, c * o:(c + 1) * o] = lit_preview(bco, n_s, GROUND)
        sheet[r * o:(r + 1) * o, S + c * o:S + (c + 1) * o] = lit_preview(bco, n_s, np.array([0.30, 0.29, 0.28]))
        print("cell %02d %-16s alpha cover %.2f" % (ci, T.CELLS[ci]["name"], float((bco[..., 3] > 0.05).mean())))

    def u8(x):
        return np.clip(np.round(x * 255.0), 0, 255).astype(np.uint8)

    cv2.imwrite(os.path.join(args.out, "T_BerlinRubbleDecal_BCO.png"),
                cv2.cvtColor(u8(bco_atlas), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(os.path.join(args.out, "T_BerlinRubbleDecal_NRS.png"),
                cv2.cvtColor(u8(nrs_atlas), cv2.COLOR_RGBA2BGRA))
    sh = cv2.cvtColor(u8(sheet), cv2.COLOR_RGB2BGR)
    for ci in cells:
        r, c = ci // T.GRID, ci % T.GRID
        for off in (0, S):
            txt = "%02d %s" % (ci, T.CELLS[ci]["name"])
            org = (off + c * o + 10, r * o + 26)
            cv2.putText(sh, txt, org, cv2.FONT_HERSHEY_SIMPLEX, 0.62, (0, 0, 0), 3, cv2.LINE_AA)
            cv2.putText(sh, txt, org, cv2.FONT_HERSHEY_SIMPLEX, 0.62, (255, 255, 255), 1, cv2.LINE_AA)
    for k in range(1, T.GRID * 2):
        sh[:, k * o] = (40, 40, 40)
    for k in range(1, T.GRID):
        sh[k * o, :] = (40, 40, 40)
    cv2.imwrite(os.path.join(args.out, "preview_sheet.png"), sh)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
