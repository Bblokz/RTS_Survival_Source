"""Fills the alert-tier and fuel responses of ORE_PressureDirector in Ger_2ndOreMountains (editor Python, re-runnable).

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission")
    import place_director_responses as pdr; importlib.reload(pdr); pdr.run(); pdr.check()

It never saves: save the map yourself after reading the report (or use run_director_responses.py, a throwaway full
editor job that rebuilds navigation, runs this, checks the size and saves). Run it AFTER place_convoy_content.py:
place_convoy_content.place_director() resets the director's TierResponses to empty TODO entries on every run.

Placed (folder OreMountains/Director, labels ORE_Director_*, tag ORE_Director; found by label and updated in place):
- Tier 1 (1 stopped convoy): ORE_Director_T1_Patrol_East / _North, patrol wave spawners in the north foothills between
  Base B and the artillery battery (small infantry + a light vehicle, more on harder difficulties).
- Tier 2 (3 stopped convoys): the director's own TargetedAttack on the two spoil-heap stations
  (ORE_Explore_Station_HN1 / _HB3) with Mosin / PTRS squads and an armoured car, spawning behind Base B and at the
  weighbridge checkpoint. bOnlyHostileTargets stays ON: a station starts with owner 0 (neutral, not hostile to
  player 2, skipped), a captured one is owner 1 (hostile, attacked) and a knocked-out one owner 2 (skipped).
- Tier 3 (5 stopped convoys): ORE_Director_T3_Raid_Sally / _MainRamp, attack wave spawners from the valley floor up
  onto the ridge bench (prow sally ramp, main ramp) to ORE_HQ_AttackMarker.
- Fuel (each escaped fuel convoy, cargo response): ORE_Director_Fuel_1..4, strongpoints of 2 light tanks around
  Base B (a 3rd on Brutal), started one per escaped fuel convoy in list order. Fuel can escape with convoys 1, 2, 3, 6.
Every placement actor has its own start on BeginPlay OFF; the director starts it. The Ammo and Concrete cargo entries
(set by place_convoy_content.py) are kept. Units are starter values for the owner to tune.

Checks (logged as ORE_DIRECTOR lines, returned in the report dict, written to
Saved/OreMountains/director/place_report.json): director + stations present, station owner 0 and KnockedOutOwner ==
the director's EnemyPlayer, ground flatness and colliding actors at spawn points and posts, the editor label problem
text of every placement actor, and (when the world has navigation data) a nav path for every leg.
Docs: Source/Docs/Missions/ConvoyPressureDirector.md, section "Ore Mountains setup".
"""
import json
import math
import os

import unreal

MAP_NAME = "Ger_2ndOreMountains"
TEST_MAP_FOLDER = "/Game/RTS_Survival/Maps/Tmp/"
PREFIX = "ORE_Director_"
TAG = "ORE_Director"
FOLDER = "OreMountains/Director"
DIRECTOR_LABEL = "ORE_PressureDirector"
STATION_LABELS = ["ORE_Explore_Station_HN1", "ORE_Explore_Station_HB3"]
HQ_MARKER_LABEL = "ORE_HQ_AttackMarker"
HQ_MARKER_FALLBACK = (-21000.0, 8000.0)
ENEMY = 2
STOPPED_CONVOYS_PER_TIER = [1, 3, 5]
REPORT_PATH = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/director/place_report.json"

# Positions are world cm (+X north, +Y east), z from the landscape. Spots were picked on the generator height file:
# flat to ~0.6 m over 9 m and clear of the Base B buildings, posts, dormant slots, ramps and exploration objects.

# ---- unit options (each spawned unit picks one option at random) ----------------------------------------------------
MOSIN = [("squad", "SQUAD_RUS_MOSIN")]
PTRS = [("squad", "SQUAD_RUS_OKHOTNIK")]
MAXIM = [("squad", "SQUAD_RUS_MAXIM")]
ARMOURED_CAR = [("vehicle", "TANK_BA12"), ("vehicle", "TANK_BA14")]
LIGHT_TANK = [("vehicle", "TANK_T70"), ("vehicle", "TANK_BT7"), ("vehicle", "TANK_T26")]
MEDIUM_TANK = [("vehicle", "TANK_T34_76")]


def entry(options, count=1, spawn_point=-1):
    return {"options": options, "count": count, "spawn_point": spawn_point}


