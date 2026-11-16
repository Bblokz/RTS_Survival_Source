"""Dressing generator for Ger_OderWarthe: every tree, prop, site object and effect as one JSON for ow_map_build.step_dress.

    python Source/Tools/OderWartheMap/ow_dress_gen.py

Reads Saved/OderWarthe/terrain/OW_layout.json + OW_masks.npz (ow_terrain_gen.py) and the kit manifest
Saved/OderWarthe/assets/ow_kit.json (the Oder-Warthe kit: cupolas, Werk block, tunnel portal, sluice gate, direction-finder
stations, dragon's teeth, dead pines, erratics). Kit pieces that are not imported yet fall back to existing project assets
(stand-ins), so the map can be built before the kit lands; rerun once it has.

Rules (critic round 0): the five Werke come from ONE template placed by centre + yaw only; every resource and station
keeps 90-100 m from every tunnel mouth (the yaws are chosen in the terrain generator); a 25 m clear capture disc at each
station; keep-outs round every site; no trees in the 85 m build circle; nothing on the choke decks.
"""
import json
import math
import os

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TERRAIN = os.path.join(ROOT, "Saved", "OderWarthe", "terrain")
KIT_FILE = os.path.join(ROOT, "Saved", "OderWarthe", "assets", "ow_kit.json")
PRY_ASSETS = os.path.join(ROOT, "Saved", "Pripyat", "assets", "pripyat_assets.json")
E = "/Game/RTS_Survival/Blueprints/Environment/"
D = E + "DestructableEnvActor/"
NAT = "/Game/RTS_Survival/Meshes/Nature/"
RES = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/"
SB = D + "NaturalCover/SandBagWalls/"
BAR = D + "Military/Barricades/"
PRY = E + "Prypyat/"

with open(os.path.join(TERRAIN, "OW_layout.json")) as handle:
    LAYOUT = json.load(handle)
_npz = np.load(os.path.join(TERRAIN, "OW_masks.npz"))
M = {k: _npz[k] for k in _npz.files}            # [iy, ix]; copy once (np.load unpacks on every access)
LAY = LAYOUT["landscape"]
SIZE = LAY["size_x"]
B = LAYOUT["bounds"]
WERKS = LAYOUT["werks"]
RNG = np.random.default_rng(5207)

BPS, INST, NOTES, FX, SPLINES, DECALS, MESHES, NAVBLOCKS = [], {}, [], [], [], [], [], []
TANK_BLOCK_AREA = "/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/RTS_NavArea_BlockAllTanks"
KEEP = []                                       # (x, y, radius): no scattered nature here
STANDINS = set()


# ---- helpers ------------------------------------------------------------------------------------------------------
def cell(x, y):
    ix = int(round((x + SIZE / 2.0) / 100.0))
    iy = int(round((y + SIZE / 2.0) / 100.0))
    return min(max(iy, 0), LAY["verts_y"] - 1), min(max(ix, 0), LAY["verts_x"] - 1)


def at(name, x, y):
    return M[name][cell(x, y)]


def inside(x, y, margin=0.0):
    return B["xmin"] + margin <= x <= B["xmax"] - margin and B["ymin"] + margin <= y <= B["ymax"] - margin


def feature(fid):
    return next(f for f in LAYOUT["features"] if f["id"] == fid)


def fpoints(fid):
    return feature(fid)["points"]


def region(rid):
    return next(r for r in LAYOUT["regions"] if r["id"] == rid)


def road(rid):
    return LAYOUT["roads"][rid]["points"]          # [x, y, h, half width] every 3 m


def bearing_yaw(dx, dy):
    return math.degrees(math.atan2(dy, dx))


def bp(label, path, x, y, yaw=0.0, dz=0.0, folder="Props", scale=1.0):
    BPS.append([label, path, round(float(x), 1), round(float(y), 1), round(float(yaw) % 360.0, 1), round(float(dz), 1), folder,
                round(float(scale), 3)])


def inst(group, mesh, x, y, yaw=None, scale=1.0, dz=0.0, collision=False, pitch=0.0, roll=0.0, shadow=True):
    g = INST.setdefault(group, {"mesh": mesh, "collision": collision, "items": [], "shadow": shadow})
    yaw = RNG.uniform(0, 360) if yaw is None else yaw
    g["items"].append([round(float(x), 1), round(float(y), 1), round(float(dz), 1), round(float(yaw), 1),
                       round(float(scale), 3), round(float(pitch), 1), round(float(roll), 1)])


def mesh_actor(label, path, x, y, yaw=0.0, dz=0.0, folder="Props", scale=1.0):
    """A single StaticMeshActor (cuts the navmesh; instanced components did not)."""
    MESHES.append([label, path, round(float(x), 1), round(float(y), 1), round(float(yaw) % 360.0, 1), round(float(dz), 1), folder,
                   round(float(scale), 3)])


def fx(label, system, x, y, dz=0.0, scale=1.0):
    FX.append([label, system, round(float(x), 1), round(float(y), 1), round(float(dz), 1), round(float(scale), 2)])


def decal(label, material, x, y, size, yaw=None):
    DECALS.append([label, material, round(float(x), 1), round(float(y), 1),
                   round(float(RNG.uniform(0, 360) if yaw is None else yaw), 1), round(float(size), 1)])


def note(nid, text, x, y):
    NOTES.append([nid, text, round(float(x), 1), round(float(y), 1)])


def keep(x, y, r):
    KEEP.append((float(x), float(y), float(r)))


def kept(x, y):
    for kx, ky, kr in KEEP:
        if (x - kx) ** 2 + (y - ky) ** 2 < kr * kr:
            return True
    return False


def _manifest(path):
    if not os.path.exists(path):
        return []
    try:
        with open(path) as handle:
            data = json.load(handle)
    except Exception:
        return []
    entries = data.get("assets", data) if isinstance(data, dict) else data
    if isinstance(entries, dict):
        entries = [dict(v, id=v.get("id", k)) for k, v in entries.items()]
    return [e for e in entries if isinstance(e, dict)]


KIT = {e.get("id"): e for e in _manifest(KIT_FILE)}
PRYK = {e.get("id"): e for e in _manifest(PRY_ASSETS)}


