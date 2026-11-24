"""Dressing generator for Ger_PripyatMe410: where every tree, shrub, prop and site object goes.

    python Source/Tools/PripyatMap/pripyat_dress_gen.py

Reads Saved/Pripyat/terrain/Pripyat_layout.json + Pripyat_masks.npz (pripyat_terrain_gen.py) and, when present,
Saved/Pripyat/assets/pripyat_assets.json (the imported Pripyat kits). Writes Saved/Pripyat/terrain/Pripyat_dress.json:
    bps    [label, blueprint path, x, y, yaw, dz, folder, scale]      one actor each (destructible / crushable)
    inst   {group: {mesh, collision, items [x, y, dz, yaw, scale, pitch, roll]}}   instanced, decoration only
    notes  [id, text, x, y]                                          editor-only labels for the mission scripter
No units and no mission actors: sites that the mission will arm get earthworks, cover and a note.
"""
import json
import math
import os

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TERRAIN = os.path.join(ROOT, "Saved", "Pripyat", "terrain")
ASSETS = os.path.join(ROOT, "Saved", "Pripyat", "assets", "pripyat_assets.json")
E = "/Game/RTS_Survival/Blueprints/Environment/"
D = E + "DestructableEnvActor/"
NAT = "/Game/RTS_Survival/Meshes/Nature/"
RES = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/"

with open(os.path.join(TERRAIN, "Pripyat_layout.json")) as handle:
    LAYOUT = json.load(handle)
_npz = np.load(os.path.join(TERRAIN, "Pripyat_masks.npz"))
M = {k: _npz[k] for k in _npz.files}            # [iy, ix]; np.load unpacks on every access, so copy once
LAY = LAYOUT["landscape"]
SIZE = LAY["size_x"]
B = LAYOUT["bounds"]
AIR = LAYOUT["airfield"]
RNG = np.random.default_rng(4101)

BPS, INST, NOTES, FX, SPLINES, DECALS = [], {}, [], [], [], []


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


def region(rid):
    return next(r for r in LAYOUT["regions"] if r["id"] == rid)


def road(rid):
    return LAYOUT["roads"][rid]["points"]          # [x, y, h, half width] every 3 m


def air(lx, ly):
    """Airfield frame (x along the runway to the north-east, y to the south-east) to world."""
    yaw = math.radians(AIR["yaw"])
    return (AIR["centre"][0] + lx * math.cos(yaw) - ly * math.sin(yaw),
            AIR["centre"][1] + lx * math.sin(yaw) + ly * math.cos(yaw))


def bearing_yaw(dx, dy):
    """Unreal yaw of a direction given as (north, east) components."""
    return math.degrees(math.atan2(dy, dx))


def bp(label, path, x, y, yaw=0.0, dz=0.0, folder="Props", scale=1.0):
    BPS.append([label, path, round(float(x), 1), round(float(y), 1), round(float(yaw), 1), round(float(dz), 1), folder, round(float(scale), 3)])


def inst(group, mesh, x, y, yaw=None, scale=1.0, dz=0.0, collision=False, pitch=0.0, roll=0.0):
    g = INST.setdefault(group, {"mesh": mesh, "collision": collision, "items": []})
    yaw = RNG.uniform(0, 360) if yaw is None else yaw
    g["items"].append([round(float(x), 1), round(float(y), 1), round(float(dz), 1), round(float(yaw), 1), round(float(scale), 3),
                       round(float(pitch), 1), round(float(roll), 1)])


def fx(label, system, x, y, dz=0.0, scale=1.0):
    FX.append([label, system, round(float(x), 1), round(float(y), 1), round(float(dz), 1), round(float(scale), 2)])


def decal(label, material, x, y, size, yaw=None):
    DECALS.append([label, material, round(float(x), 1), round(float(y), 1), round(float(RNG.uniform(0, 360) if yaw is None else yaw), 1), round(float(size), 1)])


def along(rid, frac, side=0.0, off=0.0):
    """Point beside a corridor: frac of its length, side -1 / +1 (right / left of travel), off cm past the road edge
    (negative = on the deck). Returns x, y, yaw of the travel direction."""
    pts = road(rid)
    k = min(max(int(frac * (len(pts) - 1)), 2), len(pts) - 3)
    (x0, y0), (x1, y1) = pts[k - 2][:2], pts[k + 2][:2]
    d = math.hypot(x1 - x0, y1 - y0)
    ux, uy = (x1 - x0) / d, (y1 - y0) / d
    w = pts[k][3]
    return pts[k][0] - uy * side * (w + off), pts[k][1] + ux * side * (w + off), bearing_yaw(ux, uy)


def note(nid, text, x, y):
    NOTES.append([nid, text, round(float(x), 1), round(float(y), 1)])


def asset(aid):
    """An imported Pripyat kit asset by manifest id (None until the importer has delivered it)."""
    if not os.path.exists(ASSETS):
        return None
    try:
        with open(ASSETS) as handle:
            data = json.load(handle)
    except Exception:
        return None
    entries = data.get("assets", data) if isinstance(data, dict) else data
    if isinstance(entries, dict):
        entries = list(entries.values())
    for entry in entries:
        if isinstance(entry, dict) and entry.get("id") == aid:
            return entry
    return None


def assets_like(prefix):
    if not os.path.exists(ASSETS):
        return []
    with open(ASSETS) as handle:
        data = json.load(handle)
    return [e for e in data.get("assets", []) if str(e.get("id", "")).startswith(prefix)]


def noise_field(seed, wavelength):
    from PIL import Image
    rng = np.random.default_rng(seed)
    n = int(math.ceil(LAY["verts_x"] / wavelength)) + 3
    small = rng.random((n, n)).astype(np.float32)
    img = Image.fromarray(small, mode="F").resize((int(n * wavelength), int(n * wavelength)), Image.BICUBIC)
    return np.asarray(img)[:LAY["verts_y"], :LAY["verts_x"]]


def scatter(count, accept, margin=-9000.0, tries=40):
    """Random points over the landscape (margin < 0 reaches into the camera margin) that pass accept(x, y)."""
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


# ---- nature ---------------------------------------------------------------------------------------------------------
DEAD_TREES = [NAT + "Trees/DeadTrees/SM_RTS_DeadTree_%s" % n for n in ("1", "2", "3", "04", "05", "06")]
WILLOWS = [NAT + "Trees/DeadTrees/DeadWillows/SM_DeadWillow_%d" % i for i in range(1, 6)]
SHRUBS = [NAT + "Foliage/S_Dead_Shrubs_vdknafgha_Var%d_lod3" % i for i in range(1, 16)]
LOGS = [NAT + "Trees/DeadTrees/Logs/SM_RTS_DeadTree_Log_%02d" % i for i in range(1, 11)]
STUMP = NAT + "Trees/DeadTrees/SM_RTS_DeadTree_Stump"


def nature():
    clump = noise_field(7, 38)
    clump2 = noise_field(8, 12)
    # 1. the drowned forest: dead trees standing in the bog (instances, nobody can reach them)
    def bog_tree(x, y):
        c = cell(x, y)
        z = M["height"][c]
        if not (-55.0 < z < 110.0) or M["road_dist"][c] < 200.0 or M["open_water"][c] or M["burn"][c]:
            return False
        if M["road_dist"][c] < 2000.0 and x > 14500.0 and abs(y - 6000.0) < 9000.0:
            return False                                  # the rail embankment runs in the open
        near_s = x < -18500.0 and abs(y - 5000.0) < 6000.0 and M["road_dist"][c] < 1200.0
        return clump[c] + 0.35 * clump2[c] > (0.3 if near_s else 0.62) + RNG.uniform(0.0, 0.25)

    for x, y in scatter(2700, bog_tree):
        if RNG.random() < 0.33:
            inst("BogWillow_%d" % RNG.integers(0, 5), WILLOWS[RNG.integers(0, 5)], x, y, scale=RNG.uniform(0.7, 1.15), dz=-25.0,
                 roll=RNG.uniform(-9, 9), pitch=RNG.uniform(-9, 9))
        else:
            k = RNG.integers(0, 6)
            inst("BogTree_%d" % k, DEAD_TREES[k], x, y, scale=RNG.uniform(0.55, 1.05), dz=-30.0, roll=RNG.uniform(-12, 12),
                 pitch=RNG.uniform(-12, 12))
    # charred stumps crowd the rim of the smouldering peat, the burnt-out core is nearly bare; trunks lie pointing out
    for rid in ("burning_peat_se", "burning_peat_nw"):
        bx, by = region(rid)["centre"]
        rad = region(rid)["raw"]["blob"]["r"]
        for _ in range(110 if rad > 2400.0 else 70):
            a_, d_ = RNG.uniform(0, 2 * math.pi), rad * (0.55 + 0.6 * math.sqrt(RNG.random()))
            x, y = bx + d_ * math.cos(a_), by + d_ * math.sin(a_)
            if at("height", x, y) < -20.0:
                continue
            roll = RNG.random()
            if roll < 0.45:
                inst("BurnStump", STUMP, x, y, scale=RNG.uniform(0.6, 1.5), dz=-10.0)
            elif roll < 0.75:
                k = RNG.integers(0, 10)
                inst("BurnLog_%02d" % k, LOGS[k], x, y, yaw=math.degrees(a_) + RNG.uniform(-20, 20), scale=RNG.uniform(0.8, 1.4), dz=-12.0)
            else:
                k = RNG.integers(0, 6)
                inst("BurnTree_%d" % k, DEAD_TREES[k], x, y, scale=RNG.uniform(0.35, 0.6), dz=-40.0, roll=RNG.uniform(-25, 25))
    # 2. shrubs: dead brush on the bog hummocks and thick along every bank foot
    def bog_shrub(x, y):
        c = cell(x, y)
        z = M["height"][c]
        return 12.0 < z < 115.0 and M["road_dist"][c] > 50.0 and clump2[c] > 0.45

    for x, y in scatter(5200, bog_shrub):
        k = RNG.integers(0, 15)
        inst("Shrub_%02d" % k, SHRUBS[k], x, y, scale=RNG.uniform(0.9, 2.0), dz=-6.0)

    def dry_shrub(x, y):
        c = cell(x, y)
        return M["dry"][c] and M["road_dist"][c] > 250.0 and not M["pad"][c] and M["slope"][c] < 30.0 and clump2[c] > 0.55 \
            and not compound_keep(x, y)

    for x, y in scatter(1500, dry_shrub, margin=0.0):
        k = RNG.integers(0, 15)
        inst("Shrub_%02d" % k, SHRUBS[k], x, y, scale=RNG.uniform(0.8, 1.6), dz=-5.0)
    # 3. trees a unit can reach are real crushable / destructible actors: bench, rises, ridge-top rim
    spruces = [e for e in assets_like("spruce_") if "Uprooted" not in e["id"]]
    uprooted = [e for e in assets_like("spruce_") if "Uprooted" in e["id"]]

    def reach_tree(x, y):
        c = cell(x, y)
        if not M["dry"][c] or M["road_dist"][c] < 420.0 or M["pad"][c] or M["slope"][c] > 16.0 or M["island_strip"][c]:
            return False
        if M["khutor"][c] or M["island"][c]:
            return False
        if compound_keep(x, y) or site_keep(x, y):
            return False
        want = 0.50 if M["bench"][c] else 0.58
        return clump[c] + 0.3 * clump2[c] > want + RNG.uniform(0.0, 0.3)

    for i, (x, y) in enumerate(scatter(430, reach_tree, margin=0.0)):
        roll = RNG.random()
        if uprooted and roll < 0.06:
            bp("Tree_Uprooted_%03d" % i, uprooted[0]["asset_path"], x, y, RNG.uniform(0, 360), 0.0, "Trees/DeadSpruce")
        elif spruces and roll < 0.75:
            entry = spruces[RNG.integers(0, len(spruces))]
            bp("Tree_Spruce_%03d" % i, entry["asset_path"], x, y, RNG.uniform(0, 360), -8.0, "Trees/DeadSpruce")
        elif roll < 0.9:
            bp("Tree_Dead_%03d" % i, D + "BP_Scorch_Dest_DeadTree", x, y, RNG.uniform(0, 360), -10.0, "Trees/Dead", RNG.uniform(0.6, 0.9))
        else:
            bp("Tree_Willow_%03d" % i, D + "BP_Scorch_Dest_DeadWillow", x, y, RNG.uniform(0, 360), -12.0, "Trees/Willow", RNG.uniform(0.8, 1.1))
    # a few live survivors on the start rise and the far rises: the Phytophage has not finished here
    for rid, n in (("start_rise", 5), ("rise_fuel", 3), ("rise_pilot_n", 2), ("rise_pilot_s", 2), ("rise_far_radixite", 2)):
        cx, cy = region(rid)["centre"]
        rad = region(rid)["raw"]["blob"]["r"]
        k = 0
        for _ in range(200):
            a, d = RNG.uniform(0, 2 * math.pi), RNG.uniform(0.62, 0.9) * rad
            x, y = cx + d * math.cos(a), cy + d * math.sin(a)
            c = cell(x, y)
            if M["rises"][c] and M["road_dist"][c] > 350.0 and M["slope"][c] < 10.0:
                bp("Tree_Live_%s_%d" % (rid, k), E + "WpoTreeFoliage/Spruces/" + ("BP_WpoSpruce_SmallDark" if k % 2 else "BP_WpoSpruce_TallDark"),
                   x, y, RNG.uniform(0, 360), -5.0, "Trees/Live")
                k += 1
                if k >= n:
                    break
    # 4. fallen logs as cover beside the lanes and on the bench (destructible natural cover)
    def log_spot(x, y):
        c = cell(x, y)
        return M["dry"][c] and 120.0 < M["road_dist"][c] < 900.0 and M["slope"][c] < 8.0 and not M["pad"][c] and not M["top"][c] \
            and not site_keep(x, y)

    for i, (x, y) in enumerate(scatter(46, log_spot, margin=300.0)):
        bp("Log_%02d" % i, D + "NaturalCover/Logs/BP_Cover_DeadTreeLog_%02d" % RNG.integers(1, 11), x, y, RNG.uniform(0, 360), 0.0, "Cover/Logs")
    # 5. drift logs and stumps in the bog (instances)
    for x, y in scatter(380, lambda x, y: -40.0 < at("height", x, y) < 60.0 and at("road_dist", x, y) > 300.0):
        k = RNG.integers(0, 10)
        inst("BogLog_%02d" % k, LOGS[k], x, y, scale=RNG.uniform(0.8, 1.3), dz=-18.0)
    for x, y in scatter(320, lambda x, y: 5.0 < at("height", x, y) < 110.0 and at("road_dist", x, y) > 200.0):
        inst("BogStump", STUMP, x, y, scale=RNG.uniform(0.7, 1.5), dz=-8.0)
    # 6. bog foliage kit (reeds, cattails, tussocks, algae mats) once imported
    bog_foliage()


