"""Builds the Soviet sites of Ger_2ndOreMountains from existing project content (OreMountainsSites.md).

Editor Python on the OPEN map (unreal module only). Re-runnable: every actor it owns is labelled ORE_Site_* (plus the one
radio mast, labelled ORE_BaseB_RadioMast so the convoy routes can find it). A re-run moves and re-sets actors that already
exist under their label (references from other actors stay valid), spawns missing ones and deletes ORE_Site_* actors
that are no longer in the plan.

    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission")
    import place_soviet_sites as pss; importlib.reload(pss)
    report = pss.run()            # places everything, removes the replaced placeholders, returns + logs a report
    pss.run(only=["Battery"])     # one site only: BaseA, BaseB, Radio, Checkpoint, Battery
    pss.check()                   # report only (counts, tags, owners, float/sink, overlaps), changes nothing

Positions: Unreal cm from the map centre, +X north, +Y east. Every actor is seated on the landscape by line traces over
its mesh footprint (the lowest ground point, minus a small embed). Owners: every Rus building / BXP gets RTSComponent
OwningPlayer 2 (the map's one enemy controller); the strongpoints run for EnemyPlayer 2.
Tags read by the mission stages: ORE_BaseA_Target, ORE_BaseB_Target, ORE_RadioOutpost (Mission_Ore_* track them by tag
at stage start). Every placed actor also carries ORE_SovietSite and its site tag (ORE_Site_BaseA ...).
"""
import json
import math

import unreal

PREFIX = "ORE_Site_"
RADIO_MAST_LABEL = "ORE_BaseB_RadioMast"
FOLDER = "OreMountains/SovietSites/"
ENEMY = 2
MAP_HINT = "OreMountains"

TAG_ALL = "ORE_SovietSite"
TAG_A = "ORE_BaseA_Target"
TAG_B = "ORE_BaseB_Target"
TAG_RADIO = "ORE_RadioOutpost"
TAG_MAST = "ORE_BaseB_RadioMast"

BX = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
RB = BX + "RusBuildings/"
RK = BX + "RusBunkers/"
ENV = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/"
A = {
    "supplies": RB + "Rus_Building_Supplies",
    "ammo_store": RB + "Rus_Building_AmmoStorage",
    "fuel_store": RB + "Rus_Building_FuelStorage",
    "admin": RB + "Rus_Building_AdminBunker",
    "hut": RB + "Rus_Bxp_ContainerHut",
    "megaphone": RB + "Rus_Bxp_Megaphone",
    "tower_mg": RK + "Rus_BXP_GuardTowerMG_Soviet",
    "tower_large": RK + "Rus_BXP_LargeGuardTower",
    "mg_bunker": RK + "Rus_BXP_MGBunker",
    "zis_bunker": RK + "Rus_BXP_Bunker02_ZIS",
    "pit_53k": BX + "Rus53KPit/Rus_BXP_53KPit",
    # owner 2026-11-24: the battery's guns are B-4 203 mm howitzer pits (high arc clears the ridge lip; unsupplied
    # range ~97 m, so only a supplied battery reaches the ridge)
    "pit_b4": "/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerPit",
    "pit_bs3": BX + "RusBS3Pit/Rus_BXP_BS3Pit",
    "pit_mortar": BX + "RusMortarPit/Rus_BXP_120MortarPit",
    "pit_dshk": BX + "RusDShKPit/Rus_BXP_DShKPit",
    "pit_ampulomet": BX + "RusAmpulometPit/Rus_BXP_AmpulometPit",
    "crates": ENV + "Airport/BP_Airport_Dest_CrateStack01",
    "sandbags": ENV + "NaturalCover/SandBagWalls/BP_Cover_RusSandbags_LongArc",
    "kerb": ENV + "Military/Barricades/BP_Military_Dest_ConcreteBarrier",
    # plain meshes (no collision, no nav effect)
    "antenna": "/Game/RTS_Survival/Meshes/Buildings/Radar/SM_Antenna2",
    "slab": "/Game/RTS_Survival/SurvivalMode/Data/SovietBases/Foundations/SM_SovietSlab",
}
# Rus buildings with an UEnemyBuildingTrainerComponent (barracks, tents, factories, yards) are left out on purpose:
# with a decision tree on the enemy controller they would train units, and Base A must stay loot-only.

HQ = (-17085.0, 8040.0)      # the player's HQ note: battery pits face it
BATTERY_CENTRE = (18760.0, 13400.0)
BATTERY_RADIUS = 5500.0      # AEnemyArtilleryBattery ORE_Battery auto-collects within 6000 cm


def yaw_to(x, y, tx, ty):
    return round(math.degrees(math.atan2(ty - y, tx - x)), 1)