# ---- tier 1: patrols in the north foothills (Base B <-> radio outpost <-> battery) ----------------------------------
# Units add up per difficulty like every AEnemyWaveSpawner: Hard spawns base + normal + hard.
PATROL_UNITS = {
    "base": [entry(MOSIN), entry(ARMOURED_CAR)],
    "normal": [entry(MOSIN)],
    "hard": [entry(PTRS)],
    "brutal": [entry(LIGHT_TANK)],
}
PATROLS = [
    {"label": PREFIX + "T1_Patrol_East", "behaviour": "patrol", "first_delay": 10.0,
     # foot of the Base B rear ramp; patrols the east flank lane up to the radio outpost (R3, 11000 / 4500)
     "spawn": [(18200.0, -1800.0)],
     "waypoints": [(16500.0, -800.0), (14000.0, 1800.0), (16800.0, 4800.0), (11800.0, 2200.0)]},
    {"label": PREFIX + "T1_Patrol_North", "behaviour": "patrol", "first_delay": 40.0,
     # south-west of the battery pad; patrols the dead pine wood between the outpost and the battery
     "spawn": [(15200.0, 9000.0)],
     "waypoints": [(14500.0, 6800.0), (18000.0, 7500.0), (12500.0, 9800.0), (15400.0, 11200.0)]},
]

# ---- tier 2: the director's targeted attack on the two heap stations -----------------------------------------------
# FConvoyPressureTargetedAttack has one Units list (no per-difficulty lists): these spawn on every difficulty.
TIER2 = {
    "spawn": [(14900.0, -800.0),    # 0: east foot of the Base B knoll (start of the east flank lane)
              (1600.0, 9800.0)],    # 1: north of the weighbridge checkpoint yard
    "units": [entry(MOSIN, 2), entry(PTRS, 1), entry(ARMOURED_CAR, 1, spawn_point=0)],
    "retry_seconds": 20.0,
}

# ---- tier 3: raids from the valley up onto the ridge bench to ORE_HQ_AttackMarker -----------------------------------
RAIDS = [
    {"label": PREFIX + "T3_Raid_Sally", "behaviour": "attack", "first_delay": 30.0,
     "repeat": "fixed", "wave_count": 2, "wave_interval": 300.0,
     # valley floor below Base B -> road through the viaduct gap -> prow sally ramp -> bench -> HQ
     "spawn": [(5300.0, -6800.0)],
     "waypoints": [(300.0, -6500.0), (-300.0, -4188.0), (-4400.0, -1950.0), (-9600.0, -2850.0),
                   (-14500.0, 2500.0), "HQ"],
     "units": {"base": [entry(MOSIN, 2), entry(LIGHT_TANK)],
               "normal": [entry(PTRS)],
               "hard": [entry(MEDIUM_TANK)],
               "brutal": [entry(MAXIM), entry(MEDIUM_TANK)]}},
    {"label": PREFIX + "T3_Raid_MainRamp", "behaviour": "attack", "first_delay": 90.0, "repeat": "once",
     # foothill toe east of the road -> across the Radixite field -> main ramp -> bench past the refinery -> HQ
     "spawn": [(7700.0, 13300.0)],
     "waypoints": [(700.0, 13400.0), (-4500.0, 14500.0), (-9359.0, 14164.0), (-12500.0, 12000.0),
                   (-16500.0, 9500.0), "HQ"],
     "units": {"base": [entry(MOSIN, 2), entry(ARMOURED_CAR)],
               "normal": [entry(PTRS), entry(LIGHT_TANK)],
               "hard": [entry(MOSIN)],
               "brutal": [entry(MEDIUM_TANK)]}},
]