BOG_MI = "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Map/MI_Pripyat_BogFoliage_Dark"
PLUM_MI = "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/MI_Phytophage_AshMauve"


def tint_groups():
    """Map materials on the instanced kits: the reeds read near white from above with the kit material, and the
    violet web material turns the big floating mats into scribbles."""
    for group, spec in INST.items():
        if group.startswith(("Reed_", "Tussock_")):
            spec["material"] = BOG_MI.replace("_Dark", "_Olive" if group.endswith("_0") else "_Brown")
        elif group.startswith("Cattail"):
            spec["material"] = BOG_MI
        elif group.startswith(("AlgaeMat_", "Phyto_")):
            spec["material"] = PLUM_MI


def bog_foliage():
    reeds = assets_like("bog_reed_")
    cattail = asset("bog_cattail_a")
    tussocks = assets_like("bog_sedge_")
    mats = assets_like("bog_algaemat_")
    snags = assets_like("bog_snag_")
    patch = noise_field(31, 16)
    gate = noise_field(32, 60)                       # broad areas with and without low growth
    belt = noise_field(33, 34)
    if reeds:
        def reed_spot(x, y):
            c = cell(x, y)
            z = M["height"][c]
            if M["road_dist"][c] < 60.0:
                return False
            if M["reeds"][c]:
                return True
            return -45.0 < z < 22.0 and belt[c] > 0.6 and patch[c] > 0.42
        for x, y in scatter(8000, reed_spot):
            a_ = reeds[RNG.integers(0, len(reeds))]
            inst("Reed_%s_%d" % (a_["id"][-1], RNG.integers(0, 2)), a_["asset_path"], x, y, scale=RNG.uniform(0.45, 0.85), dz=-10.0)
        if cattail:
            for x, y in scatter(1500, lambda x, y: -30.0 < at("height", x, y) < 0.0 and belt[cell(x, y)] > 0.55 and patch[cell(x, y)] > 0.5 and at("road_dist", x, y) > 60.0):
                inst("Cattail", cattail["asset_path"], x, y, scale=RNG.uniform(0.6, 0.95), dz=-8.0)
    if tussocks:
        for x, y in scatter(7000, lambda x, y: -22.0 < at("height", x, y) < 34.0 and at("road_dist", x, y) > 80.0 and gate[cell(x, y)] > 0.5 and patch[cell(x, y)] > 0.4):
            a_ = tussocks[RNG.integers(0, len(tussocks))]
            inst("Tussock_%s_%d" % (a_["id"][-1], RNG.integers(0, 2)), a_["asset_path"], x, y, scale=RNG.uniform(0.6, 1.1), dz=-6.0)
    # shore fringe: a thick belt of reeds, tussocks and dead brush on every bank toe at the waterline
    if reeds or tussocks:
        toe = lambda x, y: M["bank_toe"][cell(x, y)] and inside(x, y, -6000.0)
        for x, y in scatter(26000, toe, tries=60):
            roll = RNG.random()
            if reeds and roll < 0.55:
                a_ = reeds[RNG.integers(0, len(reeds))]
                inst("Reed_%s_%d" % (a_["id"][-1], RNG.integers(0, 2)), a_["asset_path"], x, y, scale=RNG.uniform(0.5, 0.9), dz=-10.0)
            elif tussocks and roll < 0.85:
                a_ = tussocks[RNG.integers(0, len(tussocks))]
                inst("Tussock_%s_%d" % (a_["id"][-1], RNG.integers(0, 2)), a_["asset_path"], x, y, scale=RNG.uniform(0.7, 1.2), dz=-8.0)
            else:
                k_ = RNG.integers(0, 15)
                inst("Shrub_%02d" % k_, SHRUBS[k_], x, y, scale=RNG.uniform(1.0, 1.8), dz=-8.0)
    # a brush hedge along the rim of every rise (the rim of a 60 degree bank is a sawtooth on the 1 m grid)
    rises = M["rises"] | M["aa_rises"]
    q = np.pad(rises, 2, mode="edge")
    grown = np.zeros_like(rises)
    shrunk = np.ones_like(rises)
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            part = q[2 + dy:2 + dy + rises.shape[0], 2 + dx:2 + dx + rises.shape[1]]
            if abs(dy) <= 1 and abs(dx) <= 1:
                shrunk &= part
            grown |= part
    rim = grown & ~shrunk
    rim_spot = lambda x, y: rim[cell(x, y)] and at("road_dist", x, y) > 120.0 and at("height", x, y) > 40.0 and not M["bench"][cell(x, y)]
    for x, y in scatter(9000, rim_spot, margin=0.0, tries=80):
        k_ = RNG.integers(0, 15)
        inst("Shrub_%02d" % k_, SHRUBS[k_], x, y, scale=RNG.uniform(1.4, 2.6), dz=-14.0)
    # overgrown bank faces of the rises and causeways: brush and tussocks break the sharp rim line of the 60 degree banks
    face = lambda x, y: M["bank_face"][cell(x, y)] and inside(x, y, -3000.0)
    for x, y in scatter(15000, face, tries=60):
        if tussocks and RNG.random() < 0.35:
            a_ = tussocks[RNG.integers(0, len(tussocks))]
            inst("Tussock_%s_%d" % (a_["id"][-1], RNG.integers(0, 2)), a_["asset_path"], x, y, scale=RNG.uniform(0.9, 1.5), dz=-10.0)
        else:
            k_ = RNG.integers(0, 15)
            inst("Shrub_%02d" % k_, SHRUBS[k_], x, y, scale=RNG.uniform(1.2, 2.2), dz=-12.0)
    if snags:
        for x, y in scatter(700, lambda x, y: -45.0 < at("height", x, y) < 60.0 and at("road_dist", x, y) > 250.0):
            a = snags[RNG.integers(0, len(snags))]
            inst("Snag_" + a["id"][-2:], a["asset_path"], x, y, scale=RNG.uniform(0.8, 1.4), dz=-15.0, roll=RNG.uniform(-10, 10))


# ---- keep-outs ------------------------------------------------------------------------------------------------------
SITES = []          # (x, y, radius): no scattered trees or logs here


def site_keep(x, y):
    return any(math.hypot(x - sx, y - sy) < r for sx, sy, r in SITES)


def compound_keep(x, y):
    """The whole airfield and compound on the ridge top: scatter stays out, the site code dresses it."""
    yaw = math.radians(AIR["yaw"])
    dx, dy = x - AIR["centre"][0], y - AIR["centre"][1]
    lx, ly = dx * math.cos(yaw) + dy * math.sin(yaw), -dx * math.sin(yaw) + dy * math.cos(yaw)
    return abs(lx) < 16500.0 and -8600.0 < ly < 12000.0 and math.hypot(x, y) < 12600.0


# ---- sites ----------------------------------------------------------------------------------------------------------
SB = D + "NaturalCover/SandBagWalls/"
BAR = D + "Military/Barricades/"
AIRP = D + "Airport/"


def ring_of_sandbags(tag, cx, cy, radius, gaps, folder, arc="BP_Cover_RusSandbags_LongArc", count=None):
    """Sandbag arcs closing a ring round a gun position; gaps = bearings (deg) left open. The arc mesh is a 6 m chord
    along its local X, so it is turned tangent (bearing + 90)."""
    # the arc piece is about a third of a 3.3 m circle and its pivot is the middle of its bounding box, a metre inside
    # the arc: three pieces at 2.4 m from the centre close a ring, and the gap bearing leaves one of them out
    n = 3
    radius = 240.0
    first = gaps[0] if gaps else 0.0
    for i in range(n):
        b = first + i * 120.0
        if i == 0 and gaps:
            continue
        x = cx + radius * math.cos(math.radians(b))
        y = cy + radius * math.sin(math.radians(b))
        bp("%s_Bags_%02d" % (tag, i), SB + arc, x, y, b - 90.0, 0.0, folder)