def item(site, name, asset, x, y, yaw, tags=(), **opts):
    """One placed actor. opts: scale (x, y, z), no_collision, embed (cm into the ground), top_flush (mesh pivot is its
    top face: seat the top on the highest ground point), attach (label of the parent), label (full label override)."""
    entry = {"site": site, "label": opts.pop("label", PREFIX + site + "_" + name), "asset": asset, "x": float(x),
             "y": float(y), "yaw": float(yaw), "tags": list(tags)}
    entry.update(opts)
    return entry


def post(post_id, x, y, yaw, units, from_difficulty="NEW_TO_RTS", dig_in=False, cover=True):
    """A strongpoint post at a world position; units = [("squad"|"vehicle", subtype name), ...] (one is picked)."""
    return {"id": post_id, "x": float(x), "y": float(y), "yaw": float(yaw), "units": units, "from": from_difficulty,
            "dig_in": dig_in, "cover": cover}


def strongpoint(site, name, sp_id, x, y, posts):
    return {"site": site, "label": PREFIX + site + "_" + name, "strongpoint": sp_id, "x": float(x), "y": float(y),
            "yaw": 0.0, "posts": posts, "tags": []}


MOSIN = [("squad", "Squad_Rus_Mosin")]
PTRS = [("squad", "Squad_Rus_Okhotnik")]
MAXIM = [("squad", "Squad_Rus_Maxim")]
HAZMAT_BAZOOKA = [("squad", "Squad_Rus_HazmatBazooka")]
ZIS76 = [("squad", "Squad_Rus_Zis_76MM")]
AT53K = [("squad", "Squad_Rus_53K_45MM")]

# ------------------------------------------------------------------------------------------------ the plan
# Base A, the Wismut convoy yard (yard X -1842..3852, Y 20435..27135; the valley road crosses its south part
# diagonally; the ammo track leaves east from (2010, 24120); the counterattack road group spawns at (2010, 22500)).
# Loot-only bonus: the four storage buildings carry ORE_BaseA_Target. No convoy route may use them as owning building.
BASE_A = [
    item("BaseA", "Supplies", A["supplies"], 2600, 25700, 0, [TAG_A]),        # replaces the ore plant block
    item("BaseA", "AmmoStore_1", A["ammo_store"], 3250, 24700, 0, [TAG_A]),
    item("BaseA", "AmmoStore_2", A["ammo_store"], 1500, 24800, 0, [TAG_A]),
    item("BaseA", "FuelStore", A["fuel_store"], -1400, 24300, 0, [TAG_A]),
    item("BaseA", "Tower_SW", A["tower_mg"], -1200, 20950, 270),
    item("BaseA", "Tower_NW", A["tower_mg"], 3000, 20850, 270),
    item("BaseA", "MGBunker_E", A["mg_bunker"], -200, 26700, 180),
    strongpoint("BaseA", "Garrison", "ORE_BaseA", 1200, 23600, [
        post("LightTank_1", 1200, 23300, 270, [("vehicle", "Tank_T70")]),
        post("LightTank_2", 2700, 23200, 270, [("vehicle", "Tank_BT7")]),
        post("Squad_1", 600, 26000, 180, MOSIN),
        post("Squad_2", -1100, 23000, 180, MOSIN),
        post("Squad_3", 2000, 26900, 180, PTRS, "HARD"),
    ]),
]

# Base B on the Wismut knoll (top 16 m, centre (12730, -6030)). The road below is to the south (-X). Kept clear: the
# west ramp top (~14224, -7375), the rear ramp top (~14720, -5750) and the viaduct spur's approach slab (from its
# pivot (9750, -4380) about 18 m north-west onto the top; the layout's AT point (10211, -5113) lies on it).
BASE_B = [
    item("BaseB", "Headframe", A["tower_large"], 13350, -4500, 20, [TAG_B]),      # headframe stand-in (15.7 m tower)
    item("BaseB", "HoistHouse", A["admin"], 12600, -8250, 20, [TAG_B]),          # hoist house / command post
    item("BaseB", "Store", A["ammo_store"], 13300, -6000, 20, [TAG_B]),
    item("BaseB", "RadioMast", A["megaphone"], 12500, -5900, 200, [TAG_B, TAG_MAST], label=RADIO_MAST_LABEL),
    item("BaseB", "RadioMast_Antenna", A["antenna"], 12500, -5650, 200, [], scale=(1.3, 1.3, 1.3), no_collision=True,
         attach=RADIO_MAST_LABEL),
    item("BaseB", "ZisBunker", A["zis_bunker"], 11000, -8300, 180),
    item("BaseB", "Tower_NE", A["tower_mg"], 14200, -3100, 30),
    strongpoint("BaseB", "Garrison", "ORE_BaseB", 12000, -6000, [
        post("AT_1", 10211, -6947, 180, ZIS76),
        post("AT_2", 10050, -6030, 180, AT53K),
        post("AT_3", 10100, -7800, 190, ZIS76),
        post("Squad_1", 11500, -7300, 180, MOSIN),
        post("Squad_2", 12300, -4900, 180, MOSIN),
        post("PTRS_1", 11800, -4900, 180, PTRS),
        post("MG_1", 10400, -7350, 180, MAXIM),
        post("Squad_3", 13100, -6900, 270, MOSIN, "HARD"),
        post("AT_4", 10000, -8400, 200, ZIS76, "BRUTAL"),
    ]),
]

