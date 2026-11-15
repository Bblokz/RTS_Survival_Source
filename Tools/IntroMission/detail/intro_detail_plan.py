"""Offline planner for the first-section detail pass of Ger_IntroMission.

Reads Saved/IntroDetail/heights_backup.txt (2 m height grid) and scene.json (actors + spline samples, both dumped from
the live editor) and writes Saved/IntroDetail/plan.json: paint strokes, erosion (dips, troughs), decals and props.
Deterministic (fixed seed). Run:  python intro_detail_plan.py
"""
import json
import math
import os
import random

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "Saved", "IntroDetail"))
rng = random.Random(4117)

# ---------------------------------------------------------------- inputs
lines = open(os.path.join(OUT, "heights_backup.txt")).read().split("\n")
X0, X1, Y0, Y1, ST = [int(v) for v in lines[0].split()]
H = np.array([[int(v) for v in r.split()] for r in lines[1:]], dtype=float)
VALID = H > -5000
Hf = np.where(VALID, H, 144.0)
GY, GX = np.gradient(Hf, ST)
SLOPE = np.degrees(np.arctan(np.hypot(GX, GY)))
scene = json.load(open(os.path.join(OUT, "scene.json")))
ROAD = np.array(scene["splines"]["BP_RoadSpline_AirportStopWay1"], dtype=float)[:, :2]
TRACK = np.array(scene["splines"]["BP_GroundPathSpline_TwoTrack"], dtype=float)[:, :2]
TRACK = TRACK[(TRACK[:, 1] > Y0) & (TRACK[:, 0] < -9000)]
WATER_Z = -60.0


def gi(x, y):
    return int(round((y - Y0) / ST)), int(round((x - X0) / ST))


def inside(x, y):
    r, c = gi(x, y)
    return 1 <= r < H.shape[0] - 1 and 1 <= c < H.shape[1] - 1 and VALID[r - 1:r + 2, c - 1:c + 2].all()


def z(x, y):
    r, c = gi(x, y)
    return Hf[r, c]


def slope(x, y):
    r, c = gi(x, y)
    return SLOPE[r, c]


def downhill(x, y):
    r, c = gi(x, y)
    g = np.array([GX[r, c], GY[r, c]])
    n = np.linalg.norm(g)
    return (-g / n) if n > 1e-4 else np.array([1.0, 0.0])


def dist_poly(poly, x, y):
    d = np.hypot(poly[:, 0] - x, poly[:, 1] - y)
    i = int(d.argmin())
    return float(d[i]), i


def heading(poly, i):
    a, b = poly[max(i - 1, 0)], poly[min(i + 1, len(poly) - 1)]
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


# ---------------------------------------------------------------- keep-outs
RADII = [("Hangar", 1350), ("FenceWire", 230), ("Hedgehog", 260), ("NavBlock", 180), ("NavBloker", 180),
         ("PoleLight", 220), ("DeadTree", 320), ("DeadWillow", 380), ("CactusBush", 300), ("CrateStack", 380),
         ("Bunker05", 750), ("Sandbags", 420), ("Destroyed", 520), ("Sdkfz250", 520), ("OldCar", 420),
         ("Squad", 450), ("Solo_", 200), ("Weapon", 150), ("Log", 380), ("DrainPipe", 450), ("MissileStack", 380),
         ("RusHut", 700), ("Niagara", 250), ("RTS_PLayerStart", 600), ("NavModifier", 300), ("Radixite", 500),
         ("MGBunker", 800)]
SKIP = ("Spline", "DecalActor", "Mountain", "Manager", "Controller", "Outliner", "PlayerStart", "Airport_Ground")
SOLID = []          # (x, y, r) of everything the user placed
for a in scene["actors"]:
    c = a["c"]
    if c == "StaticMeshActor" and a["n"] in ("Plane", "S_Desert_Mountain_01", "S_Desert_Mountain_04"):
        continue
    if c != "RTS_PLayerStart_C" and any(s in c for s in SKIP):
        continue
    r = 400
    for key, rad in RADII:
        if key in c or key in a["n"]:
            r = rad
            break
    SOLID.append((a["x"], a["y"], r))
SOLID = np.array(SOLID)
USER_DECALS = [(a["n"], a["x"], a["y"]) for a in scene["actors"] if a["c"] == "DecalActor"]
# mission points: move-to trigger, reinforcement square, player start group, hangar mouths, sandbag line
MISSION = [(-15697, -1275, 900), (-30984, 3366, 1000), (-31000, -400, 1300), (-11300, -2416, 700),
           (-11300, -5374, 700), (-13015, -6800, 700), (-14000, -2900, 900)]
USER_PAINT = [(-21300, 5500, 1900)]     # the user's own dark painted patch round the hut


def solid_clear(x, y, margin=0.0):
    d = np.hypot(SOLID[:, 0] - x, SOLID[:, 1] - y) - SOLID[:, 2]
    return float(d.min()) - margin


