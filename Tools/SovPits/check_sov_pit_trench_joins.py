"""Checks that BP_SovFieldTrenchSpline joins every Soviet pit's trench anchor, on the open map (TestFieldTrench).

Per pit: spawn its building expansion Blueprint, read its TrenchPort_ socket and the TrenchAnchors component's neck,
draw a Soviet spline trench from 1 m off the socket outwards, and check that it starts on the socket (anchor join, no
start cap, first piece on and along the socket, Soviet kit pieces only). Nothing is saved; spawned actors are removed.
Run in the editor: execute_python exec(open(<this file>).read()). Results: Saved/Logs/SovPitTrenchJoins.txt.
"""
import math
import unreal

TRENCH_BP = "/Game/RTS_Survival/Blueprints/Environment/Trenches/SovFieldTrench/BP_SovFieldTrenchSpline"
BXP = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions"
PITS = [
    (BXP + "/Rus53KPit/Rus_BXP_53KPit", "TrenchPort_Rear"),
    (BXP + "/Rus53KPit/Rus_BXP_53KSectorPit", "TrenchPort_Rear"),
    (BXP + "/RusBS3Pit/Rus_BXP_BS3Pit", "TrenchPort_Rear"),
    (BXP + "/RusDShKPit/Rus_BXP_DShKPit", "TrenchPort_Rear"),
    (BXP + "/RusAmpulometPit/Rus_BXP_AmpulometPit", "TrenchPort_Rear"),
    (BXP + "/RusMortarPit/Rus_BXP_120MortarPit", "TrenchPort_Left"),
]
lines, failures, actors = [], [], []
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def log(text):
    lines.append(str(text))
    unreal.log("[SovPitJoins] " + str(text))


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


def spawn(path, location):
    actor = actor_subsystem.spawn_actor_from_class(unreal.load_asset(path).generated_class(), location)
    actors.append(actor)
    return actor


def pieces(actor):
    result = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        if component.get_name() != "PieceCollisionTemplate" and mesh is not None:
            result.append((mesh.get_name(), component.get_world_transform()))
    return result


def dist2d(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def angle_diff(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


def neck_of(blueprint_path, socket_name):
    blueprint = unreal.load_asset(blueprint_path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        template = library.get_object_for_blueprint(library.get_data(handle), blueprint)
        if isinstance(template, unreal.FieldTrenchAnchorComponent):
            for definition in template.get_editor_property("anchors"):
                text = definition.export_text()
                if socket_name in text:
                    return text
    return None


def run_pit(index, blueprint_path, socket_name):
    name = blueprint_path.rsplit("/", 1)[1]
    x, y = 4000.0 * index - 10000.0, -9000.0
    pit = spawn(blueprint_path, unreal.Vector(x, y, ground(x, y)))
    socket = None
    for component in pit.get_components_by_class(unreal.StaticMeshComponent):
        if component.does_socket_exist(socket_name):
            socket = component.get_socket_transform(socket_name, unreal.RelativeTransformSpace.RTS_WORLD)
    check(name + " has " + socket_name, socket is not None)
    neck = neck_of(blueprint_path, socket_name)
    check(name + " TrenchAnchors lists the neck", neck is not None and "TrenchNeck" in neck, str(neck))
    if socket is None:
        return
    s, out = socket.translation, socket.rotation.rotator().get_forward_vector()
    yaw = socket.rotation.rotator().yaw
    points = [s + out * 100.0, s + out * 900.0, s + out * 1600.0]
    trench = spawn(TRENCH_BP, unreal.Vector(points[0].x, points[0].y, ground(points[0].x, points[0].y)))
    spline = trench.get_editor_property("TrenchPath")
    spline.clear_spline_points(False)
    for p in points:
        spline.add_spline_point(unreal.Vector(p.x, p.y, ground(p.x, p.y)), unreal.SplineCoordinateSpace.WORLD, False)
    spline.update_spline()
    trench.call_method("RebuildTrench")
    placed = pieces(trench)
    join = trench.get_editor_property("StartJoin")
    kind, target = str(join.get_editor_property("Kind")), join.get_editor_property("Target")
    first = min(placed, key=lambda p: dist2d(p[1].translation, s))
    check(name + " start joins anchor", "ANCHOR" in kind.upper() and target == pit, kind)
    check(name + " first piece on socket", dist2d(first[1].translation, s) < 1.0,
          "%s %.2f cm" % (first[0], dist2d(first[1].translation, s)))
    check(name + " first piece along socket", angle_diff(first[1].rotation.rotator().yaw, yaw) < 0.5,
          "%.2f deg" % angle_diff(first[1].rotation.rotator().yaw, yaw))
    ends = sum(1 for n, _ in placed if n.endswith("_End"))
    check(name + " only the far end capped", ends == 1, str(ends))
    check(name + " Soviet pieces only", all(n.startswith("SM_SovFieldTrench_") for n, _ in placed),
          "%d pieces" % len(placed))


try:
    for pit_index, (pit_path, pit_socket) in enumerate(PITS):
        run_pit(pit_index, pit_path, pit_socket)
except Exception as error:
    check("no exception", False, repr(error))
finally:
    for actor in reversed(actors):
        actor_subsystem.destroy_actor(actor)
log("RESULT %s (%d failed)" % ("PASS" if not failures else "FAIL", len(failures)))
with open(unreal.Paths.project_saved_dir() + "Logs/SovPitTrenchJoins.txt", "w") as handle:
    handle.write("\n".join(lines) + "\n")
