"""In-editor dressing for a survival map built by survival_terrain_gen.py + survival_map_build.py.

Every actor gets the SMD_ label prefix and lives under the SurvivalDressing outliner folder, so
`survival_map_build.clear_dressing()` + rerunning these steps replaces the dressing. Foliage (dead shrubs) is painted
through the UECP `paint_foliage` action (see `shrub_patches()` + the doc), not from here.

Rules: nothing with collision inside a build disc (start radius + 1 km), on a ramp, or within `EDGE_KEEP_CLEAR` of the
map edge; anything stamped per start area uses the start's local frame so all start areas stay identical.
"""
import math
import random

import unreal

import survival_map_build as smb

ROCKS = "/Game/Brushify/Materials/Landscape/ProceduralMeshes/Rocks/"
# Rock_01 has no desert material instance of its own, so only 02..06 (each with its *_Desert MI) are used.
BOULDERS = [(ROCKS + "Rock_0%d/S_Rock_0%d" % (i, i), ROCKS + "Rock_0%d/MI_Rock_0%d_Desert" % (i, i)) for i in
            (2, 3, 4, 5, 6)]
SPIRE = "/Game/Megascans/3D_Assets/Desert_Western_Spire_Layered_Xl_04_vennei3/S_Desert_Western_Spire_Layered_Xl_04_vennei3_lod5"
CLIFF = "/Game/Megascans/3D_Assets/Desert_Western_Cliff_Layered_Xl_14_vjrmfb1ab/S_Desert_Western_Cliff_Layered_Xl_14_vjrmfb1ab_lod5_Var1"
ENV = "/Game/RTS_Survival/Blueprints/Environment/"
CACTUS = ENV + "DestructableEnvActor/Desert/BP_Desert_Dest_Cactus"
DEAD_TREE = ENV + "DestructableEnvActor/BP_Scorch_Dest_DeadTree"
COVER = ENV + "DestructableEnvActor/NaturalCover/Cactus/BP_Cover_CactusBush_%s_%s_0%d"
GROUND_STONES = [ENV + "GroundEnvActors/Desert/BP_Desert_Ground_Stones%d" % i for i in (1, 2, 3)]
# 05_Sandy renders glossy/white from RTS height, so it is left out.
CRATER_DECALS = ["/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d_Sandy" % i for i in
                 (1, 2, 3, 4, 6)] + ["/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_04_SandyRock"]
SHRUB_TYPES = ["/Game/RTS_Survival/SurvivalMode/Maps/Foliage/FT_SM_DeadShrub_%02d" % i for i in (1, 3, 5, 7, 9, 11)]

EDGE_KEEP_CLEAR = 9000.0


def cfg_hills():
    # mirror of the generator config (kept here so the dressing does not need numpy)
    return dict(plateau_radius=1300.0, plateau_half_length=1700.0, flank=520.0, ramp_half_width=650.0,
                ramp_side_flank=420.0)


def starts():
    return smb.LAYOUT["starts"]


def half():
    return smb.LAYOUT["landscape"]["size_cm"] / 2.0


def dist_start(x, y):
    return min(math.hypot(x - s["x"], y - s["y"]) for s in starts())


def edge_dist(x, y):
    return min(half() - abs(x), half() - abs(y))


def near_feature(x, y, pad):
    f = smb.LAYOUT["features"]
    for h in f["hills"]:
        if math.hypot(x - h["x"], y - h["y"]) < 2500.0 + pad:
            return True
        hx, hy = h["ramp_head"]
        fx, fy = h["ramp_foot"]
        if point_segment(x, y, hx, hy, fx, fy) < 1300.0 + pad:
            return True
    if math.hypot(x, y) < 3400.0 + 650.0 + 3800.0 + pad:
        return True
    for b in f["buttes"]:
        if math.hypot(x - b["x"], y - b["y"]) < 3200.0 + pad:
            return True
    for m in f["mounds"]:
        if math.hypot(x - m["x"], y - m["y"]) < 1800.0 + pad:
            return True
    for c in f["craters"]:
        if math.hypot(x - c["x"], y - c["y"]) < c["radius"] * 1.3 + pad:
            return True
    return False


