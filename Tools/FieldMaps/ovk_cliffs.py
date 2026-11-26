"""Lay the Ore cliff kit (kit C, fused natural crag) along both walls of the Floeha gorge on Field_OreViadukt.

    python Source/Tools/FieldMaps/ovk_cliffs.py        (after ovk_terrain_gen.py)

Reuses the kit's own placer (ock_place.Layout.crag_face, Blender project OreCliffKit/Model, imported read-only) with
gorge-wall stations built here. Writes Saved/FieldMaps/OreViadukt/terrain/OVK_cliffs.json in the same format as
Ger_2ndOreMountains' Ore_cliffs.json (ovk_map_build.step_cliffs places it: instanced, no collision, no nav effect).
Gaps are left where the viaduct, the trestle and the two ford tracks meet the walls.
"""
import json
import math
import os
import sys

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
KIT = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreCliffKit/Model"
sys.dont_write_bytecode = True
sys.path.insert(0, KIT)
import ock_map  # noqa: E402
import ock_place  # noqa: E402

TERRAIN = os.path.join(ROOT, "Saved", "FieldMaps", "OreViadukt", "terrain")
LAYOUT = os.path.join(TERRAIN, "OVK_layout.json")
OUT = os.path.join(TERRAIN, "OVK_cliffs.json")
REACH_PAST_EDGE_CM = 4500.0     # dress the walls this far into the camera margin
GAP_HALF_M = {"viaduct": 17.0, "trestle": 6.0}


class HeightMap:
    """z(x, y) in metres from the generator's raw heights (the placer's ground anchor for loose blocks)."""

    def __init__(self, lay):
        self.l = lay["landscape"]
        self.h = np.fromfile(lay["files"]["height"], dtype="<u2").reshape(self.l["verts_y"], self.l["verts_x"])

    def z(self, x, y):
        l = self.l
        fx = min(max((x * 100.0 - l["corner"][0]) / l["quad_cm"], 0.0), l["verts_x"] - 1.001)
        fy = min(max((y * 100.0 - l["corner"][1]) / l["quad_cm"], 0.0), l["verts_y"] - 1.001)
        ix, iy = int(fx), int(fy)
        tx, ty = fx - ix, fy - iy
        h = lambda i, j: (float(self.h[j, i]) - 32768.0) / 128.0
        return (h(ix, iy) * (1 - tx) * (1 - ty) + h(ix + 1, iy) * tx * (1 - ty) + h(ix, iy + 1) * (1 - tx) * ty +
                h(ix + 1, iy + 1) * tx * ty)


def stations(axis_cm, side, rim_cm, foot_cm, lip_z, foot_z, bounds, reach):
    """One-metre stations along one wall. side +1 = NE wall, -1 = SW wall. Returns s, lip, foot, out, tan (metres)."""
    pts = np.asarray(axis_cm, float) / 100.0
    seg = np.diff(pts, axis=0)
    L = np.r_[0.0, np.cumsum(np.hypot(*seg.T))]
    s_axis = np.arange(0.0, L[-1], 1.0)
    px = np.interp(s_axis, L, pts[:, 0])
    py = np.interp(s_axis, L, pts[:, 1])
    tx, ty = np.gradient(px), np.gradient(py)
    n = np.hypot(tx, ty)
    tx, ty = tx / n, ty / n
    nx, ny = ty, -tx                       # right-hand normal of a NW -> SE walk = the NE side
    flip = (nx + ny) < 0
    nx, ny = np.where(flip, -nx, nx), np.where(flip, -ny, ny)
    lipx, lipy = px + side * nx * rim_cm / 100.0, py + side * ny * rim_cm / 100.0
    footx, footy = px + side * nx * foot_cm / 100.0, py + side * ny * foot_cm / 100.0
    lim_x, lim_y = (bounds["xmax"] + reach) / 100.0, (bounds["ymax"] + reach) / 100.0
    keep = (np.abs(lipx) < lim_x) & (np.abs(lipy) < lim_y)
    return s_axis[keep], np.c_[lipx, lipy, np.full_like(lipx, lip_z)][keep], \
        np.c_[footx, footy, np.full_like(footx, foot_z)][keep], np.c_[-side * nx, -side * ny][keep], np.c_[tx, ty][keep]


