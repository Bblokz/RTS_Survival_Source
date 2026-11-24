"""Ger_Berlin start-base resource fields, owner's pick 2026-11-18: concept A "Ostrand"
(Docs/Concepts/BerlinStartResources/v01, spec bsr_options.py OPTIONS["A"]).

Both fields on the east edge behind the base, built only from the existing resource Blueprints:
- Radixite: a glass-crater field round Resource_Radixite_Crater0 (the old bomb crater), no Growth variants (Berlin
  Radixite does not root or regrow).
- Metal: the wrecked tram depot as rails (tracks), a collapsed shed (Ruin1), the broken water tanks and a barrel row.

Labels RES_Start_* under the outliner folder Berlin/StartResources; the step clears its own prefix first, so it can be
rerun. No other build step touches the prefix (step_dress clears BER_, step_poi POI_).

    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi")
    import start_resources as sr; importlib.reload(sr)
    sr.check_sites()      # read-only: what stands in the footprints now
    sr.place()            # spawn (seated on the landscape by a pivot trace)
"""
import math

import unreal

PREFIX = "RES_Start_"
FOLDER = "Berlin/StartResources"
ROOT = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/"
RAD = ROOT + "Radixite/Resource_Radixite_"
MET = ROOT + "Metal/Resource_Metal_"

# (label, blueprint, x, y, yaw, footprint half size x/y in cm before yaw, for the overlap check)
CRATER = (17300.0, 47300.0)
PIECES = [
    ("Radixite_Crater", RAD + "Crater0", 17300, 47300, 0, (500, 500)),
    ("Radixite_Large", RAD + "Crystal_Large", 18250, 46400, 25, (300, 420)),
    ("Radixite_Point", RAD + "Crystal_LargePoint", 16500, 47900, 0, (120, 200)),
    ("Radixite_Crystal_1", RAD + "Crystal", 18050, 48150, 40, (200, 220)),
    ("Radixite_Crystal_2", RAD + "Crystal", 16450, 46700, 160, (200, 220)),
    ("Radixite_Crystal_3", RAD + "Crystal", 17300, 48450, 280, (200, 220)),
    ("Radixite_Small_1", RAD + "Crystal_SmallDecal", 18450, 47450, 70, (110, 160)),
    ("Radixite_Small_2", RAD + "Crystal_SmallDecal", 16150, 47350, 210, (110, 160)),
    ("Metal_DepotShed", MET + "Ruin1", 11300, 48100, 0, (400, 580)),
    ("Metal_Track_1", MET + "Rails", 12600, 47650, 0, (630, 110)),
    ("Metal_Track_2", MET + "Rails", 12600, 48100, 2, (630, 110)),
    ("Metal_Track_3", MET + "Rails", 12650, 48550, -3, (630, 110)),
    ("Metal_WaterTanks", MET + "BrokenWaterTank", 14150, 47900, 90, (820, 620)),
    ("Metal_Barrels", MET + "Barrels", 12600, 48950, 90, (80, 370)),
]
# footprints the pieces must stay inside (concept A polygons, checked against the streets)
CRATER_R = 2000.0      # crystals ring the Crater0 bowl; the Large one sits on the rim
DEPOT = (10700.0, 14800.0, 47000.0, 49100.0)


def _sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _corners(x, y, yaw, half):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return [(x + hx * c - hy * s, y + hx * s + hy * c) for hx, hy in
            ((half[0], half[1]), (half[0], -half[1]), (-half[0], -half[1]), (-half[0], half[1]))]


def _aabb(x, y, yaw, half):
    pts = _corners(x, y, yaw, half)
    return min(p[0] for p in pts), max(p[0] for p in pts), min(p[1] for p in pts), max(p[1] for p in pts)