def in_mountain(x, y):
    return (x > -20500 and y > 4400) or y > 6800


def mission_clear(x, y, margin=0.0):
    return min(math.hypot(x - mx, y - my) - mr for mx, my, mr in MISSION) - margin


def ok_site(x, y, r, road=900, track=600, mission=True, water=True, slope_max=14.0):
    """A spot where ground may be dug or a solid thing placed."""
    if not inside(x, y) or in_mountain(x, y):
        return False
    if solid_clear(x, y) < r + 120:
        return False
    if dist_poly(ROAD, x, y)[0] < road + r or dist_poly(TRACK, x, y)[0] < track + r:
        return False
    if mission and mission_clear(x, y) < r:
        return False
    if water and z(x, y) < WATER_Z + 90:
        return False
    return slope(x, y) <= slope_max


paint, dips, troughs, decals, props = [], [], [], [], []
D = "/Game/RTS_Survival/Materials/Decals/"
CR = "/Game/Crater_Decals/Decals/"
MS = "/Game/Megascans/Decals/"
GP = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Decals/"
M3 = "/Game/Megascans/3D_Assets/"
ROCKY = [M3 + "Sandstone_Rocky_Ground_vmhdagb/S_Sandstone_Rocky_Ground_vmhdagb_high_Var1",
         M3 + "Sandstone_Rocky_Ground_vmhdcfa/S_Sandstone_Rocky_Ground_vmhdcfa_high_Var1",
         M3 + "Sandstone_Rocky_Ground_vmhifen/S_Sandstone_Rocky_Ground_vmhifen_high_Var1"]
FORMATION = M3 + "Canyon_Sandstone_Rock_Formation_uk5pdhh/S_Canyon_Sandstone_Rock_Formation_uk5pdhh_high_Var1"
# project cacti (atlas material), not the Megascans 8K ones: user 2026-11-14 does not want the 8K materials
CACTI = ["/Game/RTS_Survival/Meshes/Desert/Cacti/SM_Desert_Cactus%d" % i for i in (1, 2, 3, 4)]
SHRUBS = ["/Game/Megascans/3D_Plants/Deadshrub-01_vdknafgha/S_Deadshrub-01_vdknafgha_Var%d_lod0" % i
          for i in (1, 3, 5, 7, 9, 11, 13, 15)]
ENV = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/"


def stroke(pts, width, falloff, layer):
    paint.append(dict(pts=[[round(p[0]), round(p[1])] for p in pts], w=width, f=falloff, layer=layer))


def dab(x, y, layer, length=160, falloff=230, yaw=None, width=25):
    yaw = rng.uniform(0, 180) if yaw is None else yaw
    c, s = math.cos(math.radians(yaw)) * length / 2, math.sin(math.radians(yaw)) * length / 2
    stroke([(x - c, y - s), (x + c, y + s)], width, falloff, layer)


def decal(mat, x, y, yaw, sy, sz=None, depth=160, tag="Decal"):
    decals.append(dict(mat=mat, x=round(x), y=round(y), yaw=round(yaw, 1), size=[depth, round(sy), round(sz or sy)],
                       tag=tag))


def prop(asset, x, y, scale, yaw=None, sink=4, align=True, tag="Prop", cls=False):
    props.append(dict(asset=asset, x=round(x), y=round(y), yaw=round(rng.uniform(0, 360) if yaw is None else yaw, 1),
                      s=round(scale, 2), sink=sink, align=align, tag=tag, cls=cls))


# ================================================================ 1. road verges
road_ds = ROAD[::3]
stroke(road_ds, 520, 420, "DesertGravel")
for i in range(4, len(ROAD) - 4, 5):
    hd = math.radians(heading(ROAD, i))
    nx, ny = -math.sin(hd), math.cos(hd)
    for side in (-1, 1):
        if rng.random() < 0.55:      # pale scree thrown off the carriageway
            off = side * rng.uniform(640, 900)
            dab(ROAD[i, 0] + nx * off, ROAD[i, 1] + ny * off, "DesertRockDark", rng.uniform(200, 520),
                rng.uniform(150, 240), math.degrees(hd) + rng.uniform(-12, 12))
        if rng.random() < 0.6:       # sand drifting back over the gravel edge
            off = side * rng.uniform(820, 1150)
            dab(ROAD[i, 0] + nx * off, ROAD[i, 1] + ny * off, "Sand", rng.uniform(250, 600), rng.uniform(220, 330),
                math.degrees(hd) + rng.uniform(-25, 25))

# ================================================================ 2. two-track wear
stroke(TRACK[::3], 120, 260, "DesertGravel")

# ================================================================ 3. lake shore: cracked mud at the waterline
lake = (Hf < WATER_Z) & VALID
rows, cols = np.nonzero(VALID)
shore_pts = []
for r in range(2, H.shape[0] - 2, 2):
    for c in range(2, H.shape[1] - 2, 2):
        if not VALID[r, c]:
            continue
        zz = Hf[r, c]
        if WATER_Z - 50 < zz < WATER_Z + 110 and lake[max(r - 6, 0):r + 7, max(c - 6, 0):c + 7].any():
            shore_pts.append((X0 + c * ST, Y0 + r * ST))
