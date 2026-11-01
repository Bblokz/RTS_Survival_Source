"""In-editor dressing for a survival map built by survival_terrain_gen.py + survival_map_build.py.

Every actor gets the SMD_ label prefix and lives under the SurvivalDressing outliner folder, so
`survival_map_build.clear_dressing()` + rerunning these steps replaces the dressing. Foliage (dead shrubs) is painted
through the UECP `paint_foliage` action (see `shrub_patches()` + the doc), not from here.

All spots come from the generator (layout["dress"]), which knows slopes and masks: rock faces on the scarps, boulders on
the talus, ledge stones on the ridge tops, stones on the terraces, props in the open desert. Nothing with collision
stands inside a build disc, on a ramp, or within `EDGE_KEEP_CLEAR` of the map edge (power line ends excepted: they run
off the map between the spawn zones).
"""
import math
import random

import unreal

import survival_map_build as smb

ROCKS = "/Game/Brushify/Materials/Landscape/ProceduralMeshes/Rocks/"
# Rock_01 has no desert material instance of its own, so only 02..06 (each with its *_Desert MI) are used.
BOULDERS = [(ROCKS + "Rock_0%d/S_Rock_0%d" % (i, i), ROCKS + "Rock_0%d/MI_Rock_0%d_Desert" % (i, i)) for i in
            (2, 3, 4, 5, 6)]
MEGA = "/Game/Megascans/3D_Assets/"
SPIRE = MEGA + "Desert_Western_Spire_Layered_Xl_04_vennei3/S_Desert_Western_Spire_Layered_Xl_04_vennei3_lod5"
CLIFF = MEGA + "Desert_Western_Cliff_Layered_Xl_14_vjrmfb1ab/S_Desert_Western_Cliff_Layered_Xl_14_vjrmfb1ab_lod5_Var1"
FORMATION = MEGA + "Desert_Western_Formation_Layered_Xl_06_uk4paip/S_Desert_Western_Formation_Layered_Xl_06_uk4paip_lod5_Var1"
# One rock family for the whole map: the red layered Desert_Western set. The grey-green sandstone scans clash with the
# rust ground. The spire and formation lod5 meshes are open shells, so scarps use only the closed cliff hull.
# Loose rock uses the same red hull at small scale: the Western rock scan renders pale green against this ground.
LEDGES = [
    MEGA + "Desert_Western_Ledge_Sand_Medium_02_xgjmfc1/S_Desert_Western_Ledge_Sand_Medium_02_xgjmfc1_lod5",
    MEGA + "Desert_Western_Ledge_Sand_Medium_03_xgjndej/S_Desert_Western_Ledge_Sand_Medium_03_xgjndej_lod5",
]
FACE_HEIGHT = 1478.0
FACE_LENGTH = 2580.0
FACE_DEPTH = 1370.0
GREEK = "/Game/Greek_island/Assets/"
PALMS = [GREEK + "Tree/SM_palm_tree_01", GREEK + "Tree/SM_palm_tree_02", GREEK + "Tree/SM_palm_tree_03"]
SMALL_PALMS = [GREEK + "Tree/SM_small_palm_tree", GREEK + "Tree/SM_small_palm_tree_02"]
SHORE_PLANTS = [GREEK + "Grass/SM_grass", GREEK + "Grass/SM_grass1", GREEK + "Grass/SM_grass_03", GREEK + "Grass/SM_plant",
                GREEK + "Grass/SM_plant_02"]
WELL = "/Game/Modular_MedievalTown_WI/Meshes/Props/SM_Well"
MESHES = "/Game/RTS_Survival/Meshes/"
CACTUS_BUSHES = [MESHES + "Desert/Cacti/CactusBush/SM_RTS_CactusBush_%s_%s_0%d" % (size, colour, index)
                 for size in ("Large", "Medium") for colour in ("Green",) for index in (1, 2, 3)]
WATER_MATERIAL = "/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_OasisWater"
MINIMAP_WATER_MATERIAL = "/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_OasisMinimapWater"
SKIRT_MATERIAL = "/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_HorizonSkirt"
# The landscape's outer 9 m blend down to this height (generator edge band), so the skirt meets it without a step.
EDGE_Z = 470.0
LANDSCAPE_HALF = 75600.0
ENV = "/Game/RTS_Survival/Blueprints/Environment/"
CACTUS = ENV + "DestructableEnvActor/Desert/BP_Desert_Dest_Cactus"
DEAD_TREE = ENV + "DestructableEnvActor/BP_Scorch_Dest_DeadTree"
COVER = ENV + "DestructableEnvActor/NaturalCover/Cactus/BP_Cover_CactusBush_%s_%s_0%d"
POLES = ENV + "DestructableEnvActor/Industrial/Electricity/"
POLE_BLUEPRINTS = {"pole1": POLES + "BP_Electricity_SinglePole1", "pole2": POLES + "BP_Electricity_SinglePole2",
                   "pole3": POLES + "BP_Electricity_SinglePole3"}