# ---- fuel: 2 light tanks at Base B per escaped fuel convoy (a 3rd on Brutal) ------------------------------------------
# (post id, x, y, yaw, from difficulty); started in this order, one strongpoint per escaped fuel convoy.
FUEL = [
    {"label": PREFIX + "Fuel_1", "posts": [("Tank_1", 9800.0, -2900.0, 160.0, "NEW_TO_RTS"),
                                           ("Tank_2", 10700.0, -2300.0, 160.0, "NEW_TO_RTS"),
                                           ("Tank_3", 9200.0, -2600.0, 160.0, "BRUTAL")]},     # valley side, east
    {"label": PREFIX + "Fuel_2", "posts": [("Tank_1", 12200.0, -1100.0, 110.0, "NEW_TO_RTS"),
                                           ("Tank_2", 13100.0, -800.0, 110.0, "NEW_TO_RTS"),
                                           ("Tank_3", 12500.0, -400.0, 110.0, "BRUTAL")]},    # east flank lane
    {"label": PREFIX + "Fuel_3", "posts": [("Tank_1", 16700.0, -10400.0, -120.0, "NEW_TO_RTS"),
                                           ("Tank_2", 17300.0, -8600.0, -120.0, "NEW_TO_RTS"),
                                           ("Tank_3", 17000.0, -9500.0, -120.0, "BRUTAL")]},   # west ramp, flank W
    {"label": PREFIX + "Fuel_4", "posts": [("Tank_1", 17000.0, -6200.0, 20.0, "NEW_TO_RTS"),
                                           ("Tank_2", 17600.0, -7700.0, 20.0, "NEW_TO_RTS"),
                                           ("Tank_3", 17300.0, -6900.0, 20.0, "BRUTAL")]},     # behind the knoll
]

PLANS_BY_LABEL = {plan["label"]: plan for plan in PATROLS + RAIDS + FUEL}
REPORT = []
PROBLEMS = []


def log(text):
    REPORT.append(str(text))
    unreal.log_warning("ORE_DIRECTOR " + str(text))


def problem(text):
    PROBLEMS.append(str(text))
    log("PROBLEM: " + str(text))


# ---- world -----------------------------------------------------------------------------------------------------------

def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def require_map():
    package = world().get_outermost().get_name()
    if package.split("/")[-1] == MAP_NAME or package.startswith(TEST_MAP_FOLDER):
        return package
    raise RuntimeError("open level is %s, expected %s (or a test copy under %s)" % (package, MAP_NAME, TEST_MAP_FOLDER))


def all_actors():
    return actors_sub().get_all_level_actors()


def find_by_label(label):
    for actor in all_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def ground_z(x, y):
    """Landscape height under (x, y); steps through anything standing on it."""
    ignore = []
    for _ in range(8):
        hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 30000.0), unreal.Vector(x, y, -10000.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        fields = hit.to_tuple()
        if not fields[0]:
            break
        actor = fields[9]
        if actor is None or isinstance(actor, unreal.LandscapeProxy):
            return fields[4].z
        ignore.append(actor)
    problem("no landscape under (%.0f, %.0f); z 0 used" % (x, y))
    return 0.0


def ground(x, y):
    return unreal.Vector(x, y, ground_z(x, y))


def to_local(actor, point):
    return actor.get_actor_transform().inverse_transform_location(point)


def find_or_spawn(label, cls, location, yaw=0.0):
    actor = find_by_label(label)
    if actor is not None and not isinstance(actor, cls):
        log("WARNING: %s exists as %s, replaced by %s" % (label, actor.get_class().get_name(), cls.__name__))
        actor.destroy_actor()
        actor = None
    rotation = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
    if actor is None:
        actor = actors_sub().spawn_actor_from_class(cls, location, rotation)
        actor.set_actor_label(label)
        log("spawned %s" % label)
    else:
        actor.set_actor_location_and_rotation(location, rotation, False, True)
        actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        log("updated %s" % label)
    actor.set_folder_path(FOLDER)
    tags = [t for t in actor.get_editor_property("tags") if str(t) != TAG]
    actor.set_editor_property("tags", tags + [unreal.Name(TAG)])
    return actor


def planned_labels():
    return [p["label"] for p in PATROLS] + [r["label"] for r in RAIDS] + [f["label"] for f in FUEL]


def remove_stale():
    keep = set(planned_labels())
    for actor in all_actors():
        label = actor.get_actor_label()
        if label.startswith(PREFIX) and label not in keep:
            log("removed stale %s" % label)
            actor.destroy_actor()


# ---- settings helpers ------------------------------------------------------------------------------------------------

def enum_value(enum_type, name):
    if hasattr(enum_type, name):
        return getattr(enum_type, name)
    key = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.replace("_", "").lower() == key:
            return getattr(enum_type, attr)
    raise RuntimeError("no %s.%s" % (enum_type.__name__, name))


def difficulty_value(name):
    # ERTSGameDifficulty's python name collides with the FRTSGameDifficulty struct: take the type from a post.
    kind = type(unreal.EnemyStrongpointPost().get_editor_property("from_difficulty"))
    return enum_value(kind, name)


def placement_unit(kind, subtype):
    unit = unreal.EnemyPlacementUnit()
    if kind == "vehicle":
        unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.VEHICLE)
        unit.set_editor_property("vehicle_subtype", enum_value(unreal.TankSubtype, subtype))
    else:
        unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.SQUAD)
        unit.set_editor_property("squad_subtype", enum_value(unreal.SquadSubtype, subtype))
    return unit


