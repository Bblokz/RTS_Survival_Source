"""Read-only: how Ger_Trains lays its wooden track (BerlinRailYard.md). Prints, per BP_RoadSpline actor, the label,
spline length, point count, mean segment length, RoadMesh and RoadMaterial. DebugGame commandlet, -nullrhi."""
import json

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/BerlinRailYard/trains_track.json"
unreal.EditorLoadingAndSavingUtils.load_map("/Game/RTS_Survival/Maps/GerMissions/Ger_Trains")
rows = []
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_class().get_name() != "BP_RoadSpline_C":
        continue
    sp = a.get_component_by_class(unreal.SplineComponent)
    n = sp.get_number_of_spline_points()
    ln = sp.get_spline_length()
    mesh = a.get_editor_property("RoadMesh")
    mat = a.get_editor_property("RoadMaterial")
    rows.append(dict(label=a.get_actor_label(), name=a.get_name(), points=n, length_m=round(ln / 100.0, 1),
                     seg_m=round(ln / max(n - 1, 1) / 100.0, 2), mesh=mesh.get_name() if mesh else None,
                     material=mat.get_name() if mat else None, collision=a.get_editor_property("bUseCollision")))
with open(OUT, "w") as f:
    json.dump(rows, f, indent=1)
unreal.log_warning("TRAINS_TRACK " + json.dumps(rows))
