"""Lists the materials every German vehicle Blueprint shows, for the team colour rollout.

Run headless (spawns each unit in a blank map that is never saved):
UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi -nosound -DisablePlugins=Fab
Writes Saved/TeamColorTest/german_vehicle_materials.json: per unit class its mesh slots and materials, and per material
the root material it derives from.
"""
import json
import os
import re

import unreal

OUTPUT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "TeamColorTest", "german_vehicle_materials.json")
GERMAN_SETUP_PROPERTIES = ["SetupLightGerVehicles", "SetupMediumGerVehicles", "SetupHeavyGerVehicles", "SetupGerNomadics"]
CLASS_PATH_PATTERN = re.compile(r"/Game/[A-Za-z0-9_/\-]+\.[A-Za-z0-9_\-]+_C")


def find_blueprints_with_parent(parent_name):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for asset in registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "Blueprint"), True):
        parent = str(asset.get_tag_value("ParentClass") or "")
        if parent_name in parent:
            found.append(str(asset.package_name))
    return found


def get_soft_class_path(soft_class):
    if not soft_class:
        return ""
    if hasattr(soft_class, "get_path_name"):
        return soft_class.get_path_name()
    return soft_class.export_text()


def get_class_paths_from_spawners():
    class_paths = {}
    for spawner_path in find_blueprints_with_parent("RTSAsyncSpawner"):
        blueprint = unreal.load_asset(spawner_path)
        defaults = unreal.get_default_object(blueprint.generated_class())
        for property_name in GERMAN_SETUP_PROPERTIES:
            for row in defaults.get_editor_property(property_name):
                soft_class = row.get_editor_property("unit_class")
                path = get_soft_class_path(soft_class)
                subtype = row.get_editor_property("tank_subtype" if "Nomadic" not in property_name else "nomadic_subtype")
                if path and path != "None":
                    class_paths.setdefault(path, set()).add(f"{property_name}:{subtype}")
    return class_paths


def get_class_paths_from_variants():
    class_paths = {}
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for asset in registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/RTS_Survival", "UnitVariantDataAsset"), True):
        data_asset = unreal.load_asset(str(asset.package_name))
        for match in CLASS_PATH_PATTERN.findall(export_text(data_asset)):
            class_paths.setdefault(match, set()).add("Variant")
    return class_paths


def export_text(asset):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(unreal.Paths.project_saved_dir(), "TeamColorTest", "variant_export.t3d")
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = None
    unreal.Exporter.run_asset_export_task(task)
    try:
        with open(task.filename, encoding="utf-8", errors="ignore") as handle:
            return handle.read()
    except OSError:
        return ""


def get_root_material(material):
    chain = []
    current = material
    while isinstance(current, unreal.MaterialInstance):
        chain.append(current.get_path_name())
        current = current.get_editor_property("parent")
    return (current.get_path_name() if current else "None"), chain


def get_mesh_slot_materials(component):
    slots = []
    for slot_index, material in enumerate(component.get_materials()):
        slots.append({"slot": slot_index, "material": material.get_path_name() if material else "None"})
    return slots


def blueprint_path_from_class_path(class_path):
    return class_path.rsplit(".", 1)[0]


def gather_actor_tree(actor, found):
    """Collects the actor, its child actors and attached actors (turrets are child actor components)."""
    if actor is None or actor in found:
        return
    found.append(actor)
    for child_component in actor.get_components_by_class(unreal.ChildActorComponent):
        gather_actor_tree(child_component.get_editor_property("child_actor"), found)
    for attached in actor.get_attached_actors():
        gather_actor_tree(attached, found)


def survey_class(class_path, actor_subsystem):
    unit_class = unreal.load_class(None, class_path)
    if unit_class is None:
        return None
    actor = actor_subsystem.spawn_actor_from_class(unit_class, unreal.Vector(0, 0, 10000))
    if actor is None:
        return None
    actors = []
    gather_actor_tree(actor, actors)
    entries = []
    for each_actor in actors:
        for component in each_actor.get_components_by_class(unreal.MeshComponent):
            entries.append({"owner": each_actor.get_class().get_name(), "component": component.get_name(),
                            "class": component.get_class().get_name(), "visible": component.is_visible(),
                            "slots": get_mesh_slot_materials(component)})
    for each_actor in reversed(actors):
        actor_subsystem.destroy_actor(each_actor)
    return entries


def main():
    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    class_paths = get_class_paths_from_spawners()
    for path, sources in get_class_paths_from_variants().items():
        class_paths.setdefault(path, set()).update(sources)
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    units = {}
    materials = {}
    for class_path, sources in sorted(class_paths.items()):
        blueprint_path = blueprint_path_from_class_path(class_path)
        entries = survey_class(class_path, actor_subsystem)
        if entries is None:
            unreal.log_warning(f"TeamColorSurvey: could not spawn {class_path}")
            continue
        units[blueprint_path] = {"sources": sorted(sources), "meshes": entries}
        for entry in entries:
            for slot in entry["slots"]:
                if slot["material"] == "None" or slot["material"] in materials:
                    materials.setdefault(slot["material"], {}).setdefault("units", set()).add(blueprint_path)
                    continue
                material = unreal.load_asset(slot["material"].split(".")[0])
                root, chain = get_root_material(material)
                materials[slot["material"]] = {"root": root, "chain": chain, "units": {blueprint_path}}
    for info in materials.values():
        info["units"] = sorted(info.get("units", []))
    with open(OUTPUT_FILE, "w", encoding="utf-8") as handle:
        json.dump({"units": units, "materials": materials}, handle, indent=1)
    unreal.log(f"TeamColorSurvey: {len(units)} units, {len(materials)} materials -> {OUTPUT_FILE}")


main()