def unit_entries(entries):
    out = []
    for e in entries:
        item = unreal.EnemyWaveSpawnerUnitEntry()
        item.set_editor_property("unit_options", [placement_unit(k, s) for k, s in e["options"]])
        item.set_editor_property("count", e["count"])
        item.set_editor_property("spawn_point_index", e["spawn_point"])
        out.append(item)
    return out


def wave_units(units):
    out = unreal.EnemyWaveSpawnerUnits()
    out.set_editor_property("base_units", unit_entries(units.get("base", [])))
    out.set_editor_property("extra_units_normal", unit_entries(units.get("normal", [])))
    out.set_editor_property("extra_units_hard", unit_entries(units.get("hard", [])))
    out.set_editor_property("extra_units_brutal", unit_entries(units.get("brutal", [])))
    return out


def set_struct(actor, prop, **values):
    struct = actor.get_editor_property(prop)
    for key, value in values.items():
        struct.set_editor_property(key, value)
    actor.set_editor_property(prop, struct)
    return struct


def hq_marker_location():
    marker = find_by_label(HQ_MARKER_LABEL)
    if marker is None:
        problem("%s missing; the raids end at the fallback (%.0f, %.0f)" % ((HQ_MARKER_LABEL,) + HQ_MARKER_FALLBACK))
        return ground(*HQ_MARKER_FALLBACK)
    return marker.get_actor_location()


def world_point(xy, hq):
    return hq if xy == "HQ" else ground(*xy)


# ---- placement -------------------------------------------------------------------------------------------------------

def place_wave_spawner(plan, units, hq):
    spawn_world = [ground(*xy) for xy in plan["spawn"]]
    waypoint_world = [world_point(xy, hq) for xy in plan["waypoints"]]
    spawner = find_or_spawn(plan["label"], unreal.EnemyWaveSpawner, spawn_world[0])
    spawner.set_editor_property("spawner_id", plan["label"])
    spawner.set_editor_property("enemy_player", ENEMY)
    is_patrol = plan["behaviour"] == "patrol"
    spawner.set_editor_property("behaviour", unreal.EnemyWaveSpawnerBehaviour.PATROL_WAVE if is_patrol
                                else unreal.EnemyWaveSpawnerBehaviour.ATTACK_WAVE)
    spawner.set_editor_property("spawn_points", [to_local(spawner, p) for p in spawn_world])
    spawner.set_editor_property("waypoints", [to_local(spawner, p) for p in waypoint_world])
    spawner.set_editor_property("units", wave_units(units))
    repeat = plan.get("repeat", "once")
    timing = spawner.get_editor_property("timing")
    timing.set_editor_property("start_on_begin_play", False)
    timing.set_editor_property("first_wave_delay", plan["first_delay"])
    if repeat == "fixed":
        timing.set_editor_property("repeat", unreal.EnemyWaveSpawnerRepeat.FIXED_COUNT)
        timing.set_editor_property("wave_count", plan["wave_count"])
        timing.set_editor_property("wave_interval", plan["wave_interval"])
    else:
        timing.set_editor_property("repeat", unreal.EnemyWaveSpawnerRepeat.ONCE)
    spawner.set_editor_property("timing", timing)
    set_struct(spawner, "ownership", wave_type=enum_value(unreal.EnemyWaveType, "WAVE_NO_OWNING_BUILDING"),
               owning_building=None)
    # raids show on the minimap as an incoming wave; patrols stay hidden
    set_struct(spawner, "minimap", show_on_minimap=not is_patrol)
    return spawner, spawn_world, waypoint_world


