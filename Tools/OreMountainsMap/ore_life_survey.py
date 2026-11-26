"""Read-only survey for the Ore life pass: decals by material, and the footprints of buildings/sites/rocks that the
seating decals go around. Headless: UnrealEditor-Cmd <uproject> <map> -run=pythonscript -script=<this> -nullrhi."""
import json
import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/life/survey.json"
MAP = "/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains"

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
decals, objects = {}, []
counts = {}
for actor in eas.get_all_level_actors():
    cls = actor.get_class().get_name()
    label = actor.get_actor_label()
    counts[cls] = counts.get(cls, 0) + 1
    if isinstance(actor, unreal.DecalActor):
        comp = actor.get_editor_property("decal")
        mat = comp.get_editor_property("decal_material")
        key = mat.get_name() if mat else "None"
        decals[key] = decals.get(key, 0) + 1
        continue
    if not (label.startswith("ORE_Site_") or label.startswith("ORE_Explore_") or label.startswith("ORE_Prop_")
            or label.startswith("ORE_Yard") or "Hangar" in label or "Rockwork" in label or label.startswith("ORE_PH_")
            or "Bxp" in cls or "BXP" in cls or "Building" in cls or "Wreck" in label):
        continue
    origin, extent = actor.get_actor_bounds(True)
    if extent.x > 6000 or extent.y > 6000:
        continue
    objects.append([label, cls, round(origin.x), round(origin.y), round(origin.z), round(extent.x), round(extent.y),
                    round(actor.get_actor_rotation().yaw, 1)])
json.dump({"decals_by_material": decals, "decal_total": sum(decals.values()), "objects": objects,
           "class_counts": dict(sorted(counts.items(), key=lambda kv: -kv[1])[:40])}, open(OUT, "w"), indent=1)
unreal.log_warning("ORE_LIFE_SURVEY written %d decals, %d objects" % (sum(decals.values()), len(objects)))
