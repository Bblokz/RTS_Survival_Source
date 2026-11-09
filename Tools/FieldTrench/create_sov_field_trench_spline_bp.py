"""Creates BP_SovFieldTrenchSpline (child of BP_FieldTrenchSpline with Kit = Soviet) and checks it on the open map.

The Soviet spline trench is the German spline actor (ARTSFieldTrenchSpline) with its kit set to EFieldTrenchKit::Soviet,
so it lays SM_SovFieldTrench_* pieces (log revetment, corduroy floor, Soviet sandbags) with the same planner, joins and
pit anchors. Run in an editor whose RTS_Survival module has EFieldTrenchKit, with a map open (TestFieldTrench):
  execute_python: exec(open(<this file>).read())
The Blueprint is saved; the map is not. Results go to Saved/Logs/SovFieldTrenchSpline_Check.txt.
"""
import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/Environment/Trenches/SovFieldTrench"
NAME = "BP_SovFieldTrenchSpline"
PARENT = "/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/BP_FieldTrenchSpline"
SOVIET_PREFIX = "SM_SovFieldTrench_"

lines = []


def log(text):
    lines.append(str(text))
    unreal.log("[SovFieldTrenchSpline] " + str(text))


def get_or_create_blueprint():
    path = FOLDER + "/" + NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        blueprint = unreal.load_asset(path)
        log("Blueprint exists: " + path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_asset(PARENT).generated_class())
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Blueprint, factory)
        log("Created " + path)
    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("kit", unreal.FieldTrenchKit.SOVIET)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    # A compile after setting a CDO value re-dirties the Blueprint; save after the last compile.
    saved = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    log("Kit on CDO after compile: %s, saved=%s" % (
        unreal.get_default_object(blueprint.generated_class()).get_editor_property("kit"), saved))
    return blueprint


def ground_height(world, x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 50000.0), unreal.Vector(x, y, -50000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    result = hit.to_tuple()
    return result[4].z if result[0] else None


def piece_components(actor):
    return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent)
            if c.get_name() != "PieceCollisionTemplate"]


def check(blueprint):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    log("World " + world.get_path_name())
    origin_x, origin_y = 0.0, -6000.0
    start_z = ground_height(world, origin_x, origin_y, []) or 0.0
    actor = actors.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector(origin_x, origin_y, start_z))
    log("Spawned: kit=%s pieces=%d" % (actor.get_editor_property("kit"), len(piece_components(actor))))
    spline = actor.get_editor_property("TrenchPath")
    spline.clear_spline_points(False)
    for local in [(0, 0), (1500, 0), (2500, 800), (2600, 2200), (1800, 3200)]:
        spline.add_spline_point(unreal.Vector(local[0], local[1], 0.0), unreal.SplineCoordinateSpace.LOCAL, False)
    spline.update_spline()
    actor.call_method("SnapSplinePointsToGround")
    log("Bent spline: pieces=%d result=%d length=%.0f spline=%.0f" % (
        len(piece_components(actor)), actor.get_editor_property("PieceCount"),
        actor.get_editor_property("TrenchLength"), spline.get_spline_length()))
    for spacing in [40.0, 50.0, 60.0]:
        actor.set_editor_property("PathSampleSpacing", spacing)
        log("Rerun (spacing %.0f): pieces=%d result=%d" % (
            spacing, len(piece_components(actor)), actor.get_editor_property("PieceCount")))
    worst_gap, names = 0.0, []
    for component in piece_components(actor):
        location = component.get_world_location()
        ground = ground_height(world, location.x, location.y, [actor])
        if ground is not None:
            worst_gap = max(worst_gap, abs(location.z - ground))
        mesh = component.get_editor_property("static_mesh")
        names.append(mesh.get_name() if mesh else "None")
    soviet = sum(1 for n in names if n.startswith(SOVIET_PREFIX))
    log("Worst piece origin vs ground: %.1f cm" % worst_gap)
    log("Soviet kit pieces: %d of %d" % (soviet, len(names)))
    log("Pieces: " + ", ".join(names))
    # The kit can be switched on a placed trench: back to German and Soviet again.
    actor.set_editor_property("kit", unreal.FieldTrenchKit.GERMAN)
    german = sum(1 for c in piece_components(actor)
                 if c.get_editor_property("static_mesh").get_name().startswith("SM_FieldTrench_"))
    actor.set_editor_property("kit", unreal.FieldTrenchKit.SOVIET)
    soviet_again = sum(1 for c in piece_components(actor)
                       if c.get_editor_property("static_mesh").get_name().startswith(SOVIET_PREFIX))
    log("Kit switch on the instance: German pieces=%d, back to Soviet=%d" % (german, soviet_again))
    actors.destroy_actor(actor)
    log("RESULT %s" % ("PASS" if soviet == len(names) and names and worst_gap < 25.0 and soviet_again == soviet
                       else "FAIL"))


blueprint = get_or_create_blueprint()
try:
    check(blueprint)
except Exception as error:
    log("FAIL exception: " + repr(error))

out = unreal.Paths.project_saved_dir() + "Logs/SovFieldTrenchSpline_Check.txt"
with open(out, "w") as handle:
    handle.write("\n".join(lines) + "\n")