def place_fuel_strongpoint(plan):
    posts_world = [(pid, ground(x, y), yaw, diff) for pid, x, y, yaw, diff in plan["posts"]]
    cx = sum(p[1].x for p in posts_world) / len(posts_world)
    cy = sum(p[1].y for p in posts_world) / len(posts_world)
    sp = find_or_spawn(plan["label"], unreal.EnemyStrongpoint, ground(cx, cy))
    sp.set_editor_property("strongpoint_id", plan["label"])
    sp.set_editor_property("enemy_player", ENEMY)
    sp.set_editor_property("units_set_by_mission", False)
    posts = []
    for pid, location, yaw, diff in posts_world:
        post = unreal.EnemyStrongpointPost()
        post.set_editor_property("post_id", pid)
        post.set_editor_property("post", unreal.Transform(to_local(sp, location), unreal.Rotator(0.0, 0.0, yaw),
                                                          unreal.Vector(1.0, 1.0, 1.0)))
        post.set_editor_property("unit_options", [placement_unit(k, s) for k, s in LIGHT_TANK])
        post.set_editor_property("from_difficulty", difficulty_value(diff))
        post.set_editor_property("dig_in", False)          # mobile base defence; the struct default digs in
        post.set_editor_property("take_nearby_cover", False)
        posts.append(post)
    sp.set_editor_property("posts", posts)
    timing = sp.get_editor_property("timing")
    timing.set_editor_property("spawn_on_begin_play", False)
    sp.set_editor_property("timing", timing)
    return sp, [p[1] for p in posts_world]


def find_stations():
    stations = []
    for label in STATION_LABELS:
        station = find_by_label(label)
        if station is None:
            raise RuntimeError("%s not in the map (run place_exploration.py first; needs AOreHeapStation in the DLL)"
                               % label)
        stations.append(station)
    return stations


def rts_owner(actor):
    for comp in actor.get_components_by_class(unreal.ActorComponent):
        if "RTSComponent" in comp.get_class().get_name() and hasattr(comp, "get_owning_player"):
            return comp.get_owning_player()
    return None


def check_station_targetability(stations):
    """bOnlyHostileTargets: only targets whose RTS component owner is hostile to the director's EnemyPlayer count."""
    rows = []
    for station in stations:
        owner = rts_owner(station)
        settings = station.get_editor_property("station_settings")
        knocked_out = settings.get_editor_property("knocked_out_owner")
        rows.append({"label": station.get_actor_label(), "class": station.get_class().get_name(), "owner": owner,
                     "knocked_out_owner": knocked_out})
        if not isinstance(station, getattr(unreal, "OreHeapStation", ())):
            problem("%s is not an AOreHeapStation" % station.get_actor_label())
        if owner != 0:
            problem("%s starts with owner %s, expected 0 (neutral, skipped until captured)"
                    % (station.get_actor_label(), owner))
        if knocked_out != ENEMY:
            problem("%s KnockedOutOwner is %s, not the director's EnemyPlayer %d: a knocked-out station would still "
                    "count as hostile" % (station.get_actor_label(), knocked_out, ENEMY))
    log("stations: %s" % rows)
    return rows


def place_director(patrols, raids, fuel, stations):
    director = find_by_label(DIRECTOR_LABEL)
    if director is None or not isinstance(director, unreal.ConvoyPressureDirector):
        raise RuntimeError("%s not in the map (run place_convoy_content.py first)" % DIRECTOR_LABEL)
    director.set_editor_property("enemy_player", ENEMY)
    director.set_editor_property("stopped_convoys_per_tier", STOPPED_CONVOYS_PER_TIER)
    director.set_editor_property("targeted_attack_retry_seconds", TIER2["retry_seconds"])
    spawn_world = [ground(*xy) for xy in TIER2["spawn"]]
    attack = unreal.ConvoyPressureTargetedAttack()
    attack.set_editor_property("enabled", True)
    attack.set_editor_property("targets", stations)
    attack.set_editor_property("only_hostile_targets", True)
    attack.set_editor_property("spawn_points", [to_local(director, p) for p in spawn_world])
    attack.set_editor_property("units", unit_entries(TIER2["units"]))
    tiers = [unreal.ConvoyPressureTierResponse(), unreal.ConvoyPressureTierResponse(),
             unreal.ConvoyPressureTierResponse()]
    tiers[0].set_editor_property("tier_name", "Tier1_FoothillPatrols")
    tiers[0].set_editor_property("placement_actors_to_start", patrols)
    tiers[1].set_editor_property("tier_name", "Tier2_RetakeStations")
    tiers[1].set_editor_property("targeted_attack", attack)
    tiers[2].set_editor_property("tier_name", "Tier3_RidgeRaids")
    tiers[2].set_editor_property("placement_actors_to_start", raids)
    director.set_editor_property("tier_responses", tiers)
    # array elements come back as copies: rebuild the list with the edited Fuel entry, keep Ammo / Concrete as they are
    responses = []
    has_fuel = False
    for response in director.get_editor_property("cargo_responses"):
        if response.get_editor_property("cargo_type") == unreal.ConvoyCargoType.FUEL:
            if has_fuel:
                log("removed a second Fuel cargo entry")
                continue
            response.set_editor_property("placement_actors_to_start", fuel)
            response.set_editor_property("placement_actors_per_escaped_convoy", 1)
            has_fuel = True
        responses.append(response)
    if not has_fuel:
        fuel_entry = unreal.ConvoyPressureCargoResponse()
        fuel_entry.set_editor_property("cargo_type", unreal.ConvoyCargoType.FUEL)
        fuel_entry.set_editor_property("placement_actors_to_start", fuel)
        fuel_entry.set_editor_property("placement_actors_per_escaped_convoy", 1)
        responses.append(fuel_entry)
    director.set_editor_property("cargo_responses", responses)
    return director, spawn_world


