"""Places the Ore Mountains convoy content in the open map Ger_2ndOreMountains (editor Python, re-runnable).

    import importlib, sys
    sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission")
    import place_convoy_content as pcc; importlib.reload(pcc); pcc.run()

It never saves: save the map yourself after reading the report it prints. Actors are found by label and updated in
place, so running it again keeps references other actors hold to them.

Places (folder OreMountains/Convoys):
- ORE_Convoy1..6: AEnemyConvoyRoute, RouteId ORE_Convoy1..6, Definition DA_Convoy_Ore_N, no start on BeginPlay,
  Repeat Once, FirstConvoyDelay 20 s. Route points follow the valley road spline (ORE_Road_Valley) from the east edge
  past the Base A yard, through the weighbridge and the viaduct gap to the west exit, every ~25 m, snapped to the
  landscape. Route 2 stops 60 s at the weighbridge; route 3 branches its ammo trucks off to the battery; route 4 drops
  hunters at the ambush corridor (attack target ORE_HQ_AttackMarker); routes 4-6 have ambush zones at the ambush
  corridor and the viaduct gap, with RadioPostActor = ORE_BaseB_RadioMast when that actor exists.
- ORE_HQ_AttackMarker: TargetPoint at the player HQ area for the hunters.
- ORE_Battery: AEnemyArtilleryBattery (BatteryId ORE_Battery, pits auto-collected within 35 m).
- ORE_PressureDirector: AConvoyPressureDirector watching the six routes; tier responses left empty for the owner
  (wave spawners); the Concrete response gets the dormant defence slots.
- ORE_BaseB_DormantSlot_1..4: ADormantDefenceSlot on the Base B knoll top (Rus AT pits / bunkers).
Then it checks every route (and the ammo branch) with UConvoyRouteLibrary::BuildNavigableRoute and prints problems.
"""
import unreal

ORE = "/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/Convoys/"
FIRST_CONVOY_DELAY = {1: 80.0, 3: 55.0}
DATA = "/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains/"  # the map's data assets, next to the level
MAP_NAME = "Ger_2ndOreMountains"
TEST_MAP_FOLDER = "/Game/RTS_Survival/Maps/Tmp/"
FOLDER = "OreMountains/Convoys"

# Valley road spline points (world cm, from ORE_Road_Valley on 2026-11-23); used only if the spline actor is missing.
ROAD_FALLBACK = [(-4728, 36660), (-1905, 28475), (-1039, 25962), (-163, 23450), (366, 20938), (608, 18425),
                 (862, 15912), (758, 13400), (189, 10888), (62, 8375), (-27, 5862), (-484, 3350), (-302, 838),
                 (-298, -1675), (-493, -4188), (214, -6700), (708, -9212), (1329, -11725), (1842, -14238),
                 (2691, -16750), (2711, -19262), (3022, -21775), (3517, -24288), (4234, -28475), (5963, -38576)]
