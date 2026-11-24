"""Read-only dump of the template and reference maps (run as -run=pythonscript)."""
import collections
import json

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/probe_maps.json"
MAPS = ["/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopyMap", "/Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission"]
result = {}


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


for path in MAPS:
    unreal.EditorLoadingAndSavingUtils.load_map(path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    counts = collections.Counter(a.get_class().get_name() for a in actors)
    info = {"counts": dict(counts.most_common()), "actors": []}
    for a in actors:
        name = a.get_class().get_name()
        if counts[name] > 12 and not isinstance(a, unreal.Landscape):
            continue
        origin, extent = a.get_actor_bounds(False)
        entry = {"label": a.get_actor_label(), "class": name, "loc": vec(a.get_actor_location()),
                 "rot": [round(a.get_actor_rotation().yaw, 1)], "scale": vec(a.get_actor_scale3d()),
                 "origin": vec(origin), "extent": vec(extent), "folder": str(a.get_folder_path())}
        if isinstance(a, unreal.Landscape):
            comps = a.get_components_by_class(unreal.LandscapeComponent)
            entry["components"] = len(comps)
            for prop in ("landscape_material", "component_size_quads", "subsection_size_quads", "num_subsections",
                         "runtime_virtual_textures", "default_phys_material", "virtual_texture_render_pass_type"):
                try:
                    entry[prop] = str(a.get_editor_property(prop))
                except Exception as error:
                    entry[prop] = "ERR " + str(error)
            try:
                entry["layers"] = [str(n) for n in a.get_target_layer_names()]
            except Exception as error:
                entry["layers"] = "ERR " + str(error)
        if name == "BP_FogOfWar_Manager_C":
            for prop in ("map_extent",):
                try:
                    entry[prop] = str(a.get_editor_property(prop))
                except Exception as error:
                    entry[prop] = "ERR " + str(error)
        info["actors"].append(entry)
    result[path] = info

# candidate ruin content
registry = unreal.AssetRegistryHelpers.get_asset_registry()
found = {}
for root in ("/Game/DBK/Prefabs", "/Game/DBK/Examples", "/Game/DBK/Models/Rubble",
             "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor"):
    assets = registry.get_assets_by_path(root, recursive=True)
    found[root] = sorted("%s|%s" % (str(d.package_name), str(d.asset_class_path.asset_name)) for d in assets)
result["assets"] = found
with open(OUT, "w") as handle:
    json.dump(result, handle, indent=1)
unreal.log_warning("BERLIN_PROBE done")
