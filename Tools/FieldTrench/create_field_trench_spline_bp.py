"""Creates BP_FieldTrenchSpline (parent ARTSFieldTrenchSpline) and checks it on the open map.

Run in a full editor with the RTS_Survival module that has ARTSFieldTrenchSpline:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/TestCover/TestFieldTrench
    -ExecutePythonScript=<this file> -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended
The Blueprint is saved; the map is not. Results go to Saved/Logs/FieldTrenchSpline_Check.txt.
"""
import os
import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench"
NAME = "BP_FieldTrenchSpline"

lines = []


def log(text):
    lines.append(str(text))
    unreal.log("[FieldTrenchSpline] " + str(text))


def get_or_create_blueprint():
    path = FOLDER + "/" + NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log("Blueprint exists: " + path)
        return unreal.load_asset(path)
    parent = unreal.load_class(None, "/Script/RTS_Survival.RTSFieldTrenchSpline")
    if parent is None:
        log("FAIL: ARTSFieldTrenchSpline not in this module build")
        return None
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    log("Created " + path + " saved=" + str(saved))
    return blueprint


def ground_height(world, x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 50000.0), unreal.Vector(x, y, -50000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    result = hit.to_tuple()
    return result[4].z if result[0] else None  # blocking hit, location


def count_pieces(actor):
    return len([c for c in actor.get_components_by_class(unreal.StaticMeshComponent)
                if c.get_name() != "PieceCollisionTemplate"])


def check(blueprint):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log("World " + world.get_path_name())
    start_z = ground_height(world, 0.0, 0.0, []) or 0.0
    actor_class = blueprint.generated_class()
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        actor_class, unreal.Vector(0.0, 0.0, start_z))
    log("Spawned with default spline: pieces=%d result=%d length=%.0f" % (
        count_pieces(actor), actor.get_editor_property("PieceCount"), actor.get_editor_property("TrenchLength")))

    spline = actor.get_editor_property("TrenchPath")
    spline.clear_spline_points(False)
    for local in [(0, 0), (1500, 0), (2500, 800), (2600, 2200), (1800, 3200)]:
        spline.add_spline_point(unreal.Vector(local[0], local[1], 0.0), unreal.SplineCoordinateSpace.LOCAL, False)
    spline.update_spline()
    actor.call_method("SnapSplinePointsToGround")
    pieces_after_snap = count_pieces(actor)
    log("Bent spline after snap: pieces=%d result=%d length=%.0f spline=%.0f steepest=%.1f" % (
        pieces_after_snap, actor.get_editor_property("PieceCount"), actor.get_editor_property("TrenchLength"),
        spline.get_spline_length(), actor.get_editor_property("SteepestPieceSlope")))

    # Changing a setting reruns the construction script (PostEditChangeProperty): the engine destroys the
    # construction-script pieces and the trench is rebuilt, so the count must not grow.
    for spacing in [40.0, 50.0, 60.0]:
        actor.set_editor_property("PathSampleSpacing", spacing)
        log("After rerun (spacing %.0f): pieces=%d result=%d" % (
            spacing, count_pieces(actor), actor.get_editor_property("PieceCount")))
    actor.set_editor_property("MaximumStraightLength", 250.0)
    log("Max straight 250: pieces=%d result=%d" % (count_pieces(actor), actor.get_editor_property("PieceCount")))

    copy = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).duplicate_actor(actor)
    log("Duplicate: original pieces=%d result=%d, copy pieces=%d result=%d" % (
        count_pieces(actor), actor.get_editor_property("PieceCount"),
        count_pieces(copy), copy.get_editor_property("PieceCount")))
    copy.set_editor_property("PathSampleSpacing", 45.0)
    log("Copy edited: original pieces=%d, copy pieces=%d" % (count_pieces(actor), count_pieces(copy)))
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(copy)
    log("Copy deleted: original pieces=%d" % count_pieces(actor))

    worst_gap = 0.0
    names = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if component.get_name() == "PieceCollisionTemplate":
            continue
        location = component.get_world_location()
        ground = ground_height(world, location.x, location.y, [actor])
        if ground is not None:
            worst_gap = max(worst_gap, abs(location.z - ground))
        mesh = component.get_editor_property("static_mesh")
        names.append(mesh.get_name() if mesh else "None")
    log("Worst piece origin vs ground: %.1f cm" % worst_gap)
    log("Pieces: " + ", ".join(names))
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)


blueprint = get_or_create_blueprint()
if blueprint is not None:
    try:
        check(blueprint)
    except Exception as error:
        log("FAIL exception: " + repr(error))

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "FieldTrenchSpline_Check.txt")
with open(out, "w") as handle:
    handle.write("\n".join(lines) + "\n")