def aa_sites():
    """Four batteries, four different fights: W plain sandbags (the first one the player meets), N a wired rail-side
    store, E a hardened concrete position next to the compound, S an improvised kolkhoz battery."""
    ind = D + "Industrial/"
    for rid, name in (("rise_aa_w", "W"), ("rise_aa_n", "N"), ("rise_aa_e", "E"), ("rise_aa_s", "S")):
        cx, cy = region(rid)["centre"]
        SITES.append((cx, cy, 2200.0))
        folder = "Sites/AA_" + name
        out = math.degrees(math.atan2(cy, cx))                     # bearing away from the hill
        co, so = math.cos(math.radians(out)), math.sin(math.radians(out))
        cl, sl = math.cos(math.radians(out + 90.0)), math.sin(math.radians(out + 90.0))

        def loc(fwd, left):
            return cx + fwd * co + left * cl, cy + fwd * so + left * sl

        guns = (loc(250.0, 800.0), loc(250.0, -800.0))
        for k, (gx, gy) in enumerate(guns):
            ring_of_sandbags("AA%s_%d" % (name, k), gx, gy, 390.0, [out + 180.0], folder, count=4)
            note("aa_%s_gun%d" % (name.lower(), k), "AA-%s gun %d (61-K)" % (name, k + 1), gx, gy)
        for k in range(-3, 4):                                     # wire on the bog side
            b = out + k * 17.0
            bp("AA%s_Wire_%d" % (name, k + 3), BAR + "BP_Military_Dest_BarbedWireLong", cx + 1650.0 * math.cos(math.radians(b)),
               cy + 1650.0 * math.sin(math.radians(b)), b, 0.0, folder)
        bp("AA%s_Light" % name, D + "Industrial/Poles/BP_Industrial_Dest_CrushablePoleLight_1", *loc(-1250.0, 700.0), out, 0.0, folder)
        if name == "W":
            bp("AAW_Crates_0", AIRP + "BP_Airport_Dest_CrateStack01", *loc(-800.0, 250.0), out + 20.0, 0.0, folder)
            bp("AAW_Crates_1", AIRP + "BP_Airport_Dest_CrateStack03", *loc(-900.0, -200.0), out - 35.0, 0.0, folder)
            bp("AAW_Barrels", AIRP + "BP_Airport_Dest_BarrelStack2", *loc(-650.0, -650.0), out + 70.0, 0.0, folder)
            bp("AAW_Camonet", D + "Military/BP_Military_Dest_Camonet", *loc(-1250.0, -500.0), out + 90.0, 0.0, folder)
            ring_of_sandbags("AAW_Dir", *loc(-350.0, 0.0), 300.0, [out + 180.0], folder, count=4)
        elif name == "N":
            for k in range(9):                                      # wire enclosure on the hill side
                b = out + 180.0 + (k - 4) * 21.0
                if k == 4:
                    continue
                bp("AAN_Fence_%d" % k, ind + "BP_Industrial_Dest_FenceWire", cx + 1500.0 * math.cos(math.radians(b)), cy + 1500.0 * math.sin(math.radians(b)), b, 0.0, folder)
            bp("AAN_Container_0", ind + "BP_Industrial_Dest_Container", *loc(-850.0, 420.0), out, 0.0, folder)
            bp("AAN_Container_1", D + "PostApo/BP_Dest_PostApo_Container", *loc(-800.0, -500.0), out + 90.0, 0.0, folder)
            for k, (f, l) in enumerate(((-250.0, 0.0), (-450.0, 250.0), (-300.0, -300.0))):
                bp("AAN_Ammo_%d" % k, D + "Military/MissileStacks/BP_Military_Dest_MissileStack_%d" % (k + 1), *loc(f, l), out + 30.0 * k, 0.0, folder)
        elif name == "E":
            for k, l in enumerate((-1150.0, -480.0, 480.0, 1150.0)):  # concrete wall towards the bog, shell holes
                bp("AAE_Wall_%d" % k, D + "Military/ConcreteWall/BP_Dest_Military_ConcreteFence_1", *loc(1150.0 - abs(l) * 0.25, l), out + 90.0 + (-12.0 if l < 0 else 12.0) * (abs(l) > 600.0), 0.0, folder)
            bp("AAE_Bunker", D + "GarrisonableBuildings/RusBunkers/BP_Garrison_RusBunker_1", *loc(-750.0, 0.0), out, 0.0, folder)
            for k, l in enumerate((-700.0, -420.0, 420.0, 700.0)):
                bp("AAE_Teeth_%d" % k, BAR + "BP_Military_Dest_ConcreteBarrier", *loc(-1350.0, l), out + 90.0, 0.0, folder)
            bp("AAE_Barrels", AIRP + "BP_Airport_Dest_BarrelStack3", *loc(-300.0, -100.0), out, 0.0, folder)
        else:
            bp("AAS_HayShed", D + "Farm/BP_Farm_Dest_HayShed", *loc(-750.0, 150.0), out + 90.0, 0.0, folder)
            for k, (f, l, kind) in enumerate(((900.0, 0.0, "BP_Cover_Log3"), (-200.0, 1350.0, "BP_Cover_Log2"), (-200.0, -1350.0, "BP_Cover_Log2"),
                                             (-1300.0, -500.0, "BP_Cover_Log1"))):
                bp("AAS_Logs_%d" % k, D + "NaturalCover/Logs/" + kind, *loc(f, l), out + 90.0, 0.0, folder)
            bp("AAS_Tractor", D + "NaturalCover/Tractors/BP_Cover_Tractor_S65", *loc(-1250.0, 500.0), out + 40.0, 0.0, folder)
            bp("AAS_Crates", AIRP + "BP_Airport_Dest_CrateStack02", *loc(-250.0, -150.0), out + 15.0, 0.0, folder)
        note("aa_" + name.lower(), "AA battery %s" % name, cx, cy)


def checkpoint():
    cx, cy = feature("checkpoint")["points"][0]
    pts = road("road_west")
    k = int(np.argmin([math.hypot(p[0] - cx, p[1] - cy) for p in pts]))
    (x0, y0), (x1, y1) = pts[max(k - 2, 0)][:2], pts[min(k + 2, len(pts) - 1)][:2]
    d = math.hypot(x1 - x0, y1 - y0)
    ux, uy = (x1 - x0) / d, (y1 - y0) / d                 # along the road towards the hill
    nx, ny = -uy, ux                                      # to the left
    yaw = bearing_yaw(ux, uy)
    folder = "Sites/Checkpoint"
    SITES.append((cx, cy, 3200.0))
    px, py = pts[k][0], pts[k][1]

    def loc(fwd, left):
        return px + ux * fwd + nx * left, py + uy * fwd + ny * left

    # the pad is 26 m wide here (terrain): concrete stubs and a gate close it to one lane a tank must shoot through
    fence = D + "Military/ConcreteWall/"
    bp("CP_Gate", fence + "BP_Dest_PostApo_ConcreteFenceGate", *loc(600.0, 0.0), yaw, 0.0, folder)
    for i, left in enumerate((-900.0, -620.0, 620.0, 900.0)):
        bp("CP_Fence_%d" % i, fence + "BP_Dest_PostApo_ConcreteFence", *loc(600.0, left), yaw, 0.0, folder)
    bp("CP_Block_0", D + "Military/BP_Military_Dest_RoadBlock", *loc(-300.0, 250.0), yaw + 90.0, 0.0, folder)
    bp("CP_Block_1", D + "Military/BP_Military_Dest_RoadBlock", *loc(-900.0, -250.0), yaw + 90.0, 0.0, folder)
    for i, left in enumerate((-560.0, 560.0)):
        bp("CP_Hedgehog_%d" % i, BAR + "BP_Military_Dest_LargeHedgehog", *loc(-1500.0, left), RNG.uniform(0, 360), 0.0, folder)
    # guard hut, bus and stores on the widened shoulders behind the wall
    bp("CP_Hut", D + "PostApo/BP_Dest_PostApo_House", *loc(1250.0, 880.0), yaw, 0.0, folder)
    bp("CP_Bus", D + "Civilian/BP_Civilian_Dest_Bus", *loc(1700.0, -950.0), yaw + 6.0, 0.0, folder)
    for side in (-1.0, 1.0):
        mx, my = loc(1000.0, side * 430.0)
        bp("CP_Nest_%s" % ("L" if side > 0 else "R"), SB + "BP_Cover_RusSandbags_Cornered", mx, my, yaw + (180.0 if side > 0 else 0.0), 0.0, folder)
        note("cp_mg_%s" % ("l" if side > 0 else "r"), "MG nest", mx, my)
    bp("CP_Crates", AIRP + "BP_Airport_Dest_CrateStack02", *loc(2100.0, 850.0), yaw + 30.0, 0.0, folder)
    bp("CP_Barrels", AIRP + "BP_Airport_Dest_BarrelStack2", *loc(550.0, -1050.0), yaw + 10.0, 0.0, folder)
    bp("CP_Light_0", D + "Civilian/Poles/BP_Civilian_Dest_CrushablePoleLight", *loc(350.0, 1080.0), yaw - 90.0, 0.0, folder)
    bp("CP_Flag", D + "Dest_SovietFlag", *loc(900.0, -1080.0), yaw, 0.0, folder)
    note("checkpoint", "Causeway checkpoint", cx, cy)


def arc_fence(tag, r, b0, b1, folder, gaps=(), piece="BP_Dest_PostApo_ConcreteFenceLong", length=1020.0):
    """Concrete fence panels along an arc round the ridge centre (panels run along their local Y)."""
    step = math.degrees(length / r)
    b = b0
    i = 0
    while b + step <= b1 + 0.01:
        mid = b + step / 2.0
        if not any(g0 <= mid <= g1 for g0, g1 in gaps):
            x, y = r * math.cos(math.radians(mid)), r * math.sin(math.radians(mid))
            broken = RNG.random() < 0.12
            if not broken:
                bp("%s_%02d" % (tag, i), D + "Military/ConcreteWall/" + piece, x, y, mid, 0.0, folder)
        b += step
        i += 1