# ---- checks ----------------------------------------------------------------------------------------------------------

SKIP_CLASS_PARTS = ("Landscape", "Volume", "Navigation", "RecastNavMesh", "Brush", "WorldSettings", "Info",
                    "EnemyPlacement", "EnemyWaveSpawner", "EnemyStrongpoint", "ConvoyPressure", "TargetPoint",
                    "DormantDefence", "EnemyConvoyRoute", "ArtilleryBattery", "Decal", "Light", "Sky", "Fog")


def overlapping_actors(point, radius):
    result = unreal.SystemLibrary.sphere_overlap_actors(
        world(), point + unreal.Vector(0.0, 0.0, 150.0), radius,
        [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2], None, [])
    if isinstance(result, tuple):
        result = result[-1]
    names = []
    for actor in result or []:
        cls = actor.get_class().get_name()
        if any(part in cls for part in SKIP_CLASS_PARTS):
            continue
        names.append("%s (%s)" % (actor.get_actor_label(), cls))
    return names


def check_spot(name, point, radius=350.0, max_spread=150.0):
    zs = [ground_z(point.x + dx, point.y + dy) for dx in (-400.0, 0.0, 400.0) for dy in (-400.0, 0.0, 400.0)]
    spread = max(zs) - min(zs)
    if spread > max_spread:
        problem("%s at (%.0f, %.0f): ground uneven by %.0f cm over 8 m" % (name, point.x, point.y, spread))
    try:
        hits = overlapping_actors(point, radius)
    except Exception as error:  # overlap queries can be missing in a world without physics
        log("overlap check skipped at %s: %r" % (name, error))
        hits = []
    if hits:
        problem("%s at (%.0f, %.0f) touches %s" % (name, point.x, point.y, ", ".join(hits[:4])))


def label_problem(actor):
    for comp in actor.get_components_by_class(unreal.TextRenderComponent):
        text = str(comp.get_editor_property("text"))
        if "\n! " in text:
            return text.split("\n! ", 1)[1]
    return ""


def nav_system():
    nav = unreal.NavigationSystemV1.get_navigation_system(world())
    if nav is None:
        return None
    try:
        if nav.is_navigation_being_built(world()):
            log("navigation is being built: running the blocking RebuildNavigation first")
            unreal.SystemLibrary.execute_console_command(world(), "RebuildNavigation")
    except Exception as error:
        log("nav build state unknown: %r" % error)
    return nav


def check_leg(name, start, end, rows):
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world(), start, end)
    points = list(path.get_editor_property("path_points")) if path is not None else []
    ok = path is not None and len(points) >= 2 and not path.is_partial()
    length = path.get_path_length() if path is not None and points else 0.0
    straight = math.hypot(end.x - start.x, end.y - start.y)
    rows.append({"leg": name, "ok": ok, "length_m": round(length / 100.0), "straight_m": round(straight / 100.0)})
    if not ok:
        problem("nav: %s has no full path (%d points, partial=%s)" % (name, len(points),
                                                                    path.is_partial() if path else None))
    elif length > straight * 1.8 + 6000.0:
        problem("nav: %s detours: %.0f m for %.0f m straight" % (name, length / 100.0, straight / 100.0))


