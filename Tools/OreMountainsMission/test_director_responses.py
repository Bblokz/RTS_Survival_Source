"""Headless test of place_director_responses.py on a THROWAWAY file copy of the Ore map. Saves nothing.

The copy is made outside Unreal (the real map is never opened):
    Content/RTS_Survival/Maps/Tmp/Ger_2ndOreMountains_DirectorTest.umap   (delete it after the run)
Run with the DebugGame commandlet from PowerShell (no GPU):
    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended ...
Checks: two runs keep the same objects, check() reads everything back, and the hostility rule the director's
bOnlyHostileTargets uses flips with the station's state (neutral -> captured -> knocked out -> recaptured).
Report: Saved/OreMountains/director/test_report.json, log lines ORE_DIRECTOR_TEST.
"""
import importlib
import json
import sys

import unreal

TEST_MAP = "/Game/RTS_Survival/Maps/Tmp/Ger_2ndOreMountains_DirectorTest"
REPORT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/director/test_report.json"
sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission")
failures = []
out = {}


def log(text):
    unreal.log_warning("ORE_DIRECTOR_TEST " + str(text))


def fail(text):
    failures.append(text)
    log("FAIL " + text)


def hostility_steps(pdr):
    """Editor world: no BeginPlay or game state; the owner and the hostility query are what this checks."""
    station = pdr.find_by_label(pdr.STATION_LABELS[1])
    hostile = unreal.RTSPlayerRelationsLibrary.is_actor_hostile_to_player
    steps = [("start", pdr.rts_owner(station), hostile(station, pdr.ENEMY))]
    station.on_station_captured(1)
    steps.append(("captured by 1", pdr.rts_owner(station), hostile(station, pdr.ENEMY)))
    station.knock_out_station()
    steps.append(("knocked out", pdr.rts_owner(station), hostile(station, pdr.ENEMY)))
    station.on_station_captured(1)
    steps.append(("recaptured", pdr.rts_owner(station), hostile(station, pdr.ENEMY)))
    expect = [(0, False), (1, True), (2, False), (1, True)]
    for (name, owner, is_hostile), (want_owner, want_hostile) in zip(steps, expect):
        if owner != want_owner or bool(is_hostile) != want_hostile:
            fail("hostility %s: owner %s hostile %s, expected %s / %s" % (name, owner, is_hostile, want_owner,
                                                                          want_hostile))
    return steps


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(TEST_MAP)
    if world is None:
        fail("could not load " + TEST_MAP)
        return
    out["level"] = world.get_path_name()
    import place_director_responses as pdr
    importlib.reload(pdr)
    first = pdr.run()
    second = pdr.run()
    out["first_problems"] = first["problems"]
    out["second_problems"] = second["problems"]
    out["nav"] = second["nav"]
    out["actors"] = second["actors"]
    if first["actors"] != second["actors"] or first["director"] != second["director"]:
        fail("second run changed the objects: %s vs %s" % (first["actors"], second["actors"]))
    if len(second["actors"]) != 8:
        fail("%d placed actors, expected 8" % len(second["actors"]))
    for text in second["problems"]:
        fail("run problem: " + text)
    result = pdr.check()
    out["check"] = result
    if not result["ok"]:
        fail("check: %s" % result["problems"])
    # the overlap check must see a solid actor: the station bin is BlockAll
    station = pdr.find_by_label(pdr.STATION_LABELS[0])
    out["overlap_on_station"] = pdr.overlapping_actors(station.get_actor_location(), 300.0)
    if not out["overlap_on_station"]:
        fail("overlap check found nothing on the station bin")
    for actor in [pdr.find_by_label(label) for label in pdr.planned_labels()] + [pdr.find_by_label(pdr.DIRECTOR_LABEL)]:
        texts = [str(c.get_editor_property("text")) for c in actor.get_components_by_class(unreal.TextRenderComponent)]
        out.setdefault("labels", {})[actor.get_actor_label()] = texts
        if not texts:
            fail("%s has no editor label component (label check would pass vacuously)" % actor.get_actor_label())
    try:
        out["hostility"] = hostility_steps(pdr)
    except Exception as error:
        fail("hostility steps: %r" % error)


try:
    main()
except Exception as error:
    fail("exception %r" % error)
out["failures"] = failures
with open(REPORT, "w") as handle:
    json.dump(out, handle, indent=1, default=str)
log("RESULT %s (%d failures)" % ("PASS" if not failures else "FAIL", len(failures)))