# Radio outpost at spot R3 (11000, 4500; footprint 28 x 24 m). Strongpoint squads spawn at runtime and carry no tags,
# so the stage's tag ORE_RadioOutpost sits on the radio hut: taking the outpost = destroying the hut.
RADIO = [
    item("Radio", "Hut", A["hut"], 11000, 4300, 200, [TAG_RADIO]),
    item("Radio", "Antenna", A["antenna"], 11000, 4700, 20, [], no_collision=True),
    item("Radio", "Crates_1", A["crates"], 11500, 3900, 15),
    item("Radio", "Crates_2", A["crates"], 11850, 4450, 80),
    item("Radio", "Crates_3", A["crates"], 10600, 3700, 120),
    item("Radio", "Sandbags_S", A["sandbags"], 10050, 4100, 0),
    item("Radio", "Sandbags_SE", A["sandbags"], 10500, 5300, 90),
    strongpoint("Radio", "Guards", "ORE_RadioOutpost", 11000, 4500, [
        post("Mosin_1", 10500, 4100, 180, MOSIN),
        post("Mosin_2", 10950, 5250, 180, MOSIN),
        post("Mosin_3", 11700, 4800, 270, MOSIN, "NORMAL"),
        post("HazmatBazooka", 11100, 3600, 180, HAZMAT_BAZOOKA),
    ]),
]

# Weighbridge checkpoint: deck in the road (placeholder spot (133, 8107), turned to the road), booth (-604, 7504),
# towers N/S. The fence, wire, hedgehogs, boom barrier and stop sign are ore_map_build.step_checkpoint's (ORE_CP_*).
DECK = (88.0, 8107.0, 5.5)   # centre on the road line, yaw along the road


def deck_point(lx, ly):
    cx, cy, yaw = DECK
    r = math.radians(yaw)
    return cx + lx * math.cos(r) - ly * math.sin(r), cy + lx * math.sin(r) + ly * math.cos(r)


CHECKPOINT = [
    item("Checkpoint", "Booth", A["hut"], -604, 7504, 0),
    # moved 2 m toward the road from the placeholder (1138, 6633): the bank rises north; its legs bury up to ~1 m
    item("Checkpoint", "Tower_N", A["tower_mg"], 950, 6450, 180, max_sink=110),
    item("Checkpoint", "Tower_S", A["tower_mg"], -872, 8978, 180),
    item("Checkpoint", "Weighbridge", A["slab"], DECK[0], DECK[1], DECK[2], [], scale=(4.0, 18.0, 1.0),
         no_collision=True, top_flush=True),
    item("Checkpoint", "Kerb_N1", A["kerb"], *deck_point(650, -450), DECK[2]),
    item("Checkpoint", "Kerb_N2", A["kerb"], *deck_point(650, 450), DECK[2]),
    strongpoint("Checkpoint", "Guards", "ORE_Checkpoint", 937, 8509, [
        post("Mosin_1", 1000, 8600, 180, MOSIN),
        post("Mosin_2", -800, 8150, 180, MOSIN),
        post("MG_1", 1250, 7650, 180, MAXIM),
        post("PTRS_1", -1100, 9500, 180, PTRS, "HARD"),
    ]),
]

# Artillery battery pits on the levelled pad around (18760, 13400), all within 55 m of the centre so ORE_Battery
# auto-collects them. Its guard is squads only: team weapons and BXPs of player 2 within 60 m would count as pits.
# Layout spots, some moved 2-10 m off the battery props (camo net, supply storage, crates, sandbags).
PITS = [("B4_A", "pit_b4", 14560, 13400), ("B4_B", "pit_b4", 22960, 13400),
        ("Mortar_A", "pit_mortar", 20050, 12550), ("Mortar_B", "pit_mortar", 18900, 15000),
        ("DShK_A", "pit_dshk", 18450, 10250), ("DShK_B", "pit_dshk", 17592, 15827),
        ("Ampulomet", "pit_ampulomet", 20700, 16300)]