def point_segment(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / max(dx * dx + dy * dy, 1e-6)))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def local_to_world(start, lx, ly):
    yaw = math.radians(start["yaw_to_map_centre_deg"])
    ix, iy = math.cos(yaw), math.sin(yaw)
    jx, jy = -iy, ix
    return start["x"] + lx * ix + ly * jx, start["y"] + lx * iy + ly * jy


def boulder(rng, label, folder, x, y, scale_range=(3.0, 5.0)):
    mesh, mat = rng.choice(BOULDERS)
    scale = rng.uniform(*scale_range)
    return smb.spawn_mesh(mesh, label, folder, x, y, yaw=rng.uniform(0, 360), scale=scale, sink=45.0 * scale,
                          material=mat, pitch=rng.uniform(-8, 8), roll=rng.uniform(-8, 8))


def step_rocks(seed=11):
    """Boulders around every hill flank (ramp side kept clear), around the mesa, spires on buttes, cliffs on mounds."""
    smb.require_map()
    rng = random.Random(seed)
    h = cfg_hills()
    count = 0
    by_start = {s["index"]: s for s in starts()}
    for hill in smb.LAYOUT["features"]["hills"]:
        s = by_start[hill["start"]]
        rdx, rdy = hill["x"] - s["x"], hill["y"] - s["y"]
        r = math.hypot(rdx, rdy)
        rdx, rdy = rdx / r, rdy / r
        tdx, tdy = -rdy, rdx
        d = h["plateau_radius"] + 380.0
        spots = [(+d, sv) for sv in (-1500.0, -500.0, 500.0, 1500.0)]  # outer side (away from the start)
        spots += [(-d, sv) for sv in (-1650.0, 1650.0)]                   # inner side, clear of the ramp
        for end in (-1.0, 1.0):                                           # end caps
            for a in (-55.0, 0.0, 55.0):
                ar = math.radians(a)
                spots.append((d * math.sin(ar), end * (h["plateau_half_length"] + d * math.cos(ar))))
        for k, (radial, tang) in enumerate(spots):
            x = hill["x"] + rdx * radial + tdx * tang
            y = hill["y"] + rdy * radial + tdy * tang
            if boulder(rng, "HillRock_%d_%d_%d" % (hill["start"], int(hill["local_angle_deg"]), k), "Rocks/Hills", x, y):
                count += 1
    # mesa ring, ramps kept clear
    ramp_angles = [math.atan2(s["y"], s["x"]) for s in starts()]
    for k in range(36):
        a = math.radians(k * 10.0 + 5.0)
        if any(abs(math.atan2(math.sin(a - ra), math.cos(a - ra))) < math.radians(24.0) for ra in ramp_angles):
            continue
        rr = 3400.0 + 420.0
        if boulder(rng, "MesaRock_%d" % k, "Rocks/Mesa", rr * math.cos(a), rr * math.sin(a), (3.5, 5.5)):
            count += 1
    for i, b in enumerate(smb.LAYOUT["features"]["buttes"]):
        tang = math.degrees(math.atan2(b["y"], b["x"])) + 90.0
        if smb.spawn_mesh(SPIRE, "ButteSpire_%d" % i, "Rocks/Buttes", b["x"], b["y"], yaw=tang + rng.uniform(-10, 10),
                          scale=0.75, sink=260.0):
            count += 1
        for j in range(5):
            a = rng.uniform(0, math.tau)
            bx, by = b["x"] + 2350.0 * math.cos(a), b["y"] + 2350.0 * math.sin(a)
            if boulder(rng, "ButteRock_%d_%d" % (i, j), "Rocks/Buttes", bx, by):
                count += 1
    for i, m in enumerate(smb.LAYOUT["features"]["mounds"]):
        if smb.spawn_mesh(CLIFF, "MoundCliff_%d" % i, "Rocks/Mounds", m["x"], m["y"], yaw=rng.uniform(0, 360),
                          scale=0.42, sink=180.0):
            count += 1
    smb.log("rocks spawned %d" % count)
    return count


def step_craters(seed=12):
    smb.require_map()
    for actor in smb.actors_sub().get_all_level_actors():
        if actor.get_actor_label().startswith(smb.DRESS_PREFIX + "CraterDecal_"):
            smb.actors_sub().destroy_actor(actor)
    rng = random.Random(seed)
    n = 0
    for i, c in enumerate(smb.LAYOUT["features"]["craters"]):
        if smb.spawn_decal(rng.choice(CRATER_DECALS), "CraterDecal_%d" % i, "Craters", c["x"], c["y"],
                           c["radius"] * 2.5, yaw=rng.uniform(0, 360)):
            n += 1
    smb.log("crater decals %d" % n)
    return n