def kit(aid):
    """Asset path of an Oder-Warthe kit piece by manifest id (or asset name), None while it is not imported."""
    entry = KIT.get(aid)
    if entry is None:
        for e in KIT.values():
            if str(e.get("asset_path", "")).split("/")[-1].split(".")[0] == aid:
                entry = e
                break
    if entry is None or entry.get("status") in ("failed", "pending"):
        return None
    return entry.get("asset_path")


def kits_like(prefix):
    out = []
    for e in KIT.values():
        name = str(e.get("asset_path", "")).split("/")[-1].split(".")[0]
        if name.startswith(prefix) and e.get("status") not in ("failed", "pending"):
            # Blueprints by package path (load_blueprint_class does not take the generated class path)
            out.append(e.get("package") or e["asset_path"].split(".")[0] if e.get("kind") == "blueprint" else e["asset_path"])
    return sorted(out)


def pry(aid):
    e = PRYK.get(aid)
    return e["asset_path"] if e else None


def piece(group, kit_name, x, y, yaw, dz=0.0, collision=True, fallback=None, fb_scale=1.0, fb_dz=0.0, fb_yaw=0.0):
    """One instance of a kit mesh; until the kit has it, a stand-in mesh (logged)."""
    path = kit(kit_name)
    if path:
        inst("Kit_" + kit_name, path, x, y, yaw, 1.0, dz, collision)
        return True
    if fallback:
        STANDINS.add(kit_name)
        inst("Standin_" + kit_name, fallback, x, y, yaw + fb_yaw, fb_scale, dz + fb_dz, collision)
        return False
    STANDINS.add(kit_name)
    return False


def scatter(count, accept, margin=-9000.0, tries=40):
    out = []
    lo_x, hi_x = B["xmin"] + margin, B["xmax"] - margin
    lo_y, hi_y = B["ymin"] + margin, B["ymax"] - margin
    lim = SIZE / 2.0 - 300.0
    n = 0
    while len(out) < count and n < count * tries:
        n += 1
        x = RNG.uniform(max(lo_x, -lim), min(hi_x, lim))
        y = RNG.uniform(max(lo_y, -lim), min(hi_y, lim))
        if accept(x, y):
            out.append((x, y))
    return out


def walk(rid, step):
    """Points every step cm along a corridor: (x, y, yaw, half width, crown height)."""
    pts = road(rid)
    out, acc = [], 0.0
    for i in range(1, len(pts)):
        seg = math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1])
        acc += seg
        if acc >= step:
            acc -= step
            j0, j1 = max(i - 2, 0), min(i + 2, len(pts) - 1)
            yaw = bearing_yaw(pts[j1][0] - pts[j0][0], pts[j1][1] - pts[j0][1])
            out.append((pts[i][0], pts[i][1], yaw, pts[i][3], pts[i][2]))
    return out


# ---- the Werk template ----------------------------------------------------------------------------------------------
# Local frame: +X = the German front (the cupolas look out over it), +Y left; the tunnel mouth 50 m behind (-X).
# Everything below is placed through werk_xy(), so the five groups are identical in their own frame.
STANDIN_DOME = "/Engine/BasicShapes/Sphere"
STANDIN_CUBE = "/Engine/BasicShapes/Cube"
WERK_TEMPLATE = {
    "block": (-300.0, 0.0, 0.0),
    "cupolas": [(900.0, -900.0), (1150.0, 650.0), (250.0, 1250.0)],
    "mast": (-1300.0, -1100.0),
    "soviet_pits": [(-1500.0, 1450.0, 135.0), (-1600.0, -1350.0, 225.0)],      # (x, y, facing yaw): MG pits on the rear flanks
    "hedgehogs": [(-2900.0, 2300.0), (-3100.0, 1500.0), (-2900.0, -2200.0), (-3200.0, -1400.0), (-3400.0, 2900.0)],
    "crates": [(-700.0, 900.0, 15.0), (-900.0, 1250.0, 80.0)],
    "camonet": (-600.0, -700.0, 20.0),
    "hq_slots": [(-3600.0, -1100.0, "command bunker entrance"), (-3600.0, 1100.0, "transmitter array"),
                 (-4200.0, 0.0, "tunnel railway entrance")],
}
TEETH_ARC = (-58.0, 58.0)           # bearings of the German front belt (deg, local, 0 = +X)
TEETH_GAP = (36.0, 48.0)            # the one deliberate vehicle gap (deg), the same at all five
TEETH_ROWS = (3200.0, 3420.0, 3640.0, 3860.0)
TEETH_STEP = 240.0                  # centre spacing along a row (a tooth is ~120 cm wide: about 1.2 m clear for infantry)


def werk_xy(fid, lx, ly):
    w = WERKS[fid]
    yaw = math.radians(w["yaw"])
    c = w["centre"]
    return c[0] + lx * math.cos(yaw) - ly * math.sin(yaw), c[1] + lx * math.sin(yaw) + ly * math.cos(yaw)