BATTERY = [item("Battery", "Pit_" + n, A[k], x, y, yaw_to(x, y, *HQ)) for n, k, x, y in PITS] + [
    # off the centre, where the other agent's AEnemyArtilleryBattery ORE_Battery stands
    strongpoint("Battery", "Guard", "ORE_BatteryGuard", 19400, 13000, [
        post("Mosin_1", 19300, 12250, 190, MOSIN),
        post("Mosin_2", 18400, 13500, 190, MOSIN),
        post("PTRS_1", 18250, 15950, 190, PTRS, "NORMAL"),
    ]),
]

SITES = {"BaseA": BASE_A, "BaseB": BASE_B, "Radio": RADIO, "Checkpoint": CHECKPOINT, "Battery": BATTERY}

# Placeholder blocks + labels these sites replace (ore_map_build.step_placeholders would bring them back on a rebuild).
REPLACED = {
    "BaseA": ["ORE_PH_wismut_ore_plant", "ORE_Note_PH_wismut_ore_plant", "ORE_Note_base_a"],
    "BaseB": ["ORE_PH_wismut_headframe", "ORE_Note_PH_wismut_headframe", "ORE_PH_wismut_hoist_house",
              "ORE_Note_PH_wismut_hoist_house", "ORE_Note_base_b"],
    "Radio": [],
    "Checkpoint": ["ORE_PH_cp_weighbridge", "ORE_Note_PH_cp_weighbridge", "ORE_PH_cp_booth", "ORE_Note_PH_cp_booth",
                   "ORE_PH_cp_tower_n", "ORE_Note_PH_cp_tower_n", "ORE_PH_cp_tower_s", "ORE_Note_PH_cp_tower_s",
                   "ORE_PH_cp_barrier", "ORE_Note_PH_cp_barrier", "ORE_Note_checkpoint"],
    "Battery": ["ORE_Note_artillery_battery"] + ["ORE_Note_pit_" + p for p in (
        "bs3_a", "bs3_b", "53k", "mortar_a", "mortar_b", "dshk_a", "dshk_b", "ampulomet")],
}

# Lines and points that must stay clear of anything solid (check only): (name, polyline, half width cm).
VALLEY_ROAD = [(-1905, 28475), (-1807, 27638), (-1483, 26800), (-1039, 25962), (-602, 25125), (-309, 24288),
               (-163, 23450), (19, 22612), (202, 21775), (366, 20938), (500, 20100), (587, 19262), (67, 10050),
               (25, 9212), (62, 8375), (142, 7538), (108, 6700), (-27, 5862)]
KEEP_CLEAR = [
    ("valley_road", VALLEY_ROAD, 520.0),
    # the ammo track from Base A to the battery (ore_01 layout "ammo_route"; the convoy 3 branch trucks drive it)
    ("ammo_track", [(2010, 24120), (3128, 23861), (4246, 23603), (5360, 23316), (6495, 23019), (7627, 22693),
                    (8710, 22244), (9738, 21524), (10715, 20672), (11725, 19966), (12926, 19549), (14150, 19246),
                    (15320, 18733), (16440, 18032), (17554, 17286), (18530, 16641), (19509, 15977), (20488, 15312)],
     350.0),
    ("counterattack_spawn", [(2010, 22500)], 600.0),
    ("knoll_west_ramp", [(13900, -7100), (14224, -7375), (14505, -8871)], 650.0),
    ("knoll_rear_ramp", [(14400, -5850), (14720, -5750), (15857, -4830)], 650.0),
    ("spur_slab", [(9750, -4380), (11000, -5700)], 700.0),    # the spur's approach slab runs onto the knoll top
    ("gate", [(133, 7102)], 600.0),
]

# ------------------------------------------------------------------------------------------------ helpers
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log_warning("ORE_SITES " + message)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def require_map():
    w = world()
    if w is None or MAP_HINT not in w.get_name():
        raise RuntimeError("open Ger_2ndOreMountains first (editor world is %s)" % (w.get_name() if w else None))
    if not [a for a in EAS.get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)]:
        raise RuntimeError("no landscape in " + w.get_name())
    return w


