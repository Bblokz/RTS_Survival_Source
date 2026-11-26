"""Headless: lists the candidate source assets for the battery piece and exports them as FBX for Blender.

Run: UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nullrhi
Out: Saved/WorldBuildSitePieces/battery/src/*.fbx and dump.txt
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "battery", "src")
os.makedirs(OUT, exist_ok=True)
LINES = []

PITS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/"
MESHES = [
    "/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/LFH18/Mesh/SK_Ger_SFH18Anim",
    PITS + "Meshes/SM_GerPit_Howitzer",
    PITS + "Meshes/SM_GerPit_Shell_150",
    PITS + "Meshes/SM_GerPit_Shell_105",
]
BLUEPRINTS = [PITS + "Ger_BXP_GerPit_SFH18", PITS + "Turrets/RTS_BP_EmbTurret_GerPit_SFH18"]


def log(text):
    LINES.append(str(text))
    unreal.log("BATTERYDUMP " + str(text))


def describe_material(material, indent="    "):
    if material is None:
        return
    log("%smaterial %s (%s)" % (indent, material.get_path_name(), material.get_class().get_name()))
    if isinstance(material, unreal.MaterialInstance):
        parent = material.get_editor_property("parent")
        log("%s  parent %s" % (indent, parent.get_path_name() if parent else None))
        for value in material.get_editor_property("texture_parameter_values"):
            log("%s  tex %s = %s" % (indent, value.parameter_info.name,
                                     value.parameter_value.get_path_name() if value.parameter_value else None))
        for value in material.get_editor_property("scalar_parameter_values"):
            log("%s  scalar %s = %.3f" % (indent, value.parameter_info.name, value.parameter_value))
        for value in material.get_editor_property("vector_parameter_values"):
            log("%s  vector %s = %s" % (indent, value.parameter_info.name, value.parameter_value))
        material = material.get_base_material()
    for texture in unreal.MaterialEditingLibrary.get_used_textures(material):
        log("%s  base uses %s" % (indent, texture.get_path_name()))


def export_fbx(asset, name):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(OUT, name + ".fbx")
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    if isinstance(asset, unreal.SkeletalMesh):
        task.exporter = unreal.SkeletalMeshExporterFBX()
    else:
        task.exporter = unreal.StaticMeshExporterFBX()
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    task.options = options
    ok = unreal.Exporter.run_asset_export_task(task)
    log("  export %s -> %s (%s)" % (name, ok, os.path.exists(task.filename)))


for path in MESHES:
    asset = unreal.load_asset(path)
    if asset is None:
        log("MISSING " + path)
        continue
    bounds = asset.get_bounds()
    if isinstance(asset, unreal.StaticMesh):
        log("%s tris=%d lods=%d" % (path, asset.get_num_triangles(0), asset.get_num_lods()))
        materials = [(m.material_slot_name, m.material_interface) for m in asset.static_materials]
    else:
        log("%s skeletal" % path)
        materials = [(m.material_slot_name, m.material_interface) for m in asset.materials]
    log("  origin=(%.1f %.1f %.1f) extent=(%.1f %.1f %.1f)" % (
        bounds.origin.x, bounds.origin.y, bounds.origin.z, bounds.box_extent.x, bounds.box_extent.y,
        bounds.box_extent.z))
    for slot, material in materials:
        log("  slot %s" % slot)
        describe_material(material)
    if isinstance(asset, unreal.StaticMesh):  # skeletal FBX export crashes under -nullrhi (material baking)
        export_fbx(asset, path.split("/")[-1])

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for path in BLUEPRINTS:
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        log("MISSING " + path)
        continue
    log("=== " + path)
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is None or template.get_name() in seen or not isinstance(template, unreal.SceneComponent):
            continue
        seen.add(template.get_name())
        parent = template.get_attach_parent()
        mesh = None
        if isinstance(template, unreal.StaticMeshComponent):
            mesh = template.static_mesh
        elif isinstance(template, unreal.SkinnedMeshComponent):
            mesh = template.get_editor_property("skeletal_mesh_asset") if hasattr(template, "skeletal_mesh_asset") \
                else None
        log("  %s [%s] parent=%s loc=(%.1f %.1f %.1f) rot=(%.1f %.1f %.1f) scale=(%.3f %.3f %.3f) mesh=%s" % (
            template.get_name(), template.get_class().get_name(), parent.get_name() if parent else None,
            template.relative_location.x, template.relative_location.y, template.relative_location.z,
            template.relative_rotation.pitch, template.relative_rotation.yaw, template.relative_rotation.roll,
            template.relative_scale3d.x, template.relative_scale3d.y, template.relative_scale3d.z,
            mesh.get_path_name() if mesh else None))
        if isinstance(template, unreal.InstancedStaticMeshComponent):
            log("    instances=%d" % template.get_instance_count())
        if isinstance(template, unreal.MeshComponent):
            for index in range(template.get_num_materials()):
                material = template.get_material(index)
                log("    mat%d %s" % (index, material.get_path_name() if material else None))
    generated = blueprint.generated_class()
    default_object = unreal.get_default_object(generated)
    for component in default_object.get_components_by_class(unreal.SceneComponent):
        log("  CDO %s [%s]" % (component.get_name(), component.get_class().get_name()))

with open(os.path.join(OUT, "..", "dump.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