def split(s, lip, foot, out, tan, gaps):
    """Segments between the gaps (ranges of axis arc length, metres)."""
    segs = []
    ok = np.ones(len(s), dtype=bool)
    for a, b in gaps:
        ok &= ~((s > a) & (s < b))
    idx = np.flatnonzero(ok)
    if len(idx) == 0:
        return segs
    runs = np.split(idx, np.flatnonzero(np.diff(idx) > 1) + 1)
    for k, run in enumerate(runs):
        if len(run) < 6:
            continue
        ss = s[run] - s[run][0]
        segs.append(dict(length=float(ss[-1]), s=ss, lip=lip[run], foot=foot[run], out=out[run], tan=tan[run],
                         open0=bool(run[0] > 0), open1=bool(run[-1] < len(s) - 1)))
    return segs


def main():
    lay = json.load(open(LAYOUT))
    g = lay["gorge"]
    axis = g["axis"]
    feats = {f["id"]: f for f in lay["features"]}
    pts = np.asarray(axis, float)
    L = np.r_[0.0, np.cumsum(np.hypot(*np.diff(pts, axis=0).T))] / 100.0

    def s_of(p):
        d = np.hypot(pts[:, 0] - p[0], pts[:, 1] - p[1])
        return float(L[int(np.argmin(d))])

    crossings = json.load(open(os.path.join(ROOT, "Saved", "FieldMaps", "OreViadukt", "crossings.json")))
    gaps_both = []
    for key in ("viaduct", "trestle"):
        c = crossings[key]["centre_cm"]
        s0 = s_of(c)
        gaps_both.append((s0 - GAP_HALF_M[key], s0 + GAP_HALF_M[key]))
    # ford tracks run down the wall: the NE track on the downstream side, the SW track upstream (gen_convoy_ore)
    sf = s_of(g["ford_centre"])
    gaps = {+1: gaps_both + [(sf - 4.0, sf + 100.0)], -1: gaps_both + [(sf - 100.0, sf + 4.0)]}
    ock_place.SLOPE = 0.55                  # our walls drop 14 m over 9 m (0.64 m/m); the kit's faces were 0.36
    m = HeightMap(lay)
    layout = ock_place.Layout(m, seed=1946)
    fs = {}
    for side, face in ((+1, "gorge_ne"), (-1, "gorge_sw")):
        st = stations(axis, side, g["rim_cm"], g["floor_edge_cm"], 0.0, g["floor_z"] / 100.0, lay["bounds"],
                      REACH_PAST_EDGE_CM)
        fs[face] = split(*st, gaps[side])
        for seg in fs[face]:
            layout.crag_face(face, seg)
    counts, per_face = {}, {}
    for i in layout.inst:
        counts[i["piece"]] = counts.get(i["piece"], 0) + 1
        per_face[i["face"]] = per_face.get(i["face"], 0) + 1
    tris = sum(ock_place.PIECES[i["piece"]].get("triangles_lod0", 0) for i in layout.inst)
    doc = dict(map="Field_OreViadukt", units="cm and degrees, Unreal world", kit="OreCliffKit",
               convention="as Ore_cliffs.json (piece +X = downhill across the face)", assignment={"gorge": "C"},
               mesh_path="/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Meshes/SM_OreCliff_<piece>",
               face_lengths_m={k: round(sum(s["length"] for s in v), 1) for k, v in fs.items()},
               gaps_m={"both": gaps_both, "ford": sf}, counts=counts, per_face=per_face,
               total_instances=len(layout.inst), triangles_lod0=tris, instances=layout.inst)
    with open(OUT, "w") as handle:
        json.dump(doc, handle, separators=(",", ":"))
    print("PLACED", len(layout.inst), "instances", per_face, "tris LOD0", tris)
    print("  counts", dict(sorted(counts.items())))
    print("  wrote", OUT)


if __name__ == "__main__":
    main()