def ground(x, y):
    """Landscape height at (x, y); every other hit is ignored (placeholders, props, our own actors)."""
    ignore = []
    for _ in range(16):
        hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 30000.0), unreal.Vector(x, y, -30000.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        t = hit.to_tuple()
        if not t[0]:
            return None
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        if t[9] is None:
            return None
        ignore.append(t[9])
    return None


_CLASSES = {}


def load_class_or_mesh(path):
    if path not in _CLASSES:
        asset = unreal.load_asset(path)
        if asset is None:
            raise RuntimeError("missing asset " + path)
        _CLASSES[path] = asset.generated_class() if isinstance(asset, unreal.Blueprint) else asset
    return _CLASSES[path]


def enum_value(enum_type, wanted):
    key = wanted.replace("_", "").lower()
    for name in dir(enum_type):
        if name.replace("_", "").lower() == key:
            return getattr(enum_type, name)
    raise RuntimeError("no %s.%s" % (enum_type.__name__, wanted))


def difficulty_value(name):
    # ERTSGameDifficulty's python name collides with the FRTSGameDifficulty struct: take the type from a post.
    kind = type(unreal.EnemyStrongpointPost().get_editor_property("from_difficulty"))
    return enum_value(kind, name)


def local_mesh_box(actor):
    """Footprint of the visible colliding meshes in the actor's own frame (cm), from the mesh bounds and the component
    transforms; the actor bounds are useless here (BXP selection boxes, an editor-empty geometry collection). The frame
    is the actor's location and yaw without its scale, so the box is in world centimetres and independent of the yaw."""
    inv = unreal.Transform(actor.get_actor_location(), actor.get_actor_rotation(), unreal.Vector(1, 1, 1)).inverse()
    lo, hi = [1e9] * 3, [-1e9] * 3
    comps = [c for c in actor.get_components_by_class(unreal.StaticMeshComponent)
             if c.get_editor_property("static_mesh") is not None and c.is_visible()]
    solid = [c for c in comps if c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION] or comps
    for comp in solid:
        box = comp.get_editor_property("static_mesh").get_bounding_box()
        xf = comp.get_world_transform()
        for cx in (box.min.x, box.max.x):
            for cy in (box.min.y, box.max.y):
                for cz in (box.min.z, box.max.z):
                    p = inv.transform_location(xf.transform_location(unreal.Vector(cx, cy, cz)))
                    for i, v in enumerate((p.x, p.y, p.z)):
                        lo[i] = min(lo[i], v)
                        hi[i] = max(hi[i], v)
    if lo[0] > hi[0]:
        return None
    return lo, hi


def footprint_points(x, y, yaw, box, inset=0.9):
    """Ground sample points (world xy) over a local box: corners, edge midpoints and centre, pulled in by `inset`."""
    (x0, y0, _), (x1, y1, _) = box
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    hx, hy = (x1 - x0) / 2.0 * inset, (y1 - y0) / 2.0 * inset
    r = math.radians(yaw)
    out = []
    for fx in (-1.0, 0.0, 1.0):
        for fy in (-1.0, 0.0, 1.0):
            lx, ly = cx + fx * hx, cy + fy * hy
            out.append((x + lx * math.cos(r) - ly * math.sin(r), y + lx * math.sin(r) + ly * math.cos(r)))
    return out


def world_corners(x, y, yaw, box):
    (x0, y0, _), (x1, y1, _) = box
    r = math.radians(yaw)
    return [(x + lx * math.cos(r) - ly * math.sin(r), y + lx * math.sin(r) + ly * math.cos(r))
            for lx, ly in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))]


def set_tags(actor, entry):
    tags = [TAG_ALL, "ORE_Site_" + entry["site"]] + entry["tags"]
    actor.set_editor_property("tags", [unreal.Name(t) for t in tags])


def set_owner(actor):
    owners = []
    for comp in actor.get_components_by_class(unreal.RTSComponent):
        comp.set_editor_property("owning_player", ENEMY)
        owners.append(int(comp.get_editor_property("owning_player")))
    return owners


def own_actors():
    found = {}
    for actor in EAS.get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith(PREFIX) or label == RADIO_MAST_LABEL:
            if label in found:          # a duplicate (copy-pasted in the editor): keep the first
                EAS.destroy_actor(actor)
                continue
            found[label] = actor
    return found


def reuse_or_spawn(existing, entry, cls_or_mesh, location, rotation):
    actor = existing.pop(entry["label"], None)
    want = unreal.StaticMeshActor.static_class() if isinstance(cls_or_mesh, unreal.StaticMesh) else (
        cls_or_mesh if not entry.get("strongpoint") else unreal.EnemyStrongpoint.static_class())
    if actor is not None and actor.get_class() != want:
        EAS.destroy_actor(actor)
        actor = None
    if actor is None:
        actor = EAS.spawn_actor_from_class(want, location, rotation)
        actor.set_actor_label(entry["label"])
    else:
        actor.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD,
                                unreal.DetachmentRule.KEEP_WORLD)
        actor.set_actor_location_and_rotation(location, rotation, False, True)
    actor.set_folder_path(FOLDER + entry["site"])
    return actor