ROAD_LABEL = "ORE_Road_Valley"
START_XY = (-1750.0, 27650.0)        # east edge, where the road enters the play area (convoys come from the east)
EXIT_XY = (4096.0, -27671.0)         # "convoys out" at the west edge
ROUTE_SPACING = 2500.0               # cm between route points (nav queries stay short)
WEIGHBRIDGE_STOP_XY = (133.0, 7900.0)  # on the weighbridge deck, short of the gate barrier at y 7102
AMBUSH_CORRIDOR_XY = (-2814.0, 1842.0)
VIADUCT_GAP_XY = (-300.0, -4188.0)
# Ammo branch (route 3): the fork on the road east of the weighbridge, then across the valley floor to the battery.
FORK_XY = (862.0, 15912.0)
BRANCH_VIA = []
BRANCH_END_XY = (19000.0, 13900.0)   # battery pad, inside the battery's 25 m truck arrival ring
BATTERY_XY = (18760.0, 13400.0)
# Hunters (route 4): up the prow sally ramp onto the bench, then to the HQ.
HUNTER_WAYPOINTS_XY = [(-4400.0, -1950.0), (-9600.0, -2850.0), (-14500.0, 2500.0)]
HQ_MARKER_XYZ = (-21000.0, 8000.0, 1800.0)
DIRECTOR_XY = (13600.0, -6000.0)
KNOLL_TOP_Z = 1600.0
# (label suffix, defence Blueprint, x, y, yaw): the knoll top around (12700, -6000), facing the valley road (-X)
DORMANT_SLOTS = [
    ("1", "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit", 11300.0, -7400.0, 180.0),
    ("2", "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_Bunker02_ZIS", 11200.0, -4600.0, 180.0),
    ("3", "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit", 12500.0, -8300.0, 200.0),
    ("4", "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_MGBunker", 12600.0, -3800.0, 160.0),
]
RADIO_MAST_LABEL = "ORE_BaseB_RadioMast"
REPORT = []


def log(text):
    REPORT.append(str(text))
    unreal.log_warning("ORE_CONVOY_PLACE " + str(text))


# ---- world -------------------------------------------------------------------------------------------------------

def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def require_map():
    package = world().get_outermost().get_name()
    if package.split("/")[-1] == MAP_NAME or package.startswith(TEST_MAP_FOLDER):
        return
    raise RuntimeError("open level is %s, expected %s (or a test copy under %s)" % (package, MAP_NAME, TEST_MAP_FOLDER))


def find_by_label(label):
    for actor in actors_sub().get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def find_or_spawn(label, cls, location, yaw=0.0):
    actor = find_by_label(label)
    if actor is not None and not isinstance(actor, cls):
        log("WARNING: %s exists as %s, replaced by %s" % (label, actor.get_class().get_name(), cls.__name__))
        actor.destroy_actor()
        actor = None
    if actor is None:
        actor = actors_sub().spawn_actor_from_class(cls, location, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        actor.set_actor_label(label)
        log("spawned %s" % label)
    else:
        actor.set_actor_location(location, False, True)
        actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), True)
        log("updated %s" % label)
    actor.set_folder_path(FOLDER)
    return actor


def is_landscape(actor):
    return isinstance(actor, unreal.LandscapeProxy)


def ground_z(x, y):
    """Landscape height under (x, y): traces down and steps through placeholders and props standing on it."""
    ignore = []
    for _ in range(8):
        hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 30000.0), unreal.Vector(x, y, -10000.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        fields = hit.to_tuple()
        if not fields[0]:
            break
        actor = fields[9]
        if actor is None or is_landscape(actor):
            return fields[4].z
        ignore.append(actor)
    log("WARNING: no landscape under (%.0f, %.0f); using 0" % (x, y))
    return 0.0


def ground(x, y, lift=50.0):
    return unreal.Vector(x, y, ground_z(x, y) + lift)


# ---- road ---------------------------------------------------------------------------------------------------------

def road_samples(step=200.0):
    road = find_by_label(ROAD_LABEL)
    spline = road.get_component_by_class(unreal.SplineComponent) if road is not None else None
    if spline is None:
        log("WARNING: %s not found; using the baked road points" % ROAD_LABEL)
        pts = []
        for (ax, ay), (bx, by) in zip(ROAD_FALLBACK[:-1], ROAD_FALLBACK[1:]):
            length = ((bx - ax) ** 2 + (by - ay) ** 2) ** 0.5
            n = max(1, int(length / step))
            for k in range(n):
                t = float(k) / n
                pts.append((ax + (bx - ax) * t, ay + (by - ay) * t))
        pts.append(ROAD_FALLBACK[-1])
        return pts
    length = spline.get_spline_length()
    pts = []
    d = 0.0
    while d < length:
        v = spline.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD)
        pts.append((v.x, v.y))
        d += step
    v = spline.get_location_at_distance_along_spline(length, unreal.SplineCoordinateSpace.WORLD)
    pts.append((v.x, v.y))
    return pts