# 05_Sandy renders glossy/white from RTS height, so it is left out.
CRATER_DECALS = ["/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d_Sandy" % i for i in
                 (1, 2, 3, 4, 6)] + ["/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_04_SandyRock"]
SHRUB_TYPES = ["/Game/RTS_Survival/SurvivalMode/Maps/Foliage/FT_SM_DeadShrub_%02d" % i for i in (1, 3, 5, 7, 9, 11)]

EDGE_KEEP_CLEAR = 9000.0
MAX_ROCK_FACES = 1900
MAX_FOOT_BOULDERS = 240
MAX_TOP_STONES = 140


def starts():
    return smb.LAYOUT["starts"]


def half():
    return smb.LAYOUT["landscape"]["size_cm"] / 2.0


def dist_start(x, y):
    return min(math.hypot(x - s["x"], y - s["y"]) for s in starts())


def dress():
    return smb.LAYOUT["dress"]


def boulder(rng, label, folder, x, y, scale_range=(3.0, 5.0)):
    mesh, mat = rng.choice(BOULDERS)
    scale = rng.uniform(*scale_range)
    return smb.spawn_mesh(mesh, label, folder, x, y, yaw=rng.uniform(0, 360), scale=scale, sink=45.0 * scale,
                          material=mat, pitch=rng.uniform(-8, 8), roll=rng.uniform(-8, 8))


def step_rock_faces(seed=21):
    """Continuous outcrop: overlapping cliff hulls along the chains the generator walked. Tops stay under the rim, so
    no lids show from above. No collision: the landscape under them is too steep to drive, so nothing can enter them."""
    smb.require_map()
    rng = random.Random(seed)
    faces = sorted(dress()["faces"], key=lambda q: -q["drop"])[:MAX_ROCK_FACES]
    count = 0
    for k, q in enumerate(faces):
        # Low rims hide their caps under the rim. Tall walls are mostly rock: chains stand proud of the face so paint never shows
        # as a stretched wall with windows.
        tall = q["drop"] > 500.0
        buried = rng.random() < (0.25 if tall else 0.72)
        scale = max(0.14, min(1.0, q["drop"] * rng.uniform(1.15, 1.5) / FACE_HEIGHT))
        height = FACE_HEIGHT * scale
        rim = q["z"] + 0.4 * q["drop"]
        top = rim - q["drop"] * (rng.uniform(0.08, 0.3) if buried else rng.uniform(-0.03, 0.04))
        stretch = max(0.6, min(1.9, q["length"] / (FACE_LENGTH * scale) * rng.uniform(0.85, 1.25)))
        depth = rng.uniform(0.5, 0.95)
        ux, uy = -math.cos(math.radians(q["downhill"])), -math.sin(math.radians(q["downhill"]))
        back = (0.08 if tall else 0.3) * FACE_DEPTH * scale * depth + (-60.0 if tall else 40.0)
        yaw = q["yaw"] + (180.0 if rng.random() < 0.4 else 0.0) + rng.uniform(-9, 9)
        actor = smb.spawn_mesh_at(CLIFF, "Face_S%d_%d" % (q["start"], k), "Rocks/Faces", q["x"] + ux * back, q["y"] + uy * back,
                                  top - height, yaw=yaw, scale=scale, pitch=rng.uniform(-5, 5), roll=rng.uniform(-5, 5),
                                  collision=False, stretch=stretch, depth=depth)
        count += 1 if actor else 0
    smb.log("rock faces %d of %d" % (count, len(dress()["faces"])))
    return count