for x, y in shore_pts:
    dh = downhill(x, y)
    tang = math.degrees(math.atan2(dh[0], -dh[1]))
    dab(x + rng.uniform(-90, 90), y + rng.uniform(-90, 90), "Weedy", 380, 260, tang, 60)
# a looser gravel rim above the mud
for x, y in shore_pts[::3]:
    dh = downhill(x, y)
    px, py = x - dh[0] * rng.uniform(450, 800), y - dh[1] * rng.uniform(450, 800)
    if inside(px, py) and z(px, py) > WATER_Z + 100:
        dab(px, py, "DesertGravel" if rng.random() < 0.7 else "DesertRockDark", rng.uniform(200, 500), 280)

# ================================================================ 4. hills: scree on the steep faces, gravel at the feet
for r in range(2, H.shape[0] - 2, 2):
    for c in range(2, H.shape[1] - 2, 2):
        if not VALID[r, c] or Hf[r, c] < WATER_Z + 120:
            continue
        x, y = X0 + c * ST + rng.uniform(-150, 150), Y0 + r * ST + rng.uniform(-150, 150)
        if in_mountain(x, y) or any(math.hypot(x - ux, y - uy) < ur for ux, uy, ur in USER_PAINT):
            continue
        s = SLOPE[r, c]
        if s > 11 and rng.random() < min(1.0, (s - 8) / 14):
            dh = downhill(x, y)
            dab(x, y, "DesertRockDark", rng.uniform(250, 520), rng.uniform(170, 260),
                math.degrees(math.atan2(dh[1], dh[0])) + rng.uniform(-25, 25), 30)
        elif 4 < s <= 11 and rng.random() < 0.45:
            dab(x, y, "DesertGravel", rng.uniform(250, 600), rng.uniform(240, 340))

# ================================================================ 5. hangar compound: gravel yard, concrete aprons, stains
for xs in range(-14000, -9500, 450):
    y = -8800
    while y < -700:
        seg = rng.uniform(700, 1700)
        if rng.random() < 0.72:
            stroke([(xs + rng.uniform(-150, 150), y), (xs + rng.uniform(-150, 150), min(y + seg, -600))],
                   rng.uniform(120, 230), rng.uniform(260, 380), "DesertGravel")
        y += seg + rng.uniform(100, 500)
APRONS = [((-11500, -5374), (-12700, -5374), 520), ((-13015, -6750), (-13015, -5750), 520)]
for a, b, w in APRONS:
    stroke([a, b], w, 220, "ScorchConcrete")
    for _ in range(4):
        t = rng.random()
        dab(a[0] + (b[0] - a[0]) * t + rng.uniform(-350, 350), a[1] + (b[1] - a[1]) * t + rng.uniform(-350, 350),
            "ScorchSandyStone", rng.uniform(80, 220), rng.uniform(110, 190))
for mx, my in ((-11500, -2416), (-11450, -5374), (-13015, -6700), (-12300, -3600), (-11000, -4300)):
    for _ in range(3):
        dab(mx + rng.uniform(-420, 420), my + rng.uniform(-420, 420), "ScorchSandyStone", rng.uniform(60, 200),
            rng.uniform(100, 170))

# ================================================================ 6. erosion under the user's own crater decals
for name, x, y in USER_DECALS:
    if not ("Crater_Dirt" in name or "BombImpact" in name):
        continue
    r = min(300.0, solid_clear(x, y) - 120)
    if r >= 170 and inside(x, y) and dist_poly(ROAD, x, y)[0] > 700 + r and dist_poly(TRACK, x, y)[0] > 450 + r:
        dips.append(dict(x=round(x), y=round(y), r=round(r), depth=round(28 + r * 0.06), yaw=rng.uniform(0, 180),
                         paint=None, why="under " + name))