def nearest_index(samples, xy):
    best, best_d = 0, None
    for i, (x, y) in enumerate(samples):
        d = (x - xy[0]) ** 2 + (y - xy[1]) ** 2
        if best_d is None or d < best_d:
            best, best_d = i, d
    return best


def road_point(samples, xy):
    x, y = samples[nearest_index(samples, xy)]
    return ground(x, y)


def route_world_points(samples):
    i0, i1 = nearest_index(samples, START_XY), nearest_index(samples, EXIT_XY)
    step = max(1, int(ROUTE_SPACING / 200.0))
    indices = list(range(i0, i1, step if i1 > i0 else -step))
    if indices[-1] != i1:
        indices.append(i1)
    return [ground(*samples[i]) for i in indices]


def relative(origin, points):
    return [unreal.Vector(p.x - origin.x, p.y - origin.y, p.z - origin.z) for p in points]


# ---- settings helpers ----------------------------------------------------------------------------------------------

def set_struct(actor, prop, **values):
    struct = actor.get_editor_property(prop)
    for key, value in values.items():
        struct.set_editor_property(key, value)
    actor.set_editor_property(prop, struct)
    return struct


def squad_unit(subtype):
    return unreal.EnemyPlacementUnit(kind=unreal.EnemyPlacementUnitKind.SQUAD, squad_subtype=subtype)


# ---- placement -----------------------------------------------------------------------------------------------------

def place_routes(samples, hq_marker, radio_mast):
    points = route_world_points(samples)
    origin = points[0]
    routes = []
    stop_world = road_point(samples, WEIGHBRIDGE_STOP_XY)
    ambush_world = [road_point(samples, AMBUSH_CORRIDOR_XY), road_point(samples, VIADUCT_GAP_XY)]
    dismount_world = road_point(samples, AMBUSH_CORRIDOR_XY)
    for n in range(1, 7):
        label = "ORE_Convoy%d" % n
        route = find_or_spawn(label, unreal.EnemyConvoyRoute, origin)
        route.set_editor_property("route_id", label)
        route.set_editor_property("definition", unreal.load_asset(DATA + "DA_Convoy_Ore_%d" % n))
        route.set_editor_property("route_points", relative(origin, points))
        route.set_editor_property("enemy_player", 2)
        # Route 1 waits longer (owner 2026-11-24): the player must reach the ambush spot before the tutorial convoy.
        # Route 3 too (same day, intro tours): its column sets off after the light counterattack's tour.
        set_struct(route, "timing", start_on_begin_play=False, first_convoy_delay=FIRST_CONVOY_DELAY.get(n, 20.0),
                   repeat=unreal.EnemyWaveSpawnerRepeat.ONCE)
        set_struct(route, "ownership", has_owning_building=False)   # Base A is loot only: never the owner
        stop = route.get_editor_property("stop")
        stop.set_editor_property("enabled", n == 2)
        if n == 2:
            stop.set_editor_property("stop_seconds", 60.0)
            route.set_editor_property("stop_point", relative(origin, [stop_world])[0])
        route.set_editor_property("stop", stop)
        branch = route.get_editor_property("branch")
        branch.set_editor_property("enabled", n == 3)
        if n == 3:
            branch.set_editor_property("cargo_type", unreal.ConvoyCargoType.AMMO)
            branch.set_editor_property("destination_actor", None)
            route.set_editor_property("branch_points", relative(origin, branch_world_points(samples)))
        route.set_editor_property("branch", branch)
        hunters = route.get_editor_property("hunters")
        hunters.set_editor_property("enabled", n == 4)
        if n == 4:
            hunters.set_editor_property("squads_per_truck", [squad_unit(unreal.SquadSubtype.SQUAD_RUS_MOSIN)])
            hunters.set_editor_property("per_truck_squads", [
                unreal.EnemyConvoyRouteHunterTruckLoad(squads=[squad_unit(unreal.SquadSubtype.SQUAD_RUS_MOSIN)]),
                unreal.EnemyConvoyRouteHunterTruckLoad(squads=[squad_unit(unreal.SquadSubtype.SQUAD_RUS_OKHOTNIK)])])
            hunters.set_editor_property("attack_target", hq_marker)
            route.set_editor_property("dismount_point", relative(origin, [dismount_world])[0])
            route.set_editor_property("hunter_waypoints", relative(origin, [ground(x, y) for x, y in HUNTER_WAYPOINTS_XY]))
        route.set_editor_property("hunters", hunters)
        ambush = route.get_editor_property("ambush")
        ambush.set_editor_property("radio_post_actor", radio_mast if n >= 4 else None)
        route.set_editor_property("ambush", ambush)
        route.set_editor_property("ambush_zone_points", relative(origin, ambush_world) if n >= 4 else [])
        routes.append(route)
    return routes