def open_desert_spot(rng, min_start=19000.0, pad=600.0, edge=EDGE_KEEP_CLEAR):
    for _ in range(400):
        x = rng.uniform(-half() + edge, half() - edge)
        y = rng.uniform(-half() + edge, half() - edge)
        if dist_start(x, y) < min_start or near_feature(x, y, pad):
            continue
        return x, y
    return None


def step_props(seed=13, cactus=45, trees=14, stones=26, cover_clusters=10):
    """Destructible cactus / dead trees / ground stones in the open desert + identical cover clusters per start."""
    smb.require_map()
    rng = random.Random(seed)
    n = 0
    for i in range(cactus):
        p = open_desert_spot(rng)
        if p and smb.spawn_blueprint(CACTUS, "Cactus_%d" % i, "Props/Cactus", p[0], p[1], yaw=rng.uniform(0, 360),
                                     scale=rng.uniform(0.8, 1.2)):
            n += 1
    for i in range(trees):
        p = open_desert_spot(rng)
        if p and smb.spawn_blueprint(DEAD_TREE, "DeadTree_%d" % i, "Props/DeadTrees", p[0], p[1],
                                     yaw=rng.uniform(0, 360), scale=rng.uniform(0.9, 1.25)):
            n += 1
    for i in range(stones):
        p = open_desert_spot(rng, min_start=16000.0, pad=900.0, edge=4000.0)
        if p and smb.spawn_blueprint(rng.choice(GROUND_STONES), "GroundStones_%d" % i, "Props/GroundStones", p[0], p[1],
                                     yaw=rng.uniform(0, 360), scale=rng.uniform(0.8, 1.3)):
            n += 1
    # stamped per start: three cactus cover clusters in the wreck ring (local angle, radius)
    stamp = [(70.0, 17600.0), (200.0, 18200.0), (330.0, 17200.0)]
    for s in starts():
        for k, (a, r) in enumerate(stamp):
            ar = math.radians(a)
            cx, cy = local_to_world(s, r * math.cos(ar), r * math.sin(ar))
            n += cover_cluster(rng, "Cover_S%d_%d" % (s["index"], k), cx, cy, s["yaw_to_map_centre_deg"] + a)
    for i in range(cover_clusters):
        p = open_desert_spot(rng, min_start=22000.0, pad=1500.0)
        if p:
            n += cover_cluster(rng, "Cover_Open_%d" % i, p[0], p[1], rng.uniform(0, 360))
    smb.log("props %d" % n)
    return n


def cover_cluster(rng, label, x, y, yaw):
    # fixed composition (one large + two small) so stamped clusters give identical cover
    parts = [("Large", 0.0, 0.0), ("Small", 420.0, 160.0), ("Small", -300.0, 380.0)]
    n = 0
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for i, (size, ox, oy) in enumerate(parts):
        px, py = x + ox * c - oy * s, y + ox * s + oy * c
        if smb.spawn_blueprint(COVER % (size, "Green" if i % 2 == 0 else "Blue", 1 + i % 3), "%s_%d" % (label, i),
                               "Props/CactusCover", px, py, yaw=yaw + i * 47.0):
            n += 1
    return n


def shrub_patches(seed=14, patches=90):
    """Patch list for the UECP paint_foliage action: (foliage_type, x, y, z, radius, density per m^2)."""
    rng = random.Random(seed)
    out = []
    tries = 0
    while len(out) < patches and tries < 5000:
        tries += 1
        radius = rng.uniform(2200.0, 4800.0)
        x = rng.uniform(-half() + radius, half() - radius)
        y = rng.uniform(-half() + radius, half() - radius)
        if dist_start(x, y) < smb.LAYOUT["starts"][0]["build_radius"] + 1500.0 + radius:
            continue
        if math.hypot(x, y) < 3400.0 + radius:
            continue
        density = rng.uniform(0.004, 0.016)
        out.append((rng.choice(SHRUB_TYPES), x, y, 900.0, radius, density))
    return out