def step_rocks(seed=11):
    """A talus fan under the rock bands, sizes falling off away from the cliff, and ledge stones on the ridge tops."""
    smb.require_map()
    rng = random.Random(seed)
    count = 0
    foot = sorted(dress()["foot"], key=lambda q: -q["near"])[:MAX_FOOT_BOULDERS]
    for k, q in enumerate(foot):
        size = (0.05 + 0.16 * min(1.0, q["near"] * 2.5)) * rng.uniform(0.6, 1.0)
        big = size > 0.13  # a boulder a tank would not drive through
        ok = smb.spawn_mesh(CLIFF, "Talus_%d" % k, "Rocks/Talus", q["x"], q["y"], yaw=q["downhill"] + rng.uniform(-40, 40),
                            scale=size, sink=320.0 * size, pitch=rng.uniform(-14, 14), roll=rng.uniform(-14, 14), collision=big)
        count += 1 if ok else 0
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


def step_props(seed=13, cactus=45, trees=14, stones=12, cover_clusters=16):
    """Destructible cactus, dead trees and stones in the open desert. Terraces stay bare: bases and start content fill them."""
    smb.require_map()
    rng = random.Random(seed)
    spots = list(dress()["open"])
    rng.shuffle(spots)
    pool = iter(spots)
    n = 0
    for i in range(cactus):
        q = next(pool)
        if smb.spawn_blueprint(CACTUS, "Cactus_%d" % i, "Props/Cactus", q["x"], q["y"], yaw=rng.uniform(0, 360),
                               scale=rng.uniform(0.8, 1.2)):
            n += 1
    for i in range(trees):
        q = next(pool)
        if smb.spawn_blueprint(DEAD_TREE, "DeadTree_%d" % i, "Props/DeadTrees", q["x"], q["y"],
                               yaw=rng.uniform(0, 360), scale=rng.uniform(0.9, 1.25)):
            n += 1
    for i in range(stones):
        q = next(pool)
        n += stone_cluster(rng, "OpenStones_%d" % i, "Props/OpenStones", q["x"], q["y"])
    for i in range(cover_clusters):
        q = next(pool)
        n += cover_cluster(rng, "Cover_Open_%d" % i, q["x"], q["y"], rng.uniform(0, 360))
    smb.log("props %d" % n)
    return n


def stone_cluster(rng, label, folder, x, y):
    """Two to four sandstone rocks lying together. BP_Desert_Ground_Stones reads as a smooth red dome from RTS height."""
    n = 0
    for k in range(rng.randint(2, 4)):
        px, py = x + rng.uniform(-260.0, 260.0), y + rng.uniform(-260.0, 260.0)
        size = rng.uniform(0.05, 0.13)
        if smb.spawn_mesh(CLIFF, "%s_%d" % (label, k), folder, px, py, yaw=rng.uniform(0, 360), scale=size, sink=320.0 * size,
                          pitch=rng.uniform(-12, 12), roll=rng.uniform(-12, 12), collision=False):
            n += 1
    return n


def cover_cluster(rng, label, x, y, yaw):
    # fixed composition (one large + two small)
    parts = [("Large", 0.0, 0.0), ("Small", 420.0, 160.0), ("Small", -300.0, 380.0)]
    n = 0
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for i, (size, ox, oy) in enumerate(parts):
        px, py = x + ox * c - oy * s, y + ox * s + oy * c
        if smb.spawn_blueprint(COVER % (size, "Green" if i % 2 == 0 else "Blue", 1 + i % 3), "%s_%d" % (label, i),
                               "Props/CactusCover", px, py, yaw=yaw + i * 47.0):
            n += 1
    return n


def step_identity():
    """What makes the names true on the ground: the wells of Dry Wells."""
    smb.require_map()
    n = 0
    rng = random.Random(31)
    for form in smb.LAYOUT["features"]["landforms"]:
        for k, well in enumerate(form["wells"]):
            if smb.spawn_mesh(WELL, "Well_%d" % k, "Identity/Wells", well["x"], well["y"], yaw=rng.uniform(0, 360), scale=1.9, sink=8.0,
                              collision=True):
                n += 1
            for j in range(7):
                a = j * math.tau / 7.0 + rng.uniform(-0.2, 0.2)
                r = well["radius"] * rng.uniform(1.0, 1.35)
                smb.spawn_mesh(CLIFF, "WellStone_%d_%d" % (k, j), "Identity/Wells", well["x"] + r * math.cos(a),
                               well["y"] + r * math.sin(a), yaw=rng.uniform(0, 360), scale=rng.uniform(0.035, 0.06), sink=18.0, collision=False)
    smb.log("wells %d" % n)
    return n