def branch_world_points(samples):
    return [road_point(samples, FORK_XY)] + [ground(x, y) for x, y in BRANCH_VIA] + [ground(*BRANCH_END_XY)]


def place_battery():
    battery = find_or_spawn("ORE_Battery", unreal.EnemyArtilleryBattery, ground(*BATTERY_XY, lift=0.0))
    battery.set_editor_property("battery_id", "ORE_Battery")
    battery.set_editor_property("enemy_player", 2)
    set_struct(battery, "pits", auto_collect_pits=True, auto_collect_radius_cm=3500.0, minimum_calibre_mm=20.0)
    return battery


def place_slots():
    slots = []
    for suffix, bp_path, x, y, yaw in DORMANT_SLOTS:
        location = ground(x, y, lift=0.0)
        if abs(location.z - KNOLL_TOP_Z) > 300.0:
            log("WARNING: dormant slot %s at (%.0f, %.0f) is at z %.0f, not on the knoll top (~%.0f)"
                % (suffix, x, y, location.z, KNOLL_TOP_Z))
        slot = find_or_spawn("ORE_BaseB_DormantSlot_" + suffix, unreal.DormantDefenceSlot, location, yaw)
        slot.set_editor_property("slot_id", "ORE_BaseB_Dormant_" + suffix)
        slot.set_editor_property("enemy_player", 2)
        cls = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
        if cls is None:
            log("WARNING: no defence Blueprint " + bp_path)
        slot.set_editor_property("defence_class", cls)
        slots.append(slot)
    return slots


def place_director(routes, slots):
    director = find_or_spawn("ORE_PressureDirector", unreal.ConvoyPressureDirector, ground(*DIRECTOR_XY, lift=0.0))
    director.set_editor_property("enemy_player", 2)
    director.set_editor_property("watched_routes", routes)
    # Owner: fill the tier responses with wave spawners (tier 1 patrols, tier 3 ridge raids) and the tier 2
    # targeted attack on the 2 spoil-heap stations; the names say so in the details panel.
    director.set_editor_property("tier_responses", [
        unreal.ConvoyPressureTierResponse(tier_name="Tier1_Patrols_TODO_AddWaveSpawners"),
        unreal.ConvoyPressureTierResponse(tier_name="Tier2_RetakeStations_TODO_TargetedAttack"),
        unreal.ConvoyPressureTierResponse(tier_name="Tier3_RidgeRaids_TODO_AddWaveSpawners")])
    # array elements come back as copies: rebuild the list with the edited Concrete entry
    responses = []
    has_concrete = False
    for response in director.get_editor_property("cargo_responses"):
        if response.get_editor_property("cargo_type") == unreal.ConvoyCargoType.CONCRETE:
            response.set_editor_property("dormant_defences", slots)
            if response.get_editor_property("dormant_defences_per_escaped_truck") < 1:
                response.set_editor_property("dormant_defences_per_escaped_truck", 1)
            has_concrete = True
        responses.append(response)
    if not has_concrete:
        responses.append(unreal.ConvoyPressureCargoResponse(cargo_type=unreal.ConvoyCargoType.CONCRETE,
                                                            dormant_defences=slots, dormant_defences_per_escaped_truck=1))
    director.set_editor_property("cargo_responses", responses)
    concrete = [r for r in director.get_editor_property("cargo_responses")
                if r.get_editor_property("cargo_type") == unreal.ConvoyCargoType.CONCRETE]
    log("director: %d watched routes, Concrete response holds %d dormant slots"
        % (len(director.get_editor_property("watched_routes")),
           len(concrete[0].get_editor_property("dormant_defences")) if concrete else 0))
    return director


