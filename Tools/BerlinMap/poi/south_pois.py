"""Two exploration POIs in the south-central ruin field of Ger_Berlin (owner request 2026-11-26):

* Flak position (FLAK_CENTRE): ground cleared round it, two crewless Flak 88 team weapons (bStartAbandoned, re-crewed
  by any infantry squad with enough men), sandbags and ammo boxes, and three scavengeable tank wrecks (vehicle parts).
* Downed "Ju 52" (DC-3 crash mesh stand-in, owner pick) in the oval mud crater (JU52_CENTRE): hull, scattered plane
  parts, scavengeable cargo crates. The survivor squad comes from the stage Mission_Berlin_Ju52Crash (player near).

Labels start with SPOI_ (not POI_: berlin_map_build.step_poi() deletes every POI_ actor). Outliner folders
Mission/POI_FlakPosition and Mission/POI_Ju52Crash.

Runs in the editor that has Ger_Berlin open (throwaway editor via berlin_cmd.py, or UECP execute_python):
    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi")
    import south_pois; importlib.reload(south_pois)
    south_pois.plan()      # what the clearing would remove
    south_pois.apply()     # clear + place, one undoable transaction, logs everything (Saved/BerlinSouthPOIs)
    south_pois.check()     # what stands where, nav on the crater floor
Rerunning apply() first removes the SPOI_ actors it placed before.
"""
import datetime
import json
import math
import os

import unreal

FLAK_CENTRE = (-12500.0, -3000.0)
JU52_CENTRE = (-16700.0, 4000.0)
# zone centre, radius (cm): ruins, heaps, furniture, smoulder FX and rubble instances inside are removed
CLEAR_ZONES = {"flak": (FLAK_CENTRE, 2200.0), "ju52": (JU52_CENTRE, 1600.0)}
OUT_DIR = "D:/UE5Projects/RTS_Survival/Saved/BerlinSouthPOIs"
PREFIX = "SPOI_"
REMOVE_PREFIXES = ("RUIN_", "BER_HeapCover_", "FOOT_HeapCover_", "BER_Cover_", "FURN_", "BP_BerlinFurn_",
                   "ATM_Smoulder_", "BER_Ruin", "DET_B_")
KEEP_PREFIXES = ("RES_", "FCRATER_", "RY_", "FORGE_", "DECAL_", "RUBDECAL_", "AMB_", "ATM_Sheet", "SKIRT_", "POI_",
                 PREFIX)
INSTANCE_ACTOR_PREFIXES = ("BER_ISM_", "FOOT_", "DET_I_")

BP = "/Game/RTS_Survival/Blueprints/"
FLAK88 = BP + "Squads/TeamWeapons/Flak88Wheels/Ger_TW_Flak88.Ger_TW_Flak88_C"
FLAK88_CREW = BP + "Squads/TeamWeapons/Flak88Wheels/Ger_SquadTW_Flak88.Ger_SquadTW_Flak88_C"
WRECK = BP + "Environment/Berlin/PowerStation/ScavObj_Berlin_%s_Parts.ScavObj_Berlin_%s_Parts_C"
CRATE = BP + "Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts%d.ScavObj_VehicleParts%d_C"
DC3 = BP + "Environment/ScavengeObjects/CrashedPlanes/DC3/SM_RTS_CrashedPlane_DC3"
PLANE_PART = "/Game/RTS_Survival/Meshes/Wrecks/PlaneParts/SM_RTS_PlaneWreck_%d"
SANDBAG = "/Game/RTS_Survival/Meshes/Props/SandBags/LooseSandBags/SM_Ger_LooseSandBag_%d"
AMMO_LONG = "/Game/RTS_Survival/Meshes/Props/Ammo/SM_Ger_Ammobox_Long"
AMMO = "/Game/RTS_Survival/Meshes/Props/Ammo/SM_Ger_Ammobox"
CRATES_MESH = "/Game/RTS_Survival/Meshes/Props/Crates/CrateCollection/SM_CrateCollection1"

# Flak position, offsets from FLAK_CENTRE (cm) and yaw: the guns point north, at the Forge crater.
FLAK_GUNS = [("SPOI_Flak88_A", (500, -800), 8.0), ("SPOI_Flak88_B", (500, 800), -8.0)]
FLAK_WRECKS = [("SPOI_FlakWreck_PzIV", "PZIV", (-1400, -1400), 40.0, -30.0),
               ("SPOI_FlakWreck_Stug", "Stug", (1400, 1500), -130.0, -30.0),
               ("SPOI_FlakWreck_Pz38t", "Pz38T", (-1500, 1400), 170.0, -28.0)]