def compound():
    """The Soviet compound on the ridge top, south-east of the airfield kit: two short streets of destructible
    buildings, fenced yards, the resource corner and the gate works at the three ramp heads. Airfield frame (m):
    kit structures reach y 63, AA platform (26..50, 76..108), pit platform (-74..-56, 80..102), north ramp head
    (72, 86), HQ deploy site (-7, 30) with the open ground round it kept free."""
    folder = "Sites/Compound"
    fence = D + "Military/ConcreteWall/"
    # rim fence: about 70 % of the rim that has no ramp cut under it, broken panels in between
    arc_fence("Rim_N", 12050.0, 338.0, 420.0, folder + "/Fence", gaps=((364.0, 376.0), (396.0, 401.0)))
    arc_fence("Rim_SE_a", 12050.0, 123.0, 160.0, folder + "/Fence", gaps=((140.0, 144.0),))
    arc_fence("Rim_SE_b", 12050.0, 175.0, 198.0, folder + "/Fence")
    arc_fence("Rim_SW", 12050.0, 226.0, 250.0, folder + "/Fence")

    def put(tag, what, lx, ly, yaw=0.0, sub=""):
        x, y = air(lx, ly)
        bp(tag, what, x, y, AIR["yaw"] + yaw, 0.0, folder + sub)
        return x, y

    # barracks street along the kit's dumps
    x, y = put("Barracks_0", AIRP + "BP_Airport_Dest_Building01", -8800.0, 7000.0)
    note("garrison", "Garrison", x, y)
    put("Barracks_1", AIRP + "BP_Airport_Dest_Building01", -3200.0, 7050.0)
    put("Barracks_House_0", D + "PostApo/BP_Dest_PostApo_House", -6500.0, 7150.0, 90.0)
    put("Barracks_House_1", D + "PostApo/BP_Dest_PostApo_House", -5600.0, 7150.0, 0.0)
    x, y = put("CommandPost", D + "Military/BP_Military_Dest_Bunker05", -3000.0, 8900.0)
    note("command_post", "Soviet command post (capture target)", x, y)
    gx, gy = air(-3000.0, 7900.0)
    ring_of_sandbags("CP_Guard", gx, gy, 300.0, [AIR["yaw"] + 90.0, AIR["yaw"] - 90.0], folder, count=4)
    # works street: workshop hangar, spotter tower, generator yard
    put("Workshop", D + "Industrial/BP_Industrial_Dest_Hangar", 1300.0, 7700.0, 90.0)
    x, y = put("SpotterWaterTower", D + "PostApo/BP_Dest_PostApo_WaterTower", -700.0, 8100.0)
    note("spotter_post", "Spotter / radar post", x, y)
    put("Generator", D + "PostApo/BP_Dest_PostApo_PwrStation5", 6000.0, 7150.0, 0.0)
    put("Gen_Fuel", D + "Industrial/BP_Industrial_Dest_OilTank2", 5200.0, 8300.0, 0.0)
    # kolkhoz silo and chimney: the pre-war farm the airfield was built on
    # inner fences: yards with gaps, concrete to punch through
    for i, lx in enumerate((-1900.0, -900.0, 2600.0)):
        put("Yard_Fence_a_%d" % i, fence + "BP_Dest_PostApo_ConcreteFenceLong", lx, 6600.0, 90.0, "/Fence")
    for i, ly in enumerate((7300.0, 8350.0)):
        put("Yard_Fence_b_%d" % i, fence + "BP_Dest_PostApo_ConcreteFenceLong", -1800.0 + 150.0 * i, ly + 2700.0, 90.0, "/Fence")
    put("Yard_Gate", fence + "BP_Dest_PostApo_ConcreteFenceGate", 150.0, 6600.0, 90.0, "/Fence")
    for i, (lx, ly, turn) in enumerate(((-7500.0, 6850.0, 90.0), (-4600.0, 6850.0, 90.0), (-9950.0, 7500.0, 0.0), (-1900.0, 8500.0, 0.0),
                                        (3300.0, 8650.0, 90.0), (4400.0, 6650.0, 90.0), (7200.0, 7300.0, 0.0))):
        put("Yard_Link_%d" % i, fence + "BP_Dest_PostApo_ConcreteFenceLong", lx, ly, turn, "/Fence")
    # stores
    for i, (lx, ly, what, yaw) in enumerate(((2500.0, 9300.0, AIRP + "BP_Airport_Dest_CrateStack01", 10.0),
                                            (2050.0, 9000.0, AIRP + "BP_Airport_Dest_CrateStack03", 60.0),
                                            (-1500.0, 7200.0, AIRP + "BP_Airport_Dest_BarrelStack3", 0.0),
                                            (-7400.0, 8100.0, AIRP + "BP_Airport_Dest_CrateStack02", 35.0),
                                            (-8700.0, 8200.0, D + "Industrial/BP_Industrial_Dest_Container", 20.0),
                                            (1200.0, 9600.0, D + "PostApo/BP_Dest_PostApo_Container", 100.0),
                                            (-1100.0, 9300.0, D + "PostApo/BP_Dest_PostApo_WaterTank", 0.0),
                                            (6900.0, 8000.0, D + "PostApo/BP_Dest_PostApo_Engine", 30.0),
                                            (-6000.0, 8300.0, E + "Props/OldCars/Cover/BP_Cover_OldCar_PanelVan", 70.0),
                                            (300.0, 8900.0, AIRP + "BP_Airport_Dest_BarrelStack2", 0.0))):
        put("Yard_%02d" % i, what, lx, ly, yaw, "/Yard")
    # resources: the Radixite seep at the south-east rim and the aircraft boneyard (metal)
    rx, ry = air(-300.0, 11000.0)
    bp("Res_Radixite_Base_0", RES + "Radixite/Resource_Radixite_Crystal_Large", rx, ry, 30.0, 0.0, "Resources/Radixite")
    bp("Res_Radixite_Base_1", RES + "Radixite/Resource_Radixite_Crystal_SmallDecal", rx - 300.0, ry - 650.0, 120.0, 0.0, "Resources/Radixite")
    bp("Res_Radixite_Base_2", RES + "Radixite/Resource_Radixite_Crystal_SmDecal_Growth", rx - 650.0, ry + 420.0, 250.0, 0.0, "Resources/Radixite")
    note("radixite_base", "Radixite seep", rx, ry)
    mx, my = air(-3300.0, 10600.0)
    bp("Res_Metal_Base_0", RES + "Metal/Resource_Metal_BrokenWaterTank", mx, my, AIR["yaw"] + 15.0, 0.0, "Resources/Metal")
    bp("Res_Metal_Base_1", RES + "Metal/Resource_Metal_Barrels", mx + 900.0, my + 300.0, 80.0, 0.0, "Resources/Metal")
    bp("Res_Metal_Base_2", RES + "Metal/Resource_Metal_Rails", mx - 300.0, my - 800.0, AIR["yaw"], 0.0, "Resources/Metal")
    note("metal_base", "Aircraft boneyard (metal)", mx, my)
    # west gate at the top of the west ramp: two bunkers, a gate in a short fence, dragon's teeth
    gate = road("ramp_w")[-2]
    prev = road("ramp_w")[-8]
    ux, uy = gate[0] - prev[0], gate[1] - prev[1]
    d = math.hypot(ux, uy)
    ux, uy = ux / d, uy / d
    nx, ny = -uy, ux
    yaw = bearing_yaw(ux, uy)
    gx, gy = gate[0] + ux * 600.0, gate[1] + uy * 600.0
    for side in (-1.0, 1.0):
        tag = "L" if side > 0 else "R"
        bp("Gate_Bunker_" + tag, D + "GarrisonableBuildings/RusBunkers/BP_Garrison_RusBunker_1",
           gx + nx * side * 1750.0 + ux * 950.0, gy + ny * side * 1750.0 + uy * 950.0, yaw + 180.0, 0.0, folder + "/WestGate")
        bp("Gate_Fence_" + tag, fence + "BP_Dest_PostApo_ConcreteFenceLong", gx + nx * side * 745.0 + ux * 150.0, gy + ny * side * 745.0 + uy * 150.0, yaw, 0.0, folder + "/WestGate")
        bp("Gate_Fence2_" + tag, fence + "BP_Dest_PostApo_ConcreteFenceLong", gx + nx * side * 2420.0 + ux * 150.0, gy + ny * side * 2420.0 + uy * 150.0, yaw, 0.0, folder + "/WestGate")
    bp("Gate_Gate", fence + "BP_Dest_PostApo_ConcreteFenceGate", gx + ux * 150.0 - nx * 105.0, gy + uy * 150.0 - ny * 105.0, yaw, 0.0, folder + "/WestGate")
    bp("Gate_AT_Bags", SB + "BP_Cover_RusSandbags_LongArc", gx + ux * 1700.0, gy + uy * 1700.0, yaw - 90.0, 0.0, folder + "/WestGate")
    note("west_gate", "West gate bunkers (2 MG bunkers, 1 AT pit)", gx, gy)
    for rid, label in (("ramp_n", "N head pit"), ("ramp_s", "S / E head pits")):
        top, before = road(rid)[-2], road(rid)[-8]
        ux, uy = top[0] - before[0], top[1] - before[1]
        d = math.hypot(ux, uy)
        ux, uy = ux / d, uy / d
        nx, ny = -uy, ux
        yaw = bearing_yaw(ux, uy)
        for k, side in enumerate((-1.0, 1.0)):
            px, py = top[0] + ux * 900.0 + nx * side * 1300.0, top[1] + uy * 900.0 + ny * side * 1300.0
            ring_of_sandbags("Pit_%s_%d" % (rid[-1].upper(), k), px, py, 390.0, [yaw], folder + "/Pits", count=4)
            note("pit_%s_%d" % (rid[-1], k), label, px, py)
    airfield_ruin()


def airfield_ruin():
    """Thirty years of neglect on the kit's clean pavement: weeds in the joints, crater scars, closed-runway crosses,
    scrap on the hardstands, a stripped Ju 87 hull in one bay."""
    folder = "Sites/Airfield"
    crater = "/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d"
    for i, (lx, ly, size) in enumerate(((-6200.0, 300.0, 520.0), (-3100.0, -600.0, 380.0), (-500.0, 500.0, 620.0), (2400.0, -300.0, 420.0),
                                       (5200.0, 600.0, 560.0), (8100.0, -500.0, 360.0), (-8000.0, -4000.0, 420.0), (3000.0, -4000.0, 480.0),
                                       (0.0, 3000.0, 400.0))):
        x, y = air(lx, ly)
        decal("Runway_Crater_%d" % i, crater % (1 + i % 6), x, y, size)
    for i, lx in enumerate((-4500.0, 4500.0)):
        x, y = air(lx, 0.0)
        bp("Runway_X_%d" % i, AIRP + "Runway_InsDest/BP_Airport_InstDes_RunWayX", x, y, AIR["yaw"], 20.0, folder)
    tufts = ["/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Meshes/SM_GF_DryTuft_A", "/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Meshes/SM_GF_DryTuft_B",
             "/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Meshes/SM_GF_DryLitter"]
    for i in range(1100):
        # weeds follow the slab joints (a 10 m grid) on the runway, taxiway and hardstands
        lx, ly = RNG.uniform(-10000.0, 10000.0), RNG.uniform(-7000.0, 1500.0)
        if not (abs(ly) < 1500.0 or -4600.0 < ly < -3400.0 or ly < -4600.0):
            continue
        if RNG.random() < 0.4:
            if RNG.random() < 0.5:
                lx = round(lx / 1000.0) * 1000.0 + RNG.uniform(-25, 25)
            else:
                ly = round(ly / 1000.0) * 1000.0 + RNG.uniform(-25, 25)
        x, y = air(lx, ly)
        k = int(RNG.integers(0, 3))
        g = INST.setdefault("RunwayWeed_%d" % k, {"mesh": tufts[k], "collision": False, "items": [], "abs_z": AIR["z"] + 16.0, "shadow": False})
        g["items"].append([round(x, 1), round(y, 1), 0.0, round(RNG.uniform(0, 360), 1), round(RNG.uniform(0.7, 1.5), 3), 0.0, 0.0])
    # hardstands (six bays at local x -80 .. 80 m, y -59 m): scrap to scavenge and crush, one stripped hull
    parts = E + "ScavengeObjects/VehicleParts/"
    for i, (lx, what, yaw) in enumerate(((-8000.0, parts + "ScavObj_VehicleParts2", 0.0), (-4800.0, AIRP + "BP_Airport_Dest_DrainPipeStack", 90.0),
                                        (-1600.0, parts + "Cars/ScavObj_PostApo_Car_2", 30.0), (4800.0, parts + "ScavObj_VehicleParts4", 0.0),
                                        (8000.0, parts + "Cars/ScavObj_PostApo_Car_4", 200.0))):
        x, y = air(lx + RNG.uniform(-200, 200), -5900.0 + RNG.uniform(-200, 200))
        bp("Bay_%d" % i, what, x, y, AIR["yaw"] + yaw, 15.0, folder + "/Bays")
    ju = asset("wreck_downed_ju87")
    if ju:
        x, y = air(1600.0, -5800.0)
        g = INST.setdefault("Ju87", {"mesh": ju["asset_path"], "collision": True, "items": []})
        g["items"].append([round(x, 1), round(y, 1), 12.0, AIR["yaw"] + 100.0, 1.0, 0.0, 0.0])


def fuel_depot():
    cx, cy = region("rise_fuel")["centre"]
    SITES.append((cx, cy, 2400.0))
    folder = "Sites/FuelDepot"
    ind = D + "Industrial/"
    items = ((-300.0, -700.0, ind + "BP_Industrial_Dest_OilTank1", 0.0), (-300.0, 300.0, ind + "BP_Industrial_Dest_OilTank2", 90.0),
             (500.0, -500.0, ind + "BP_Industrial_Dest_OilTankBarrels1", 0.0), (600.0, 500.0, ind + "BP_Industrial_Dest_OilTankBarrels2", 45.0),
             (-1000.0, 1000.0, ind + "BP_Industrial_Dest_OilTankBarrels3", 20.0), (1200.0, 0.0, AIRP + "BP_Airport_Dest_BarrelStack3", 0.0),
             (-1200.0, -200.0, ind + "BP_Industrial_Dest_Container", 80.0), (200.0, 1300.0, AIRP + "BP_Airport_Dest_CrateStack01", 15.0))
    for i, (dx, dy, what, yaw) in enumerate(items):
        bp("Fuel_%02d" % i, what, cx + dx, cy + dy, yaw, 0.0, folder)
    for i in range(10):
        b = 200.0 + i * 26.0
        if 250.0 < b < 300.0:
            continue
        bp("Fuel_Fence_%02d" % i, ind + "BP_Industrial_Dest_FenceWire", cx + 1750.0 * math.cos(math.radians(b)), cy + 1750.0 * math.sin(math.radians(b)), b, 0.0, folder + "/Fence")
    gx, gy = feature("fuel_guard")["points"][0]
    gx, gy = cx + (gx - cx) * 0.75, cy + (gy - cy) * 0.75
    ring_of_sandbags("Fuel_Guard", gx, gy, 330.0, [bearing_yaw(cx - gx, cy - gy)], folder)
    note("fuel_depot", "Fuel depot (optional capture)", cx, cy)
    note("fuel_guard", "Depot guard", gx, gy)


