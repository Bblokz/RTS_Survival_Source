"""Candidate B (hangar and apron): reads and exports the source models to FBX for the Blender step. Read-only.

Run headless: sh run_py.sh export_sources.py
Out: Saved/WorldBuildSitePieces/airfield_B/  (*.fbx, sources.txt)
"""
import os
import traceback

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_B")
BUILDING = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/"
STATICS = {
    "base": BUILDING + "BuildingMeshes/SM_Ger_AircraftBuilding_BaseMesh",
    "preview": BUILDING + "BuildingMeshes/SM_Ger_AircraftBuildingPreview",
    "mount": BUILDING + "Mount/SM_GerAircraftBase_Mount",
}
SKELETALS = {
    "roof": BUILDING + "BuildingMeshes/SK_Ger_AircraftBuilding_Roof",
    "stuka": "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO",
}
ANIMS = {
    "open": BUILDING + "BuildingMeshes/Anims/Anim_GerAirbase_OpenStationary",
    "closetoopen": BUILDING + "BuildingMeshes/Anims/Anim_GerAirbase_CloseToOpen",
}
LINES = []


def log(text):
    LINES.append(str(text))
    unreal.log("AIRFIELDB " + str(text))


def describe_material(material):
    if material is None:
        return "None"
    text = material.get_path_name()
    if isinstance(material, unreal.MaterialInstance):
        for parameter in material.get_editor_property("texture_parameter_values"):
            texture = parameter.parameter_value
            text += "\n        tex %s = %s" % (parameter.parameter_info.name, texture.get_path_name() if texture else None)
        for parameter in material.get_editor_property("vector_parameter_values"):
            text += "\n        vec %s = %s" % (parameter.parameter_info.name, parameter.parameter_value)
    return text


def export(asset, exporter, name, options):
    if asset is None:
        log("NOT EXPORTED (None) " + name)
        return
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(OUT, name + ".fbx")
    task.exporter = exporter
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.options = options
    ok = unreal.Exporter.run_asset_export_task(task)
    log("export %s ok=%s exists=%s" % (name, ok, os.path.exists(task.filename)))


def options(**values):
    result = unreal.FbxExportOption()
    result.collision = False
    result.level_of_detail = False
    # Material baking renders through the skinned mesh object, which does not exist under -nullrhi (assert).
    result.bake_material_inputs = unreal.FbxMaterialBakeMode.DISABLED
    for key, value in values.items():
        result.set_editor_property(key, value)
    return result


def run():
    os.makedirs(OUT, exist_ok=True)
    for name, path in STATICS.items():
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING " + path)
            continue
        bounds = mesh.get_bounds()
        log("SM %s %s tris=%d lods=%d nanite=%s origin=(%.1f %.1f %.1f) extent=(%.1f %.1f %.1f)" % (
            name, path, mesh.get_num_triangles(0), mesh.get_num_lods(),
            mesh.get_editor_property("nanite_settings").enabled, bounds.origin.x, bounds.origin.y, bounds.origin.z,
            bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z))
        for index, static_material in enumerate(mesh.static_materials):
            log("    slot %d %s -> %s" % (index, static_material.material_slot_name,
                                         describe_material(static_material.material_interface)))
        export(mesh, unreal.StaticMeshExporterFBX(), name, options())
    for name, path in SKELETALS.items():
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING " + path)
            continue
        bounds = mesh.get_bounds()
        skeleton = mesh.get_editor_property("skeleton")
        log("SK %s %s skeleton=%s origin=(%.1f %.1f %.1f) extent=(%.1f %.1f %.1f)" % (
            name, path, skeleton.get_path_name() if skeleton else None, bounds.origin.x, bounds.origin.y,
            bounds.origin.z, bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z))
        for index, skeletal_material in enumerate(mesh.materials):
            log("    slot %d %s -> %s" % (index, skeletal_material.material_slot_name,
                                         describe_material(skeletal_material.material_interface)))
        export(mesh, unreal.SkeletalMeshExporterFBX(), name, options())
    for name, path in ANIMS.items():
        anim = unreal.load_asset(path)
        if anim is None:
            log("MISSING " + path)
            continue
        preview = anim.get_preview_mesh() if hasattr(anim, "get_preview_mesh") else None
        log("ANIM %s %s length=%.3f frames=%s preview=%s" % (
            name, path, anim.get_editor_property("sequence_length") if hasattr(anim, "sequence_length") else
            unreal.AnimationLibrary.get_sequence_length(anim),
            unreal.AnimationLibrary.get_num_frames(anim), preview.get_path_name() if preview else None))
        export(anim, unreal.AnimSequenceExporterFBX(), "anim_" + name, options(export_preview_mesh=True))

    # Which meshes does the deployed building blueprint use?
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    dep_options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                        include_hard_package_references=True)
    for package in (BUILDING + "Ger_Airbase_Truck",
                    "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/Airbase/DA_Airbase_Deploy"):
        for dependency in registry.get_dependencies(package, dep_options) or []:
            text = str(dependency)
            if "Mesh" in text or "Aircraft" in text or "Airbase" in text or "SK_" in text or "SM_" in text:
                log("dep %s -> %s" % (package.split("/")[-1], text))


try:
    run()
except Exception:
    log(traceback.format_exc())
with open(os.path.join(OUT, "sources.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
