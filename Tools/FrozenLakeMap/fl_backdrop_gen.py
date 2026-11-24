"""Backdrop generator for Ger_FrozenLake: the dry Vygozero lakebed and the Svarog crater rim beyond the north dam.

    python Source/Tools/FrozenLakeMap/fl_backdrop_gen.py [CONCEPT ...]      (default: the picked concept, C)

Decisions 21 + 22 (FrozenLakeMission.md): the bay is cut off by the Forge's dam; beyond it the rest of the lake has
steamed away to a cracked, dry lakebed, and a Svarog firebreak strike left a glassed, scorched crater rim out there.
Backdrop only: nobody can drive there (it starts past the play edge and its blockers).
Owner pick 2026-11-24: concept C "Radixite scar" (A, B and D stay below as the rejected alternatives).

The mesh is also the map's horizon skirt: a ring round the whole landscape square. East, west and south it continues
the landscape's edge height (tucked TUCK under it) and eases to SKIRT_Z, painted as burnt bog. North it is the old
shore bluff, the dry bed and the crater. Cells hidden under the landscape are left out.

Writes Saved/FrozenLake/backdrop/<CONCEPT>/
    SM_FL_Backdrop.obj     heightfield mesh in UE cm (x north, y east; the OBJ import mirrors y, so the file has -y)
    T_FL_BackdropMask.png  RGBA over the mesh extent like a map: north up (row 0 = MESH_X[1]), west left;
                           R scorch, G glass, B Radixite glow, A wet (pool / canal outflow)
    FL_height_backdrop.raw the landscape heights with the north margin pushed under the mesh (same format as
                           terrain/FL_height.raw)
    backdrop.json          extents, join line, crater, for the editor step
    preview.png            top view (mask colours over hillshade)

Height frame as fl_terrain_gen: ice = z 0, the land north of the dam about +1.7 m. The old shore bluff drops from the
land to the lakebed (about -3.5 m) just past the play edge's blockers, and the bed falls slowly to the north.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TERRAIN = os.path.join(ROOT, "Saved", "FrozenLake", "terrain")
OUT_ROOT = os.path.join(ROOT, "Saved", "FrozenLake", "backdrop")

LS_CORNER = -37800.0                 # landscape corner (both axes), 100 cm quads, 757 vertices
LS_QUAD = 100.0
LS_V = 757
PLAY_EDGE = 29300.0                  # fog-of-war / camera square
BLOCKER_OUTER = 30500.0              # boundary blockers reach this far (600 cm thick, centred 600 cm out)

LS_HALF = 37800.0                    # landscape half size
MESH_X = (-80000.0, 112000.0)        # backdrop ring extent
MESH_Y = (-80000.0, 80000.0)
# mesh rows / columns: 4 m where the crater, the bed and the tour frame are, coarser out on the skirt
GRID_X = [(-80000.0, -40000.0, 1600.0), (-40000.0, 30000.0, 800.0), (30000.0, 80000.0, 400.0),
          (80000.0, 112000.0, 1600.0)]
GRID_Y = [(-80000.0, -40000.0, 1600.0), (-40000.0, 40000.0, 400.0), (40000.0, 80000.0, 1600.0)]
HOLE_INSET = 800.0                   # cells this far inside the landscape square (south of the old shore) are left out
MASK_CELL = 100.0                    # mask texel
TUCK = 30.0                          # mesh sits this far under the landscape at the join
SKIRT_Z = 150.0                      # east / west / south skirt level (the landscape edge runs 1.2-2.3 m)
SKIRT_RUN = 4000.0                   # edge height -> SKIRT_Z over this run outside the landscape
BUILD_CONCEPTS = ("C",)

BED0 = -350.0                        # lakebed at the foot of the old shore bluff (the canal floor is -380)
BED_FAR = -900.0                     # lakebed far out
BED_FALL = 45000.0                   # BED0 -> BED_FAR over this distance past the bluff
BLUFF_RUN = 2400.0                   # land -> bed over this run
CANAL_Y = -4300.0                    # the spillway canal leaves the play area here (fl layout, path "canal")
CANAL_HALF = 750.0
CANAL_WATER_END = 30250.0            # FL_Water_canal_1 (z -70) ends here

CONCEPTS = {
    # A: the rim right behind the play edge, north-west; a glass lip over the near ground
    "A": dict(title="Glass lip", crater=(52000.0, -15000.0), radius=14000.0, rim=3600.0, depth=2600.0,
              breach=None, glass=1.0, scorch_reach=1.7, rays=0.6, glow_fissures=0.0, glow_floor=0.25, pool=0.0,
              outflow="bed", glow_strength=1.5),
    # B: a wide, pale, cracked lakebed; the crater a low dark ring far out
    "B": dict(title="Far ring", crater=(80000.0, 8000.0), radius=22000.0, rim=2400.0, depth=1600.0,
              breach=None, glass=0.6, scorch_reach=1.5, rays=1.0, glow_fissures=0.0, glow_floor=0.0, pool=0.0,
              outflow="bed", glow_strength=0.0),
    # C: the vein is exposed: amber Radixite fissures run from the crater over the bed toward the dam
    "C": dict(title="Radixite scar", crater=(62000.0, -3000.0), radius=16000.0, rim=3000.0, depth=2400.0,
              breach=None, glass=0.9, scorch_reach=1.6, rays=0.7, glow_fissures=1.0, glow_floor=1.0, pool=0.0,
              outflow="crater", glow_strength=5.0),
    # D: the rim is breached toward the dam; the canal runs into a last steaming pool on the glassed floor
    "D": dict(title="Breached rim, last pool", crater=(58000.0, 6000.0), radius=16000.0, rim=3200.0, depth=2200.0,
              breach=(200.0, 38.0), glass=0.9, scorch_reach=1.6, rays=0.5, glow_fissures=0.25, glow_floor=0.5,
              pool=1.0, outflow="crater", glow_strength=3.0),
}


# ---- noise -----------------------------------------------------------------------------------------------------
def _value_noise(x, y, scale, seed):
    """Smooth value noise in [0, 1] at world cm, lattice spacing `scale`."""
    rng = np.random.RandomState(seed)
    table = rng.rand(4096)
    gx, gy = x / scale, y / scale
    ix, iy = np.floor(gx).astype(np.int64), np.floor(gy).astype(np.int64)
    fx, fy = gx - ix, gy - iy
    ux, uy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

    def h(i, j):
        return table[((i * 73856093) ^ (j * 19349663)) & 4095]

    a, b = h(ix, iy), h(ix + 1, iy)
    c, d = h(ix, iy + 1), h(ix + 1, iy + 1)
    return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy


def fbm(x, y, scale, seed, octaves=4):
    total, amp, norm = 0.0, 1.0, 0.0
    for o in range(octaves):
        total = total + amp * _value_noise(x, y, scale / (2 ** o), seed + 101 * o)
        norm += amp
        amp *= 0.5
    return total / norm


def smoothstep(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# ---- landscape ---------------------------------------------------------------------------------------------------
def load_landscape():
    raw = np.fromfile(os.path.join(TERRAIN, "FL_height.raw"), dtype="<u2").reshape(LS_V, LS_V)
    return raw                                   # [j (y), i (x)]


def landscape_z(raw, x, y):
    """Bilinear landscape height (cm) at world x/y (arrays)."""
    fx = np.clip((x - LS_CORNER) / LS_QUAD, 0, LS_V - 1.001)
    fy = np.clip((y - LS_CORNER) / LS_QUAD, 0, LS_V - 1.001)
    i, j = np.floor(fx).astype(int), np.floor(fy).astype(int)
    tx, ty = fx - i, fy - j
    h = (raw.astype(np.float64) - 32768.0) * 100.0 / 128.0
    return (h[j, i] * (1 - tx) * (1 - ty) + h[j, i + 1] * tx * (1 - ty) +
            h[j + 1, i] * (1 - tx) * ty + h[j + 1, i + 1] * tx * ty)


def land_z(raw, x, y):
    """Ground outside the lakebed: the landscape (tucked) inside its square, eased to SKIRT_Z outside it."""
    cxp = np.clip(x, -LS_HALF, LS_HALF)
    cyp = np.clip(y, -LS_HALF, LS_HALF)
    edge = landscape_z(raw, cxp, cyp) - TUCK
    d = np.hypot(x - cxp, y - cyp)
    t = smoothstep(0.0, SKIRT_RUN, d)
    far = SKIRT_Z + (fbm(x, y, 7000.0, 13, 3) - 0.5) * 160.0
    return edge * (1 - t) + far * t, d


def join_x(y):
    """The old shore line: where the mesh takes over from the landscape (past the blockers, a little ragged)."""
    return BLOCKER_OUTER + 500.0 + 1800.0 * fbm(y * 0 + 1.0, y, 4000.0, 7, 3)


# ---- the backdrop surface ----------------------------------------------------------------------------------------
def crater_terms(c, x, y):
    cx, cy = c["crater"]
    R, rim, depth = c["radius"], c["rim"], c["depth"]
    dx, dy = x - cx, y - cy
    ang = np.arctan2(dy, dx)
    # a slightly irregular ring: radius wobbles with angle
    wob = 1.0 + 0.06 * (fbm(np.cos(ang) * 30000.0, np.sin(ang) * 30000.0, 14000.0, 31, 3) - 0.5) * 2.0
    r = np.hypot(dx, dy) / (R * wob)
    inner = -depth + (rim + depth) * np.clip(r, 0, 1) ** 2.6               # bowl up to the crest
    outer = rim * np.clip(r, 1.0, None) ** -3.2                            # ejecta apron falls away
    z = np.where(r < 1.0, inner, outer)
    crest_noise = (fbm(x, y, 3000.0, 41, 4) - 0.5) * 2.0
    z = z + crest_noise * 0.18 * rim * np.exp(-((r - 1.0) / 0.25) ** 2)
    floor = np.clip(1.0 - r / 0.45, 0, 1)
    if c["breach"] is not None:
        bearing, half = c["breach"]
        d_ang = np.degrees(np.abs((ang - math.radians(bearing) + math.pi) % (2 * math.pi) - math.pi))
        gate = smoothstep(half, half * 0.45, d_ang) * np.exp(-((r - 1.0) / 0.5) ** 2) * (r > 0.55)
        z = z - gate * (z - (-depth * 0.35 + (rim * 0.15) * r))
    return z, r, ang, floor


def surface(c, raw, x, y):
    """Backdrop height (cm) and the parts the mask needs."""
    j = join_x(y)
    s = x - j                                                              # distance past the old shore line
    land, d_out = land_z(raw, np.minimum(x, j), y)
    bed = BED0 + (BED_FAR - BED0) * smoothstep(0.0, BED_FALL, s)
    bed = bed + (fbm(x, y, 9000.0, 3, 4) - 0.5) * 120.0                    # gentle swells on the bed
    bluff = smoothstep(0.0, BLUFF_RUN, s + 300.0 * (fbm(x, y, 1500.0, 5, 3) - 0.5))
    z = land * (1 - bluff) + bed * bluff
    # the spillway canal leaves the dam and runs out over the bed
    cx, cy = c["crater"]
    wander = CANAL_Y + 1800.0 * np.sin((x - PLAY_EDGE) / 9000.0) * smoothstep(PLAY_EDGE, PLAY_EDGE + 8000.0, x)
    if c["outflow"] == "crater":
        t = smoothstep(PLAY_EDGE + 4000.0, cx - c["radius"] * 0.9, x)
        wander = wander * (1 - t) + (cy + (CANAL_Y - cy) * 0.15) * t
    canal_d = np.abs(y - wander)
    reach = (x < cx) if c["outflow"] == "crater" else (x < PLAY_EDGE + 26000.0)
    # it starts where the play area's canal ends (its bed is -380 up to about x 300 m), past the play edge
    channel = smoothstep(CANAL_HALF * 1.5, CANAL_HALF * 0.6, canal_d) * reach * \
        smoothstep(PLAY_EDGE + 300.0, PLAY_EDGE + 900.0, x)
    cz, r, ang, floor = crater_terms(c, x, y)
    blend = smoothstep(2.6, 1.4, r)                                       # crater replaces the bed near it
    z = z * (1 - blend) + (bed + cz) * blend
    # the channel is cut after the crater, so it runs through the apron into the bowl
    # in the margin: a sill just past the canal water plane's end (x 302 m, water at -70), so the water ends on it,
    # then the cut falls to the bed down the bluff
    sill = -40.0 + 30.0 * (fbm(x, y, 400.0, 17, 2) - 0.5)
    margin_floor = np.where(x < CANAL_WATER_END, -380.0 + (sill + 380.0) * smoothstep(CANAL_WATER_END - 250.0,
                                                                                     CANAL_WATER_END, x),
                            sill + (-330.0 - sill) * smoothstep(CANAL_WATER_END + 200.0, j, x))
    floor_z = np.where(s < 0, margin_floor, np.minimum(bed + cz * blend, -380.0) - 120.0 * smoothstep(0.0, 3000.0, s))
    z = np.where(channel > 0, np.minimum(z, z * (1 - channel) + floor_z * channel), z)
    land = np.where(channel > 0, np.minimum(land, land * (1 - channel) + floor_z * channel), land)
    z = np.where(s < 0, land, z)                                          # never above the landscape before the join
    return z, dict(s=s, r=r, ang=ang, floor=floor, channel=channel, bed=bed, d_out=land_z(raw, x, y)[1])


def build_mask(c, raw, x, y):
    z, p = surface(c, raw, x, y)
    r, ang, s = p["r"], p["ang"], p["s"]
    R = c["radius"]
    n1 = fbm(x, y, 2500.0, 61, 4)
    n2 = fbm(x, y, 700.0, 67, 3)
    # polar coordinates round the crater: u runs round the ring (cm of arc at radius R), v outward (cm); the
    # angle's seam is put on the crater's far (north) side, away from the dam and the camera
    cx, cy = c["crater"]
    u = (np.arctan2(-(y - cy), -(x - cx)) + math.pi) * R
    v = r * R
    # scorch: a dark halo round the crater plus irregular rays thrown out along the bed (noise stretched outward)
    halo = smoothstep(c["scorch_reach"], 1.0, r)
    ray_n = fbm(u * 6.0, v * 0.25, 9000.0, 71, 3)
    rays = smoothstep(0.52, 0.72, ray_n) * smoothstep(c["scorch_reach"] * 2.4, 1.0, r) * c["rays"]
    scorch = np.clip(halo * (0.75 + 0.5 * n1) + rays * (0.5 + 0.7 * n2), 0, 1) * (s > 0)
    # glass: the crest and the inner wall, broken up
    glass = np.exp(-((r - 0.92) / 0.22) ** 2) + 0.6 * smoothstep(0.75, 0.3, r)
    glass = np.clip(glass * (0.55 + 0.9 * n2) * c["glass"], 0, 1)
    # glow: the floor and (optionally) fissures running out of the crater over the bed toward the dam
    glow = smoothstep(0.5, 0.05, r) * c["glow_floor"]
    if c["glow_fissures"] > 0:
        # ridged noise stretched outward: a few bold fissures running from the crater, with side branches
        crack = np.abs(fbm(u * 3.0 + 4000.0 * fbm(x, y, 6000.0, 83, 2), v * 0.3, 14000.0, 81, 3) - 0.5)
        branch = np.abs(fbm(x, y, 4000.0, 87, 3) - 0.5)
        lines = np.maximum(smoothstep(0.022, 0.0, crack), 0.6 * smoothstep(0.012, 0.0, branch) * smoothstep(2.2, 1.0, r))
        fan = smoothstep(4.5, 0.8, r) * smoothstep(-500.0, 3000.0, s)
        glow = np.maximum(glow, lines * fan * c["glow_fissures"] * (0.5 + 0.7 * n2))
        glow = np.maximum(glow, p["channel"] * 0.03 * c["glow_fissures"])   # warm outflow, faint
    glow = np.clip(glow, 0, 1)
    # the skirt east, west and south: burnt bog, faded in from the landscape edge
    bog = smoothstep(0.0, 1500.0, p["d_out"]) * smoothstep(800.0, -800.0, s)
    # the old shore bank: dark at its top, fading to the pale bed at its foot (no hard line at the join)
    bank = smoothstep(BLUFF_RUN * 1.4, 0.0, s) * (s > -400.0)
    scorch = np.maximum(scorch, bank * np.clip(0.55 + 0.6 * (n1 - 0.5) + 0.25 * n2, 0, 1))
    scorch = np.maximum(scorch, bog * np.clip(0.45 + 0.7 * (n1 - 0.5) + 0.3 * n2, 0, 1))
    wet = p["channel"] * 0.8
    if c["pool"] > 0:
        wet = np.maximum(wet, smoothstep(0.42, 0.3, r) * c["pool"])
    wet = np.clip(wet, 0, 1)
    return z, np.stack([scorch, glass, glow, wet], axis=-1)


def axis(spec):
    parts = [np.arange(a, b, step) for a, b, step in spec]
    return np.concatenate(parts + [np.array([spec[-1][1]])])


def write_obj(path, xs, ys, z, keep):
    """Grid mesh; keep[i, j] says whether the quad between rows i, i+1 and columns j, j+1 is written. Unused
    vertices are dropped. Returns the triangle count."""
    nx, ny = len(xs), len(ys)
    used = np.zeros((nx, ny), dtype=bool)
    used[:-1, :-1] |= keep
    used[1:, :-1] |= keep
    used[:-1, 1:] |= keep
    used[1:, 1:] |= keep
    index = np.zeros((nx, ny), dtype=np.int64)
    index[used] = np.arange(1, used.sum() + 1)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    tris = 0
    with open(path, "w") as handle:
        handle.write("# Ger_FrozenLake backdrop (fl_backdrop_gen.py), UE cm, y written negated (OBJ import mirrors y)\n")
        for xv, yv, zv in zip(X[used], Y[used], z[used]):
            handle.write("v %.1f %.1f %.1f\n" % (xv, -yv, zv))
        for xv, yv in zip(X[used], Y[used]):
            handle.write("vt %.5f %.5f\n" % (xv / 10000.0, yv / 10000.0))
        for i, j in zip(*np.nonzero(keep)):
            a, b, cc, d = index[i, j], index[i + 1, j], index[i + 1, j + 1], index[i, j + 1]
            # imports face-up, like the ice mesh: in UE coordinates the triangle runs clockwise seen from
            # above (the reverse order was checked to import face-down, 2026-11-24)
            handle.write("f %d/%d %d/%d %d/%d\n" % (a, a, d, d, b, b))
            handle.write("f %d/%d %d/%d %d/%d\n" % (b, b, d, d, cc, cc))
            tris += 2
    return tris


def build(concept):
    c = CONCEPTS[concept]
    out = os.path.join(OUT_ROOT, concept)
    os.makedirs(out, exist_ok=True)
    raw = load_landscape()
    xs, ys = axis(GRID_X), axis(GRID_Y)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    z, _ = surface(c, raw, X, Y)
    # leave out quads the landscape hides: well inside its square and south of the old shore line
    hidden = (np.abs(X) < LS_HALF - HOLE_INSET) & (np.abs(Y) < LS_HALF - HOLE_INSET) & (X < join_x(Y) - HOLE_INSET)
    keep = ~(hidden[:-1, :-1] & hidden[1:, :-1] & hidden[:-1, 1:] & hidden[1:, 1:])
    tris = write_obj(os.path.join(out, "SM_FL_Backdrop.obj"), xs, ys, z, keep)
    # mask: image row = x from north (top) to south, col = y west..east, so the PNG reads like a map, north up
    mx = np.arange(MESH_X[1], MESH_X[0] - 1, -MASK_CELL)
    my = np.arange(MESH_Y[0], MESH_Y[1] + 1, MASK_CELL)
    MX, MY = np.meshgrid(mx, my, indexing="ij")
    mz, mask = build_mask(c, raw, MX, MY)
    Image.fromarray((mask * 255).astype(np.uint8), "RGBA").save(os.path.join(out, "T_FL_BackdropMask.png"))
    # landscape: push the margin past the join under the mesh
    li = LS_CORNER + np.arange(LS_V) * LS_QUAD
    LX, LY = np.meshgrid(li, li, indexing="xy")                # [j (y), i (x)]
    lz, lp = surface(c, raw, LX, LY)
    h = (raw.astype(np.float64) - 32768.0) * 100.0 / 128.0
    past = LX > join_x(LY) + 50.0
    h2 = np.where(past, np.minimum(h, lz - 150.0), h)
    # the canal's dead end: cut it through the margin to the bluff (the cut is the mesh's own canal, tucked)
    cut = (lp["channel"] > 0.02) & ~past & (LX > PLAY_EDGE + 300.0)
    h2 = np.where(cut, np.minimum(h2, lz + TUCK - 10.0), h2)
    past = past | cut
    changed = int((np.abs(h2 - h) > 0.5).sum())
    inside = int(((np.abs(h2 - h) > 0.5) & (np.abs(LX) <= PLAY_EDGE) & (np.abs(LY) <= PLAY_EDGE)).sum())
    np.clip(np.round(h2 * 128.0 / 100.0 + 32768.0), 0, 65535).astype("<u2").tofile(
        os.path.join(out, "FL_height_backdrop.raw"))
    # preview: hillshade x mask tint, north up
    gz = np.gradient(mz, MASK_CELL)
    shade = np.clip(0.55 + 0.45 * (-gz[0] * 0.6 + gz[1] * 0.6) / np.sqrt(1 + gz[0] ** 2 + gz[1] ** 2) * 3, 0, 1)
    rgb = np.stack([shade * 0.78, shade * 0.74, shade * 0.66], -1)
    rgb = rgb * (1 - 0.8 * mask[..., :1])
    rgb = rgb * (1 - mask[..., 1:2]) + mask[..., 1:2] * np.array([0.10, 0.16, 0.18])
    rgb = rgb * (1 - mask[..., 2:3]) + mask[..., 2:3] * np.array([1.0, 0.55, 0.1])
    rgb = rgb * (1 - mask[..., 3:4]) + mask[..., 3:4] * np.array([0.05, 0.08, 0.12])
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((rgb.shape[1] // 4, rgb.shape[0] // 4)).save(
        os.path.join(out, "preview.png"))
    info = dict(concept=concept, title=c["title"], mesh_x=MESH_X, mesh_y=MESH_Y, mask_cell=MASK_CELL, rim_half=LS_HALF,
                crater=c["crater"], radius=c["radius"], glow_strength=c["glow_strength"], z_range=[float(z.min()), float(z.max())],
                tris=tris, landscape_vertices_changed=changed,
                landscape_vertices_changed_in_play_area=inside)
    with open(os.path.join(out, "backdrop.json"), "w") as handle:
        json.dump(info, handle, indent=1)
    print(json.dumps(info))
    return info


if __name__ == "__main__":
    for name in (sys.argv[1:] or BUILD_CONCEPTS):
        build(name)