def _trace(x, y):
    """[(z, actor label, actor class, component)] top to bottom."""
    hits = unreal.SystemLibrary.line_trace_multi(_world(), unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    out = []
    for h in hits or []:
        t = h.to_tuple()
        actor, comp = t[9], t[10]
        out.append((t[4].z, actor.get_actor_label() if actor else "?", actor.get_class().get_name() if actor else "?",
                    comp.get_name() if comp else "?"))
    return out


def ground_z(x, y):
    for z, label, cls, comp in _trace(x, y):
        if cls.startswith("Landscape"):
            return z
    raise RuntimeError("no landscape under %.0f %.0f" % (x, y))


def layout_problems():
    """Overlaps between pieces and pieces outside their field."""
    problems = []
    boxes = [(p[0], _aabb(p[2], p[3], p[4], p[5])) for p in PIECES]
    for i in range(len(boxes)):
        for j in range(i + 1, len(boxes)):
            a, b = boxes[i][1], boxes[j][1]
            if a[0] < b[1] and b[0] < a[1] and a[2] < b[3] and b[2] < a[3]:
                problems.append("overlap %s / %s" % (boxes[i][0], boxes[j][0]))
    for label, bp, x, y, yaw, half in PIECES:
        for cx, cy in _corners(x, y, yaw, half):
            if label.startswith("Radixite") and math.hypot(cx - CRATER[0], cy - CRATER[1]) > CRATER_R:
                problems.append("%s corner %.0f %.0f outside the crater field" % (label, cx, cy))
                break
            if label.startswith("Metal") and not (DEPOT[0] <= cx <= DEPOT[1] and DEPOT[2] <= cy <= DEPOT[3]):
                problems.append("%s corner %.0f %.0f outside the depot field" % (label, cx, cy))
                break
    return problems


def check_sites(step=100.0):
    """Read-only: every non-landscape thing in the two fields - trace grid (1 m) plus instanced meshes."""
    found = {}
    fields = [("crater", CRATER[0] - CRATER_R, CRATER[0] + CRATER_R, CRATER[1] - CRATER_R, CRATER[1] + CRATER_R),
              ("depot",) + DEPOT]
    for name, x0, x1, y0, y1 in fields:
        x = x0
        while x <= x1:
            y = y0
            while y <= y1:
                if name != "crater" or math.hypot(x - CRATER[0], y - CRATER[1]) <= CRATER_R:
                    gz = None
                    for z, label, cls, comp in _trace(x, y):
                        if cls.startswith("Landscape"):
                            gz = z
                    for z, label, cls, comp in _trace(x, y):
                        if not cls.startswith("Landscape") and (gz is None or z > gz + 5):
                            key = (name, label, cls, comp)
                            n, top = found.get(key, (0, -1e9))
                            found[key] = (n + 1, max(top, z - (gz or 0)))
                y += step
            x += step
    inst = []
    for actor in _sub().get_all_level_actors():
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            for k in range(comp.get_instance_count()):
                p = comp.get_instance_transform(k, True).translation
                for name, x0, x1, y0, y1 in fields:
                    if x0 - 300 <= p.x <= x1 + 300 and y0 - 300 <= p.y <= y1 + 300 and (
                            name != "crater" or math.hypot(p.x - CRATER[0], p.y - CRATER[1]) <= CRATER_R + 300):
                        inst.append((name, actor.get_actor_label(), comp.get_name(),
                                     comp.static_mesh.get_name() if comp.static_mesh else "?", round(p.x), round(p.y)))
    for key, (n, top) in sorted(found.items()):
        print("TRACE", key, "%d cells, top %.0f cm above ground" % (n, top))
    for row in inst:
        print("INSTANCE", row)
    for p in layout_problems():
        print("LAYOUT", p)
    print("check_sites: %d traced, %d instances, %d layout problems" % (len(found), len(inst), len(layout_problems())))
    return found, inst


def clear():
    n = 0
    for actor in _sub().get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            _sub().destroy_actor(actor)
            n += 1
    return n


def place():
    problems = layout_problems()
    if problems:
        raise RuntimeError("layout problems: %s" % problems)
    removed = clear()
    totals = {}
    for label, bp, x, y, yaw, half in PIECES:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(bp)
        if cls is None:
            raise RuntimeError("missing " + bp)
        z = ground_z(x, y)
        actor = _sub().spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label(PREFIX + label)
        actor.set_folder_path(FOLDER)
        rc = actor.get_component_by_class(unreal.ResourceComponent)
        kind = str(rc.get_editor_property("resource_type")).split(".")[-1].split(":")[0]
        totals[kind] = totals.get(kind, 0) + rc.get_editor_property("total_amount")
        print("placed %-28s %-36s at %6.0f %6.0f z %5.1f yaw %4.0f" % (PREFIX + label, bp.split("/")[-1], x, y, z, yaw))
    print("removed %d old, placed %d, totals %s" % (removed, len(PIECES), totals))
    return totals