# Ju 52 cargo on the crater's west rim (world x, y) and yaw; reachable from the flak side.
JU52_CARGO = [(1, (-17650, 2500), 20.0), (2, (-17150, 2300), -35.0), (3, (-18150, 2800), 75.0)]
JU52_CRATE_STACK = (-17450, 2000)
# static dressing round each gun: mesh, offset from the gun in the gun's frame, yaw offset, sink (cm), scale.
# A low arc of loose sandbags in front (north), two ammo boxes behind (the box meshes are 1.7 x 2.2-3.7 m: halved).
GUN_DRESS = [(SANDBAG % 3, (420, -330), 25.0, -4.0, 1.3), (SANDBAG % 1, (460, -110), 5.0, -4.0, 1.3),
             (SANDBAG % 4, (460, 110), -10.0, -4.0, 1.3), (SANDBAG % 2, (420, 330), -30.0, -4.0, 1.3),
             (SANDBAG % 5, (330, 480), -60.0, -4.0, 1.3),
             (AMMO_LONG, (-300, -260), 90.0, 0.0, 0.5), (AMMO, (-330, -40), 15.0, 0.0, 0.5)]


def _eas():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _ground(x, y):
    """Landscape height under (x, y), stepping through anything else."""
    ignore = []
    for _ in range(16):
        hit = unreal.SystemLibrary.line_trace_single(_world(), unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        ignore.append(t[9])
    return 0.0


def _in_zone(x, y):
    for name, ((cx, cy), r) in CLEAR_ZONES.items():
        if math.hypot(x - cx, y - cy) < r:
            return name
    return None


def _targets():
    actors, instances = [], []
    for a in _eas().get_all_level_actors():
        label = a.get_actor_label()
        if label.startswith(KEEP_PREFIXES):
            continue
        if label.startswith(INSTANCE_ACTOR_PREFIXES) and a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            for comp in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
                for i in range(comp.get_instance_count()):
                    t = comp.get_instance_transform(i, True)
                    if _in_zone(t.translation.x, t.translation.y):
                        instances.append((a, comp, i, t))
            continue
        if label.startswith(REMOVE_PREFIXES):
            loc = a.get_actor_location()
            if _in_zone(loc.x, loc.y):
                actors.append(a)
    return actors, instances


def plan():
    actors, instances = _targets()
    out = dict(actors=sorted((_in_zone(a.get_actor_location().x, a.get_actor_location().y), a.get_actor_label())
                             for a in actors), instances={})
    for a, comp, i, t in instances:
        key = "%s | %s" % (a.get_actor_label(), comp.get_editor_property("static_mesh").get_name())
        out["instances"][key] = out["instances"].get(key, 0) + 1
    print(json.dumps(out, indent=1))
    return out


def _rot2(dx, dy, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return dx * c - dy * s, dx * s + dy * c


def _seat_mesh_z(mesh, x, y, yaw, sink, scale=1.0, pitch=0.0, roll=0.0):
    """Actor z so the mesh's own bounding box bottom sits on the lowest landscape point under its footprint (plus
    sink): on a slope the uphill side digs in instead of the downhill side floating. Never by actor bounds."""
    bb = mesh.get_bounding_box()
    tilt = math.radians(max(abs(pitch), abs(roll)))
    half = max(bb.max.x - bb.min.x, bb.max.y - bb.min.y) * 0.5 * scale
    heights = []
    for fx in (0.15, 0.5, 0.85):
        for fy in (0.15, 0.5, 0.85):
            lx = (bb.min.x + (bb.max.x - bb.min.x) * fx) * scale
            ly = (bb.min.y + (bb.max.y - bb.min.y) * fy) * scale
            ox, oy = _rot2(lx, ly, yaw)
            heights.append(_ground(x + ox, y + oy))
    return min(heights) - bb.min.z * scale - half * math.sin(tilt) * 0.5 + sink


def _spawn_mesh(label, folder, path, x, y, yaw, sink=0.0, scale=1.0, pitch=0.0, roll=0.0, centre_pivot=False):
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError("missing mesh " + path)
    if centre_pivot:
        bb = mesh.get_bounding_box()
        cx, cy = _rot2((bb.max.x + bb.min.x) * 0.5 * scale, (bb.max.y + bb.min.y) * 0.5 * scale, yaw)
        x, y = x - cx, y - cy
    z = _seat_mesh_z(mesh, x, y, yaw, sink, scale, pitch, roll)
    actor = _eas().spawn_actor_from_object(mesh, unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    smc = actor.get_component_by_class(unreal.StaticMeshComponent)
    smc.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    return actor


def _spawn_class(label, folder, path, x, y, yaw, z_offset=0.0):
    cls = unreal.load_class(None, path)
    if cls is None:
        raise RuntimeError("missing class " + path)
    actor = _eas().spawn_actor_from_class(cls, unreal.Vector(x, y, _ground(x, y) + z_offset),
                                          unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def _crater_floor_axis(cx, cy, radius=2600.0, step=100.0, depth=-500.0):
    """Centroid and long-axis yaw of the crater floor (landscape below depth cm) round (cx, cy)."""
    pts = []
    n = int(radius // step)
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            x, y = cx + i * step, cy + j * step
            if math.hypot(x - cx, y - cy) <= radius and _ground(x, y) < depth:
                pts.append((x, y))
    if len(pts) < 10:
        return cx, cy, 0.0, 0
    mx = sum(p[0] for p in pts) / len(pts)
    my = sum(p[1] for p in pts) / len(pts)
    sxx = sum((p[0] - mx) ** 2 for p in pts)
    syy = sum((p[1] - my) ** 2 for p in pts)
    sxy = sum((p[0] - mx) * (p[1] - my) for p in pts)
    yaw = math.degrees(0.5 * math.atan2(2 * sxy, sxx - syy))
    return mx, my, yaw, len(pts)


def _remove_previous():
    gone = 0
    for a in _eas().get_all_level_actors():
        if a.get_actor_label().startswith(PREFIX):
            _eas().destroy_actor(a)
            gone += 1
    return gone


def _clear(log):
    actors, instances = _targets()
    for a in actors:
        loc, rot = a.get_actor_location(), a.get_actor_rotation()
        log["removed_actors"].append(dict(label=a.get_actor_label(), cls=a.get_class().get_path_name(),
                                          loc=[loc.x, loc.y, loc.z], rot=[rot.roll, rot.pitch, rot.yaw],
                                          scale=[a.get_actor_scale3d().x, a.get_actor_scale3d().y,
                                                 a.get_actor_scale3d().z], folder=str(a.get_folder_path())))
    for a, comp, i, t in instances:
        log["removed_instances"].append(dict(actor=a.get_actor_label(), comp=comp.get_name(),
                                             mesh=comp.get_editor_property("static_mesh").get_path_name(), index=i,
                                             loc=[t.translation.x, t.translation.y, t.translation.z],
                                             rot=[t.rotation.rotator().roll, t.rotation.rotator().pitch,
                                                  t.rotation.rotator().yaw],
                                             scale=[t.scale3d.x, t.scale3d.y, t.scale3d.z]))
    by_comp = {}
    for a, comp, i, t in instances:
        by_comp.setdefault(comp, (a, []))[1].append(i)
    for comp, (a, idx) in by_comp.items():
        a.modify()
        comp.modify()
        for i in sorted(idx, reverse=True):
            comp.remove_instance(i)
    for a in actors:
        _eas().destroy_actor(a)


def _place_flak(log):
    folder = "Mission/POI_FlakPosition"
    crew = unreal.load_class(None, FLAK88_CREW)
    fx, fy = FLAK_CENTRE
    for label, (dx, dy), yaw in FLAK_GUNS:
        gx, gy = fx + dx, fy + dy
        gun = _spawn_class(label, folder, FLAK88, gx, gy, yaw, 0.0)
        gun.set_editor_property("start_abandoned", True)
        gun.set_editor_property("start_abandoned_crew_class", crew)
        log["placed"].append(label)
        for k, (path, (ox, oy), dyaw, sink, scale) in enumerate(GUN_DRESS):
            rx, ry = _rot2(ox, oy, yaw)
            name = "%s_Dress%d" % (label, k)
            _spawn_mesh(name, folder, path, gx + rx, gy + ry, yaw + dyaw, sink, scale)
            log["placed"].append(name)
    for label, kind, (dx, dy), yaw, zoff in FLAK_WRECKS:
        _spawn_class(label, folder, WRECK % (kind, kind), fx + dx, fy + dy, yaw, zoff)
        log["placed"].append(label)


def _place_ju52(log):
    folder = "Mission/POI_Ju52Crash"
    cx, cy, axis, n = _crater_floor_axis(*JU52_CENTRE)
    log["crater_floor"] = dict(centre=[cx, cy], axis_yaw=axis, samples=n)
    # The hull lies along the crater, nose dug into the north-east slope, slight roll as it broke its back.
    hull_yaw = axis + 8.0
    _spawn_mesh("SPOI_Ju52_Hull", folder, DC3, cx, cy, hull_yaw, sink=-70.0, roll=7.0, centre_pivot=True)
    log["placed"].append("SPOI_Ju52_Hull")
    # wreck parts: part number, offset along / across the hull axis (cm), yaw offset, sink
    parts = [(1, (-1500, -900), 35.0, -20.0), (4, (1300, 1000), -60.0, -10.0), (6, (-900, 1300), 110.0, -10.0),
             (8, (600, -1500), 15.0, -15.0), (2, (-300, 1700), 70.0, -5.0), (5, (1700, -400), 0.0, -5.0)]
    for k, (num, (along, across), dyaw, sink) in enumerate(parts):
        ox, oy = _rot2(along, across, hull_yaw)
        name = "SPOI_Ju52_Part%d" % k
        _spawn_mesh(name, folder, PLANE_PART % num, cx + ox, cy + oy, hull_yaw + dyaw, sink)
        log["placed"].append(name)
    # Cargo thrown out over the west rim on impact: scavengeable crates (metal + vehicle parts) and a dressing stack.
    # On the rim, not the floor: the 8 m deep crater floor is a navmesh island no unit can reach (checked 2026-11-26).
    for num, (x, y), yaw in JU52_CARGO:
        name = "SPOI_Ju52_Cargo%d" % num
        _spawn_class(name, folder, CRATE % (num, num), x, y, yaw, -5.0)
        log["placed"].append(name)
    _spawn_mesh("SPOI_Ju52_CrateStack", folder, CRATES_MESH, JU52_CRATE_STACK[0], JU52_CRATE_STACK[1], 40.0, -8.0)
    log["placed"].append("SPOI_Ju52_CrateStack")


def apply():
    os.makedirs(OUT_DIR, exist_ok=True)
    log = {"zones": {k: [v[0], v[1]] for k, v in CLEAR_ZONES.items()}, "removed_actors": [],
           "removed_instances": [], "placed": []}
    with unreal.ScopedEditorTransaction("South POIs: flak position + Ju 52 crash"):
        log["previous_spoi_removed"] = _remove_previous()
        _clear(log)
        _place_flak(log)
        _place_ju52(log)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    with open(os.path.join(OUT_DIR, "apply_%s.json" % stamp), "w") as f:
        json.dump(log, f, indent=1)
    print("removed %d actors, %d instances; placed %d; crater %s; log apply_%s.json" % (
        len(log["removed_actors"]), len(log["removed_instances"]), len(log["placed"]), log.get("crater_floor"), stamp))
    return log


def check():
    """Placed SPOI_ actors with their ground offset, and nav reach to the crates / guns from the flak position."""
    w = _world()
    rows = []
    for a in _eas().get_all_level_actors():
        if not a.get_actor_label().startswith(PREFIX):
            continue
        p = a.get_actor_location()
        rows.append((a.get_actor_label(), round(p.x), round(p.y), round(p.z - _ground(p.x, p.y))))
    for r in sorted(rows):
        print("%-28s (%6d,%6d) z-ground %5d" % r)
    navs = {n.get_name().replace("RecastNavMesh-", ""): n
            for n in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh)}
    ns = unreal.NavigationSystemV1
    start = unreal.Vector(FLAK_CENTRE[0] + 2000, FLAK_CENTRE[1] + 3000, 0)
    for label in ("SPOI_Ju52_Cargo1", "SPOI_Ju52_Cargo2", "SPOI_Ju52_Cargo3", "SPOI_Flak88_A", "SPOI_Flak88_B",
                  "SPOI_FlakWreck_PzIV", "SPOI_FlakWreck_Stug", "SPOI_FlakWreck_Pz38t"):
        target = next((a for a in _eas().get_all_level_actors() if a.get_actor_label() == label), None)
        if target is None:
            print(label, "missing")
            continue
        for agent in ("Character", "MediumTank"):
            nav = navs.get(agent)
            a = ns.project_point_to_navigation(w, start, nav, None, unreal.Vector(600, 600, 2000))
            b = ns.project_point_to_navigation(w, target.get_actor_location(), nav, None, unreal.Vector(900, 900, 2000))
            if a is None or b is None or (b.x == 0 and b.y == 0):
                print("%-24s %-10s no projection" % (label, agent))
                continue
            path = ns.find_path_to_location_synchronously(w, a, b, nav, None)
            pts = list(path.path_points) if path else []
            end_miss = math.hypot(pts[-1].x - b.x, pts[-1].y - b.y) / 100 if pts else -1
            print("%-24s %-10s path %s partial=%s end_miss=%.1fm" % (label, agent, bool(pts),
                                                                      path.is_partial() if path else None, end_miss))
    return rows
