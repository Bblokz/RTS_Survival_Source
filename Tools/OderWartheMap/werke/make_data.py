"""Build the nine concepts: heightfield + colours (npz), objects (json), site fit check, site shorelines."""
import json, math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib
import concepts as C
CM = importlib.import_module(sys.argv[1]) if len(sys.argv) > 1 else C

OUT = os.path.dirname(os.path.abspath(__file__))
TER = os.path.join(OUT, "..", "terrain")
L = json.load(open(os.path.join(TER, "OW_layout.json")))
M = np.load(os.path.join(TER, "OW_masks.npz"))
WATER = M["water"] | M["bog"]
ROAD = M["road_near"] | M["rail_near"]
OTHER = [(f["id"], f["kind"], f["points"][0]) for f in L["features"] if f["points"] and f["kind"] in
         ("resource", "capture", "enemy_group", "enemy_spawn", "enemy_base", "ford")]


def world(site, lx, ly):
    w = L["werks"][site]
    cx, cy = w["centre"]
    a = math.radians(w["yaw"])
    return cx + 100.0 * (lx * math.cos(a) - ly * math.sin(a)), cy + 100.0 * (lx * math.sin(a) + ly * math.cos(a))


def mask_at(mask, wx, wy):
    j = int(round((wx + 44100.0) / 100.0)); i = int(round((wy + 44100.0) / 100.0))
    if 0 <= i < mask.shape[0] and 0 <= j < mask.shape[1]:
        return bool(mask[i, j])
    return True


def site_shores():
    """Each site's water/bog in the Werk frame on a 1 m grid over the sketch area."""
    ax = np.arange(-90.0, 90.5, 1.0)
    out = {}
    for site in L["werks"]:
        g = np.zeros((ax.size, ax.size), bool)
        for j, ly in enumerate(ax):
            for i, lx in enumerate(ax):
                g[j, i] = mask_at(WATER, *world(site, lx, ly))
        out[site] = g
    return ax, out


def colours(w):
    H = w.H
    gy, gx = np.gradient(H, C.STEP)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    n = 0.5 + 0.5 * np.sin(C.X / 3.1 + np.cos(C.Y / 4.3) * 2.0) * np.cos(C.Y / 2.7)
    n = 0.5 + 0.5 * np.sin(C.X / 23.0 + 0.7 * np.cos(C.Y / 17.0)) * np.cos(C.Y / 29.0)
    grass = np.stack([0.42 + 0.025 * n, 0.41 + 0.025 * n, 0.30 + 0.012 * n], -1)
    sod = np.array([0.35, 0.38, 0.23])
    earth = np.array([0.37, 0.32, 0.23])
    gravel = np.array([0.55, 0.53, 0.48])
    mud = np.array([0.24, 0.20, 0.16])
    col = grass.copy()
    def mix(c, wgt):
        nonlocal col
        wgt = np.clip(wgt, 0.0, 1.0)[..., None]
        col = col * (1 - wgt) + np.asarray(c) * wgt
    mix(sod, 0.7 * w.paint["sod"] + 0.6 * C.smoothstep(12.0, 22.0, slope) * (1 - C.smoothstep(34.0, 42.0, slope)))
    mix(earth, 0.85 * C.smoothstep(33.0, 44.0, slope))
    mix(gravel, w.paint["gravel"])
    mix(mud, w.paint["mud"])
    return col.astype(np.float32), slope


def fit(w, shores_ax, shores):
    pts = []
    foot = (np.abs(w.H) > 0.5)
    for j in range(0, C.N, 2):
        for i in range(0, C.N, 2):
            if foot[j, i]:
                pts.append((C.AX[i], C.AX[j]))
    for o in w.objs:
        if o["t"] == "kit" and o["k"].startswith("tooth"):
            pts.append(tuple(o["c"]))
    res = {}
    for site in L["werks"]:
        wet, road, feats = [], 0, set()
        for lx, ly in pts:
            wx, wy = world(site, lx, ly)
            if mask_at(WATER, wx, wy):
                wet.append((lx, ly))
            elif mask_at(ROAD, wx, wy):
                road += 1
        for fid, kind, p in OTHER:
            if fid in (site, "garr_" + site, "tunnel_" + site):
                continue
            q = (p[0] - L["werks"][site]["centre"][0], p[1] - L["werks"][site]["centre"][1])
            a = math.radians(L["werks"][site]["yaw"])
            lx = (q[0] * math.cos(a) + q[1] * math.sin(a)) / 100.0
            ly = (-q[0] * math.sin(a) + q[1] * math.cos(a)) / 100.0
            if abs(lx) < 90 and abs(ly) < 90:
                d = min(math.hypot(lx - x, ly - y) for x, y in pts[::5])
                if d < 12.0:
                    feats.add(fid)
        area = len(wet) * 1.0                                   # every 2nd cell of 0.5 m = 1 m2
        sides = []
        if wet:
            for name, test in (("front", lambda x, y: x > 15), ("back", lambda x, y: x < -15),
                               ("right", lambda x, y: y > 15), ("left", lambda x, y: y < -15)):
                k = sum(1 for x, y in wet if test(x, y))
                if k > 0.15 * len(wet):
                    sides.append(name)
        res[site] = {"wet_m2": round(area), "sides": sides, "road": road, "near": sorted(feats)}
    return res


def main():
    shores_ax, shores = site_shores()
    np.savez_compressed(os.path.join(OUT, "shores.npz"), ax=shores_ax, **shores)
    summary = {}
    for fn in CM.CONCEPTS:
        w = fn()
        col, slope = colours(w)
        np.savez_compressed(os.path.join(OUT, "c%s.npz" % w.key), H=w.H.astype(np.float32), col=col,
                            **{k: np.clip(v, 0, 1).astype(np.float32) for k, v in w.paint.items()})
        foot = np.abs(w.H) > 0.5
        xs = C.X[foot]; ys = C.Y[foot]
        info = {"key": w.key, "title": w.title, "name": w.name, "objs": w.objs, "water": w.water, "hq": w.hq,
                "notes": w.notes, "walls": w.walls, "hero": list(w.hero), "teeth_line": getattr(w, "teeth_line", []),
                "teeth_gaps": getattr(w, "teeth_gaps", []),
                "dims": {"x": [float(xs.min()), float(xs.max())], "y": [float(ys.min()), float(ys.max())],
                         "hmax": float(w.H.max()), "hmin": float(w.H.min()),
                         "cupolas": sum(1 for o in w.objs if o["t"] == "kit" and o["k"] == "cupola"),
                         "teeth": sum(1 for o in w.objs if o["t"] == "kit" and o["k"].startswith("tooth"))},
                "fit": fit(w, shores_ax, shores)}
        json.dump(info, open(os.path.join(OUT, "c%s.json" % w.key), "w"), indent=1)
        summary[w.key] = (w.title, info["dims"], info["fit"])
        print(w.key, w.title, "%.0f x %.0f m, h %.1f" % (xs.max() - xs.min(), ys.max() - ys.min(), w.H.max()),
              {s: (f["wet_m2"], f["sides"], f["near"]) for s, f in info["fit"].items()})


main()