def far_radixite():
    cx, cy = region("rise_far_radixite")["centre"]
    SITES.append((cx, cy, 2000.0))
    rad = RES + "Radixite/"
    bp("Res_Radixite_Far_0", rad + "Resource_Radixite_Crystal_Large", cx + 300.0, cy + 500.0, 0.0, 0.0, "Resources/Radixite")
    bp("Res_Radixite_Far_1", rad + "Resource_Radixite_Crystal_LargeWithGrowth", cx - 900.0, cy + 1200.0, 140.0, 0.0, "Resources/Radixite")
    bp("Res_Radixite_Far_2", rad + "Resource_Radixite_Crystal_LargePoint", cx + 1300.0, cy - 200.0, 60.0, 0.0, "Resources/Radixite")
    bp("Res_Radixite_Far_3", rad + "Resource_Radixite_Crystal_SmDecal_Growth", cx - 200.0, cy - 900.0, 220.0, 0.0, "Resources/Radixite")
    bp("Res_Radixite_Far_4", rad + "Resource_Radixite_Crystal", cx + 900.0, cy + 1500.0, 300.0, 0.0, "Resources/Radixite")
    for i, (dx, dy) in enumerate(((-1700.0, 300.0), (1500.0, 1300.0), (500.0, -1700.0), (-900.0, -1500.0))):
        bp("FarRad_Tree_%d" % i, D + "BP_Scorch_Dest_DeadTree_Radixite", cx + dx, cy + dy, RNG.uniform(0, 360), -10.0, "Trees/Radixite", 0.8)
    note("radixite_far", "Big Radixite deposit (optional)", cx, cy)


