"""Read-only check of the SAVED Ger_Berlin after the rail-yard D1 build (DebugGame commandlet, -nullrhi):
actor counts per prefix, BERW_Rails instances left, every RY_ actor with owner, the track's points and material,
the strongpoint posts, and the Forge mission entry. Writes Saved/BerlinRailYard/verify.json."""
import json

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/BerlinRailYard/verify.json"
unreal.EditorLoadingAndSavingUtils.load_map("/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
res = {"counts": {}, "ry": [], "rail_instances": None}
for a in actors:
    label = a.get_actor_label()
    for pre in ("FORGE_", "RES_", "POI_", "RY_", "BER_Yard", "BERW_"):
        if label.startswith(pre):
            res["counts"][pre] = res["counts"].get(pre, 0) + 1
    if label == "BERW_Rails":
        res["rail_instances"] = sum(c.get_instance_count() for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent))
    if label.startswith("RY_"):
        row = {"label": label, "cls": a.get_class().get_name()}
        owners = [c.get_editor_property("owning_player") for c in a.get_components_by_class(unreal.RTSComponent)]
        if owners:
            row["owner"] = owners
        if label == "RY_Rail_Main":
            sp = a.get_component_by_class(unreal.SplineComponent)
            mat = a.get_editor_property("RoadMaterial")
            row.update(points=sp.get_number_of_spline_points(), length_m=round(sp.get_spline_length() / 100.0, 1),
                       mesh=a.get_editor_property("RoadMesh").get_name(), material=mat.get_name() if mat else None)
        if label == "RY_Strongpoint":
            row["posts"] = [[str(p.get_editor_property("post_id")), round(p.get_editor_property("post").translation.x),
                             round(p.get_editor_property("post").translation.y), p.get_editor_property("dig_in"),
                             p.get_editor_property("take_nearby_cover"),
                             str(p.get_editor_property("unit_options")[0].get_editor_property("kind"))]
                            for p in a.get_editor_property("posts")]
        res["ry"].append(row)
    if "MissionManager" in a.get_class().get_name():
        res["missions"] = [m.get_class().get_name() if m else None for m in a.get_editor_property("missions")]
with open(OUT, "w") as f:
    json.dump(res, f, indent=1, default=str)
unreal.log_warning("RAILYARD_VERIFY " + json.dumps(res["counts"]))