def werks():
    tooth_paths = kits_like("SM_OW_DragonTooth")
    for fid in sorted(WERKS):
        w = WERKS[fid]
        yaw = w["yaw"]
        folder = "Werke/" + fid
        tag = "Werk_" + fid
        hill_z = w["base"] + LAYOUT["levels"]["werk_hill"]
        keep(w["centre"][0], w["centre"][1], 4600.0)
        keep(w["tunnel"][0], w["tunnel"][1], 1700.0)
        # the concrete block and the steel cupolas on the hill top
        bx, by = werk_xy(fid, *WERK_TEMPLATE["block"][:2])
        if not piece("Werk_Block", "SM_OW_WerkBlock", bx, by, yaw, collision=True):
            bp(tag + "_Block", D + "Military/BP_Military_Dest_Bunker05", bx, by, yaw, 0.0, folder)
        for k, (lx, ly) in enumerate(WERK_TEMPLATE["cupolas"]):
            x, y = werk_xy(fid, lx, ly)
            piece("Werk_Cupola", "SM_OW_SteelCupola", x, y, yaw + 30.0 * k, collision=True,
                  fallback=STANDIN_DOME, fb_scale=3.0, fb_dz=-110.0)
        x, y = werk_xy(fid, *WERK_TEMPLATE["mast"])
        if not piece("Werk_Mast", "SM_OW_WerkMast", x, y, yaw, collision=True):
            bp(tag + "_Mast", D + "Industrial/Electricity/BP_Electricity_SinglePole1", x, y, yaw, 0.0, folder)
        # Soviet additions facing the rear (the player's side): MG pits, rail hedgehogs, wire, a net and crates
        for k, (lx, ly, face) in enumerate(WERK_TEMPLATE["soviet_pits"]):
            x, y = werk_xy(fid, lx, ly)
            bp("%s_Pit_%d" % (tag, k), SB + "BP_Cover_RusSandbags_LongArc", x, y, yaw + face - 90.0, 0.0, folder)
        for k, (lx, ly) in enumerate(WERK_TEMPLATE["hedgehogs"]):
            x, y = werk_xy(fid, lx, ly)
            bp("%s_Hedgehog_%d" % (tag, k), BAR + "BP_Military_Dest_LargeHedgehog", x, y, yaw + 17.0 * k, 0.0, folder)
        for k in range(6):
            b = math.radians(130.0 + 20.0 * k)
            x, y = werk_xy(fid, 2300.0 * math.cos(b), 2300.0 * math.sin(b))
            bp("%s_Wire_%d" % (tag, k), BAR + "BP_Military_Dest_BarbedWireLong", x, y, yaw + math.degrees(b) + 90.0, 0.0, folder)
        for k, (lx, ly, ty) in enumerate(WERK_TEMPLATE["crates"]):
            x, y = werk_xy(fid, lx, ly)
            bp("%s_Crates_%d" % (tag, k), PRY + "Airfield/Blueprints/BP_Prypyat_ServiceCrates", x, y, yaw + ty, 0.0, folder)
        # dragon's teeth: a German belt of four staggered rows round the front, one identical vehicle gap
        n = 0
        for row, r in enumerate(TEETH_ROWS):
            span = math.radians(TEETH_ARC[1] - TEETH_ARC[0]) * r
            count = int(span / TEETH_STEP)
            for i in range(count + 1):
                b = TEETH_ARC[0] + (TEETH_ARC[1] - TEETH_ARC[0]) * (i + 0.5 * (row % 2)) / max(count, 1)
                if b > TEETH_ARC[1] or TEETH_GAP[0] <= b <= TEETH_GAP[1]:
                    continue
                x, y = werk_xy(fid, r * math.cos(math.radians(b)), r * math.sin(math.radians(b)))
                if tooth_paths:
                    mesh_actor("%s_Tooth_%03d" % (tag, n), tooth_paths[n % len(tooth_paths)], x, y,
                               yaw + b + (11.0 * ((i * 7 + row * 3) % 5) - 22.0), 0.0, folder + "/Teeth")
                else:
                    STANDINS.add("SM_OW_DragonTooth")
                    mesh_actor("%s_Tooth_%03d" % (tag, n), STANDIN_CUBE, x, y, yaw + b + 45.0, -10.0, folder + "/Teeth", 1.1)
                n += 1
        # the belt's nav: tank-only blockers along the arc (infantry walks between the teeth)
        r_mid = (TEETH_ROWS[0] + TEETH_ROWS[-1]) / 2.0
        depth = TEETH_ROWS[-1] - TEETH_ROWS[0] + 260.0
        b0 = TEETH_ARC[0]
        segs = []
        while b0 < TEETH_ARC[1]:
            b1 = min(b0 + 6.0, TEETH_ARC[1])
            if not (b1 <= TEETH_GAP[0] or b0 >= TEETH_GAP[1]):
                b1 = max(b0, TEETH_GAP[0]) if b0 < TEETH_GAP[0] else b1
                if b0 >= TEETH_GAP[0]:
                    b0 = TEETH_GAP[1]
                    continue
            if b1 > b0:
                segs.append((b0, b1))
            b0 = b1
        for k, (s0, s1) in enumerate(segs):
            bm = math.radians((s0 + s1) / 2.0)
            half_len = r_mid * math.radians(s1 - s0) / 2.0 + 60.0
            x, y = werk_xy(fid, r_mid * math.cos(bm), r_mid * math.sin(bm))
            NAVBLOCKS.append(["%s_Teeth_%02d" % (tag, k), round(x, 1), round(y, 1), 0.0, round((yaw + math.degrees(bm)) % 360.0, 1),
                              round(depth / 2.0, 1), round(half_len, 1), 250.0, TANK_BLOCK_AREA])
        # the tunnel mouth: opening faces out of the werk (local -X), flat apron in front of it
        tx, ty = w["tunnel"]
        if not piece("Werk_Tunnel", "SM_OW_TunnelPortal", tx, ty, yaw + 180.0, collision=True):
            bp(tag + "_TunnelStandin", D + "GarrisonableBuildings/Bunkers/BP_Garrison_Bunker_2", tx, ty, yaw + 180.0, 0.0, folder)
        note(fid + "_tunnel", "%s tunnel mouth: wave spawn" % fid.upper(), tx, ty)
        note(fid + "_garrison", "%s garrison (same at all five)" % fid.upper(), *werk_xy(fid, 0.0, 0.0))
        for k, (lx, ly, what) in enumerate(WERK_TEMPLATE["hq_slots"]):
            note("%s_hqslot_%d" % (fid, k), "HQ slot: %s (hidden until the fix)" % what, *werk_xy(fid, lx, ly))
    return n


# ---- stations, outposts, guards -------------------------------------------------------------------------------------
STATION_KIND = {"s1": "Loop", "s2": "Adcock", "s3": "Loop", "s4": "Adcock", "s5": "Adcock"}


def stations():
    for fid, kind in STATION_KIND.items():
        x, y = fpoints(fid)[0]
        label = feature(fid)["label"]
        keep(x, y, 2600.0)                                   # the 25 m capture disc stays clear
        # face the hut door towards the nearest track / the player's ridge
        start = fpoints("start")[0]
        yaw = bearing_yaw(start[0] - x, start[1] - y)
        name = "SM_OW_DFStation_" + kind
        if not piece("DF_" + kind, name, x, y, yaw, collision=True):
            path = pry("radiomast_island")
            if path:
                inst("Standin_DFMast", path, x, y, yaw, 0.55, -20.0, True)
            bp("DF_%s_Hut" % fid, PRY + "Village/Blueprints/BP_PolesieLogHouse_B", x + 900.0 * math.cos(math.radians(yaw)),
               y + 900.0 * math.sin(math.radians(yaw)), yaw, 0.0, "Stations")
        note(fid, "%s: capture zone (25 m)" % label, x, y)
    # DF 1 is the old pump house's listening post: the pump house stands on the nearest Packlitz shore, facing the water
    x, y = fpoints("s1")[0]
    best = None
    for k in range(0, 360, 6):
        for r in range(1500, 8000, 200):
            px, py = x + r * math.cos(math.radians(k)), y + r * math.sin(math.radians(k))
            if at("water", px, py):
                if best is None or r < best[0]:
                    best = (r, k)
                break
    if best:
        r, k = best
        px, py = x + (r - 900.0) * math.cos(math.radians(k)), y + (r - 900.0) * math.sin(math.radians(k))
        bp("PumpHouse", D + "PostApo/BP_Dest_PostApo_House", px, py, k, 0.0, "Stations")
        keep(px, py, 1500.0)


