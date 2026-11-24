"""Debris decals from the furniture atlas: builds the lookup texture T_BerlinFurniture_DebrisMap.png (1024, RGBA16).

    python Source/Tools/BerlinMap/furniture/debris_mask_gen.py      (needs Saved/Berlin/furniture/uv_coverage.npy from
                                                                     debris_uv_coverage.py, headless Blender)

The decal material takes colour, normal and roughness from the furniture atlas itself (T_BerlinFurniture_D/_N/_ORM);
there is no new colour texture. This map only says, per decal texel, WHICH atlas texel to show and whether a shard is
there:  R, G = atlas UV (0..1, V down as Unreal samples it), B = opacity.

Layout: 4 x 4 cells of 256 px = 16 decal variants. Variant k (cell k, row 0 on top) is the spill of furniture piece
k + 1: about 70 % of its shards are cut from that piece's atlas cell, the rest from the other pieces. One decal texel is
one atlas texel (about 1 cm), so a decal is meant to be projected 2.6-4.5 m wide.

Per variant: splinters, angular shards, sheets (books, paper, tiles) and crumbs scattered from two or three loose
centres, thinning to nothing well inside the cell border (never a square edge). Each shard maps to a randomly turned
spot inside one UV island of the atlas (5 px clear of the island edge), so no shard shows atlas background or a seam.
The UV field of a shard continues 2 px past its outline, so bilinear filtering at the rim stays on the same surface.
"""
import math
import os
import struct
import zlib

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = "D:/UE5Projects/RTS_Survival/"
SRC = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinFurniture/Textures/"
COVER = ROOT + "Saved/Berlin/furniture/uv_coverage.npy"
OUT_DIR = ROOT + "Saved/Berlin/furniture/"
ATLAS, ATLAS_CELL = 2048, 512
SIZE, CELL = 1024, 256
PER_CELL = 64