def step_oasis():
    """Water plane at the basin's water level, then palms, small palms and shore plants thinning out with distance."""
    smb.require_map()
    rng = random.Random(41)
    basin = [b for f in smb.LAYOUT["features"]["world"] for b in f["basins"] if b.get("water_z")][0]
    xs = [p[0] for p in basin["outline"]]
    ys = [p[1] for p in basin["outline"]]
    cx, cy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    actor = smb.actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx, cy, basin["water_z"]))
    comp = actor.static_mesh_component
    comp.set_static_mesh(plane)
    comp.set_material(0, unreal.load_asset(WATER_MATERIAL))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("can_ever_affect_navigation", False)
    # The minimap capture has post-processing off and blows translucent water out: it sees a plain opaque plane instead.
    comp.set_editor_property("hidden_in_scene_capture", True)
    # Far past the basin on every side: the landscape, not the plane's edge, draws the shoreline.
    actor.set_actor_scale3d(unreal.Vector((max(xs) - min(xs) + 7000.0) / 100.0, (max(ys) - min(ys) + 7000.0) / 100.0, 1.0))
    actor.set_actor_label(smb.DRESS_PREFIX + "OasisWater")
    actor.set_folder_path(smb.DRESS_FOLDER + "/Oasis")
    twin = smb.actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx, cy, basin["water_z"]))
    tc = twin.static_mesh_component
    tc.set_static_mesh(plane)
    tc.set_material(0, unreal.load_asset(MINIMAP_WATER_MATERIAL))
    tc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    tc.set_editor_property("can_ever_affect_navigation", False)
    tc.set_editor_property("cast_shadow", False)
    tc.set_editor_property("visible_in_scene_capture_only", True)
    twin.set_actor_scale3d(actor.get_actor_scale3d())
    twin.set_actor_label(smb.DRESS_PREFIX + "OasisMinimapWater")
    twin.set_folder_path(smb.DRESS_FOLDER + "/Oasis")
    # The bed is too shallow for a slope to stop Recast, so the water is taken off the navmesh with null-area boxes.
    null_area = unreal.load_class(None, "/Script/NavigationSystem.NavArea_Null")
    for k, (x0, x1, y0, y1) in enumerate(dress()["water_boxes"]):
        box = smb.actors_sub().spawn_actor_from_class(unreal.NavModifierVolume, unreal.Vector((x0 + x1) / 2.0, (y0 + y1) / 2.0, basin["water_z"]))
        box.set_editor_property("area_class", null_area)
        box.set_actor_scale3d(unreal.Vector((x1 - x0) / 200.0, (y1 - y0) / 200.0, 6.0))
        box.set_actor_label(smb.DRESS_PREFIX + "OasisNavBox_%d" % k)
        box.set_folder_path(smb.DRESS_FOLDER + "/Oasis")
    n = 0
    plants = dress()["oasis"]
    for k, q in enumerate(plants["reeds"]):
        if smb.spawn_mesh(rng.choice(SHORE_PLANTS[:3]), "OasisReed_%d" % k, "Oasis/Reeds", q["x"], q["y"], yaw=rng.uniform(0, 360),
                          scale=rng.uniform(1.6, 3.0), sink=-2.0, collision=False):
            n += 1
    for k, q in enumerate(plants["palms"]):
        if smb.spawn_mesh(rng.choice(PALMS + SMALL_PALMS[:1]), "OasisPalm_%d" % k, "Oasis/Palms", q["x"], q["y"], yaw=rng.uniform(0, 360),
                          scale=q["scale"], sink=12.0, pitch=q["lean"], roll=rng.uniform(-6, 6), collision=False):
            n += 1
    for k, q in enumerate(plants["bank"]):
        if smb.spawn_mesh(rng.choice(SMALL_PALMS + SHORE_PLANTS), "OasisBank_%d" % k, "Oasis/Bank", q["x"], q["y"], yaw=rng.uniform(0, 360),
                          scale=rng.uniform(1.2, 2.4), sink=4.0, collision=False):
            n += 1
    for k, q in enumerate(plants["scrub"]):
        # The cactus bushes render as black specks at this low sun, so the dry fringe is sparse grass.
        if rng.random() < 0.5 and smb.spawn_mesh(rng.choice(SHORE_PLANTS[3:]), "OasisScrub_%d" % k, "Oasis/Scrub", q["x"], q["y"],
                                                 yaw=rng.uniform(0, 360), scale=rng.uniform(0.8, 1.4), sink=4.0, collision=False):
            n += 1
    smb.log("oasis plants %d" % n)
    return n