def sandbag_ring(tag, cx, cy, face, folder):
    for i in range(3):
        b = face + 60.0 + i * 120.0
        x, y = cx + 240.0 * math.cos(math.radians(b)), cy + 240.0 * math.sin(math.radians(b))
        bp("%s_Bags_%d" % (tag, i), SB + "BP_Cover_RusSandbags_LongArc", x, y, b - 90.0, 0.0, folder)


def clear_of_werks(x, y, dist):
    """Push a point straight away from the nearest Werk until it stands dist from every Werk centre (on dry ground)."""
    for _ in range(40):
        near = min(WERKS.values(), key=lambda w: math.hypot(x - w["centre"][0], y - w["centre"][1]))
        d = math.hypot(x - near["centre"][0], y - near["centre"][1])
        if d >= dist and not at("water", x, y):
            return x, y
        ux, uy = (x - near["centre"][0]) / max(d, 1.0), (y - near["centre"][1]) / max(d, 1.0)
        x, y = x + ux * 300.0, y + uy * 300.0
    return x, y


def outposts():
    for f in LAYOUT["features"]:
        raw = f["raw"]
        if f["kind"] != "enemy_group" or not f["points"]:
            continue
        x, y = f["points"][0]
        if raw.get("outpost"):
            x, y = clear_of_werks(x, y, 7500.0)
            # face the crossing it watches: the nearest corridor point
            best = None
            for rid, rd in LAYOUT["roads"].items():
                for p in rd["points"][::4]:
                    d = math.hypot(p[0] - x, p[1] - y)
                    if best is None or d < best[0]:
                        best = (d, p)
            face = bearing_yaw(best[1][0] - x, best[1][1] - y) if best else 0.0
            sandbag_ring("Outpost_" + f["id"], x, y, face, "Outposts")
            for k, side in enumerate((-1.0, 1.0)):
                b = math.radians(face + 90.0 * side)
                bp("Outpost_%s_Wire_%d" % (f["id"], k), BAR + "BP_Military_Dest_BarbedWireLong", x + 900.0 * math.cos(b),
                   y + 900.0 * math.sin(b), face, 0.0, "Outposts")
            bp("Outpost_%s_Barrier" % f["id"], BAR + "BP_Military_Dest_ConcreteBarrier",
               x - 500.0 * math.cos(math.radians(face)), y - 500.0 * math.sin(math.radians(face)), face + 90.0, 0.0, "Outposts")
            keep(x, y, 1300.0)
            note(f["id"], "Outpost (bait): %s" % raw.get("where", ""), x, y)
        elif f["id"].startswith("guard_"):
            start = fpoints("start")[0]
            face = bearing_yaw(start[0] - x, start[1] - y)
            bp("Guard_%s_Bags" % f["id"], SB + "BP_Cover_RusSandbags_DoubleStraight", x, y, face + 90.0, 0.0, "Stations")
            keep(x, y, 900.0)
            note(f["id"], "Station guard", x, y)


# ---- resources ------------------------------------------------------------------------------------------------------
RAD = RES + "Radixite/"
MET = RES + "Metal/"
RESOURCE_SETS = {
    "rad_base": [RAD + "Resource_Radixite_Crystal_SmallDecal", RAD + "Resource_Radixite_Crystal_SmDecal_Growth"],
    "metal_base": [MET + "Resource_Metal_Ruin1", MET + "Resource_Metal_Barrels"],
    "rad_near": [RAD + "Resource_Radixite_Crystal", RAD + "Resource_Radixite_Crystal_SmallDecal", RAD + "Resource_Radixite_Crystal"],
    "metal_near": [MET + "Resource_Metal_Rails", MET + "Resource_Metal_Rails"],
    "metal_n": [MET + "Resource_Metal_Barrels", MET + "Resource_Metal_BrokenWaterTank", MET + "Resource_Metal_Ruin1"],
    "rad_se": [RAD + "Resource_Radixite_Crystal_LargeWithGrowth", RAD + "Resource_Radixite_Crystal_Large",
               RAD + "Resource_Radixite_Crystal_LargeWithGrowth", RAD + "Resource_Radixite_Crystal_Large",
               RAD + "Resource_Radixite_Crystal_SmDecal_Growth"],
    "metal_c": [MET + "Resource_MetalTower", MET + "Resource_Metal_Rails", MET + "Resource_Metal_Rails"],
    "rad_ne": [RAD + "Resource_Radixite_Crystal_LargeWithGrowth", RAD + "Resource_Radixite_Crystal"],
}