# ================================================================ 7. new shell craters (dip + scorched ground + decal)
CRATER_MATS = [D + "CraterDirt/MI_Decal_Crater_Dirt_0%d_Sandy" % i for i in (1, 2, 3, 4, 5, 6)]
SCORCH_MATS = [CR + "Scorched/MI_Decal_Crater_Scorched_Dirt_0%d" % i for i in (1, 2, 3, 4, 5)]
ZONES = [  # (x0, x1, y0, y1, count, big) - battle-scarred ground the vehicles cross
    (-30500, -26500, 300, 2900, 4, True),       # field south-west of the lake
    (-26000, -22500, -300, 3300, 4, True),      # hedgehog line / bunker
    (-22000, -18800, -1500, 1800, 4, True),     # lake to road, east
    (-19800, -16200, -5200, -2000, 4, False),   # destroyed Pz II, approach to the sandbags
    (-19000, -15800, 400, 3400, 3, False),      # south of the road before the compound
    (-29500, -24500, 4700, 6500, 3, False),     # south of the road, west
    (-17000, -15500, -8500, -5500, 2, False),   # outside the fence, north
]
craters = []
for x0, x1, y0, y1, n, big in ZONES:
    got, tries = 0, 0
    while got < n and tries < 400:
        tries += 1
        x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
        r = rng.uniform(380, 560) if (big and rng.random() < 0.6) else rng.uniform(260, 380)
        if not ok_site(x, y, r, slope_max=9.0):
            continue
        if any(math.hypot(x - cx, y - cy) < r + cr + 250 for cx, cy, cr in craters):
            continue
        if any(math.hypot(x - ux, y - uy) < r + 500 for _, ux, uy in USER_DECALS):
            continue
        craters.append((x, y, r))
        got += 1
        depth = 26 + r * 0.085
        yaw = rng.uniform(0, 360)
        dips.append(dict(x=round(x), y=round(y), r=round(r), depth=round(depth), yaw=yaw, paint="ScorchSandyStone",
                         why="crater"))
        scorched = rng.random() < 0.4
        decal(rng.choice(SCORCH_MATS if scorched else CRATER_MATS), x, y, yaw, r * 1.25, tag="Crater")
        if rng.random() < 0.6:
            a = rng.uniform(0, 6.28)
            decal(CR + "Dirt/MI_Decal_Dirt_Scatter_0%d" % rng.randint(1, 3), x + math.cos(a) * r * 0.9,
                  y + math.sin(a) * r * 0.9, rng.uniform(0, 360), r * 1.1, tag="Scatter")
        for _ in range(rng.randint(1, 3)):          # thrown-out stones on the rim
            a = rng.uniform(0, 6.28)
            prop(rng.choice(ROCKY), x + math.cos(a) * r * rng.uniform(1.05, 1.5), y + math.sin(a) * r * rng.uniform(1.05, 1.5),
                 rng.uniform(0.5, 1.0), tag="RimRock")