def island():
    """The air-only base: things with a silhouette for the ME410 to hit, spread along the strip."""
    folder = "Sites/Island"
    a, b = feature("island_airfield")["raw"]["ends"]
    ux, uy = b[0] - a[0], b[1] - a[1]
    d = math.hypot(ux, uy)
    ux, uy = ux / d, uy / d
    nx, ny = -uy, ux
    yaw = bearing_yaw(ux, uy)
    mx, my = (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0

    def loc(fwd, left):
        return mx + ux * fwd + nx * left, my + uy * fwd + ny * left

    def put(tag, what, fwd, left, turn=0.0):
        x, y = loc(fwd, left)
        if not at("island", x, y):
            return None
        bp(tag, what, x, y, yaw + turn, 0.0, folder)
        return x, y

    for k, sgn in enumerate((-1.0, 1.0)):
        for j, side in enumerate((-1.0, 1.0)):
            put("Isl_EndMark_%d_%d" % (k, j), AIRP + "BP_Airport_Dest_BarrelStack2", sgn * (d / 2.0 + 250.0), side * 850.0, 90.0)
    # south-east of the strip: hangar under a net, generator building, command bunker, fuel
    put("Isl_Hangar", D + "PostApo/BP_Dest_PostApo_Hangar1", -900.0, -2300.0, 90.0)
    put("Isl_HangarNet", D + "Military/BP_Military_Dest_Camonet_2", -2300.0, -1900.0, 0.0)
    put("Isl_Generator", D + "PostApo/BP_Dest_PostApo_PwrStation7", 1700.0, -2300.0, 0.0)
    pos = put("Isl_CommandBunker", D + "Military/BP_Military_Dest_Bunker05", 3600.0, -1500.0, 0.0)
    if pos:
        note("island_base", "AIR-ONLY BASE: generator building + command bunker", *pos)
    put("Isl_Fuel_0", D + "Industrial/BP_Industrial_Dest_OilTank1", -3500.0, -1300.0, 0.0)
    put("Isl_Fuel_1", D + "Industrial/BP_Industrial_Dest_OilTank2", -3900.0, -700.0 - 500.0, 60.0)
    put("Isl_Crates", AIRP + "BP_Airport_Dest_CrateStack01", 600.0, -1300.0, 20.0)
    # north-west of the strip: three AA pits 40 m apart, dispersal nets between them
    for k, fwd in enumerate((-3800.0, 0.0, 3800.0)):
        x, y = loc(fwd, 1900.0 if k != 1 else 2500.0)
        if at("island", x, y):
            ring_of_sandbags("Isl_AA_%d" % k, x, y, 390.0, [yaw - 90.0], folder, count=4)
            note("island_aa_%d" % k, "Island light AA", x, y)
    for k, fwd in enumerate((-1900.0, 1900.0)):
        pos = put("Isl_Dispersal_%d" % k, D + "Military/BP_Military_Dest_Camonet_2", fwd, 1500.0, 0.0)
        if pos:
            note("island_fighter_%d" % k, "Fighter dispersal", *pos)


def lanes():
    """Old wrecks, poles and cover along the approach lanes. The 14 m lanes keep their decks clear (the only chokes
    are the sketch's 10 m ones); the 35 m east lane carries a wreck kill zone to weave through."""
    tanks = D + "Military/DestTanks/"
    folder = "Sites/Lanes"
    # east sand road: one kill zone from the first Soviet retreat, three hulls within 25 m and one to scavenge
    kx, ky, kyaw = along("road_east", 0.60)
    co, so = math.cos(math.radians(kyaw)), math.sin(math.radians(kyaw))
    for i, (fwd, left, what, turn) in enumerate(((-900.0, 900.0, tanks + "BP_Env_DestroyedT34", 35.0), (300.0, -1000.0, tanks + "BP_Env_Destroyed_KV1E", -70.0),
                                                (1300.0, 700.0, tanks + "BP_Env_DestroyedT34", 160.0),
                                                (-200.0, -200.0, E + "ScavengeObjects/DestroyedTanks/German/Medium/PZIV/ScavObj_Ger_PZIV", 110.0))):
        x, y = kx + co * fwd - so * left, ky + so * fwd + co * left
        bp("Wreck_E_%d" % i, what, x, y, kyaw + turn, 0.0, folder + "/Wrecks")
        decal("Wreck_E_Scorch_%d" % i, "/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d" % (1 + i), x, y, 700.0)
    SITES.append((kx, ky, 2600.0))
    note("kill_zone_e", "Wreck kill zone (cover on the E lane)", kx, ky)
    x, y, yaw = along("road_east", 0.30, 1.0, -520.0)             # the lone IS-3 near the ford: a landmark
    bp("Wreck_E_IS3", tanks + "BP_Env_Destroyed_IS_3", x, y, yaw + 25.0, 0.0, folder + "/Wrecks")
    for i, (fwd, left, what, turn) in enumerate(((2300.0, -600.0, tanks + "BP_Env_DestroyedPz38T", 80.0), (-2100.0, -700.0, tanks + "BP_Env_DestroyedPzIII", 200.0))):
        x, y = kx + co * fwd - so * left, ky + so * fwd + co * left
        bp("Wreck_E_b%d" % i, what, x, y, kyaw + turn, 0.0, folder + "/Wrecks")
    # the khutor on the sand rise beside the east lane: farm ruins with shell holes, a log house, a stork pole
    kc = LAYOUT.get("khutor", [-500.0, 26700.0, 2500.0])
    hx, hy = kc[0], kc[1]
    for i, (aid, dx, dy, turn) in enumerate((("village_loghouse_a", 600.0, 800.0, 20.0), ("village_loghouse_b", 900.0, -800.0, 200.0),
                                             ("village_loghouse_a", -900.0, -300.0, 110.0), ("village_sweepwell", 0.0, 100.0, 40.0))):
        entry = asset(aid)
        if entry:
            bp("Khutor_%d" % i, entry["asset_path"], hx + dx, hy + dy, turn, 0.0, "Sites/Khutor")
    for i, (dx, dy, turn) in enumerate(((-300.0, -1500.0, 90.0), (-1400.0, -1000.0, 0.0))):
        bp("Khutor_Wall_%d" % i, D + "Military/ConcreteWall/BP_Dest_PostApo_ConcreteFenceLong", hx + dx, hy + dy, turn, 0.0, "Sites/Khutor")
    stork = asset("village_storknest_a")
    if stork:
        bp("Khutor_Stork", stork["asset_path"], hx - 300.0, hy + 1500.0, 0.0, 0.0, "Sites/Khutor")
    bp("Khutor_Tractor", D + "NaturalCover/Tractors/BP_Cover_Tractor_STZ1", hx - 1400.0, hy + 900.0, 60.0, 0.0, "Sites/Khutor")
    SITES.append((hx, hy, 2600.0))
    note("khutor", "Khutor ruin", hx, hy)
    # rail halt on the north pilot rise, peat-cutters yard on the south one
    px, py = region("rise_pilot_n")["centre"]
    bp("Halt_House", D + "PostApo/BP_Dest_PostApo_House", px - 300.0, py + 500.0, 0.0, 0.0, "Sites/RailHalt")
    bp("Halt_Tank", D + "PostApo/BP_Dest_PostApo_WaterTank", px + 600.0, py + 900.0, 0.0, 0.0, "Sites/RailHalt")
    for i in range(4):
        bp("Halt_Fence_%d" % i, D + "Industrial/BP_Industrial_Dest_FenceWire", px - 1000.0, py - 500.0 + 380.0 * i, 0.0, 0.0, "Sites/RailHalt")
    px, py = region("rise_pilot_s")["centre"]
    bp("Peat_HayShed", D + "Farm/BP_Farm_Dest_HayShed", px - 300.0, py + 600.0, 90.0, 0.0, "Sites/PeatYard")
    bp("Peat_Tractor", D + "NaturalCover/Tractors/BP_Cover_Tractor_S65", px + 800.0, py + 900.0, 30.0, 0.0, "Sites/PeatYard")
    bp("Peat_Logs_0", D + "NaturalCover/Logs/BP_Cover_Log3", px + 900.0, py - 200.0, 0.0, 0.0, "Sites/PeatYard")
    bp("Peat_Logs_1", D + "NaturalCover/Logs/BP_Cover_Log2", px - 1100.0, py - 300.0, 90.0, 0.0, "Sites/PeatYard")
    # ford: marker posts on the upstream edge of the bar, a drowned car on the downstream one
    fx_, fy_ = feature("ford_e")["points"][0]
    fpts = road("road_east")
    kf = int(np.argmin([math.hypot(q[0] - fx_, q[1] - fy_) for q in fpts]))
    fyaw = bearing_yaw(fpts[kf + 2][0] - fpts[kf - 2][0], fpts[kf + 2][1] - fpts[kf - 2][1])
    fu, fv = math.cos(math.radians(fyaw)), math.sin(math.radians(fyaw))
    for i, t in enumerate((-1100.0, -550.0, 0.0, 550.0, 1100.0)):
        for j, side in enumerate((-1.0, 1.0)):
            qx, qy = fpts[kf][0] + fu * t - fv * side * 1180.0, fpts[kf][1] + fv * t + fu * side * 1180.0
            if at("height", qx, qy) > -40.0:
                bp("Ford_Post_%d_%d" % (i, j), BAR + "BP_Military_Dest_Pylon", qx, qy, fyaw, -15.0, "Sites/Ford")
    bp("Ford_Car", E + "Props/OldCars/Cover/BP_Cover_OldCar_Sedan", fpts[kf][0] + fu * 200.0 + fv * 1250.0, fpts[kf][1] + fv * 200.0 - fu * 1250.0, fyaw + 70.0, -30.0, "Sites/Ford")
    raft("ford", fpts[kf][0] + fv * 2300.0, fpts[kf][1] - fu * 2300.0, 7, 600.0, fyaw)
    # telegraph poles: both shoulders on the north rail lane, one shoulder elsewhere (crushable)
    for rid, sides in (("road_west", (1.0,)), ("road_east", (-1.0,)), ("rail_north", (1.0, -1.0))):
        pts = road(rid)
        for side in sides:
            for i in range(4, len(pts) - 4, 11):
                x, y, yaw = along(rid, i / (len(pts) - 1.0), side, -110.0)
                if not inside(x, y, 300.0) or site_keep(x, y) or at("height", x, y) < 100.0:
                    continue
                kind = "BP_Civilian_Dest_CrushablePole_Electric%d" % (1 + (i // 11) % 2)
                bp("Pole_%s_%03d_%d" % (rid[-5:], i, int(side > 0)), D + "Civilian/Poles/" + kind, x, y, yaw + RNG.uniform(-6, 6), 0.0, folder + "/Poles")
    # cars pushed onto the wide sand shoulder
    for i, (frac, side, car) in enumerate(((0.55, 1.0, "PanelVan"), (0.84, -1.0, "Pickup"))):
        x, y, yaw = along("road_east", frac, side, -330.0)
        bp("Car_%d" % i, E + "Props/OldCars/Cover/BP_Cover_OldCar_" + car, x, y, yaw + RNG.uniform(-25, 25), 0.0, folder + "/Vehicles")
    # notes
    for sid, text in (("spawn_n", "COLUMNS N: rail entry"), ("spawn_e", "COLUMNS E: sand road entry"), ("spawn_s", "COLUMNS S: log causeway entry")):
        x, y = feature(sid)["points"][0]
        note(sid, text, x, y)
    for fid in ("wb_n_slump", "wb_n_bridge", "wb_s_logs", "wb_s_bridge"):
        x, y = feature(fid)["points"][0]
        note(fid, "wreck seal: " + fid, x, y)
    for fid in ("pilot_n", "pilot_s", "pilot_e", "pilot_bog_a", "pilot_bog_b"):
        x, y = feature(fid)["points"][0]
        note(fid, "Downed pilot", x, y)
    x, y = feature("demeter_pod")["points"][0]
    note("demeter_pod", "Demeter pod crash site", x, y)
    road_splines()


def road_splines():
    """Road surfaces as ground-path splines over the paint: the ring and the ramps as the worn macadam perimeter
    track of the old airfield, the west and east lanes and the dam as two-track ruts."""
    def pts_of(rid, thin=3, lo=0, hi=None):
        pts = road(rid)[lo:hi]
        out = [[p[0], p[1]] for p in pts[::thin]]
        if out[-1] != [pts[-1][0], pts[-1][1]]:
            out.append([pts[-1][0], pts[-1][1]])
        return [q for q in out if abs(q[0]) < SIZE / 2.0 - 600.0 and abs(q[1]) < SIZE / 2.0 - 600.0]

    worn = "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Map/MI_Pripyat_Road_Dark"
    ring = pts_of("ring_road", 2)
    ring = ring + [ring[0]]
    n, k, seg = len(ring), 0, 0
    lengths = [14, 9, 11, 16, 8, 13, 10, 15, 9, 12, 14, 10, 13, 11, 9, 16]
    while k < n - 2:
        m = min(lengths[seg % len(lengths)], n - 1 - k)
        part = ring[k:k + m + 1]
        if len(part) >= 3:
            if seg % 3 == 0:
                SPLINES.append(["Road_Ring_%02d" % seg, "BP_GroundPathSpline_TwoTrack", part, 680.0, None])   # the macadam tiles stay pale under any tint: a wider worn track instead
                for end in (part[0], part[-1]):
                    decal("Ring_End_%02d_%d" % (seg, int(end is part[-1])), "/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d" % (1 + seg % 6),
                          end[0] + RNG.uniform(-80, 80), end[1] + RNG.uniform(-80, 80), 950.0)
                    for _ in range(2):
                        decal("Ring_EndS_%02d_%d_%d" % (seg, int(end is part[-1]), _), "/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d" % (1 + (seg + _) % 6),
                              end[0] + RNG.uniform(-450, 450), end[1] + RNG.uniform(-450, 450), RNG.uniform(350.0, 520.0))
            else:
                SPLINES.append(["Road_Ring_%02d" % seg, "BP_GroundPathSpline_TwoTrack", part, 540.0, None])
        k += m
        seg += 1
    for rid in ("ramp_n", "ramp_s", "ramp_w"):
        SPLINES.append(["Road_" + rid, "BP_GroundPathSpline_TwoTrack", pts_of(rid, 4), 520.0, None])
    SPLINES.append(["Road_West", "BP_GroundPathSpline_TwoTrack", pts_of("road_west", 4), 520.0, None])
    SPLINES.append(["Road_East", "BP_GroundPathSpline_TwoTrack", pts_of("road_east", 5), 620.0, None])
    SPLINES.append(["Road_Dam", "BP_GroundPathSpline_TwoTrack", pts_of("dam_far_radixite", 4), 420.0, None])


def start():
    cx, cy = region("start_rise")["centre"]
    SITES.append((cx, cy, 2700.0))                       # the strike group needs the middle of the rise
    folder = "Sites/Start"
    bp("Start_Tractor", D + "NaturalCover/Tractors/BP_Cover_Tractor_STZ1", cx - 2300.0, cy - 1300.0, 140.0, 0.0, folder)
    bp("Start_Car", E + "Props/OldCars/Cover/BP_Cover_OldCar_Pickup", cx - 1600.0, cy + 2500.0, 60.0, 0.0, folder)
    bp("Start_Logs_0", D + "NaturalCover/Logs/BP_Cover_Log3", cx + 2500.0, cy - 1500.0, 30.0, 0.0, folder)
    bp("Start_Logs_1", D + "NaturalCover/Logs/BP_Cover_Log2", cx + 300.0, cy - 2800.0, 80.0, 0.0, folder)
    note("start", "START (strike group)", cx, cy)


def bench():
    """Things to fight over on the 5 m bench: a sandbag outpost at each lane mouth (on the shoulders, the roadway
    stays clear), one encounter set per ring quadrant, and a crushable pole line round the ring."""
    folder = "Sites/Bench"
    for i, (rid, back) in enumerate((("rail_north", 5), ("road_east", 6), ("causeway_south", 5), ("road_west", 5))):
        pts = road(rid)
        end, before = pts[-back], pts[-back - 6]
        ux, uy = end[0] - before[0], end[1] - before[1]
        d = math.hypot(ux, uy)
        ux, uy = ux / d, uy / d
        nx, ny = -uy, ux
        yaw = bearing_yaw(ux, uy)
        for k, side in enumerate((-1.0, 1.0)):
            x, y = end[0] + nx * side * (end[3] + 500.0), end[1] + ny * side * (end[3] + 500.0)
            if at("height", x, y) < 600.0:
                continue
            bp("Bench_Post_%d_%d" % (i, k), SB + ("BP_Cover_RusSandbags_ZigZag" if k else "BP_Cover_RusSandbags_Cornered"), x, y, yaw + 90.0, 0.0, folder)
            bx, by = end[0] + nx * side * (end[3] + 120.0) - ux * 900.0, end[1] + ny * side * (end[3] + 120.0) - uy * 900.0
            if at("height", bx, by) > 600.0:
                bp("Bench_Barrier_%d_%d" % (i, k), BAR + "BP_Military_Dest_MossyConcreteBarrier", bx, by, yaw + RNG.uniform(-25, 25), 0.0, folder)
    # quadrant encounters on the outer bench strip (bearing from north, clockwise)
    def spot(bearing, r=17500.0):
        return r * math.cos(math.radians(bearing)), r * math.sin(math.radians(bearing))

    def put(tag, what, bearing, fwd, left, turn=0.0, r=17500.0):
        cx, cy = spot(bearing, r)
        co, so = math.cos(math.radians(bearing)), math.sin(math.radians(bearing))
        x, y = cx + co * fwd - so * left, cy + so * fwd + co * left
        if at("height", x, y) < 700.0 or at("road_dist", x, y) < 250.0:
            return
        bp(tag, what, x, y, bearing + turn, 0.0, folder + "/Encounters")

    cars = E + "ScavengeObjects/VehicleParts/Cars/"
    # NE motor pool
    b = 80.0
    pool = [(300.0, -900.0 + 330.0 * i, cars + "ScavObj_PostApo_Car_%d" % (1 + i % 5), 90.0 + (8.0 if i % 2 else -6.0)) for i in range(5)]
    pool += [(-450.0, -700.0 + 360.0 * i, E + "Props/OldCars/Cover/BP_Cover_OldCar_" + c, 0.0) for i, c in enumerate(("PanelVan", "Pickup", "Sedan"))]
    pool += [(-300.0, 900.0, D + "NaturalCover/Tractors/BP_Cover_Tractor_S65", 10.0), (500.0, 1150.0, D + "PostApo/BP_Dest_PostApo_Container", 0.0),
             (-950.0, 500.0, AIRP + "BP_Airport_Dest_CrateStack01", 0.0), (-1000.0, -900.0, AIRP + "BP_Airport_Dest_CrateStack03", 90.0),
             (0.0, 400.0, E + "ScavengeObjects/VehicleParts/ScavObj_VehicleParts3", 0.0), (700.0, 500.0, AIRP + "BP_Airport_Dest_BarrelStack3", 0.0)]
    pool += [(820.0, -1000.0 + 380.0 * i, D + "Industrial/BP_Industrial_Dest_FenceWire", 0.0) for i in range(6)]
    for k, (fwd, left, what, turn) in enumerate(pool):
        put("MotorPool_%d" % k, what, b, fwd, left, turn)
    SITES.append((*spot(b), 2000.0))
    note("motor_pool", "Motor pool wrecks (scavenge)", *spot(b))
    # SE signals post
    b = 112.0
    put("Signals_House", D + "PostApo/BP_Dest_PostApo_House", b, 0.0, 0.0, 30.0)
    for k, (fwd, left) in enumerate(((600.0, 700.0), (-200.0, -900.0), (700.0, -300.0))):
        put("Signals_Pole_%d" % k, D + "Industrial/Electricity/BP_Electricity_SinglePole%d" % (k + 1), b, fwd, left)
    put("Signals_Bags", SB + "BP_Cover_RusSandbags_ZigZag", b, -700.0, 300.0, 90.0)
    mast = asset("radio_mast") or asset("mast_radio_leaning")
    if mast:
        put("Signals_Mast", mast["asset_path"], b, 300.0, 100.0)
    SITES.append((*spot(b), 1800.0))
    note("signals_post", "Signals post", *spot(b))
    # SW dug-in picket
    b = 236.0
    put("Picket_Bags_0", SB + "BP_Cover_RusSandbags_DoubleStraight", b, 620.0, 0.0, 0.0)
    put("Picket_Bags_1", SB + "BP_Cover_RusSandbags_DoubleStraight", b, 200.0, 520.0, 90.0)
    put("Picket_Bags_2", SB + "BP_Cover_RusSandbags_DoubleStraight", b, 200.0, -520.0, 90.0)
    put("Picket_Bunker", D + "GarrisonableBuildings/Bunkers/BP_Garrison_Bunker_2", b, -150.0, 0.0, 0.0)
    put("Picket_Logs", D + "NaturalCover/Logs/BP_Cover_Log3", b, 900.0, 300.0, 90.0)
    put("Picket_Crates", AIRP + "BP_Airport_Dest_CrateStack03", b, -300.0, 200.0, 20.0)
    put("Picket_Wire_0", BAR + "BP_Military_Dest_BarbedWireLong", b, 1150.0, -500.0, 0.0)
    put("Picket_Wire_1", BAR + "BP_Military_Dest_BarbedWireLong", b, 1150.0, 500.0, 0.0)
    SITES.append((*spot(b), 1800.0))
    note("picket_sw", "Dug-in picket", *spot(b))
    # NW kolkhoz outbuilding (the log houses come from polesie())
    put("NW_Hut", D + "PostApo/BP_Dest_PostApo_House", 324.0, 0.0, 0.0, 20.0)
    # the pole line round the ring: outer edge, every 36 m, skipping the lane mouths and sites
    ring = road("ring_road")
    for i in range(0, len(ring), 12):
        x, y, yaw = along("ring_road", i / (len(ring) - 1.0), -1.0, 60.0)
        if at("height", x, y) < 700.0:
            x, y, yaw = along("ring_road", i / (len(ring) - 1.0), 1.0, 60.0)
        if at("height", x, y) < 700.0 or site_keep(x, y) or at("road_dist", x, y) < 30.0:
            continue
        bp("RingPole_%03d" % i, D + "Civilian/Poles/BP_Civilian_Dest_CrushablePole_Electric%d" % (1 + (i // 12) % 2), x, y, yaw + RNG.uniform(-8, 8), 0.0, folder + "/Poles")


def polesie():
    """The imported Polesie kits: hamlets (destructible log houses with shell holes, crushable poles and wells), the
    partisan memorial, the place-name signs at the entries, downed Ju 87s at the pilot sites, Phytophage growth."""
    house_a, house_b = asset("village_loghouse_a"), asset("village_loghouse_b")
    stork_a, stork_b, well = asset("village_storknest_a"), asset("village_storknest_b"), asset("village_sweepwell")
    fence = E + "CrushableInstancedActor/FarmCrops/BP_DestSpline_FarmFence"
    curtain = assets_like("phytophage_curtain_")
    corner = assets_like("phytophage_corner_")
    mats = assets_like("phytophage_mat_")
    wraps = assets_like("phytophage_wrap_")

    def growth(x, y, yaw, tag):
        """Purple webs hung on a building's walls."""
        if not curtain:
            return
        for k, (off, turn) in enumerate(((330.0, 0.0), (-330.0, 180.0))):
            if RNG.random() < 0.75:
                c = curtain[k % len(curtain)]
                inst("Phyto_" + c["id"][-10:], c["asset_path"], x + off * math.cos(math.radians(yaw + 90.0)), y + off * math.sin(math.radians(yaw + 90.0)),
                     yaw=yaw + turn, scale=RNG.uniform(1.0, 1.4), dz=60.0)


    def hamlet(tag, cx, cy, spots, folder):
        for k, (dx, dy, what, yaw) in enumerate(spots):
            entry = {"A": house_a, "B": house_b, "S": stork_a if k % 2 else stork_b, "W": well,
                     "F": {"asset_path": D + "PostApo/BP_Dest_PostApo_House"}}.get(what)
            if not entry:
                continue
            x, y = cx + dx, cy + dy
            bp("%s_%s_%d" % (tag, what, k), entry["asset_path"], x, y, yaw, 0.0, folder)
            if what in ("A", "B"):
                growth(x, y, yaw, tag)
                SITES.append((x, y, 900.0))

    # start rise: the abandoned edge of a kolkhoz hamlet, on the rim so the middle stays free for the strike group
    cx, cy = region("start_rise")["centre"]
    hamlet("Start", cx, cy, ((-2100.0, 1300.0, "A", 20.0), (1900.0, 2300.0, "B", 200.0), (-900.0, 2700.0, "W", 90.0), (2700.0, 300.0, "S", 0.0),
                             (-2800.0, -400.0, "S", 0.0)), "Sites/Start")
    # two bench hamlets between the lane mouths: houses to fight through on the ring
    for tag, bearing, spots in (("BenchNE", 47.0, ((600.0, -1100.0, "A", 150.0), (700.0, 1000.0, "B", 20.0), (-900.0, 0.0, "F", 90.0), (0.0, 0.0, "W", 40.0), (1300.0, 0.0, "S", 0.0))),
                               ("BenchSW", 200.0, ((600.0, 1000.0, "B", 300.0), (500.0, -1100.0, "A", 100.0), (-900.0, 0.0, "F", 90.0), (0.0, 0.0, "W", 0.0), (1300.0, 200.0, "S", 0.0))),
                               ("BenchNW", 312.0, ((600.0, -1000.0, "A", 30.0), (700.0, 1000.0, "B", 200.0), (0.0, 0.0, "W", 200.0), (1300.0, -100.0, "S", 0.0)))):
        # spots are (outward, along the ring) so every hamlet stands round a yard on the outer bench strip
        co_, so_ = math.cos(math.radians(bearing)), math.sin(math.radians(bearing))
        spots = tuple((f * co_ - l * so_, f * so_ + l * co_, w, yw + bearing) for f, l, w, yw in spots)
        hx, hy = 17650.0 * math.cos(math.radians(bearing)), 17650.0 * math.sin(math.radians(bearing))
        ok = [(dx, dy, w, yw) for dx, dy, w, yw in spots if at("bench", hx + dx, hy + dy) or (at("height", hx + dx, hy + dy) > 700.0 and at("road_dist", hx + dx, hy + dy) > 500.0)]
        hamlet(tag, hx, hy, ok, "Sites/Hamlets")
        SITES.append((hx, hy, 2300.0))
        note("hamlet_" + tag.lower(), "Kolkhoz hamlet ruin", hx, hy)
    # the purple-stained cottage on the smouldering peat (out of reach: a landmark, not cover)
    for rid, what in (("burning_peat_se", house_b), ("burning_peat_nw", house_a)):
        if what:
            bx, by = region(rid)["centre"]
            bp("Cottage_" + rid[-2:], what["asset_path"], bx + 500.0, by - 300.0, RNG.uniform(0, 360), 30.0, "Sites/BurningPeat")
            growth(bx + 500.0, by - 300.0, 0.0, "Cottage")
    # partisan memorial on the start rise beside the west road, inscription towards the road
    mem = asset("memorial_partisan")
    if mem:
        pts = road("road_west")
        k0 = int(np.argmin([math.hypot(p[0] - cx, p[1] - cy) for p in pts]))
        k = next(i for i in range(k0, len(pts)) if math.hypot(pts[i][0] - cx, pts[i][1] - cy) > 2500.0)
        (x0, y0), (x1, y1) = pts[k - 2][:2], pts[k + 2][:2]
        d = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / d, (y1 - y0) / d
        # on the rim of the start rise, a few metres left of where the road leaves it
        mx, my = pts[k][0] - uy * 1150.0, pts[k][1] + ux * 1150.0
        for back in range(0, 2600, 100):
            qx, qy = pts[k][0] - ux * back - uy * 1250.0, pts[k][1] - uy * back + ux * 1250.0
            if at("rises", qx, qy) and at("height", qx, qy) > 430.0 and at("road_dist", qx, qy) > 250.0:
                mx, my = qx, qy
                break
        # the face is local +Y: turn +Y towards the road centre
        to_road = bearing_yaw(pts[k][0] - mx, pts[k][1] - my)
        bp("Memorial", mem["asset_path"], mx, my, to_road - 90.0, 0.0, "Sites/Start")
        SITES.append((mx, my, 700.0))
        note("memorial", "Partisan memorial (audio log)", mx, my)
    # place-name signs: one per entry, 30 m inside the edge on the shoulder, the board towards the incoming traffic
    b = LAYOUT["bounds"]
    for rid, sign, back in (("rail_north", "sign_railsign_kalinkavichy", False), ("road_east", "sign_roadsign_mozyr", False),
                            ("causeway_south", "sign_roadsign_stolin", False), ("road_west", "sign_roadsign_pinsk", True),
                            ("fuel_siding", "sign_railsign_luninets", True), ("dam_far_radixite", "sign_roadsign_turov", True)):
        entry = asset(sign)
        if not entry:
            continue
        pts = [p for p in road(rid) if b["xmin"] + 2600.0 < p[0] < b["xmax"] - 2600.0 and b["ymin"] + 2600.0 < p[1] < b["ymax"] - 2600.0]
        k = 6 if not back else min(12, len(pts) - 3)
        while pts[k][2] < 150.0 and k < len(pts) - 4:    # never in the ford
            k += 4
        (x0, y0), (x1, y1) = pts[k - 2][:2], pts[k + 2][:2]
        d = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / d, (y1 - y0) / d
        w = pts[k][3]
        sx, sy = pts[k][0] - uy * (w - 90.0), pts[k][1] + ux * (w - 90.0)
        # +Y of the board towards the traffic that comes in along -u
        bp("Sign_" + rid, entry["asset_path"], sx, sy, bearing_yaw(-ux, -uy) - 90.0, 0.0, "Sites/Signs")
    # downed Ju 87s at the five pilot sites
    ju = asset("wreck_downed_ju87")
    if ju:
        for fid in ("pilot_n", "pilot_s", "pilot_e", "pilot_bog_a", "pilot_bog_b"):
            x, y = feature(fid)["points"][0]
            if fid == "pilot_e":                      # on the east lane: the wreck lies on the sand shoulder, clear of the gravel
                pts = road("road_east")
                k = int(np.argmin([math.hypot(p[0] - x, p[1] - y) for p in pts]))
                (x0, y0), (x1, y1) = pts[k - 2][:2], pts[k + 2][:2]
                d = math.hypot(x1 - x0, y1 - y0)
                x, y = pts[k][0] + (y1 - y0) / d * 1150.0, pts[k][1] - (x1 - x0) / d * 1150.0
            if fid in ("pilot_n", "pilot_s"):
                x, y = x + 700.0, y - 900.0              # off the middle: the rise stays drivable
            g = INST.setdefault("Ju87", {"mesh": ju["asset_path"], "collision": True, "items": []})
            g["items"].append([round(x, 1), round(y, 1), -6.0, round(RNG.uniform(0, 360), 1), 1.0, 0.0, 0.0])
            SITES.append((x, y, 1300.0))
            for k in range(3):
                bp("Pilot_%s_Debris_%d" % (fid, k), D + "NaturalCover/Logs/BP_Cover_DeadTreeLog_%02d" % RNG.integers(1, 11), x + RNG.uniform(-900, 900),
                   y + RNG.uniform(-900, 900), RNG.uniform(0, 360), 0.0, "Sites/Pilots") if fid in ("pilot_n", "pilot_s") else None
    # Phytophage on the drowned trees nearest the algae water, and veined mats on the algae shores
    if wraps:
        done = 0
        for group, spec in list(INST.items()):
            if not (group.startswith("BogTree_") or group.startswith("BogWillow_")):
                continue
            for x, y, dz, yaw, scale, pitch, roll in spec["items"]:
                c = cell(x, y)
                if M["height"][c] < 12.0 and RNG.random() < 0.55 and done < 260:
                    w = wraps[done % len(wraps)]
                    inst("Phyto_" + w["id"][-7:], w["asset_path"], x, y, scale=RNG.uniform(0.7, 1.1), dz=-10.0)
                    done += 1
    if mats:
        pass


def peat_fires():
    """Smouldering peat: wisps and a few heavier columns over both burnt patches, vapour at the hottest spots."""
    atmo = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Atmosphere/"
    for rid, n in (("burning_peat_se", 16), ("burning_peat_nw", 10)):
        cx, cy = region(rid)["centre"]
        rad = region(rid)["raw"]["blob"]["r"]
        for k in range(n):
            a, d = RNG.uniform(0, 2 * math.pi), rad * math.sqrt(RNG.uniform(0.02, 0.8))
            x, y = cx + d * math.cos(a), cy + d * math.sin(a)
            kind = "NS_BerlinAtmo_Smoulder_Hero" if k % 6 == 0 else ("NS_BerlinAtmo_CraterVapour" if k % 6 == 3 else "NS_BerlinAtmo_Smoulder_Wisp")
            fx("Peat_%s_%02d" % (rid[-2:], k), atmo + kind, x, y, 20.0)
        note(rid, "Smouldering peat (impassable)", cx, cy)


def mesh(group, aid_or_path, x, y, yaw, dz=0.0, collision=False, abs_z=None, scale=1.0):
    """One instance of a kit mesh in its own group (group = kind of piece)."""
    entry = asset(aid_or_path) if not aid_or_path.startswith("/") else {"asset_path": aid_or_path}
    if not entry:
        return False
    g = INST.setdefault(group, {"mesh": entry["asset_path"], "collision": collision, "items": []})
    if abs_z is not None:
        g["abs_z"] = abs_z
    g["items"].append([round(float(x), 1), round(float(y), 1), round(float(dz), 1), round(float(yaw), 1), scale, 0.0, 0.0])
    return True


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


def raft(tag, x, y, count, spread, heading=None):
    """Algae rafts are off: scaled up, the kit's web material draws bright line-art on the dark water. The foam rim
    of the water material carries the algae on its own."""
    return
    mats = assets_like("bog_algaemat_")
    if not mats:
        return
    for k in range(count):
        a_ = mats[k % len(mats)]
        g = INST.setdefault("AlgaeMat_" + a_["id"][-1], {"mesh": a_["asset_path"], "collision": False, "items": [], "abs_z": 1.0, "shadow": False})
        ang = RNG.uniform(0, 2 * math.pi)
        dist = spread * math.sqrt(RNG.random())
        rx, ry = x + dist * math.cos(ang), y + dist * math.sin(ang)
        if at("height", rx, ry) > -20.0:
            continue
        g["items"].append([round(rx, 1), round(ry, 1), 0.0, round((heading if heading is not None else RNG.uniform(0, 360)) + RNG.uniform(-25, 25), 1),
                           round(RNG.uniform(2.0, 3.6), 3), 0.0, 0.0])


def crossings():
    """The crossing kits: the blackened rail bridge and its track on the north lane, the log bridge and corduroy on
    the south lane, the plank walk to the Demeter pod, the radio masts and the kolkhoz silo."""
    lim = SIZE / 2.0 - 700.0
    # --- north lane: rail bridge at bridge_n, track along the whole embankment and the fuel siding
    bx, by = feature("bridge_n")["points"][0]
    pts = road("rail_north")
    k = int(np.argmin([math.hypot(q[0] - bx, q[1] - by) for q in pts]))
    yaw = bearing_yaw(pts[k + 2][0] - pts[k - 2][0], pts[k + 2][1] - pts[k - 2][1])
    ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    nx, ny = -uy, ux
    cx, cy = pts[k][0], pts[k][1]
    mesh("RailBridge_Deck", "railbridge_deck", cx, cy, yaw, 3.0)
    mesh("RailBridge_Truss", "railbridge_truss", cx + nx * 545.0, cy + ny * 545.0, yaw, 0.0, collision=True)
    mesh("RailBridge_TrussBroken", "railbridge_truss_broken", cx - nx * 545.0, cy - ny * 545.0, yaw, 0.0, collision=True)
    for sx in (-1150.0, 1150.0):
        mesh("RailBridge_Abutment", "railbridge_abutment", cx + ux * sx + nx * 700.0, cy + uy * sx + ny * 700.0, yaw + 180.0, -60.0, collision=False)
        mesh("RailBridge_Abutment", "railbridge_abutment", cx + ux * sx - nx * 700.0, cy + uy * sx - ny * 700.0, yaw, -60.0, collision=False)
    mesh("RailBridge_FallenSpan", "railbridge_fallen_span", cx - nx * 1250.0, cy - ny * 1250.0, yaw - 90.0, abs_z=-150.0)
    INST["RailBridge_FallenSpan"]["items"][-1][5:7] = [14.0, 22.0]
    raft("bridge_n_up", cx + nx * 900.0, cy + ny * 900.0, 10, 700.0, yaw)
    raft("bridge_n_down", cx - nx * 1900.0, cy - ny * 1900.0, 6, 600.0, yaw - 90.0)
    mesh("RailBridge_Pier", "railbridge_pier", cx - nx * 1000.0 + ux * 250.0, cy - ny * 1000.0 + uy * 250.0, yaw - 90.0, abs_z=-40.0)
    SITES.append((cx, cy, 2200.0))
    track = "/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01"
    for rid in ("rail_north", "fuel_siding"):
        for x, y, tyaw, half, h in walk(rid, 1000.0):
            if abs(x) > lim or abs(y) > lim or math.hypot(x - cx, y - cy) < 1700.0:
                continue
            if rid == "rail_north" and x < cx - 2600.0:
                break                                   # past the bridge the rails were lifted for salvage: bare trackbed
            if rid == "fuel_siding" and math.hypot(x - region("rise_fuel")["centre"][0], y - region("rise_fuel")["centre"][1]) < 2300.0:
                break
            tx, ty = x - math.cos(math.radians(tyaw)) * 500.0, y - math.sin(math.radians(tyaw)) * 500.0
            mesh("RailTrack", track, tx, ty, tyaw, -6.0)
    sv = pts[min(k + 11, len(pts) - 1)]                  # on the trackbed just past the last rail
    bp("Rail_Salvage_0", RES + "Metal/Resource_Metal_Rails", sv[0] + nx * 300.0, sv[1] + ny * 300.0, yaw + 8.0, 0.0, "Resources/Metal")
    note("rail_salvage", "Lifted rails (metal)", sv[0], sv[1])
    # --- south lane: log bridge, corduroy
    bx, by = feature("bridge_s")["points"][0]
    pts = road("causeway_south")
    k = int(np.argmin([math.hypot(q[0] - bx, q[1] - by) for q in pts]))
    yaw = bearing_yaw(pts[k + 2][0] - pts[k - 2][0], pts[k + 2][1] - pts[k - 2][1])
    ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    nx, ny = -uy, ux
    sx_, sy_ = pts[k][0], pts[k][1]
    mesh("LogBridge_Deck", "logbridge_deck", sx_, sy_, yaw, 4.0)
    for side in (-1.0, 1.0):
        mesh("LogBridge_SideRail", "logbridge_siderail", sx_ + nx * side * 540.0, sy_ + ny * side * 540.0, yaw, 0.0, collision=True)
        mesh("LogBridge_Piles", "logbridge_pilecluster", sx_ + nx * side * 760.0 + ux * 300.0 * side, sy_ + ny * side * 760.0 + uy * 300.0 * side, yaw + 40.0 * side, abs_z=-60.0)
    raft("bridge_s_up", sx_ + nx * 950.0, sy_ + ny * 950.0, 9, 650.0, yaw)
    SITES.append((sx_, sy_, 2200.0))
    flip = 0
    last_yaw = None
    for x, y, cyaw, half, h in walk("causeway_south", 815.0):
        turn = 0.0 if last_yaw is None else abs((cyaw - last_yaw + 180.0) % 360.0 - 180.0)
        last_yaw = cyaw
        if abs(x) > lim or abs(y) > lim or math.hypot(x - sx_, y - sy_) < 1500.0 or h > 420.0:
            continue
        if turn > 22.0:                                 # on the bends the logs have sunk into the mud: loose trunks only
            k_ = RNG.integers(0, 10)
            inst("BogLog_%02d" % k_, LOGS[k_], x + RNG.uniform(-300, 300), y + RNG.uniform(-300, 300), yaw=cyaw + 90.0 + RNG.uniform(-25, 25), scale=1.2, dz=-10.0)
            continue
        flip += 1
        kind = "corduroy_sunken_10m" if half < 620.0 else "corduroy_14m"
        mesh("Corduroy_" + kind[-3:], kind, x, y, cyaw + (180.0 if flip % 2 else 0.0), 6.0)
    # --- boardwalk to the pod: plank tiles on the dyke, a broken one every fifth, marker posts
    for i, (x, y, byaw, half, h) in enumerate(walk("boardwalk_pod", 600.0)):
        mesh("Boardwalk_Broken" if i % 5 == 3 else "Boardwalk", "boardwalk_broken_6m" if i % 5 == 3 else "boardwalk_6m", x, y, byaw, 8.0)
        if i % 4 == 0:
            mesh("Boardwalk_Post", "boardwalk_post", x - math.sin(math.radians(byaw)) * 260.0, y + math.cos(math.radians(byaw)) * 260.0, byaw, 20.0)
    # the Demeter pod on its hummock, half sunk, Phytophage round it
    dx_, dy_ = feature("demeter_pod")["points"][0]
    mesh("DemeterPod", "/Game/RTS_Survival/Blueprints/Environment/Berlin/DemeterPod/SM_Berlin_DemeterPod", dx_ + 250.0, dy_ - 150.0, 35.0, -45.0, collision=True)
    # a second pod of the same stick, sunk in the algae water south of the walk with the growth spreading from its hatch
    # (the Act 1 clue; optional intel / lore find). Pivot = waterline, so it sits at z = 0; nose away from the walk.
    mesh("SunkenDemeterPod", "/Game/RTS_Survival/Blueprints/Environment/Prypyat/SunkenPod/SM_Pripyat_SunkenDemeterPod", dx_ + 400.0, dy_ - 5100.0, -130.0, collision=True, abs_z=0.0)
    note("sunken_pod", "Sunken Demeter pod (optional intel / lore find)", dx_ + 400.0, dy_ - 5100.0)
    # --- masts and the silo
    x, y = air(8200.0, 6800.0)
    mesh("RadioMast_Leaning", "radiomast_leaning", x, y, 200.0, -10.0, collision=True)
    note("radio_mast", "Airfield radio mast (repair objective pointer)", x, y)
    x, y = 17500.0 * math.cos(math.radians(112.0)) + 350.0, 17500.0 * math.sin(math.radians(112.0)) + 150.0
    mesh("RadioMast_Leaning", "radiomast_leaning", x, y, 40.0, -10.0, collision=True)
    a, b = feature("island_airfield")["raw"]["ends"]
    d = math.hypot(b[0] - a[0], b[1] - a[1])
    ix, iy = (a[0] + b[0]) / 2.0 + (b[0] - a[0]) / d * 3900.0 + (b[1] - a[1]) / d * 2500.0, (a[1] + b[1]) / 2.0 + (b[1] - a[1]) / d * 3900.0 - (b[0] - a[0]) / d * 2500.0
    if at("island", ix, iy):
        mesh("RadioMast_Island", "radiomast_island", ix, iy, 70.0, -10.0, collision=True)
    x, y = air(-4500.0, 9900.0)
    mesh("KolkhozSilo", "existing_kolkhoz_silo", x, y, AIR["yaw"] + 20.0, -5.0, collision=True)
    note("kolkhoz_silo", "Kolkhoz grain silo", x, y)


def main():
    for bearing in (47.0, 200.0, 312.0):                 # hamlets (placed after the scatter)
        SITES.append((17650.0 * math.cos(math.radians(bearing)), 17650.0 * math.sin(math.radians(bearing)), 2600.0))
    for rid in ("rise_pilot_n", "rise_pilot_s"):         # rail halt and peat yard
        SITES.append((*region(rid)["centre"], 2100.0))
    for fid in ("bridge_n", "bridge_s"):
        SITES.append((*feature(fid)["points"][0], 2400.0))
    for rid in ("ramp_w", "ramp_n", "ramp_s"):           # the gate works at the ramp heads
        SITES.append((road(rid)[-2][0], road(rid)[-2][1], 3200.0))
    start()
    checkpoint()
    aa_sites()
    compound()
    fuel_depot()
    far_radixite()
    island()
    lanes()
    bench()
    nature()
    polesie()
    crossings()
    peat_fires()
    tint_groups()
    out = {"bps": BPS, "inst": INST, "notes": NOTES, "fx": FX, "splines": SPLINES, "decals": DECALS}
    with open(os.path.join(TERRAIN, "Pripyat_dress.json"), "w") as handle:
        json.dump(out, handle)
    print("blueprint actors %d, instance groups %d with %d instances, notes %d, fx %d" % (
        len(BPS), len(INST), sum(len(g["items"]) for g in INST.values()), len(NOTES), len(FX)))
    by = {}
    for b in BPS:
        by[b[6].split("/")[0] + "/" + b[6].split("/")[1] if "/" in b[6] else b[6]] = by.get(b[6].split("/")[0] + "/" + b[6].split("/")[1] if "/" in b[6] else b[6], 0) + 1
    print(by)


if __name__ == "__main__":
    main()