def resources():
    for fid, paths in RESOURCE_SETS.items():
        pts = fpoints(fid)
        for k, p in enumerate(pts):
            path = paths[k % len(paths)]
            bp("Res_%s_%d" % (fid, k), path, p[0], p[1], RNG.uniform(0, 360), 0.0, "Resources/" + fid)
            keep(p[0], p[1], 1100.0)
        label = feature(fid)["label"]
        note(fid, label, *pts[0])
    # scene dressing round the metal sites (not resources themselves)
    x, y = fpoints("metal_base")[0]
    for k, (dx, dy, what, yaw) in enumerate(((900.0, 500.0, "BP_Env_DestroyedPzIII", 40.0), (-700.0, 1300.0, "BP_Env_DestroyedPz38T", 215.0),
                                              (1700.0, -600.0, "BP_Env_DestroyedPzII", 120.0))):
        bp("Column_%d" % k, D + "Military/DestTanks/" + what, x + dx, y + dy, yaw, 0.0, "Resources/metal_base")
        keep(x + dx, y + dy, 700.0)
    # the rail yard (rich metal): three sidings on the ballast bed of the line that runs off the east edge (critic R1-10)
    pts = fpoints("metal_c")
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    track = pry("existing_rail_track") or "/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01"
    line = LAYOUT["rail_line"]
    (ax0, ay0), (ax1, ay1) = line[0], line[1]
    yaw_line = bearing_yaw(ax1 - ax0, ay1 - ay0)
    for off in (-450.0, 0.0, 450.0):
        for i in range(-3, 6):
            ax = ax0 + 1000.0 * i * math.cos(math.radians(yaw_line)) - off * math.sin(math.radians(yaw_line))
            ay = ay0 + 1000.0 * i * math.sin(math.radians(yaw_line)) + off * math.cos(math.radians(yaw_line))
            if not at("water", ax, ay) and not kept_station(ax, ay):
                inst("RailYard_Track", track, ax, ay, yaw_line, 1.0, -8.0, False)
    for seg in range(1, len(line) - 1):
        (bx0, by0), (bx1, by1) = line[seg], line[seg + 1]
        yaw_s = bearing_yaw(bx1 - bx0, by1 - by0)
        n = int(math.hypot(bx1 - bx0, by1 - by0) / 1000.0)
        for i in range(n + 1):
            ax, ay = bx0 + (bx1 - bx0) * i / max(n, 1), by0 + (by1 - by0) * i / max(n, 1)
            if not at("water", ax, ay) and not kept_station(ax, ay):
                inst("RailLine_Track", track, ax, ay, yaw_s, 1.0, -8.0, False)
    for k, (i, off) in enumerate(((-2.0, 450.0), (1.5, -450.0))):
        ax = ax0 + 1000.0 * i * math.cos(math.radians(yaw_line)) - off * math.sin(math.radians(yaw_line))
        ay = ay0 + 1000.0 * i * math.sin(math.radians(yaw_line)) + off * math.cos(math.radians(yaw_line))
        bp("RailYard_Wagon_%d" % k, D + "PostApo/BP_Dest_PostApo_Container", ax, ay, yaw_line, 0.0, "Resources/metal_c")
    keep(cx, cy, 2600.0)


