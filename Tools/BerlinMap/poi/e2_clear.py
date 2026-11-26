"""Clears building room round the E2 "Rosenthaler Trichter" Radixite field on Ger_Berlin (owner request 2026-11-26:
"clear up the area around the radixite field so that the player can place buildings around it").

Runs in the editor that has Ger_Berlin open (UECP execute_python or a throwaway editor):
    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi")
    import e2_clear; importlib.reload(e2_clear)
    e2_clear.plan()          # dry run: what would go
    e2_clear.apply()         # one undoable transaction; logs every removal with its transform
    e2_clear.check()         # blockers and ground slope left in the cleared ring

Inside CLEAR_RADIUS of the field centre it removes ruin shells, cover heaps, footprint plates, furniture, loose rubble
instances and the ambient wisps / crows that sat on them. It keeps the field (RES_Extra_E2_*), its crater rim
(FCRATER_E2_*), decals, ambient litter and anything else not listed in REMOVE_PREFIXES.
"""
import datetime
import json
import math
import os

import unreal

CENTRE = (19650.0, 16850.0)          # E2 field centre (extra_radixite.py)
CLEAR_RADIUS = 2800.0                # cm: field crystals reach ~11 m, so this leaves a ~16 m free ring
OUT_DIR = "D:/UE5Projects/RTS_Survival/Saved/BerlinE2Clear"
REMOVE_PREFIXES = ("RUIN_", "BER_HeapCover_", "FOOT_HeapCover_", "BER_Cover_", "FURN_", "BP_BerlinFurn_",
                   "ATM_Smoulder_", "BER_Ruin")
KEEP_PREFIXES = ("RES_", "FCRATER_", "RY_", "FORGE_", "DECAL_", "RUBDECAL_", "AMB_", "ATM_Sheet", "SKIRT_")
INSTANCE_ACTOR_PREFIXES = ("BER_ISM_", "FOOT_")     # instanced rubble / footprint plates: remove single instances


def _eas():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _dist(x, y):
    return math.hypot(x - CENTRE[0], y - CENTRE[1])


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
                    if _dist(t.translation.x, t.translation.y) < CLEAR_RADIUS:
                        instances.append((a, comp, i, t))
            continue
        if label.startswith(REMOVE_PREFIXES):
            loc = a.get_actor_location()
            if _dist(loc.x, loc.y) < CLEAR_RADIUS:
                actors.append(a)
    return actors, instances


def plan():
    actors, instances = _targets()
    out = dict(actors=sorted((round(_dist(a.get_actor_location().x, a.get_actor_location().y) / 100.0, 1),
                              a.get_actor_label()) for a in actors),
               instances={})
    for a, comp, i, t in instances:
        key = "%s | %s" % (a.get_actor_label(), comp.get_editor_property("static_mesh").get_name())
        out["instances"][key] = out["instances"].get(key, 0) + 1
    print(json.dumps(out, indent=1))
    return out


def apply():
    actors, instances = _targets()
    os.makedirs(OUT_DIR, exist_ok=True)
    log = {"centre": CENTRE, "radius": CLEAR_RADIUS, "actors": [], "instances": []}
    for a in actors:
        loc, rot = a.get_actor_location(), a.get_actor_rotation()
        log["actors"].append(dict(label=a.get_actor_label(), cls=a.get_class().get_path_name(),
                                  loc=[loc.x, loc.y, loc.z], rot=[rot.roll, rot.pitch, rot.yaw],
                                  scale=[a.get_actor_scale3d().x, a.get_actor_scale3d().y, a.get_actor_scale3d().z],
                                  folder=str(a.get_folder_path())))
    for a, comp, i, t in instances:
        log["instances"].append(dict(actor=a.get_actor_label(), comp=comp.get_name(),
                                     mesh=comp.get_editor_property("static_mesh").get_path_name(), index=i,
                                     loc=[t.translation.x, t.translation.y, t.translation.z],
                                     rot=[t.rotation.rotator().roll, t.rotation.rotator().pitch, t.rotation.rotator().yaw],
                                     scale=[t.scale3d.x, t.scale3d.y, t.scale3d.z]))
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    with open(os.path.join(OUT_DIR, "removed_%s.json" % stamp), "w") as f:
        json.dump(log, f, indent=1)
    with unreal.ScopedEditorTransaction("Clear building room round E2 Radixite field"):
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
    print("removed %d actors, %d instances; log removed_%s.json" % (len(actors), len(instances), stamp))
    return log


def _ground(world, x, y):
    ignore = []
    for _ in range(12):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None, ignore
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z, ignore
        ignore.append(t[9])
    return None, ignore


def check(step=200.0, inner=1300.0):
    """On a 2 m grid in the ring inner..CLEAR_RADIUS: what still stands above the landscape, and the ground slope."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    blockers = {}
    heights = {}
    n = int(CLEAR_RADIUS // step)
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            x, y = CENTRE[0] + i * step, CENTRE[1] + j * step
            d = _dist(x, y)
            if not (inner <= d < CLEAR_RADIUS):
                continue
            z, hits = _ground(world, x, y)
            heights[(i, j)] = z
            for h in hits:
                owner = h.get_owner() if hasattr(h, "get_owner") else h
                name = owner.get_actor_label() if owner and hasattr(owner, "get_actor_label") else str(h)
                blockers[name] = blockers.get(name, 0) + 1
    slopes = []
    for (i, j), z in heights.items():
        for di, dj in ((1, 0), (0, 1)):
            z2 = heights.get((i + di, j + dj))
            if z is not None and z2 is not None:
                slopes.append(abs(z2 - z) / step)
    slopes.sort()
    out = dict(samples=len(heights), blockers=blockers,
               slope_pct_median=round(100 * slopes[len(slopes) // 2], 1) if slopes else None,
               slope_pct_p95=round(100 * slopes[int(len(slopes) * 0.95)], 1) if slopes else None,
               height_range_cm=[round(min(v for v in heights.values() if v is not None)),
                                round(max(v for v in heights.values() if v is not None))])
    print(json.dumps(out, indent=1))
    return out
