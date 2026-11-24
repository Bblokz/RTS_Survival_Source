"""Gap and float check for the Werke: nothing may hover over the landscape, no bunker foot may show a gap, and no door
or embrasure may be filled with ground.

    python Source/Tools/OderWartheMap/ow_werk_gaps.py      (after ow_terrain_gen.py, ow_dress_gen.py and the kit bounds dump)

Works from the same data the map is built from: the landscape heights file (ow_map_build.ground_z: bilinear, as placed),
OW_dress.json (where every Werk piece went) and werke/c<key>.json (each bunker's footprint and base). Kit mesh bounds come
from Saved/OderWarthe/terrain/werk_kit_bounds.json (dumped in the editor by ow_werk_kit_bounds()).

Checks per Werk:
- bunker feet: along every footprint edge (5 cm outside, every 25 cm) the landscape must stand over the concrete's buried
  foot (the mesh runs FOOT under each module's base): a lower landscape is a visible gap under the concrete;
- wing walls: same along both faces;
- door and embrasure thresholds: in the 2.6 m strip behind every face the landscape must stay under the module's floor;
- kit pieces (cupolas, mast, rocks, teeth): the landscape under the rim of the mesh's base must reach the base (no float).
Writes Saved/OderWarthe/terrain/werk_gaps.txt; exit code 1 on any failure.
"""
import array
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
TERRAIN = os.path.join(ROOT, "Saved", "OderWarthe", "terrain")
FOOT_CM = 40.0
TOL_CM = 3.0

with open(os.path.join(TERRAIN, "OW_layout.json")) as handle:
    LAYOUT = json.load(handle)
LAY = LAYOUT["landscape"]
with open(LAYOUT["files"]["height"], "rb") as handle:
    HEIGHTS = array.array("H", handle.read())
with open(os.path.join(TERRAIN, "OW_dress.json")) as handle:
    DRESS = json.load(handle)


def ground_z(x, y):
    vx, vy = LAY["verts_x"], LAY["verts_y"]
    fx = min(max((x - LAY["corner"][0]) / LAY["quad_cm"], 0.0), vx - 1.001)
    fy = min(max((y - LAY["corner"][1]) / LAY["quad_cm"], 0.0), vy - 1.001)
    ix, iy = int(math.floor(fx)), int(math.floor(fy))
    tx, ty = fx - ix, fy - iy

    def h(i, j):
        return (HEIGHTS[j * vx + i] - 32768.0) * 100.0 / 128.0

    return (h(ix, iy) * (1 - tx) * (1 - ty) + h(ix + 1, iy) * tx * (1 - ty) + h(ix, iy + 1) * (1 - tx) * ty +
            h(ix + 1, iy + 1) * tx * ty)


def werk_xy(w, lx, ly):
    yaw = math.radians(w["yaw"])
    c = w["centre"]
    return c[0] + lx * math.cos(yaw) - ly * math.sin(yaw), c[1] + lx * math.sin(yaw) + ly * math.cos(yaw)