# ------------------------------------------------------------------------------------------------ placing
def place_item(entry, existing):
    asset = load_class_or_mesh(entry["asset"])
    gz = ground(entry["x"], entry["y"])
    if gz is None:
        raise RuntimeError("no landscape under " + entry["label"])
    # measure the footprint unrotated, then turn and seat it
    actor = reuse_or_spawn(existing, entry, asset, unreal.Vector(entry["x"], entry["y"], gz), unreal.Rotator(0, 0, 0))
    if isinstance(asset, unreal.StaticMesh):
        comp = actor.static_mesh_component
        # an attached mesh follows a movable BXP, so it must be movable itself
        comp.set_mobility(unreal.ComponentMobility.MOVABLE if entry.get("attach") else unreal.ComponentMobility.STATIC)
        comp.set_static_mesh(asset)
        if entry.get("no_collision"):
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_editor_property("can_ever_affect_navigation", False)
    scale = entry.get("scale", (1.0, 1.0, 1.0))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    box = local_mesh_box(actor)
    if box is None:
        box = ([-100.0, -100.0, 0.0], [100.0, 100.0, 100.0])
    samples = [ground(px, py) for px, py in footprint_points(entry["x"], entry["y"], entry["yaw"], box)]
    samples = [s for s in samples if s is not None] or [gz]
    if entry.get("top_flush"):
        z = max(samples) + 2.0                      # pivot on the top face (slab): top just above the road
    else:
        z = min(samples) - entry.get("embed", 5.0)  # Rus BXPs and props have their pivot on their ground line
    actor.set_actor_location_and_rotation(unreal.Vector(entry["x"], entry["y"], z),
                                          unreal.Rotator(0.0, 0.0, entry["yaw"]), False, True)
    set_tags(actor, entry)
    set_owner(actor)
    return actor


def make_post(p, sp_location):
    unit_options = []
    for kind, subtype in p["units"]:
        unit = unreal.EnemyPlacementUnit()
        if kind == "vehicle":
            unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.VEHICLE)
            unit.set_editor_property("vehicle_subtype", enum_value(unreal.TankSubtype, subtype))
        else:
            unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.SQUAD)
            unit.set_editor_property("squad_subtype", enum_value(unreal.SquadSubtype, subtype))
        unit_options.append(unit)
    gz = ground(p["x"], p["y"])
    rel = unreal.Vector(p["x"] - sp_location.x, p["y"] - sp_location.y, (gz if gz is not None else sp_location.z) - sp_location.z)
    out = unreal.EnemyStrongpointPost()
    out.set_editor_property("post_id", p["id"])
    out.set_editor_property("post", unreal.Transform(rel, unreal.Rotator(0.0, 0.0, p["yaw"]), unreal.Vector(1, 1, 1)))
    out.set_editor_property("unit_options", unit_options)
    out.set_editor_property("from_difficulty", difficulty_value(p["from"]))
    out.set_editor_property("dig_in", p["dig_in"])
    out.set_editor_property("take_nearby_cover", p["cover"])
    return out


def place_strongpoint(entry, existing):
    gz = ground(entry["x"], entry["y"])
    location = unreal.Vector(entry["x"], entry["y"], gz)
    actor = reuse_or_spawn(existing, entry, None, location, unreal.Rotator(0, 0, 0))
    actor.set_editor_property("strongpoint_id", entry["strongpoint"])
    actor.set_editor_property("enemy_player", ENEMY)
    actor.set_editor_property("units_set_by_mission", False)
    actor.set_editor_property("posts", [make_post(p, location) for p in entry["posts"]])
    timing = actor.get_editor_property("timing")
    timing.set_editor_property("spawn_on_begin_play", True)
    actor.set_editor_property("timing", timing)
    set_tags(actor, entry)
    return actor


def remove_placeholders(sites):
    removed = []
    by_label = {a.get_actor_label(): a for a in EAS.get_all_level_actors()}
    for site in sites:
        for label in REPLACED[site]:
            actor = by_label.get(label)
            if actor is not None:
                EAS.destroy_actor(actor)
                removed.append(label)
    return removed


def run(only=None):
    """Places the sites (all, or the names in `only`), removes the replaced placeholders, returns the check report."""
    require_map()
    sites = list(only) if only else list(SITES)
    existing = own_actors()
    placed = {}
    for site in sites:
        for entry in SITES[site]:
            if entry.get("strongpoint"):
                placed[entry["label"]] = place_strongpoint(entry, existing)
            else:
                placed[entry["label"]] = place_item(entry, existing)
        for entry in SITES[site]:
            parent = entry.get("attach")
            if parent and parent in placed:
                placed[entry["label"]].attach_to_actor(placed[parent], "", unreal.AttachmentRule.KEEP_WORLD,
                                                       unreal.AttachmentRule.KEEP_WORLD,
                                                       unreal.AttachmentRule.KEEP_WORLD, False)
    # what is left in `existing` belongs to a site we ran but is no longer planned (other sites are kept)
    stale = []
    for label, actor in existing.items():
        site = label[len(PREFIX):].split("_")[0] if label.startswith(PREFIX) else "BaseB"
        if site in sites:
            stale.append(label)
            EAS.destroy_actor(actor)
    removed = remove_placeholders(sites)
    report = check()
    report["removed_placeholders"] = removed
    report["removed_stale"] = stale
    log("placed %d actors in %s, removed %d placeholders, %d stale" % (len(placed), sites, len(removed), len(stale)))
    return report


