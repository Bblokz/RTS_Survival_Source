"""Read-only: after a fresh load of Ger_FrozenLake, log LakeTourShots as Missions[0] (the instanced Arrival stage in the
map) holds them, next to the class default. Run headless with the map on the command line and -ExecutePythonScript."""
import sys

import unreal

sys.path.append("D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/mission")
import wire_fl_stages as wfs  # noqa: E402

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out = ["map " + world.get_name()]
missions = wfs.find_manager(world).get_editor_property("missions")
arrival = missions[0]
out.append("Missions[0] %s outer %s" % (arrival.get_class().get_name(), arrival.get_outer().get_name()))
for k, shot in enumerate(arrival.get_editor_property("LakeTourShots")):
    out.append("instance shot %d: %s" % (k, shot.export_text()))
cdo = unreal.get_default_object(arrival.get_class())
for k, shot in enumerate(cdo.get_editor_property("LakeTourShots")):
    out.append("default shot %d: %s" % (k, shot.export_text()))
with open(unreal.Paths.project_saved_dir() + "FrozenLake/verify_tour_shots.txt", "w") as handle:
    handle.write("\n".join(out))
unreal.log_warning("FL_TOURCHECK done")
