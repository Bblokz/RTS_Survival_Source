"""Checks how BP_FieldTrenchSpline ends join pit anchors and other spline trenches, on the open map.

Run in a full editor with the RTS_Survival module that has ARTSFieldTrenchSpline:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/TestCover/TestFieldTrench
    -ExecutePythonScript=<this file> -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended
Nothing is saved. Results go to Saved/Logs/FieldTrenchSpline_Joins.txt (PASS/FAIL per check).
"""
import math
import os
import unreal

TRENCH_BP = "/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/BP_FieldTrenchSpline"
PIT_BP = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Pak40"
PIT_SOCKET = "TrenchPort_Rear"
NOMINAL = {"125": 125.0, "250": 250.0, "500": 500.0}

lines = []
failures = []
actors = []
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def log(text):
    lines.append(str(text))
    unreal.log("[FieldTrenchJoins] " + str(text))


def check(name, condition, detail=""):
    log(("PASS " if condition else "FAIL ") + name + ("  (" + detail + ")" if detail else ""))
    if not condition:
        failures.append(name)


def ground(x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 50000.0), unreal.Vector(x, y, -50000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, actors, unreal.DrawDebugTrace.NONE, True)
    values = hit.to_tuple() if hit else None
    return values[4].z if values and values[0] else 0.0


def spawn(path, location, yaw=0.0):
    cls = unreal.load_asset(path).generated_class()
    actor = actor_subsystem.spawn_actor_from_class(cls, location, unreal.Rotator(0.0, 0.0, yaw))
    actors.append(actor)
    return actor


def spawn_trench(points):
    trench = spawn(TRENCH_BP, unreal.Vector(points[0][0], points[0][1], ground(points[0][0], points[0][1])))
    set_points(trench, points)
    return trench


def set_points(trench, points):
    spline = trench.get_editor_property("TrenchPath")
    spline.clear_spline_points(False)
    for x, y in points:
        spline.add_spline_point(unreal.Vector(x, y, ground(x, y)), unreal.SplineCoordinateSpace.WORLD, False)
    spline.update_spline()
    trench.call_method("RebuildTrench")


def pieces(actor):
    result = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        if component.get_name() == "PieceCollisionTemplate" or mesh is None:
            continue
        transform = component.get_world_transform()
        result.append((mesh.get_name(), transform))
    return result


def nearest_origin(actor, point):
    # The piece whose origin (its entry port) is nearest the point; component order is not the plan order.
    return min(pieces(actor), key=lambda p: dist2d(p[1].translation, point))


def nearest_exit(actor, point):
    straights = [p for p in pieces(actor) if exit_point(p[0], p[1]) is not None]
    return min(straights, key=lambda p: dist2d(exit_point(p[0], p[1]), point))


def count(actor, mesh_name):
    return len([1 for name, _ in pieces(actor) if name == mesh_name])


def exit_point(name, transform):
    nominal = next((v for k, v in NOMINAL.items() if k in name), None)
    if nominal is None:
        return None
    forward = transform.rotation.rotator().get_forward_vector()
    scale = transform.scale3d.x
    return transform.translation + forward * (nominal * scale)


