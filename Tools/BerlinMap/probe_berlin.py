"""Read-only check of the saved Ger_Berlin in a fresh process (run as -run=pythonscript)."""
import collections
import json

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
out = {"classes": dict(collections.Counter(a.get_class().get_name() for a in actors).most_common(40))}
total = 0
with_material = 0
for a in actors:
    label = a.get_actor_label()
    name = a.get_class().get_name()
    if label.startswith("BER_ISM_") or label == "BERW_Rails":
        for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            total += c.get_instance_count()
            if "MI_Berlin_Ruin" in c.get_material(0).get_name():
                with_material += 1
    if isinstance(a, unreal.Landscape):
        out["landscape"] = {"material": a.get_editor_property("landscape_material").get_name(),
                            "layers": [str(n) for n in a.get_target_layer_names()],
                            "components": len(a.get_components_by_class(unreal.LandscapeComponent)),
                            "rvt": [t.get_name() for t in a.get_editor_property("runtime_virtual_textures")],
                            "bounds": [str(v) for v in a.get_actor_bounds(False)]}
    if name == "RuntimeVirtualTextureVolume":
        comp = a.get_component_by_class(unreal.RuntimeVirtualTextureComponent)
        out.setdefault("rvt_volumes", []).append([label, comp.get_editor_property("virtual_texture").get_name(),
                                                  str(a.get_actor_location()), str(a.get_actor_scale3d())])
    if name in ("NavMeshBoundsVolume", "RecastNavMesh"):
        out.setdefault("nav", []).append([label] + [str(v) for v in a.get_actor_bounds(False)])
    if name == "BP_FogOfWar_Manager_C":
        out["fow"] = [str(a.get_actor_location()), a.get_editor_property("map_extent")]
    if label.startswith("POI_") and not label.startswith("POI_Note"):
        out.setdefault("poi", []).append([label, name, str(a.get_actor_location())])
out["instances"] = total
out["ruin_material_components"] = with_material
with open("D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/probe_berlin.json", "w") as handle:
    json.dump(out, handle, indent=1)