def step_power_lines():
    """Spawns the wired trellis poles and links them; the DestructibleWire of each pole builds its wires at BeginPlay."""
    smb.require_map()
    lines = {}
    count = 0
    for line in smb.LAYOUT["power_lines"]:
        actors = []
        for k, pole in enumerate(line["poles"]):
            actor_class = unreal.EditorAssetLibrary.load_blueprint_class(POLE_BLUEPRINTS[pole["kind"]])
            actor = smb.actors_sub().spawn_actor_from_class(actor_class, unreal.Vector(pole["x"], pole["y"], pole["z"] - 25.0),
                                                            unreal.Rotator(roll=0.0, pitch=0.0, yaw=pole["yaw"]))
            actor.set_actor_label("%sPole_%s_%02d" % (smb.DRESS_PREFIX, line["name"], k))
            actor.set_folder_path(smb.DRESS_FOLDER + "/Power/" + line["name"])
            actors.append(actor)
            count += 1
        lines[line["name"]] = actors
    links = 0
    for line in smb.LAYOUT["power_lines"]:
        actors = lines[line["name"]]
        children = {id(a): [] for a in actors}
        for a, b in zip(actors[:-1], actors[1:]):
            a.set_editor_property("ConnectToActor", [b])
            links += 1
        parent = line.get("branch_of")
        if parent:
            first = line["poles"][0]
            candidates = list(zip(lines[parent], [p for ln in smb.LAYOUT["power_lines"] if ln["name"] == parent for p in ln["poles"]]))
            junction = min(candidates, key=lambda c: math.hypot(c[1]["x"] - first["x"], c[1]["y"] - first["y"]))[0]
            existing = list(junction.get_editor_property("ConnectToActor"))
            junction.set_editor_property("ConnectToActor", existing + [actors[0]])
            links += 1
    smb.log("power poles %d, links %d" % (count, links))
    return count


def shrub_patches(seed=14, patches=110):
    """Patch list for the UECP paint_foliage action: (foliage_type, x, y, z, radius, density per m^2)."""
    rng = random.Random(seed)
    out = []
    tries = 0
    while len(out) < patches and tries < 5000:
        tries += 1
        radius = rng.uniform(2200.0, 4800.0)
        x = rng.uniform(-half() + radius, half() - radius)
        y = rng.uniform(-half() + radius, half() - radius)
        if dist_start(x, y) < smb.LAYOUT["starts"][0]["flat_radius"] + 1000.0 + radius:
            continue
        if math.hypot(x, y) < 3400.0 + radius:
            continue
        density = rng.uniform(0.004, 0.016)
        out.append((rng.choice(SHRUB_TYPES), x, y, 1500.0, radius, density))
    return out


def step_horizon_skirt(edge_z=EDGE_Z, reach=150000.0, overlap=20.0):
    """Flat sand past the landscape edge so no camera sees void: four strips (each covers one side and its corners),
    no collision, no navigation, no shadow, hidden from the minimap capture. Height = the landscape edge band height."""
    for a in smb.actors_sub().get_all_level_actors():
        if a.get_actor_label().startswith(smb.DRESS_PREFIX + "HorizonSkirt"):
            smb.actors_sub().destroy_actor(a)
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    material = unreal.load_asset(SKIRT_MATERIAL)
    inner = half() - overlap  # round 8: from the layout (the landscape is 88 200 cm now)
    depth = reach - inner
    strips = {"PX": (inner + depth / 2.0, 0.0, depth, 2.0 * reach), "NX": (-(inner + depth / 2.0), 0.0, depth, 2.0 * reach),
              "PY": (0.0, inner + depth / 2.0, 2.0 * inner, depth), "NY": (0.0, -(inner + depth / 2.0), 2.0 * inner, depth)}
    for name, (cx, cy, sx, sy) in strips.items():
        actor = smb.actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx, cy, edge_z - 0.5))
        comp = actor.static_mesh_component
        comp.set_static_mesh(plane)
        comp.set_material(0, material)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
        comp.set_editor_property("cast_shadow", False)
        comp.set_editor_property("hidden_in_scene_capture", True)
        actor.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, 1.0))
        actor.set_actor_label(smb.DRESS_PREFIX + "HorizonSkirt_" + name)
        actor.set_folder_path(smb.DRESS_FOLDER + "/Edge")
    return "skirt strips 4 at z %.1f out to %.0f" % (edge_z - 0.5, reach)