# ------------------------------------------------------------------------------------------------ checking
def _dist_to_polyline(px, py, line):
    if len(line) == 1:
        return math.hypot(px - line[0][0], py - line[0][1])
    best = 1e12
    for (ax, ay), (bx, by) in zip(line[:-1], line[1:]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(px - ax - t * dx, py - ay - t * dy))
    return best


def _poly_box(corners):
    xs = [c[0] for c in corners]
    ys = [c[1] for c in corners]
    return min(xs), min(ys), max(xs), max(ys)


def _overlap(a, b, margin=0.0):
    return a[0] < b[2] - margin and b[0] < a[2] - margin and a[1] < b[3] - margin and b[1] < a[3] - margin


def check():
    """Counts, tags, owners, seating (float/sink over the footprint), corridor clearance and overlaps. Changes nothing."""
    require_map()
    plan = {e["label"]: e for entries in SITES.values() for e in entries}
    actors = own_actors()
    rows, problems = [], []
    boxes = {}
    post_points = []
    for label, actor in sorted(actors.items()):
        entry = plan.get(label)
        loc = actor.get_actor_location()
        row = {"label": label, "class": actor.get_class().get_name(), "x": round(loc.x), "y": round(loc.y),
               "z": round(loc.z), "yaw": round(actor.get_actor_rotation().yaw, 1),
               "tags": [str(t) for t in actor.get_editor_property("tags")]}
        if entry is None:
            problems.append(label + ": not in the plan")
        if isinstance(actor, unreal.EnemyStrongpoint):
            posts = actor.get_editor_property("posts")
            row["strongpoint_id"] = str(actor.get_editor_property("strongpoint_id"))
            row["enemy_player"] = int(actor.get_editor_property("enemy_player"))
            row["posts"] = []
            for p in posts:
                xf = p.get_editor_property("post")
                units = [str(u.get_editor_property("squad_subtype") if u.get_editor_property("kind") ==
                             unreal.EnemyPlacementUnitKind.SQUAD else u.get_editor_property("vehicle_subtype")).split(".")[-1]
                         for u in p.get_editor_property("unit_options")]
                wx, wy = loc.x + xf.translation.x, loc.y + xf.translation.y
                gz = ground(wx, wy)
                post_points.append(("%s/%s" % (label, p.get_editor_property("post_id")), wx, wy))
                row["posts"].append([str(p.get_editor_property("post_id")), round(wx), round(wy), units,
                                     str(p.get_editor_property("from_difficulty")).split(".")[-1].split(":")[0]])
                if not units:
                    problems.append("%s/%s: no unit" % (label, p.get_editor_property("post_id")))
                if gz is None or abs(loc.z + xf.translation.z - gz) > 60.0:
                    problems.append("%s/%s: post %s cm off the ground" % (label, p.get_editor_property("post_id"),
                                                                         None if gz is None else round(loc.z + xf.translation.z - gz)))
            rows.append(row)
            continue
        row["owners"] = [int(c.get_editor_property("owning_player")) for c in actor.get_components_by_class(unreal.RTSComponent)]
        if row["owners"] and any(o != ENEMY for o in row["owners"]):
            problems.append(label + ": owner " + str(row["owners"]))
        rot = actor.get_actor_rotation()
        box = local_mesh_box(actor)
        if box is not None:
            samples = [ground(px, py) for px, py in footprint_points(loc.x, loc.y, rot.yaw, box)]
            samples = [s for s in samples if s is not None]
            row["size_m"] = [round((box[1][i] - box[0][i]) / 100.0, 1) for i in range(3)]
            if samples:
                row["float_cm"] = round(loc.z - min(samples))       # pivot (ground line) above the lowest point
                row["sink_cm"] = round(max(samples) - loc.z)         # highest ground point above the ground line
                flat = entry is not None and entry.get("top_flush")
                if flat:
                    if not 0 <= loc.z - max(samples) <= 10:
                        problems.append("%s: deck top %d cm off the road" % (label, round(loc.z - max(samples))))
                elif row["float_cm"] > 15:
                    problems.append("%s floats %s cm" % (label, row["float_cm"]))
                elif row["sink_cm"] > (entry or {}).get("max_sink", 80):
                    problems.append("%s sinks %s cm on its high side" % (label, row["sink_cm"]))
            corners = world_corners(loc.x, loc.y, rot.yaw, box)
            blocking = not (entry or {}).get("no_collision")
            if blocking:
                boxes[label] = _poly_box(corners)
                for name, line, half in KEEP_CLEAR:
                    pts = corners + [(loc.x, loc.y)] + [((corners[i][0] + corners[(i + 1) % 4][0]) / 2.0,
                                                         (corners[i][1] + corners[(i + 1) % 4][1]) / 2.0) for i in range(4)]
                    d = min(_dist_to_polyline(px, py, line) for px, py in pts)
                    if d < half:
                        problems.append("%s within %d cm of %s" % (label, round(d), name))
        if entry is not None and entry["site"] == "Battery":
            d = math.hypot(loc.x - BATTERY_CENTRE[0], loc.y - BATTERY_CENTRE[1])
            row["battery_dist_m"] = round(d / 100.0, 1)
            if d > BATTERY_RADIUS:
                problems.append(label + ": %.1f m from the battery centre" % (d / 100.0))
        rows.append(row)
    # overlaps with each other and with other solid actors near the sites
    labels = list(boxes)
    for i, a in enumerate(labels):
        for b in labels[i + 1:]:
            if _overlap(boxes[a], boxes[b], 20.0) and not (plan.get(a, {}).get("attach") == b or plan.get(b, {}).get("attach") == a):
                problems.append("overlap %s / %s" % (a, b))
    others = []
    for actor in EAS.get_all_level_actors():
        label = actor.get_actor_label()
        if label in actors or label.startswith(("ORE_Trees_", "ORE_Scatter_", "ORE_Rocks_", "ORE_Cliff_", "ORE_Road_",
                                                 "ORE_Decal", "ORE_CliffDecal", "ORE_Fx_", "ORE_Note_", "ORE_Intercept",
                                                 "ORE_Junction")):
            continue
        if not (label.startswith(("ORE_Prop_", "ORE_CP_", "ORE_PH_", "ORE_Spur", "ORE_CrushRock_", "ORE_Tree_"))):
            continue
        # placeholder blocks have no collision but stand for models to come: keep off them too
        origin, extent = actor.get_actor_bounds(not label.startswith("ORE_PH_"))
        here = actor.get_actor_location()
        # some destructibles report a box stretched to the world origin (an editor-empty geometry collection)
        if extent.x <= 0 or extent.x > 4000 or extent.y > 4000 or abs(origin.x - here.x) > 1500 or abs(origin.y - here.y) > 1500:
            continue
        others.append((label, (origin.x - extent.x, origin.y - extent.y, origin.x + extent.x, origin.y + extent.y)))
    for a in labels:
        for label, ob in others:
            if _overlap(boxes[a], ob, 20.0):
                problems.append("overlap %s / %s" % (a, label))
    # posts: off the corridors, outside solid boxes, and the first thing a trace down hits is the landscape
    for name, wx, wy in post_points:
        for corridor, line, half in KEEP_CLEAR:
            d = _dist_to_polyline(wx, wy, line)
            if d < half:
                problems.append("post %s within %d cm of %s" % (name, round(d), corridor))
        point = (wx, wy, wx, wy)
        for other, box in list(boxes.items()) + others:
            if _overlap(point, (box[0] - 50.0, box[1] - 50.0, box[2] + 50.0, box[3] + 50.0)):
                problems.append("post %s inside %s" % (name, other))
        hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(wx, wy, 30000.0), unreal.Vector(wx, wy, -30000.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        first = hit.to_tuple()[9] if hit is not None and hit.to_tuple()[0] else None
        if first is not None and not isinstance(first, unreal.LandscapeProxy):
            problems.append("post %s: first hit %s" % (name, first.get_actor_label()))
    counts = {}
    for row in rows:
        site = row["label"][len(PREFIX):].split("_")[0] if row["label"].startswith(PREFIX) else "BaseB"
        counts[site] = counts.get(site, 0) + 1
    tagged = {}
    for tag in (TAG_A, TAG_B, TAG_RADIO, TAG_MAST):
        tagged[tag] = sorted(a.get_actor_label() for a in EAS.get_all_level_actors()
                             if tag in [str(t) for t in a.get_editor_property("tags")])
    report = {"map": world().get_name(), "counts": counts, "tagged": tagged, "actors": rows, "problems": problems}
    log("check: %d actors %s, tagged %s, %d problems" % (len(rows), counts, {k: len(v) for k, v in tagged.items()},
                                                         len(problems)))
    for p in problems:
        log("  problem: " + p)
    return report


def report_json(report):
    return json.dumps(report, indent=1, default=str)