def check_nav(legs):
    if nav_system() is None:
        log("nav checks SKIPPED: no navigation system in this world (commandlet?)")
        return None
    probe = unreal.NavigationSystemV1.find_path_to_location_synchronously(world(), legs[0][1], legs[0][2])
    if probe is None or len(list(probe.get_editor_property("path_points"))) < 2:
        log("nav checks SKIPPED: no navigation data answered the first query")
        return None
    rows = []
    for name, start, end in legs:
        check_leg(name, start, end, rows)
    log("nav legs: %d checked, %d failed" % (len(rows), len([r for r in rows if not r["ok"]])))
    return rows


# ---- run / check -----------------------------------------------------------------------------------------------------

def run():
    """Places / updates the ORE_Director_* actors and fills the director. Returns the report dict (not saved)."""
    del REPORT[:]
    del PROBLEMS[:]
    package = require_map()
    log("map %s" % package)
    stations = find_stations()
    station_rows = check_station_targetability(stations)
    remove_stale()
    hq = hq_marker_location()
    legs = []
    spots = []
    patrols = []
    for plan in PATROLS:
        spawner, spawn_world, waypoint_world = place_wave_spawner(plan, PATROL_UNITS, hq)
        patrols.append(spawner)
        spots += [(plan["label"] + " spawn", p) for p in spawn_world]
        legs += [("%s -> patrol point %d" % (plan["label"], i), spawn_world[0], p) for i, p in enumerate(waypoint_world)]
    raids = []
    for plan in RAIDS:
        spawner, spawn_world, waypoint_world = place_wave_spawner(plan, plan["units"], hq)
        raids.append(spawner)
        spots += [(plan["label"] + " spawn", p) for p in spawn_world]
        chain = spawn_world[:1] + waypoint_world
        legs += [("%s leg %d" % (plan["label"], i), a, b) for i, (a, b) in enumerate(zip(chain[:-1], chain[1:]))]
    fuel = []
    for plan in FUEL:
        sp, posts = place_fuel_strongpoint(plan)
        fuel.append(sp)
        spots += [("%s post %d" % (plan["label"], i + 1), p) for i, p in enumerate(posts)]
    director, tier2_spawns = place_director(patrols, raids, fuel, stations)
    spots += [("Tier2 spawn %d" % i, p) for i, p in enumerate(tier2_spawns)]
    for spawn_index, spawn in enumerate(tier2_spawns):
        for station in stations:
            # the bin carves the navmesh: aim at its drop-off point, 8 m out along +X, where squads stand
            drop_off = station.get_actor_location() + station.get_actor_forward_vector() * 800.0
            legs.append(("Tier2 spawn %d -> %s" % (spawn_index, station.get_actor_label()), spawn, drop_off))
    for name, point in spots:
        check_spot(name, point)
    nav_rows = check_nav(legs)
    for actor in patrols + raids + fuel + [director]:
        text = label_problem(actor)
        if text:
            problem("%s editor label: %s" % (actor.get_actor_label(), text))
    report = {"map": package, "actors": {a.get_actor_label(): a.get_path_name() for a in patrols + raids + fuel},
              "director": director.get_path_name(), "stations": station_rows, "nav": nav_rows,
              "problems": list(PROBLEMS), "log": list(REPORT)}
    log("done: %d patrols, %d raids, %d fuel strongpoints, targeted attack on %d stations, %d problems (not saved)"
        % (len(patrols), len(raids), len(fuel), len(stations), len(PROBLEMS)))
    write_report(report)
    return report


def labels_of(actors):
    return [a.get_actor_label() if a is not None else None for a in actors]