# ================================================================ 8. gullies down the lake banks and hill sides
gullies = []
starts = [(x, y) for x, y in shore_pts if True]
rng.shuffle(starts)
for sx, sy in starts:
    if len(gullies) >= 7:
        break
    # walk uphill from the shore to find the head of the gully
    p = np.array([sx, sy], dtype=float)
    path = [p.copy()]
    for _ in range(14):
        p = p - downhill(p[0], p[1]) * 100
        if not inside(p[0], p[1]):
            break
        path.append(p.copy())
        if z(p[0], p[1]) > 120 and slope(p[0], p[1]) < 3:
            break
    if len(path) < 6 or z(path[-1][0], path[-1][1]) - z(sx, sy) < 110:
        continue
    mid = path[len(path) // 2]
    if any(np.hypot(mid[0] - g[0], mid[1] - g[1]) < 1400 for g in gullies):
        continue
    if min(solid_clear(q[0], q[1]) for q in path) < 300 or min(dist_poly(ROAD, q[0], q[1])[0] for q in path) < 900:
        continue
    gullies.append(mid)
    troughs.append(dict(pts=[[round(q[0]), round(q[1])] for q in path[::-1]], w=45, f=170, depth=24,
                        paint="DesertGravel", why="gully"))
    hd = math.degrees(math.atan2(path[0][1] - path[-1][1], path[0][0] - path[-1][0]))
    ln = float(np.hypot(*(path[0] - path[-1])))
    decal(D + "DirtTracks/MI_Decal_Erosion_Channel_0%d" % rng.randint(1, 5), mid[0], mid[1], hd, 190, ln * 0.55,
          tag="Gully")

# ================================================================ 9. worn tank tracks leaving the road (trough + decals)
RUNS = [[(-27100, 3300), (-26300, 2700), (-25300, 2500), (-24300, 2500)],          # road -> hedgehog line
        [(-20900, 1500), (-21500, 600), (-22300, -100), (-23000, -500)],           # road -> lake shore
        [(-18300, -1900), (-18900, -2900), (-19200, -3900)],                       # past the wrecked Pz II
        [(-28800, 4500), (-27800, 5300), (-26600, 5600)]]                           # south of the road
for run in RUNS:
    pts = [q for q in run if inside(q[0], q[1])]
    if len(pts) < 2:
        continue
    troughs.append(dict(pts=[[q[0], q[1]] for q in pts], w=110, f=150, depth=13, paint="DesertGravel", why="tracks"))
    for a, b in zip(pts[:-1], pts[1:]):
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        hd = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        k = max(1, int(seg / 520))
        for j in range(k):
            t = (j + 0.5) / k
            decal(D + "DirtTracks/MI_Decal_Dirt_TracksSandy_0%d" % rng.randint(1, 4), a[0] + (b[0] - a[0]) * t,
                  a[1] + (b[1] - a[1]) * t, hd + rng.choice((0, 180)), 250, tag="Tracks")

# ================================================================ 10. decals on and beside the road, compound, bunker
for i in range(8, len(ROAD) - 6, 1):
    if rng.random() > 0.085:
        continue
    hd = heading(ROAD, i)
    n = (-math.sin(math.radians(hd)), math.cos(math.radians(hd)))
    kind = rng.random()
    if kind < 0.4:        # sand blown across the tarmac
        side = rng.choice((-1, 1))
        decal(GP + "MI_PathDecal_DustSpill", ROAD[i, 0] + n[0] * side * 380, ROAD[i, 1] + n[1] * side * 380,
              hd - 90 + (0 if side > 0 else 180), rng.uniform(260, 380), rng.uniform(300, 420), tag="DustSpill")
    elif kind < 0.7:      # cracked / broken tarmac
        mat = rng.choice([MS + "Asphalt_Crack_Patch_sfxmang/MI_Asphalt_Crack_Patch_sfxmang_2K",
                          MS + "Road_Damage_sfkmasg/MI_Road_Damage_sfkmasg_2K",
                          MS + "Asphalt_Crack_rmoqcqp2/MI_Asphalt_Crack_rmoqcqp2_1K"])
        off = rng.uniform(-220, 220)
        decal(mat, ROAD[i, 0] + n[0] * off, ROAD[i, 1] + n[1] * off, rng.uniform(0, 360), rng.uniform(140, 230),
              depth=90, tag="RoadDamage")
    else:                 # rubble kicked to the edge
        side = rng.choice((-1, 1))
        decal(D + "Concrete/MI_RTS_Decal_Asphalt_Debris_01_Sandy1", ROAD[i, 0] + n[0] * side * 520,
              ROAD[i, 1] + n[1] * side * 520, rng.uniform(0, 360), rng.uniform(200, 300), tag="RoadDebris")
for x, y in ((-12250, -4250), (-11300, -6900), (-13900, -5900), (-12100, -1500), (-10900, -8100), (-22500, 1500),
             (-23700, 2900), (-13600, -8300)):
    decal(D + "Concrete/MI_RTS_Decal_Concrete_Debris_0%d_Sandy" % rng.choice((1, 2, 3, 5)), x + rng.uniform(-200, 200),
          y + rng.uniform(-200, 200), rng.uniform(0, 360), rng.uniform(230, 330), tag="Debris")
for _ in range(40):       # arid litter, bones and loose gravel out on the flats
    x, y = rng.uniform(-33500, -15000), rng.uniform(-10500, 6500)
    if not ok_site(x, y, 250, road=700, track=400, slope_max=10) or len([d for d in decals if d["tag"] == "Flat"]) >= 16:
        continue
    k = rng.random()
    mat = (MS + "Arid_Debris_vbzebfrh/MI_Arid_Debris_vbzebfrh_2K" if k < 0.5 else
           MS + "Gravel_vcedbbxh/MI_Gravel_vcedbbxh_1K" if k < 0.85 else
           MS + "Scattered_Bones_uk1ceb1h/MI_Scattered_Bones_uk1ceb1h_4K")
    decal(mat, x, y, rng.uniform(0, 360), rng.uniform(150, 260), depth=90, tag="Flat")

# ================================================================ 11. props
def scatter(n, region, fn, min_gap, tries=60):
    placed = []
    for _ in range(n * tries):
        if len(placed) >= n:
            break
        x, y = rng.uniform(region[0], region[1]), rng.uniform(region[2], region[3])
        if any(math.hypot(x - px, y - py) < min_gap for px, py in placed):
            continue
        if fn(x, y):
            placed.append((x, y))
    return placed


SECTION = (-34500, -9800, -11200, 6600)
soft = lambda x, y: (inside(x, y) and not in_mountain(x, y) and solid_clear(x, y) > 80 and z(x, y) > WATER_Z + 40
                     and dist_poly(ROAD, x, y)[0] > 720 and dist_poly(TRACK, x, y)[0] > 380
                     and mission_clear(x, y) > -300 and not any(math.hypot(x - cx, y - cy) < cr for cx, cy, cr in craters))
# dead shrubs in loose clumps (denser on the lake rim)
for cx, cy in scatter(46, SECTION, soft, 900):
    near_lake = lake[max(gi(cx, cy)[0] - 12, 0):gi(cx, cy)[0] + 13, max(gi(cx, cy)[1] - 12, 0):gi(cx, cy)[1] + 13].any()
    for _ in range(rng.randint(3, 6) if near_lake else rng.randint(1, 4)):
        x, y = cx + rng.gauss(0, 260), cy + rng.gauss(0, 260)
        if soft(x, y):
            prop(rng.choice(SHRUBS), x, y, rng.uniform(1.6, 3.2), sink=3, align=False, tag="Shrub")
# stony ground: hill feet, verges, lake rim
stony = lambda x, y: soft(x, y) and (3.5 < slope(x, y) < 20 or 700 < dist_poly(ROAD, x, y)[0] < 1500 or rng.random() < 0.12)
for x, y in scatter(60, SECTION, stony, 500):
    prop(rng.choice(ROCKY), x, y, rng.uniform(0.9, 2.3), sink=5, tag="Rocky")
    if rng.random() < 0.45:
        prop(rng.choice(ROCKY), x + rng.uniform(-260, 260), y + rng.uniform(-260, 260), rng.uniform(0.6, 1.3), sink=4,
             tag="Rocky")
ledge = lambda x, y: soft(x, y) and slope(x, y) > 7 and mission_clear(x, y) > 300
for x, y in scatter(14, SECTION, ledge, 1300):
    dh = downhill(x, y)
    prop(FORMATION, x, y, rng.uniform(1.0, 1.9), yaw=math.degrees(math.atan2(dh[0], -dh[1])) + rng.uniform(-20, 20),
         sink=10, tag="Ledge")
# cacti: singles and pairs away from the fighting lanes
for x, y in scatter(26, SECTION, lambda x, y: soft(x, y) and slope(x, y) < 9 and mission_clear(x, y) > 200, 1100):
    prop(rng.choice(CACTI), x, y, rng.uniform(1.3, 2.2), sink=4, align=False, tag="Cactus")
    if rng.random() < 0.4:
        prop(rng.choice(CACTI), x + rng.uniform(-170, 170), y + rng.uniform(-170, 170), rng.uniform(1.0, 1.6), sink=4,
             align=False, tag="Cactus")
# a few more of the dead trees the map already uses (they block like the existing ones: kept well off every route)
tree_ok = lambda x, y: ok_site(x, y, 350, road=1300, track=900, slope_max=10) and mission_clear(x, y) > 700
for i, (x, y) in enumerate(scatter(7, (-33000, -15500, -10500, 6300), tree_ok, 3200)):
    prop(ENV + ("BP_Scorch_Dest_DeadWillow" if i % 4 == 3 else "BP_Scorch_Dest_DeadTree"), x, y, 1.0, sink=0,
         align=False, tag="DeadTree", cls=True)

# ================================================================ 12. paint from masks
# Earlier attempts (short dabs, then long bands) read as dots and stripes from above. The paint is now built as
# noise-shaped masks on the 2 m grid and written as scan-line strokes (one per run of cells), which lets the engine's
# spline painter fill any shape. The first-version strokes above are kept only so the random stream (and with it
# every crater and prop position) stays the same.
paint_v1 = paint
paint = []
pr = random.Random(9021)
nr = np.random.RandomState(515)
# the hut patch is centred further west than the first-version zone assumed (that mistake wiped it once; it was
# rebuilt with intro_detail_repair.py)
USER_PAINT2 = USER_PAINT + [(-28167, -3280, 1300), (-27345, -767, 600), (-22400, 5600, 2100)]
ST2 = ST // 2


def up(a):
    """Bilinear 2x upsample of a grid (2 m -> 1 m)."""
    o = np.zeros((a.shape[0] * 2 - 1, a.shape[1] * 2 - 1))
    o[::2, ::2] = a
    o[1::2, ::2] = (a[:-1] + a[1:]) / 2
    o[:, 1::2] = (o[:, :-2:2] + o[:, 2::2]) / 2
    return o


def gi2(x, y):
    return int(round((y - Y0) / ST2)), int(round((x - X0) / ST2))


Hs = up(Hf)
_gy, _gx = np.gradient(Hs, ST2)
SLOPEs = up(SLOPE)
VALIDs = up(VALID.astype(float)) > 0.99
lakes = (Hs < WATER_Z) & VALIDs
NR, NC = Hs.shape
XX, YY = np.meshgrid(X0 + np.arange(NC) * ST2, Y0 + np.arange(NR) * ST2)


def vnoise(cell):
    gh, gw = NR // cell + 3, NC // cell + 3
    g = nr.rand(gh, gw)
    fy, fx = np.arange(NR) / cell, np.arange(NC) / cell
    iy, ix = fy.astype(int), fx.astype(int)
    ty, tx = fy - iy, fx - ix
    ty, tx = ty * ty * (3 - 2 * ty), tx * tx * (3 - 2 * tx)
    a = g[iy][:, ix] * (1 - tx) + g[iy][:, ix + 1] * tx
    b = g[iy + 1][:, ix] * (1 - tx) + g[iy + 1][:, ix + 1] * tx
    return a * (1 - ty)[:, None] + b * ty[:, None]


def fbm(cells, weights):
    n = sum(vnoise(c) * w for c, w in zip(cells, weights)) / sum(weights)
    return (n - n.min()) / (n.max() - n.min())


def dist_field(poly, step=1):
    d = np.full(Hs.shape, 1e9)
    for px_, py_ in poly[::step]:
        d = np.minimum(d, np.hypot(XX - px_, YY - py_))
    return d


def disc(x, y, r):
    return np.hypot(XX - x, YY - y) < r


USERZ = np.zeros(Hs.shape, bool)
for ux, uy, ur in USER_PAINT2:
    USERZ |= disc(ux, uy, ur)
MOUNT = ((XX > -20500) & (YY > 4400)) | (YY > 6800)
LAND = VALIDs & ~USERZ & ~MOUNT
D_ROAD, D_TRACK = dist_field(ROAD, 2), dist_field(TRACK, 2)
lake_near = np.zeros(Hs.shape, bool)
lr, lc = np.nonzero(lakes)
for dr in range(-14, 15, 2):
    for dc in range(-14, 15, 2):
        rr, cc = np.clip(lr + dr, 0, NR - 1), np.clip(lc + dc, 0, NC - 1)
        lake_near[rr, cc] = True
YARD = (XX > -14250) & (XX < -9300) & (YY > -8950) & (YY < -450)

n_big, n_mid, n_fine = fbm((44, 18, 8, 3), (1.0, 0.6, 0.3, 0.12)), fbm((20, 8, 4), (1.0, 0.6, 0.35)), fbm((10, 4, 2), (1.0, 0.5, 0.3))
# gravel: drifts on the flats, shoulders of the road, hill feet, the yard, the two-track
g_score = n_big * 0.75 + n_fine * 0.25
g_score = g_score + 0.22 * ((D_ROAD > 480) & (D_ROAD < 1150)) + 0.16 * ((SLOPEs > 3) & (SLOPEs < 11)) + 0.2 * YARD
g_score = g_score + 0.3 * (D_TRACK < 320) + 0.12 * (lake_near & (Hs > WATER_Z + 90))
GRAVEL = (g_score > 0.71) & LAND & (Hs > WATER_Z + 60)
# pale scree: steep faces, and stony cores inside the bigger gravel drifts
SCREE = (((SLOPEs > 12.5) & (n_fine > 0.3)) | ((g_score > 0.9) & (n_mid > 0.55))) & LAND & (Hs > WATER_Z + 60)
SCREE &= ~((D_ROAD < 560) | YARD)
# cracked mud: the waterline (width varied by noise) and a few dry pans in flat hollows
band_top = WATER_Z + 60 + 90 * n_mid
MUD = lake_near & (Hs > WATER_Z - 70) & (Hs < band_top) & LAND
PAN = (n_big < 0.16) & (n_mid < 0.5) & (SLOPEs < 2.5) & (D_ROAD > 1500) & (D_TRACK > 900) & LAND & ~YARD \
    & (Hs > WATER_Z + 150) & (XX > -33500)
MUD |= PAN


def smooth(m, k=1):
    """Majority filter: knocks single-cell teeth off a mask."""
    f = m.astype(float)
    acc = np.zeros_like(f)
    for dr in range(-k, k + 1):
        for dc in range(-k, k + 1):
            acc += np.roll(np.roll(f, dr, 0), dc, 1)
    return acc > ((2 * k + 1) ** 2) / 2.0


GRAVEL, SCREE, MUD = smooth(GRAVEL, 2) & LAND, smooth(SCREE, 2) & LAND, smooth(MUD, 1) & LAND
# scorch: crater floors, yard stains; concrete aprons in front of two hangars
SCORCH = np.zeros(Hs.shape, bool)
for d in dips:
    if d["paint"]:
        SCORCH |= disc(d["x"], d["y"], d["r"] * (0.3 + 0.22 * n_fine))
for mx, my in ((-11500, -2416), (-11450, -5374), (-13015, -6700), (-12300, -3600), (-11000, -4300), (-12700, -5374),
               (-13015, -5900)):
    for _ in range(3):
        SCORCH |= disc(mx + pr.uniform(-450, 450), my + pr.uniform(-450, 450), pr.uniform(130, 260))
SCORCH &= LAND
APRON = (((XX > -12750) & (XX < -11450) & (abs(YY + 5374) < 560)) | ((abs(XX + 13015) < 560) & (YY > -6800) & (YY < -5700)))
# worn ground along the gullies and tank ruts
WORN = np.zeros(Hs.shape, bool)
for t in troughs:
    WORN |= dist_field(np.array(t["pts"], dtype=float)) < 230
GRAVEL |= WORN & LAND
# everything the two earlier passes touched goes back to sand first (never inside the user's own patches)
RESET = np.zeros(Hs.shape, bool)
prev = paint_v1 + json.load(open(os.path.join(OUT, "plan_v2.json")))["paint"]
prev += [s for s in json.load(open(os.path.join(OUT, "plan_v3.json")))["paint"] if s["layer"] != "Sand"]
for s in prev:
    pts = np.array(s["pts"], dtype=float)
    dense = [pts[0]]
    for a, b in zip(pts[:-1], pts[1:]):
        n = max(1, int(np.hypot(*(b - a)) / 150))
        dense += [a + (b - a) * (k + 1) / n for k in range(n)]
    reach = s["w"] + s["f"] + 250
    for q in dense:
        r0, c0 = gi2(q[0], q[1])
        k = int(reach / ST2) + 1
        ra, rb, ca, cb = max(r0 - k, 0), min(r0 + k + 1, NR), max(c0 - k, 0), min(c0 + k + 1, NC)
        if ra < rb and ca < cb:
            RESET[ra:rb, ca:cb] |= np.hypot(XX[ra:rb, ca:cb] - q[0], YY[ra:rb, ca:cb] - q[1]) < reach
for d in dips:
    RESET |= disc(d["x"], d["y"], d["r"] * 1.2)
RESET &= VALIDs & ~USERZ


def scan(mask, layer, half=50, fall=90):
    n = 0
    for r in range(NR):
        row = mask[r]
        c = 0
        while c < NC:
            if row[c]:
                c1 = c
                while c1 + 1 < NC and row[c1 + 1]:
                    c1 += 1
                stroke([(X0 + c * ST2 - 50, Y0 + r * ST2), (X0 + c1 * ST2 + 50, Y0 + r * ST2)], half, fall, layer)
                n += 1
                c = c1 + 1
            else:
                c += 1
    return n


counts = dict(reset=scan(RESET, "Sand", 50, 110), gravel=scan(GRAVEL, "DesertGravel"),
              scree=scan(SCREE, "DesertRockDark"), mud=scan(MUD, "Weedy"), apron=scan(APRON & LAND, "ScorchConcrete"),
              scorch=scan(SCORCH, "ScorchSandyStone"))
cover = dict(gravel=GRAVEL.mean(), scree=SCREE.mean(), mud=MUD.mean(), scorch=SCORCH.mean(), reset=RESET.mean())
print("strokes", counts, "cover", {k: round(float(v), 3) for k, v in cover.items()})
np.save(os.path.join(OUT, "masks.npy"), np.stack([GRAVEL, SCREE, MUD, SCORCH, APRON & LAND, USERZ]))

# ================================================================ 13. corrections after looking at the result
# - scorched crater decals are solid black stars on this ground: use the sandy crater set the map already uses
# - the dust-spill path decal draws as an opaque square off its ribbon; the grey Megascans gravel decal clashes
# - the long sandstone formation reads as a red sausage from above: dropped
# - the pale sandstone clusters only sit right on the pale scree paint, smaller
def near_scree(x, y, reach=2):
    r, c = gi2(x, y)
    return bool(SCREE[max(r - reach, 0):r + reach + 1, max(c - reach, 0):c + reach + 1].any())


for k, d in enumerate(decals):
    if d["tag"] == "Tracks":
        d["yaw"] = round(d["yaw"] + 90.0, 1)      # the ruts in these textures run across the decal yaw
    if "Scorched" in d["mat"]:
        d["mat"] = CRATER_MATS[k % len(CRATER_MATS)]
# (the sandy asphalt-debris instance also draws as an opaque square on the landscape)
decals = [d for d in decals if d["tag"] not in ("DustSpill", "RoadDebris") and "Gravel_vcedbbxh" not in d["mat"]]
kept = []
DESERT_ROCKS = "/Game/RTS_Survival/Meshes/Desert/Rocks/"
rock_rng = random.Random(31)
for p in props:
    if p["tag"] == "Ledge":
        continue
    if p["tag"] in ("Rocky", "RimRock"):
        # User 2026-11-14: "do not use this 8k material give them something else" (the Megascans 8K sandstone
        # clusters drew white). The project's own desert rocks (2K outback material, LODs, no collision) take over.
        if not near_scree(p["x"], p["y"]):
            continue
        k = rock_rng.random()
        if k < 0.82:
            p["asset"] = DESERT_ROCKS + "SM_Rock_0%d" % rock_rng.choice((1, 2, 3, 6, 7))
            p["s"] = round(rock_rng.uniform(0.45, 1.0), 2)
        else:
            p["asset"] = DESERT_ROCKS + "SM_Rock_0%d" % rock_rng.choice((4, 5))
            p["s"] = round(rock_rng.uniform(0.3, 0.5), 2)
        p["sink"] = 8
    kept.append(p)
props = kept

plan = dict(paint=paint, paint_v1=paint_v1, user_paint=USER_PAINT2, dips=dips, troughs=troughs, decals=decals, props=props)
json.dump(plan, open(os.path.join(OUT, "plan.json"), "w"))
print({k: len(v) for k, v in plan.items()})
import collections
print("paint", collections.Counter(p["layer"] for p in paint))
print("decals", collections.Counter(d["tag"] for d in decals))
print("props", collections.Counter(p["tag"] for p in props))
print("dips", [(d["x"], d["y"], d["r"], d["depth"], d["why"][:14]) for d in dips])
print("troughs", [(t["why"], len(t["pts"]), t["pts"][0], t["pts"][-1]) for t in troughs])