def rect_edge(cx, cy, sx, sy, yaw, out=5.0, step=25.0):
    """Points along a rectangle's edges, `out` cm outside (local cm, yaw degrees)."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    hx, hy = sx / 2.0 + out, sy / 2.0 + out
    pts = []
    for (ax, ay), (bx, by) in (((hx, -hy), (hx, hy)), ((hx, hy), (-hx, hy)), ((-hx, hy), (-hx, -hy)), ((-hx, -hy), (hx, -hy))):
        n = max(1, int(math.hypot(bx - ax, by - ay) / step))
        for k in range(n):
            u = k / n
            lx, ly = ax + (bx - ax) * u, ay + (by - ay) * u
            pts.append((cx + lx * c - ly * s, cy + lx * s + ly * c))
    return pts


def openings(o):
    """Sample points (label, Werk-frame cm, opening bottom over the Werk base in cm) where the ground must stay under an
    opening: door sills (in front and in the recess), embrasures, ports and observation slots (werk_parts.py sizes)."""
    k, p = o["k"], o["p"]
    W, D, H = p["W"], p["D"], p["H"]
    z0 = o["z"] * 100.0
    fx, fy, yaw = o["c"][0] * 100.0, o["c"][1] * 100.0, o["yaw"]
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    out = []

    def add(label, lx, ly, bottom):
        out.append((label, fx + lx * c - ly * s, fy + lx * s + ly * c, bottom))

    def door(ly, face=0.0, sign=1.0, half=60.0):
        for dx in (-30.0, 20.0, 100.0, 200.0):
            for dy in (-half, 0.0, half):
                add("door", sign * (face + dx), ly + dy, z0 + 25.0)

    def front(label, ly, bottom, half):
        for dx in (-140.0, -50.0, 50.0, 200.0):
            for dy in (-half, 0.0, half):
                add(label, dx, ly + dy, bottom)

    sg = p.get("pier", 1)
    if k == "regelbau":
        if p.get("door"):
            door(-sg * W * 12.0)
        if p.get("front_emb"):
            front("embrasure", -sg * 80.0, z0 + H * 40.0 - 54.0, 90.0)
        else:
            front("port", -sg * (W * 50.0 - 160.0), z0 + H * 45.0, 40.0)
        if p.get("side_emb"):
            for dx in (-420.0, -320.0, -220.0):
                for d in (20.0, 100.0, 300.0):
                    add("side embrasure", dx, sg * (W * 50.0 + d), z0 + H * 42.0 - 36.0)
    elif k == "eingang":
        door(-120.0)
        front("port", 160.0, z0 + 90.0, 20.0)
    elif k == "leitstand":
        door(0.0, face=D * 50.0, sign=-1.0)
    elif k == "peilkasematte":
        half = W * 50.0 - 2.4 * 60.0 - 30.0
        front("comb slot", 0.0, z0 - 30.0 + H * 42.0, half)
    elif k == "peilturm" and p.get("annex"):
        for dx in (150.0, 250.0):
            add("annex port", dx, -W * 50.0 - 300.0, z0 + H * 14.0 - 30.0)
    return [(label, x, y, b) for label, x, y, b in out]


def main():
    with open(os.path.join(TERRAIN, "werk_kit_bounds.json")) as handle:
        bounds = json.load(handle)
    lines, fails = [], 0
    pivots = {m[0]: m for m in DRESS["meshes"]}
    for fid in sorted(LAYOUT["werks"]):
        w = LAYOUT["werks"][fid]
        with open(os.path.join(HERE, "werke", "c%s.json" % w["design"])) as handle:
            info = json.load(handle)
        _lab, _path, px, py, _pyaw, pdz, _f, _s = pivots["Werk_%s_Bunkers" % fid]
        pivot_z = ground_z(px, py) + pdz
        worst_gap, worst_fill, where_gap, where_fill, n_pts = 0.0, 0.0, None, None, 0
        for o in info["objs"]:
            if o["t"] == "box" and o.get("fp"):
                bottom = pivot_z + 100.0 * o["z"] - FOOT_CM
                for lx, ly in rect_edge(o["c"][0] * 100.0, o["c"][1] * 100.0, o["s"][0] * 100.0, o["s"][1] * 100.0, o["yaw"]):
                    x, y = werk_xy(w, lx, ly)
                    gap = bottom - ground_z(x, y)
                    n_pts += 1
                    if gap > worst_gap:
                        worst_gap, where_gap = gap, (round(lx / 100.0, 1), round(ly / 100.0, 1))
            elif o["t"] == "wedge":
                (ax, ay), (bx, by) = o["a"], o["b"]
                bottom = pivot_z + 100.0 * o["z"] - 30.0
                nx, ny = -(by - ay), bx - ax
                ln = math.hypot(nx, ny)
                for k in range(21):
                    u = k / 20.0
                    for sgn in (-1, 1):
                        lx = (ax + (bx - ax) * u) * 100.0 + sgn * nx / ln * (o["th"] * 50.0 + 5.0)
                        ly = (ay + (by - ay) * u) * 100.0 + sgn * ny / ln * (o["th"] * 50.0 + 5.0)
                        x, y = werk_xy(w, lx, ly)
                        gap = bottom - ground_z(x, y)
                        n_pts += 1
                        if gap > worst_gap:
                            worst_gap, where_gap = gap, (round(lx / 100.0, 1), round(ly / 100.0, 1))
            if o["t"] == "mod":
                for label, lx, ly, bottom in openings(o):
                    x, y = werk_xy(w, lx, ly)
                    fill = ground_z(x, y) - (pivot_z + bottom)
                    if fill > worst_fill:
                        worst_fill, where_fill = fill, (o["k"], label, round(lx / 100.0, 1), round(ly / 100.0, 1))
        ok_gap, ok_fill = worst_gap <= TOL_CM, worst_fill <= TOL_CM
        fails += (not ok_gap) + (not ok_fill)
        lines.append("%-4s %s %s (%s): bunker feet %d points, worst gap %.0f cm at %s" % (
            "ok" if ok_gap else "FAIL", fid, w["design"], "pivot z %.0f" % pivot_z, n_pts, worst_gap, where_gap))
        lines.append("%-4s %s openings: worst ground over an opening bottom %.0f cm at %s" % (
            "ok" if ok_fill else "FAIL", fid, worst_fill, where_fill))
    # kit pieces round every Werk: HISM groups Werk_* and the tooth mesh actors
    items = []
    for group, g in DRESS["inst"].items():
        if group.startswith("Werk_") or group == "Kit_SM_OW_TunnelPortal":
            name = g["mesh"].split(".")[-1]
            for x, y, dz, yaw, scale, _p, _r in g["items"]:
                items.append((group, name, x, y, dz, yaw, scale))
    for lab, path, x, y, yaw, dz, folder, scale in DRESS["meshes"]:
        if "_Tooth_" in lab:
            items.append(("Tooth", path.split(".")[-1], x, y, dz, yaw, scale))
    worst = {}
    for group, name, x, y, dz, yaw, scale in items:
        b = bounds.get(name)
        if b is None:
            lines.append("FAIL no bounds for %s" % name)
            fails += 1
            continue
        (mnx, mny, mnz), (mxx, mxy, _mxz) = b
        base_z = ground_z(x, y) + dz + mnz * scale
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        float_cm = 0.0
        for fx_, fy_ in ((0.85, 0.0), (-0.85, 0.0), (0.0, 0.85), (0.0, -0.85), (0.6, 0.6), (-0.6, 0.6), (0.6, -0.6), (-0.6, -0.6)):
            lx = (mxx + mnx) / 2.0 + fx_ * (mxx - mnx) / 2.0
            ly = (mxy + mny) / 2.0 + fy_ * (mxy - mny) / 2.0
            wx, wy = x + (lx * c - ly * s) * scale, y + (lx * s + ly * c) * scale
            float_cm = max(float_cm, base_z - ground_z(wx, wy))
        key = group
        if float_cm > worst.get(key, (-1e9,))[0]:
            worst[key] = (float_cm, round(x), round(y))
    for key, (f, x, y) in sorted(worst.items()):
        ok = f <= TOL_CM
        fails += not ok
        lines.append("%-4s kit %-28s worst float %.0f cm (at %d, %d)" % ("ok" if ok else "FAIL", key, f, x, y))
    text = "\n".join(lines)
    with open(os.path.join(TERRAIN, "werk_gaps.txt"), "w") as handle:
        handle.write(text + "\n")
    print(text)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