def check():
    """Reads everything back from the director and the placed actors; returns {'ok', 'problems', ...}."""
    issues = []
    director = find_by_label(DIRECTOR_LABEL)
    if director is None:
        return {"ok": False, "problems": ["no director"]}
    out = {"thresholds": list(director.get_editor_property("stopped_convoys_per_tier")),
           "watched_routes": len(director.get_editor_property("watched_routes"))}
    if out["thresholds"] != STOPPED_CONVOYS_PER_TIER:
        issues.append("thresholds %s" % out["thresholds"])
    tiers = director.get_editor_property("tier_responses")
    expected = [[p["label"] for p in PATROLS], [], [r["label"] for r in RAIDS]]
    out["tiers"] = []
    started_actors = []
    for index, tier in enumerate(tiers):
        actors = list(tier.get_editor_property("placement_actors_to_start"))
        started_actors += actors
        attack = tier.get_editor_property("targeted_attack")
        row = {"name": str(tier.get_editor_property("tier_name")), "actors": labels_of(actors),
               "attack": attack.get_editor_property("enabled")}
        if row["attack"]:
            row["targets"] = labels_of(attack.get_editor_property("targets"))
            row["only_hostile"] = attack.get_editor_property("only_hostile_targets")
            row["units"] = sum(u.get_editor_property("count") for u in attack.get_editor_property("units"))
            row["spawn_points"] = len(attack.get_editor_property("spawn_points"))
        out["tiers"].append(row)
        if index < len(expected) and row["actors"] != expected[index]:
            issues.append("tier %d actors %s" % (index + 1, row["actors"]))
    if len(tiers) != 3:
        issues.append("%d tier responses" % len(tiers))
    elif not (out["tiers"][1]["attack"] and out["tiers"][1].get("targets") == STATION_LABELS
              and out["tiers"][1].get("only_hostile") and out["tiers"][1].get("units", 0) > 0):
        issues.append("tier 2 targeted attack %s" % out["tiers"][1])
    out["cargo"] = []
    for response in director.get_editor_property("cargo_responses"):
        actors = list(response.get_editor_property("placement_actors_to_start"))
        started_actors += actors
        cargo_type = response.get_editor_property("cargo_type")
        out["cargo"].append({"cargo": str(cargo_type).split(".")[-1].split(":")[0].strip("<> "),
                             "actors": labels_of(actors),
                             "per_convoy": response.get_editor_property("placement_actors_per_escaped_convoy"),
                             "dormant": len(response.get_editor_property("dormant_defences")),
                             "ammo_seconds": response.get_editor_property("ammo_supply_seconds_per_escaped_truck")})
    fuel_rows = [c for c in out["cargo"] if c["cargo"] == "FUEL"]
    if len(fuel_rows) != 1 or fuel_rows[0]["actors"] != [f["label"] for f in FUEL] or fuel_rows[0]["per_convoy"] != 1:
        issues.append("fuel cargo %s" % fuel_rows)
    for actor in started_actors:
        if actor is None:
            issues.append("empty placement actor slot")
            continue
        plan = PLANS_BY_LABEL.get(actor.get_actor_label())
        if isinstance(actor, unreal.EnemyWaveSpawner):
            on = actor.get_editor_property("timing").get_editor_property("start_on_begin_play")
            base = actor.get_editor_property("units").get_editor_property("base_units")
            waypoints = len(actor.get_editor_property("waypoints"))
            if not base or any(not e.get_editor_property("unit_options") for e in base):
                issues.append("%s has no base units" % actor.get_actor_label())
            if plan is not None and waypoints != len(plan["waypoints"]):
                issues.append("%s has %d waypoints, plan %d" % (actor.get_actor_label(), waypoints,
                                                                len(plan["waypoints"])))
            if not actor.get_editor_property("spawn_points"):
                issues.append("%s has no spawn points" % actor.get_actor_label())
        elif isinstance(actor, unreal.EnemyStrongpoint):
            on = actor.get_editor_property("timing").get_editor_property("spawn_on_begin_play")
            posts = actor.get_editor_property("posts")
            if plan is not None and len(posts) != len(plan["posts"]):
                issues.append("%s has %d posts, plan %d" % (actor.get_actor_label(), len(posts), len(plan["posts"])))
            if any(p.get_editor_property("dig_in") or not p.get_editor_property("unit_options") for p in posts):
                issues.append("%s has a post that digs in or has no unit" % actor.get_actor_label())
        else:
            on = None
        if on:
            issues.append("%s starts on BeginPlay" % actor.get_actor_label())
        text = label_problem(actor)
        if text:
            issues.append("%s label: %s" % (actor.get_actor_label(), text))
    text = label_problem(director)
    if text:
        issues.append("director label: %s" % text)
    out["placed"] = sorted(a.get_actor_label() for a in all_actors() if a.get_actor_label().startswith(PREFIX))
    if out["placed"] != sorted(planned_labels()):
        issues.append("placed %s" % out["placed"])
    out["problems"] = issues
    out["ok"] = not issues
    log("check: %s" % json.dumps(out))
    return out


def write_report(report):
    try:
        os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
        with open(REPORT_PATH, "w") as handle:
            json.dump(report, handle, indent=1, default=str)
    except Exception as error:
        log("report not written: %r" % error)


if __name__ == "__main__":
    run()
    check()