def write_png16(path, rgba):
    """RGBA uint16 [h, w, 4] -> 16-bit PNG (Pillow cannot write this mode)."""
    h, w, _ = rgba.shape
    raw = b"".join(b"\x00" + rgba[y].astype(">u2").tobytes() for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 16, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def outline(rng, kind):
    if kind == "splinter":
        length, width = rng.uniform(18.0, 60.0), rng.uniform(2.5, 5.5)
        return [(-length / 2, 0), (-length / 4, -width / 2), (length / 2, -width / 4), (length / 3, width / 2)]
    if kind == "sheet":
        w, h = rng.uniform(10.0, 22.0), rng.uniform(14.0, 30.0)
        return [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]
    if kind == "crumb":
        rad = rng.uniform(1.8, 4.5)
        return [(rad * math.cos(a), rad * math.sin(a)) for a in np.linspace(0, 2 * math.pi, 5, endpoint=False)]
    rad = rng.uniform(7.0, 24.0)
    angs = np.sort(rng.uniform(0.0, 2.0 * math.pi, int(rng.integers(3, 7))))
    return [(rad * rng.uniform(0.55, 1.0) * math.cos(a), rad * rng.uniform(0.55, 1.0) * math.sin(a)) for a in angs]


def main():
    rng = np.random.default_rng(1945)
    cover = np.load(COVER) > 0
    inner = np.asarray(Image.fromarray((cover * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(11))) > 0
    u_map = np.zeros((SIZE, SIZE), dtype=np.float64)
    v_map = np.zeros((SIZE, SIZE), dtype=np.float64)
    mask = np.zeros((SIZE, SIZE), dtype=np.float64)
    taken = np.zeros((SIZE, SIZE), dtype=bool)
    counts = []
    for cell in range(16):
        ox, oy = (cell % 4) * CELL, (cell // 4) * CELL
        centres = [(CELL / 2 + rng.uniform(-38, 38), CELL / 2 + rng.uniform(-38, 38), rng.uniform(45.0, 72.0))
                   for _ in range(int(rng.integers(2, 4)))]
        placed, tries = 0, 0
        while placed < PER_CELL and tries < 9000:
            tries += 1
            cx0, cy0, spread = centres[int(rng.integers(len(centres)))]
            x, y = rng.normal(cx0, spread * 0.6), rng.normal(cy0, spread * 0.6)
            near = min(math.hypot(x - a, y - b) / s for a, b, s in centres)
            kind = str(rng.choice(["splinter", "shard", "sheet", "crumb"], p=[0.22, 0.3, 0.16, 0.32]))
            if near > 1.15 and kind != "crumb":
                continue                                          # only crumbs out at the fringe
            local = outline(rng, kind)
            turn = rng.uniform(0.0, 2.0 * math.pi)
            c, s = math.cos(turn), math.sin(turn)
            pts = [(x + px * c - py * s, y + px * s + py * c) for px, py in local]
            xs, ys = [p[0] for p in pts], [p[1] for p in pts]
            if min(xs) < 22 or min(ys) < 22 or max(xs) > CELL - 22 or max(ys) > CELL - 22:
                continue
            # where in the atlas this shard is cut from: a spot in the piece's own cell (or another piece's), turned
            src_cell = cell if rng.random() < 0.7 else int(rng.integers(16))
            sox, soy = (src_cell % 4) * ATLAS_CELL, (src_cell // 4) * ATLAS_CELL
            src_turn = rng.uniform(0.0, 2.0 * math.pi)
            sc, ss = math.cos(src_turn), math.sin(src_turn)
            ok = False
            for _ in range(30):
                sx, sy = sox + rng.uniform(30.0, ATLAS_CELL - 30.0), soy + rng.uniform(30.0, ATLAS_CELL - 30.0)
                probe = [(sx + px * sc - py * ss, sy + px * ss + py * sc) for px, py in local + [(0.0, 0.0)]]
                if all(0 <= int(py) < ATLAS and 0 <= int(px) < ATLAS and inner[int(py), int(px)] for px, py in probe):
                    ok = True
                    break
            if not ok:
                continue
            x0, y0 = int(min(xs)) - 4, int(min(ys)) - 4
            w, h = int(max(xs)) - x0 + 5, int(max(ys)) - y0 + 5
            tile = Image.new("L", (w, h), 0)
            ImageDraw.Draw(tile).polygon([(px - x0, py - y0) for px, py in pts], fill=255)
            exact = np.asarray(tile) > 0
            wide = np.asarray(tile.filter(ImageFilter.MaxFilter(5))) > 0
            if not exact.any():
                continue
            gy, gx = oy + y0, ox + x0
            if taken[gy:gy + h, gx:gx + w][wide].any():
                continue
            # decal texel p -> shard-local (undo the turn) -> atlas texel
            py_, px_ = np.mgrid[0:h, 0:w]
            dx, dy = px_ + x0 + 0.5 - x, py_ + y0 + 0.5 - y
            lx, ly = dx * c + dy * s, -dx * s + dy * c
            au, av = sx + lx * sc - ly * ss, sy + lx * ss + ly * sc
            u_map[gy:gy + h, gx:gx + w][wide] = au[wide] / ATLAS
            v_map[gy:gy + h, gx:gx + w][wide] = av[wide] / ATLAS
            mask[gy:gy + h, gx:gx + w][exact] = 1.0
            taken[gy:gy + h, gx:gx + w][wide] = True
            placed += 1
        counts.append(placed)
    soft = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.5))) / 255.0
    rgba = np.stack([u_map, v_map, soft, np.ones_like(soft)], axis=-1)
    write_png16(OUT_DIR + "T_BerlinFurniture_DebrisMap.png", np.clip(np.round(rgba * 65535.0), 0, 65535).astype(np.uint16))
    # preview: the 16 decals on a ground-grey card, sampled exactly as the material does
    atlas = np.asarray(Image.open(SRC + "T_BerlinFurniture_D.png").convert("RGB")).astype(np.float32)
    iu = np.clip((u_map * ATLAS).astype(int), 0, ATLAS - 1)
    iv = np.clip((v_map * ATLAS).astype(int), 0, ATLAS - 1)
    colour = atlas[iv, iu]
    a = soft[:, :, None]
    ground = np.full_like(colour, 118.0) * np.array([1.0, 0.96, 0.92])
    Image.fromarray((ground * (1 - a) + colour * a).astype(np.uint8)).resize((1400, 1400), Image.NEAREST).save(
        OUT_DIR + "debris_decal_preview.png")
    print("shards per variant", counts, "opacity share %.3f" % float((soft > 0.5).mean()))


if __name__ == "__main__":
    main()
