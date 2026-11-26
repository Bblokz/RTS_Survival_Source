"""Ore Mountains life pass, step 2 (throwaway FULL editor job): place the planned decals and VAT animals, import the
blended weightmaps, rebuild the navmesh and save Ger_2ndOreMountains.

Launch from PowerShell (Git Bash rewrites /Game/... arguments):
  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains
    -ExecutePythonScript=<this file> -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash
The editor quits when this returns, so everything is synchronous. Re-runnable: every actor it places is labelled
ORE_Life_* and is deleted first. Inputs: Saved/OreMountains/life/life_plan.json (ore_life_gen.py) and
Saved/OreMountains/life/weights_blend/*.raw (ore_life_blend.py; set ORE_LIFE_SKIP_BLEND=1 to keep the weights).
Commandlets crash saving landscape maps, hence the full editor. Report: Saved/OreMountains/life/place_report.json.
"""
import json
import os

import unreal

LIFE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/life/"
UMAP = "D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains.umap"
FOLDER = "OreMountains/Life/"
LABEL = "ORE_Life_"
DECAL_FADE_SCREEN_SIZE = 0.01      # the engine default: small decals cull when the camera zooms out
DECAL_SORT_ORDER = 2               # over the older terrain decals (0-1), under the impact craters


def log(text):
    unreal.log_warning("ORE_LIFE " + str(text))


def eas():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def landscape():
    return [a for a in eas().get_all_level_actors() if isinstance(a, unreal.Landscape)][0]


def ground_z(x, y):
    """Landscape height under (x, y): the first landscape hit of a multi trace (not a tree, rock or building)."""
    hits = unreal.SystemLibrary.line_trace_multi(world(), unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True) or []
    for hit in hits:
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
    return hits[-1].to_tuple()[4].z if hits else 0.0


def clear_old():
    removed = 0
    for actor in eas().get_all_level_actors():
        if actor.get_actor_label().startswith(LABEL):
            eas().destroy_actor(actor)
            removed += 1
    return removed


def place_decals(plan):
    materials = {k: unreal.load_asset(v) for k, v in plan["materials"].items()}
    missing = sorted(k for k, v in materials.items() if v is None)
    placed = 0
    land = landscape()
    for label, key, x, y, yaw, depth, half_y, half_z in plan["decals"]:
        mat = materials.get(key)
        if mat is None:
            continue
        z = ground_z(x, y)
        actor = eas().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, z),
                                             unreal.Rotator(roll=0.0, pitch=-90.0, yaw=yaw))
        comp = actor.get_editor_property("decal")
        comp.set_editor_property("decal_material", mat)
        comp.set_editor_property("decal_size", unreal.Vector(depth, half_y, half_z))
        comp.set_editor_property("fade_screen_size", DECAL_FADE_SCREEN_SIZE)
        comp.set_editor_property("sort_order", DECAL_SORT_ORDER)
        actor.set_actor_label(label)
        actor.set_folder_path(FOLDER + "Decals/" + label.split("_")[2])
        placed += 1
    return placed, missing


def place_animals(plan):
    classes = {k: unreal.load_class(None, v + "." + v.rsplit("/", 1)[1] + "_C") for k, v in plan["animals_bp"].items()}
    placed, problems = 0, []
    for a in plan["animals"]:
        cls = classes.get(a["kind"])
        if cls is None:
            problems.append("no class for " + a["kind"])
            continue
        z = ground_z(a["x"], a["y"])
        actor = eas().spawn_actor_from_class(cls, unreal.Vector(a["x"], a["y"], z + 5.0),
                                             unreal.Rotator(0.0, 0.0, a.get("yaw", 0.0)))
        if actor is None:
            problems.append("spawn failed " + a["label"])
            continue
        try:
            if "semi_x" in a:
                ellipse = actor.get_editor_property("ellipse")
                ellipse.set_editor_property("semi_axis_x", a["semi_x"])
                ellipse.set_editor_property("semi_axis_y", a["semi_y"])
                ellipse.set_editor_property("clockwise", a["clockwise"])
                actor.set_editor_property("ellipse", ellipse)
                flight = actor.get_editor_property("flight_settings")
                flight.set_editor_property("bird_count", a["birds"])
                flight.set_editor_property("flight_height", a["height"])
                flight.set_editor_property("speed", a["speed"])
                flight.set_editor_property("min_scale", a["scale"] * 0.92)
                flight.set_editor_property("max_scale", a["scale"] * 1.08)
                flight.set_editor_property("seed", a["seed"])
                actor.set_editor_property("flight_settings", flight)
            else:
                move = actor.get_editor_property("movement_settings")
                move.set_editor_property("wander_radius", a["wander"])
                actor.set_editor_property("movement_settings", move)
        except Exception as error:
            problems.append("%s settings: %r" % (a["label"], error))
        actor.set_actor_label(a["label"])
        actor.set_folder_path(FOLDER + "Animals")
        placed += 1
    return placed, problems


def import_blend():
    if os.environ.get("ORE_LIFE_SKIP_BLEND") == "1":
        return "skipped"
    import sys
    sys.path.append("D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
    import ore_map_build as omb
    region = json.load(open(LIFE + "weights/region.json"))
    names = region["layers"]
    files = [LIFE + "weights_blend/%s.raw" % n for n in names]
    if not all(os.path.exists(f) for f in files):
        return "missing blend files"
    infos = [unreal.load_asset(omb.LAYER_INFOS[n]) for n in names]
    x0, y0, x1, y1 = region["region"]
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(landscape(), x0, y0, x1, y1, "", infos, files)
    return "imported %s layers ok=%s" % (len(names), ok)


def main():
    if world().get_name() != "Ger_2ndOreMountains":
        log("ABORT wrong map " + world().get_name())
        return
    before = os.path.getsize(UMAP)
    plan = json.load(open(LIFE + "life_plan.json"))
    report = {"removed_old": clear_old()}
    report["blend"] = import_blend()
    report["decals"], report["missing_materials"] = place_decals(plan)
    report["animals"], report["animal_problems"] = place_animals(plan)
    unreal.SystemLibrary.execute_console_command(world(), "RebuildNavigation")
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    after = os.path.getsize(UMAP)
    report.update({"saved": bool(saved), "size_before": before, "size_after": after, "ratio": round(after / before, 3)})
    json.dump(report, open(LIFE + "place_report.json", "w"), indent=1)
    log(json.dumps(report))


main()