def dist2d(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def yaw_of(transform):
    return transform.rotation.rotator().yaw


def join_kind(trench, which):
    join = trench.get_editor_property(which)
    return str(join.get_editor_property("Kind")), join.get_editor_property("Target")


def socket_of(pit):
    for component in pit.get_components_by_class(unreal.StaticMeshComponent):
        if component.does_socket_exist(PIT_SOCKET):
            return component.get_socket_transform(PIT_SOCKET, unreal.RelativeTransformSpace.RTS_WORLD)
    return None


def dump(label, actor):
    joins = []
    for which in ("StartJoin", "EndJoin"):
        kind, target = join_kind(actor, which)
        joins.append("%s=%s:%s" % (which, kind.split(".")[-1].split(":")[0], target.get_name() if target else "-"))
    log("  %s %s %s" % (label, actor.get_name(), " ".join(joins)))
    for name, t in pieces(actor):
        log("    %-32s (%7.1f, %7.1f, %6.1f) yaw %7.1f sx %.3f" % (
            name, t.translation.x, t.translation.y, t.translation.z, yaw_of(t), t.scale3d.x))


def angle_diff(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


def run():
    log("World " + world.get_path_name())

    # (a) A trench drawn from 1 m off a pit's rear socket starts on the socket.
    pit = spawn(PIT_BP, unreal.Vector(0.0, -6000.0, ground(0.0, -6000.0)))
    socket = socket_of(pit)
    check("pit has " + PIT_SOCKET, socket is not None)
    if socket is None:
        return
    out = socket.rotation.rotator().get_forward_vector()
    s = socket.translation
    log("socket at %s yaw %.1f" % (s, socket.rotation.rotator().yaw))
    t1 = spawn_trench([(s.x + out.x * 80 + out.y * 60, s.y + out.y * 80 - out.x * 60),
                       (s.x + out.x * 1500, s.y + out.y * 1500)])
    kind, target = join_kind(t1, "StartJoin")
    check("(a) start joins anchor", "ANCHOR" in kind.upper() and target == pit, kind)
    first = nearest_origin(t1, s)
    check("(a) no start cap", first[0] != "SM_FieldTrench_End", first[0])
    check("(a) first piece on socket", dist2d(first[1].translation, s) < 1.0,
          "%.2f cm" % dist2d(first[1].translation, s))
    check("(a) first piece along socket", angle_diff(yaw_of(first[1]), socket.rotation.rotator().yaw) < 0.5,
          "%.2f deg" % angle_diff(yaw_of(first[1]), socket.rotation.rotator().yaw))
    check("(a) one end cap", count(t1, "SM_FieldTrench_End") == 1, str(count(t1, "SM_FieldTrench_End")))

    # (b) Pit to pit: a second pit 13.4 m away, facing back; the trench closes onto its socket.
    offset = s - pit.get_actor_location()
    far_socket = s + out * 1340.0
    pit2_location = far_socket + unreal.Vector(offset.x, offset.y, 0.0)  # pit2 is turned round, so its offset flips
    pit2 = spawn(PIT_BP, unreal.Vector(pit2_location.x, pit2_location.y, ground(pit2_location.x, pit2_location.y)),
                 pit.get_actor_rotation().yaw + 180.0)
    socket2 = socket_of(pit2)
    s2 = socket2.translation
    log("second socket at %s yaw %.1f, %.0f cm from the first" % (s2, socket2.rotation.rotator().yaw, dist2d(s, s2)))
    actor_subsystem.destroy_actor(t1)
    actors.remove(t1)
    out2 = socket2.rotation.rotator().get_forward_vector()
    t2 = spawn_trench([(s.x + out.x * 60, s.y + out.y * 60), ((s.x + s2.x) / 2 + 150, (s.y + s2.y) / 2 + 150),
                       (s2.x + out2.x * 60, s2.y + out2.y * 60)])
    k_start, _ = join_kind(t2, "StartJoin")
    k_end, target_end = join_kind(t2, "EndJoin")
    check("(b) both ends join anchors", "ANCHOR" in k_start.upper() and "ANCHOR" in k_end.upper() and target_end == pit2,
          k_start + " / " + k_end)
    check("(b) no caps", count(t2, "SM_FieldTrench_End") == 0, str(count(t2, "SM_FieldTrench_End")))
    last = nearest_exit(t2, s2)
    end = exit_point(last[0], last[1])
    gap = dist2d(end, s2) if end else 9999.0
    check("(b) closes on second socket", gap < 2.0, "%s scale %.3f gap %.2f cm" % (last[0], last[1].scale3d.x, gap))
    check("(b) closing stretch in range", 0.5 <= last[1].scale3d.x <= 1.6, "%.3f" % last[1].scale3d.x)
    dump("(b)", t2)

    # (c) Trench to trench, end to end.
    a = spawn_trench([(3000.0, 3000.0), (4500.0, 3000.0)])
    b = spawn(TRENCH_BP, unreal.Vector(4650.0, 3100.0, ground(4650.0, 3100.0)))
    dump("(c) A after B spawned", a)
    dump("(c) B spawned", b)
    set_points(b, [(4650.0, 3100.0), (6000.0, 4200.0)])
    dump("(c) A after B set", a)
    dump("(c) B set", b)
    kind, target = join_kind(b, "StartJoin")
    check("(c) B start joins A end", "END" in kind.upper() and target == a, kind)
    check("(c) A keeps one cap", count(a, "SM_FieldTrench_End") == 1, str(count(a, "SM_FieldTrench_End")))
    check("(c) B keeps one cap", count(b, "SM_FieldTrench_End") == 1, str(count(b, "SM_FieldTrench_End")))
    b_start = b.get_editor_property("TrenchPath").get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD)
    a_last = nearest_exit(a, b_start)
    a_end = exit_point(a_last[0], a_last[1])
    b_first = nearest_origin(b, a_end)
    check("(c) B starts where A ends", a_end is not None and dist2d(a_end, b_first[1].translation) < 2.0,
          "%.2f cm" % (dist2d(a_end, b_first[1].translation) if a_end else -1))
    set_points(b, [(7000.0, 3100.0), (8000.0, 4200.0)])
    kind, _ = join_kind(b, "StartJoin")
    check("(c) moved away: no join", "NONE" in kind.upper(), kind)
    check("(c) moved away: A gets its cap back", count(a, "SM_FieldTrench_End") == 2, str(count(a, "SM_FieldTrench_End")))
    check("(c) moved away: B two caps", count(b, "SM_FieldTrench_End") == 2, str(count(b, "SM_FieldTrench_End")))
    # Joined again, then the target is edited: the joiner follows.
    set_points(b, [(4650.0, 3100.0), (6000.0, 4200.0)])
    set_points(a, [(3000.0, 2600.0), (4500.0, 3000.0)])
    a_last = nearest_exit(a, b_start)
    a_end = exit_point(a_last[0], a_last[1])
    b_first = nearest_origin(b, a_end)
    check("(c) B follows an edit of A", a_end is not None and dist2d(a_end, b_first[1].translation) < 2.0,
          "%.2f cm" % (dist2d(a_end, b_first[1].translation) if a_end else -1))

    # (d) Tee: B2 starts beside the middle of A2.
    a2 = spawn_trench([(3000.0, 6000.0), (5500.0, 6000.0)])
    b2 = spawn_trench([(4200.0, 6200.0), (4200.0, 8000.0)])
    kind, target = join_kind(b2, "StartJoin")
    check("(d) B2 start joins A2 side", "SIDE" in kind.upper() and target == a2, kind)
    tees = [p for p in pieces(a2) if p[0] == "SM_FieldTrench_Tee"]
    check("(d) A2 has a Tee", len(tees) == 1, str(len(tees)))
    if tees:
        tee = tees[0][1]
        side = tee.translation + tee.rotation.rotator().get_forward_vector() * 218.75 - tee.rotation.rotator().get_right_vector() * 250.0
        b2_first = nearest_origin(b2, side)
        check("(d) B2 starts on the Tee's side port", dist2d(side, b2_first[1].translation) < 2.0,
              "%.2f cm" % dist2d(side, b2_first[1].translation))
        check("(d) B2 one cap", count(b2, "SM_FieldTrench_End") == 1, str(count(b2, "SM_FieldTrench_End")))
    # The Tee goes when B2 is deleted.
    actor_subsystem.destroy_actor(b2)
    actors.remove(b2)
    check("(d) B2 deleted: A2 loses its Tee", count(a2, "SM_FieldTrench_Tee") == 0, str(count(a2, "SM_FieldTrench_Tee")))
    # Tee on the other side.
    b3 = spawn_trench([(4200.0, 5800.0), (4200.0, 4000.0)])
    dump("(d) A2", a2)
    dump("(d) B3", b3)
    tees = [p for p in pieces(a2) if p[0] == "SM_FieldTrench_Tee"]
    check("(d) Tee on the other side", len(tees) == 1, str(len(tees)))
    if tees:
        tee = tees[0][1]
        side = tee.translation + tee.rotation.rotator().get_forward_vector() * 218.75 - tee.rotation.rotator().get_right_vector() * 250.0
        b3_first = nearest_origin(b3, side)
        check("(d) B3 starts on the Tee's side port", dist2d(side, b3_first[1].translation) < 2.0,
              "%.2f cm" % dist2d(side, b3_first[1].translation))
        check("(d) B3 heads away from A2", b3_first[1].translation.y < 5900.0 and
              abs(angle_diff(yaw_of(b3_first[1]), -90.0)) < 15.0, "yaw %.1f" % yaw_of(b3_first[1]))


try:
    run()
except Exception as error:
    import traceback
    log("FAIL exception: " + repr(error))
    log(traceback.format_exc())
    failures.append("exception")
for actor in actors:
    try:
        actor_subsystem.destroy_actor(actor)
    except Exception:
        pass
log("RESULT " + ("PASS" if not failures else "FAIL: " + ", ".join(failures)))
out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "FieldTrenchSpline_Joins.txt")
with open(out, "w") as handle:
    handle.write("\n".join(lines) + "\n")