# ---- checks --------------------------------------------------------------------------------------------------------

def nav_ready():
    nav = unreal.NavigationSystemV1.get_navigation_system(world())
    if nav is None:
        log("PROBLEM: no navigation system in this world; drive paths not checked")
        return False
    if nav.is_navigation_being_built(world()):
        log("navigation is being built: running the blocking RebuildNavigation first")
        unreal.SystemLibrary.execute_console_command(world(), "RebuildNavigation")
    return True


def check_path(context, points, name, problems):
    route, reached = unreal.ConvoyRouteLibrary.build_navigable_route(context, points, 800.0, name)
    count = len(route.get_editor_property("points"))
    length = unreal.ConvoyRouteLibrary.get_route_length(route) if count > 1 else 0.0
    straight = 0.0
    for a, b in zip(points[:-1], points[1:]):
        straight += ((b.x - a.x) ** 2 + (b.y - a.y) ** 2) ** 0.5
    log("drive path %s: reached=%s points=%d length=%.0f m (control polyline %.0f m)"
        % (name, reached, count, length / 100.0, straight / 100.0))
    if not reached or count < 2:
        problems.append("%s: the drive path does not reach its end" % name)
    elif length > straight * 1.35 + 5000.0:
        problems.append("%s: the drive path is %.0f m for a %.0f m polyline (detour)" % (name, length / 100.0, straight / 100.0))


def check_routes(routes, samples):
    problems = []
    if not nav_ready():
        return ["navigation missing"]
    for route in routes:
        origin = route.get_actor_location()
        points = [unreal.Vector(origin.x + p.x, origin.y + p.y, origin.z + p.z) for p in route.get_editor_property("route_points")]
        check_path(route, points, route.get_actor_label(), problems)
        try:
            route.call_method("PreviewDrivePath")
        except Exception as error:
            log("PreviewDrivePath not called: %s" % error)
    check_path(routes[2], branch_world_points(samples), "ORE_Convoy3 ammo branch", problems)
    return problems


def run():
    del REPORT[:]
    require_map()
    samples = road_samples()
    log("road: %d samples" % len(samples))
    hq_marker = find_or_spawn("ORE_HQ_AttackMarker", unreal.TargetPoint, ground(HQ_MARKER_XYZ[0], HQ_MARKER_XYZ[1]))
    radio_mast = find_by_label(RADIO_MAST_LABEL)
    if radio_mast is None:
        log("NOTE: %s not in the map yet; Ambush.RadioPostActor stays empty on routes 4-6 (run again once Base B "
            "is placed, or set it by hand)" % RADIO_MAST_LABEL)
    routes = place_routes(samples, hq_marker, radio_mast)
    battery = place_battery()
    slots = place_slots()
    director = place_director(routes, slots)
    problems = check_routes(routes, samples)
    for problem in problems:
        log("PROBLEM: " + problem)
    log("done: %d routes, battery %s, director %s, %d dormant slots, %d problems (not saved)"
        % (len(routes), battery.get_actor_label(), director.get_actor_label(), len(slots), len(problems)))
    return REPORT