# ---- crossings ------------------------------------------------------------------------------------------------------
def crossings():
    # sluice gates in the upstream face of the two dams at their chokes (the kit's +X = water side, long axis Y, deck
    # level = pivot, waterline 1.8 m under the deck): the deck sits 1.8 m over the dammed pond, the piers in the pond
    up_water = LAYOUT["levels"].get("up_water", 170.0)
    for did, choke in (("dam_packlitz", (4800.0, -28500.0)), ("dam_lubow", (12000.0, 27000.0))):
        pts = road(did)
        k = min(range(len(pts)), key=lambda i: math.hypot(pts[i][0] - choke[0], pts[i][1] - choke[1]))
        (x0, y0), (x1, y1) = pts[max(k - 3, 0)][:2], pts[min(k + 3, len(pts) - 1)][:2]
        d = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / d, (y1 - y0) / d
        w = pts[k][3]
        # upstream = the side of the dam where the pond is
        side = 1.0
        if not at("pond", pts[k][0] - uy * (w + 900.0), pts[k][1] + ux * (w + 900.0)):
            side = -1.0
        nx, ny = -uy * side, ux * side                                  # unit normal towards the pond
        gx, gy = pts[k][0] + nx * (w + 170.0), pts[k][1] + ny * (w + 170.0)
        yaw = bearing_yaw(nx, ny)                                       # local +X (water side) faces the pond
        path = kit("SM_OW_SluiceGate")
        if path:
            g = INST.setdefault("Kit_SluiceGate", {"mesh": path, "collision": True, "items": [], "shadow": True,
                                                   "abs_z": LAYOUT["levels"]["crown"] - 10.0})
            g["items"].append([round(gx, 1), round(gy, 1), 0.0, round(yaw, 1), 1.0, 0.0, 0.0])
        else:
            STANDINS.add("SM_OW_SluiceGate")
        note(did + "_sluice", "Sluice gate (10 m choke)", pts[k][0], pts[k][1])
        keep(pts[k][0], pts[k][1], 2200.0)
    for cid, choke in (("causeway_kalau", (-21000.0, -2000.0)), ("causeway_island", (-2960.0, 2790.0))):
        pts = road(cid)
        k = min(range(len(pts)), key=lambda i: math.hypot(pts[i][0] - choke[0], pts[i][1] - choke[1]))
        (x0, y0), (x1, y1) = pts[max(k - 3, 0)][:2], pts[min(k + 3, len(pts) - 1)][:2]
        d = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / d, (y1 - y0) / d
        w = pts[k][3]
        slab = "/Game/QuarryEnv/Meshes/SM_ConcreteBlock_%d"
        for j in range(8):
            s = (j - 3.5) * 230.0
            for side in (-1.0, 1.0):
                x = pts[k][0] + ux * s - uy * side * (w + 60.0 + RNG.uniform(0, 160))
                y = pts[k][1] + uy * s + ux * side * (w + 60.0 + RNG.uniform(0, 160))
                if j % 2:
                    inst("BreachRubble", "/Game/RTS_Survival/Meshes/Nature/Rocks/RockMoss/SM_RTS_RockMoss_%d" % (1 + j % 6), x, y,
                         None, RNG.uniform(0.5, 0.9), -40.0, False)
                else:
                    inst("BreachSlab_%d" % (1 + j % 2), slab % (1 + j % 2), x, y, bearing_yaw(ux, uy) + RNG.uniform(-35, 35),
                         RNG.uniform(0.35, 0.55), -60.0, False, pitch=RNG.uniform(-25, 25), roll=RNG.uniform(-20, 20))
        note(cid + "_breach", "Breach (10-11 m choke)", pts[k][0], pts[k][1])
        keep(pts[k][0], pts[k][1], 2000.0)
    # riprap: stones along both faces of every dam and causeway at the waterline (critic R1-5)
    for rid in ("dam_packlitz", "causeway_kalau", "causeway_island", "dam_lubow"):
        for x, y, yaw, w, h in walk(rid, 320.0):
            for side in (-1.0, 1.0):
                for rr in (120.0, 260.0):
                    px = x - math.sin(math.radians(yaw)) * side * (w + rr + RNG.uniform(-40, 60))
                    py = y + math.cos(math.radians(yaw)) * side * (w + rr + RNG.uniform(-40, 60))
                    if at("height", px, py) < 180.0:
                        inst("Riprap_%d" % RNG.integers(6), ROCKS[int(RNG.integers(6))], px, py, None, RNG.uniform(0.18, 0.32), -25.0, False)
    # boardwalks: plank tiles on the infantry dykes
    tile = pry("boardwalk_6m")
    post = pry("boardwalk_post")
    for rid in ("boardwalk_nischlitz", "boardwalk_hochwald"):
        for x, y, yaw, w, h in walk(rid, 600.0):
            if tile:
                inst("Boardwalk_Tile", tile, x, y, yaw, 1.0, 0.0, False)
        note(rid, "Boardwalk (infantry only)", *road(rid)[len(road(rid)) // 2][:2])
    # the ford: marker posts on both edges of the gravel bar
    if post:
        for x, y, yaw, w, h in walk("ford_hochwald", 600.0):
            for side in (-1.0, 1.0):
                px = x - math.sin(math.radians(yaw)) * side * (w + 40.0)
                py = y + math.cos(math.radians(yaw)) * side * (w + 40.0)
                inst("Ford_Post", post, px, py, RNG.uniform(0, 360), 3.2, 60.0, False)
    note("ford", "Hochwald ford (wade, 15 m)", *road("ford_hochwald")[len(road("ford_hochwald")) // 2][:2])
    for rid in LAYOUT["roads"]:
        for x, y, yaw, w, h in walk(rid, 400.0):
            keep(x, y, w + 250.0)                                       # nothing on the decks, a 2.5 m halo


# ---- nature ---------------------------------------------------------------------------------------------------------
DEAD_TREES = [NAT + "Trees/DeadTrees/SM_RTS_DeadTree_%s" % n for n in ("1", "2", "3", "04", "05", "06")]
LOGS = [NAT + "Trees/DeadTrees/Logs/SM_RTS_DeadTree_Log_%02d" % i for i in range(1, 11)]
SHRUBS = [NAT + "Foliage/S_Dead_Shrubs_vdknafgha_Var%d_lod3" % i for i in range(1, 16)]
TUFTS = [E + "GroundFoliage/Meshes/SM_GF_DryTuft_A", E + "GroundFoliage/Meshes/SM_GF_DryTuft_B"]
LITTER = E + "GroundFoliage/Meshes/SM_GF_DryLitter"
ROCKS = ["/Game/RTS_Survival/Meshes/Nature/Rocks/RockMoss/SM_RTS_RockMoss_%d" % i for i in range(1, 7)]
ORE_SPRUCES = [E + "OreMountains/DeadForest/BP_OreDeadSpruce_%s" % n for n in
               ("Skeleton_13m", "Skeleton_11m", "Skeleton_11m_B", "Black_8m", "Snapped_7m", "Leaning_11m")]


def pine_paths():
    bps = [p for p in kits_like("BP_OW_DeadPine")]
    return bps if bps else ORE_SPRUCES


def kept_station(x, y):
    for f in LAYOUT["features"]:
        if f["raw"].get("df") == "station":
            p = f["points"][0]
            if math.hypot(x - p[0], y - p[1]) < 2600.0:
                return True
    return False


def dry_ok(x, y):
    return (not at("water", x, y)) and (not at("road_near", x, y)) and at("slope", x, y) < 30.0 and not kept(x, y)


def build_circle(x, y):
    s = fpoints("start")[0]
    return (x - s[0]) ** 2 + (y - s[1]) ** 2 < 9200.0 ** 2


def nature():
    pines = pine_paths()
    if pines is ORE_SPRUCES:
        STANDINS.add("BP_OW_DeadPine")
    count = 0
    # plantation forest: jittered rows per forest region, 6 m along a row, 7 m between rows; 35 % gone (fallen, cut)
    for r in LAYOUT["regions"]:
        if r["kind"] != "forest":
            continue
        cx, cy = r["centre"]
        ang = math.radians(RNG.uniform(0, 180))
        rad = r["raw"].get("blob", {}).get("r", 6000.0) * 1.5
        for i in np.arange(-rad, rad, 700.0):
            for j in np.arange(-rad, rad, 600.0):
                x = cx + i * math.cos(ang) - j * math.sin(ang) + RNG.normal(0, 90)
                y = cy + i * math.sin(ang) + j * math.cos(ang) + RNG.normal(0, 90)
                if not at("forest", x, y) or not dry_ok(x, y) or build_circle(x, y):
                    continue
                roll = RNG.random()
                if roll < 0.30:
                    continue
                if roll < 0.38:
                    inst("Logs", LOGS[RNG.integers(len(LOGS))], x, y, math.degrees(ang) + RNG.normal(0, 25), RNG.uniform(0.32, 0.45), -4.0)
                    continue
                if inside(x, y, 300.0):
                    bp("Pine_%04d" % count, pines[RNG.integers(len(pines))], x, y, RNG.uniform(0, 360), -8.0, "Trees/Forest")
                    count += 1
                else:
                    inst("MarginTree", DEAD_TREES[RNG.integers(len(DEAD_TREES))], x, y, None, RNG.uniform(0.9, 1.25), -10.0)
                if RNG.random() < 0.5:
                    inst("Litter", LITTER, x + RNG.normal(0, 150), y + RNG.normal(0, 150), None, RNG.uniform(0.8, 1.4), 0.0, shadow=False)
    # lone dead pines on the lowland, the heath and the ridge top (not in the build circle)
    for x, y in scatter(150, lambda x, y: dry_ok(x, y) and not at("forest", x, y) and not build_circle(x, y)
                        and inside(x, y, 300.0) and not at("crag", x, y)):
        bp("Pine_%04d" % count, pines[RNG.integers(len(pines))], x, y, RNG.uniform(0, 360), -8.0, "Trees/Lone")
        count += 1
    # margin trees past the play edge
    for x, y in scatter(500, lambda x, y: (not inside(x, y, -200.0)) and dry_ok(x, y), margin=-9000.0):
        inst("MarginTree", DEAD_TREES[RNG.integers(len(DEAD_TREES))], x, y, None, RNG.uniform(0.9, 1.3), -10.0)
    # dry tufts and dead shrubs on the open ground, sedge in the bogs
    for x, y in scatter(5000, lambda x, y: dry_ok(x, y) and not at("bog", x, y), margin=-6000.0):
        if RNG.random() < 0.75:
            inst("Tuft_%d" % RNG.integers(2), TUFTS[RNG.integers(2)], x, y, None, RNG.uniform(0.7, 1.3), 0.0, shadow=False)
        else:
            inst("Shrub_%02d" % RNG.integers(15), SHRUBS[RNG.integers(15)], x, y, None, RNG.uniform(0.6, 1.1), -5.0)
    sedge = [pry("bog_sedge_a"), pry("bog_sedge_b")]
    if all(sedge):
        for x, y in scatter(1800, lambda x, y: at("bog", x, y) and dry_ok(x, y), margin=-6000.0):
            g = "BogSedge_%d" % RNG.integers(2)
            inst(g, sedge[int(g[-1])], x, y, None, RNG.uniform(0.8, 1.3), 0.0, shadow=False)
            INST[g]["material"] = "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Map/MI_Pripyat_BogFoliage_Dark"
    # reeds and cattails at the waterline (bank toes), drowned snags in the shallows
    reeds = [p for p in (pry("bog_reed_a"), pry("bog_reed_b"), pry("bog_cattail_a")) if p]
    if reeds:
        # clumps of 5-14 stems standing in 10-60 cm of water, irregular groups, dark (critic R1-2)
        for x, y in scatter(950, lambda x, y: -60.0 < at("height", x, y) < 10.0 and not kept(x, y), margin=-6000.0):
            n = int(RNG.integers(5, 15))
            spread = RNG.uniform(150.0, 420.0)
            for _ in range(n):
                rx, ry = x + RNG.normal(0, spread), y + RNG.normal(0, spread * 0.6)
                if -80.0 < at("height", rx, ry) < 15.0 and not kept(rx, ry):
                    g = "Reed_%d" % RNG.integers(len(reeds))
                    inst(g, reeds[int(g[-1])], rx, ry, None, RNG.uniform(0.9, 1.4), -10.0, shadow=False)
                    INST[g]["material"] = "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Map/MI_Pripyat_BogFoliage_Dark"
    snags = [p for p in (pry("bog_snag_a"), pry("bog_snag_b")) if p]
    if snags:
        for x, y in scatter(160, lambda x, y: at("water", x, y) and -140.0 < at("height", x, y) < -20.0, margin=-6000.0):
            inst("Snag_%d" % RNG.integers(len(snags)), snags[RNG.integers(len(snags))], x, y, None, RNG.uniform(0.9, 1.4), -40.0)
    # glacial erratics: lowland, heath and the ridge top
    erratics = kits_like("SM_OW_Erratic") or ROCKS
    if erratics is ROCKS:
        STANDINS.add("SM_OW_Erratic")
    for x, y in scatter(90, lambda x, y: at("crag", x, y) is not None and not at("crag", x, y) and at("ridge_any", x, y)
                        and not at("ridge_top", x, y) and dry_ok(x, y), margin=-4000.0):
        inst("Talus_%d" % RNG.integers(6), ROCKS[int(RNG.integers(6))], x, y, None, RNG.uniform(0.5, 1.1), -30.0, True)
    rim = scatter(160, lambda x, y: at("ridge_top", x, y) and not at("crag", x, y) and any(
        at("crag", x + dx, y + dy) for dx, dy in ((350, 0), (-350, 0), (0, 350), (0, -350))) and not kept(x, y), margin=-4000.0)
    kept_rim = []
    for x, y in rim:                                                 # thin to 12-20 m spacing along the crest
        if all(math.hypot(x - a, y - b) > RNG.uniform(1200.0, 2000.0) for a, b in kept_rim):
            kept_rim.append((x, y))
            inst("Crest_%d" % RNG.integers(len(erratics)), erratics[RNG.integers(len(erratics))], x, y, None, RNG.uniform(0.7, 1.2), -70.0, True)
    # R2-8: erratics lie in groups of 2-5, half sunk (not single eggs on the grass)
    for x, y in scatter(28, lambda x, y: dry_ok(x, y) and not build_circle(x, y) and inside(x, y, 400.0)):
        for _ in range(int(RNG.integers(2, 6))):
            ex, ey = x + RNG.normal(0, 260), y + RNG.normal(0, 260)
            if dry_ok(ex, ey):
                inst("Erratic_%d" % RNG.integers(len(erratics)), erratics[RNG.integers(len(erratics))], ex, ey, None,
                     RNG.uniform(0.6, 1.3), -RNG.uniform(50.0, 110.0), True, pitch=RNG.uniform(-12, 12), roll=RNG.uniform(-12, 12))
    return count


def peat_fires():
    atmo = E + "Berlin/Atmosphere/"
    r = region("bog_se")
    cx, cy = r["centre"]
    rad = r["raw"].get("blob", {}).get("r", 3000.0)
    for k in range(10):
        a, d = RNG.uniform(0, 2 * math.pi), rad * math.sqrt(RNG.uniform(0.02, 0.7))
        x, y = cx + d * math.cos(a), cy + d * math.sin(a)
        kind = "NS_BerlinAtmo_Smoulder_Hero" if k % 5 == 0 else "NS_BerlinAtmo_Smoulder_Wisp"
        fx("Peat_%02d" % k, atmo + kind, x, y, 20.0)
    note("bog_se", "Smouldering peat (passable, slow)", cx, cy)


def ridge():
    """The start: the road from Berlin up onto the ridge, a sign; the ramp heads clear (pads)."""
    s = fpoints("start")[0]
    note("start", "START: HQ deploys here (85 m build circle)", s[0], s[1])
    keep(s[0], s[1], 9200.0)
    for rid in ("ramp_north", "ramp_prow", "ramp_east"):
        p = road(rid)[0]
        keep(p[0], p[1], 1700.0)
        note(rid + "_head", rid.replace("_", " ") + " head", p[0], p[1])
    p = road("road_berlin")[-6]
    sign = pry("sign_roadsign_blankroad")
    if sign:
        bp("Sign_Frankfurt", sign, p[0] + 700.0, p[1] + 300.0, 200.0, 0.0, "Ridge")


SCORCH = "/Game/Crater_Decals/Decals/Scorched/"
IMPACT = "/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/"
DECAL_SETS = {
    "crater": [IMPACT + "MI_BombImpactDecal", IMPACT + "MI_LandscapeImpactDecal", SCORCH + "MI_Decal_Crater_Scorched_Dirt_01"],
    "scorch": [SCORCH + "MI_Decal_Scorch_01", SCORCH + "MI_Decal_Scorch_02", SCORCH + "MI_Decal_Scorch_03"],
    "debris": [SCORCH + "MI_Decal_Crater_Scorched_Scatter_01", SCORCH + "MI_Decal_Scorched_Dirt_Debris_01",
               "/Game/Crater_Decals/Decals/Dirt/MI_Decal_Dirt_Scatter_01"],
    "soot": [SCORCH + "MI_Decal_Scorch_04"],
}
# the Werk template's old craters (same local points as WERK_CRATERS in ow_terrain_gen.py): identical at all five
WERK_CRATERS = [(2100.0, -1500.0, 260.0), (-2600.0, 1900.0, 320.0), (1500.0, 1900.0, 220.0), (-900.0, -2100.0, 280.0)]


def decals():
    """Thirty-year-old battle scars: the Werk craters (template, Werk frame), shell craters on the open lowland in front
    of the belts (1945), soot under the wrecked column, and scorch round the smouldering peat."""
    for fid in sorted(WERKS):
        yaw = WERKS[fid]["yaw"]
        for k, (lx, ly, r) in enumerate(WERK_CRATERS):
            x, y = werk_xy(fid, lx, ly)
            decal("Werk_%s_Crater_%d" % (fid, k), DECAL_SETS["crater"][k % 3], x, y, r * 2.6, yaw + 37.0 * k)
    n = 0
    for x, y in scatter(70, lambda x, y: dry_ok(x, y) and not at("forest", x, y) and inside(x, y, 500.0)
                        and min(math.hypot(x - w["centre"][0], y - w["centre"][1]) for w in WERKS.values()) > 6500.0
                        and not build_circle(x, y)):
        r = RNG.uniform(250.0, 520.0)
        decal("Crater_%03d" % n, DECAL_SETS["crater"][n % 3], x, y, r * 2.2)
        a = RNG.uniform(0, 2 * math.pi)
        decal("Debris_%03d" % n, DECAL_SETS["debris"][n % 3], x + r * 1.5 * math.cos(a), y + r * 1.5 * math.sin(a), r * 1.6)
        n += 1
    x0, y0 = fpoints("metal_base")[0]
    for k, (dx, dy) in enumerate(((900.0, 500.0), (-700.0, 1300.0), (1700.0, -600.0))):
        decal("Column_Soot_%d" % k, DECAL_SETS["soot"][0], x0 + dx, y0 + dy, 1100.0)
    r = region("bog_se")
    for k in range(8):
        a, d = RNG.uniform(0, 2 * math.pi), r["raw"].get("blob", {}).get("r", 3000.0) * math.sqrt(RNG.uniform(0.05, 0.7))
        decal("Peat_Scorch_%d" % k, DECAL_SETS["scorch"][k % 3], r["centre"][0] + d * math.cos(a), r["centre"][1] + d * math.sin(a),
              RNG.uniform(600.0, 1000.0))
    return n


def werk_diff():
    """Critic R0-1: every object within 60 m of a Werk centre, in that Werk's own frame, compared with W1. The template
    pieces must match exactly; anything else in the disc is listed as foreign."""
    def local(fid, x, y):
        w = WERKS[fid]
        yaw = math.radians(w["yaw"])
        dx, dy = x - w["centre"][0], y - w["centre"][1]
        return dx * math.cos(yaw) + dy * math.sin(yaw), -dx * math.sin(yaw) + dy * math.cos(yaw)

    items = []
    for lab, path, x, y, yaw, dz, folder, scale in BPS + MESHES:
        items.append((lab, path, x, y, yaw))
    for group, g in INST.items():
        for it in g["items"]:
            items.append((group, g["mesh"], it[0], it[1], it[3]))
    for nb in NAVBLOCKS:
        items.append((nb[0], "navblock", nb[1], nb[2], nb[4]))
    sets, foreign = {}, {}
    for fid in sorted(WERKS):
        w = WERKS[fid]
        own, other = [], []
        for lab, path, x, y, yaw in items:
            if math.hypot(x - w["centre"][0], y - w["centre"][1]) > 6000.0:
                continue
            lx, ly = local(fid, x, y)
            entry = (path.split("/")[-1], round(lx / 10.0), round(ly / 10.0), round(((yaw - w["yaw"]) % 360.0) / 5.0) % 72)
            if lab.startswith("Werk_" + fid) or lab.startswith("Werk_") and lab.endswith(fid) or "Werk_" + fid in lab or \
                    lab.startswith(("Kit_Werk", "Standin_SM_OW", "Kit_SM_OW", "Werk_")) or path == "navblock" and fid in lab:
                own.append(entry)
            else:
                other.append((lab, path.split("/")[-1], round(lx / 100.0), round(ly / 100.0)))
        sets[fid] = sorted(own)
        foreign[fid] = other
    lines = []
    ref = sets["w1"]
    for fid in sorted(sets):
        a, b = set(ref), set(sets[fid])
        lines.append("%s: %d template pieces; differs from w1 by %d / %d; foreign objects within 60 m: %d %s" % (
            fid, len(sets[fid]), len(a - b), len(b - a), len(foreign[fid]), foreign[fid][:6]))
    text = "\n".join(lines)
    with open(os.path.join(TERRAIN, "werk_diff.txt"), "w") as handle:
        handle.write(text + "\n")
    print(text)


def main():
    ridge()
    teeth = werks()
    stations()
    outposts()
    resources()
    crossings()
    peat_fires()
    trees = nature()
    print("craters %d, decals %d" % (decals(), len(DECALS)))
    out = {"bps": BPS, "inst": INST, "notes": NOTES, "fx": FX, "splines": SPLINES, "decals": DECALS, "meshes": MESHES, "navblocks": NAVBLOCKS,
           "standins": sorted(STANDINS)}
    with open(os.path.join(TERRAIN, "OW_dress.json"), "w") as handle:
        json.dump(out, handle)
    print("blueprint actors %d (trees %d), instance groups %d with %d instances, teeth %d, notes %d, fx %d" % (
        len(BPS), trees, len(INST), sum(len(g["items"]) for g in INST.values()), teeth, len(NOTES), len(FX)))
    print("stand-ins (kit not imported yet): %s" % sorted(STANDINS))
    werk_diff()


if __name__ == "__main__":
    main()
