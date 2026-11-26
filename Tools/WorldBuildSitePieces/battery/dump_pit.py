"""Headless: textures of the pit materials, LOD triangle counts, and the pit FBX with all LODs (for Blender)."""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "battery", "src")
PIT = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/SM_GerPit_Howitzer"
LINES = []


def log(text):
    LINES.append(str(text))
    unreal.log("BATTERYDUMP " + str(text))


mesh = unreal.load_asset(PIT)
for lod in range(mesh.get_num_lods()):
    log("lod %d tris=%d" % (lod, mesh.get_num_triangles(lod)))
for static_material in mesh.static_materials:
    material = static_material.material_interface
    log("slot %s %s" % (static_material.material_slot_name, material.get_path_name()))
    base = material.get_base_material()
    for texture in unreal.MaterialEditingLibrary.get_used_textures(base):
        log("   tex %s srgb=%s" % (texture.get_path_name(), texture.srgb))
    for name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(base):
        log("   scalar %s" % name)
    for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(base):
        log("   vector %s" % name)
    for name in unreal.MaterialEditingLibrary.get_texture_parameter_names(base):
        log("   texparam %s" % name)

task = unreal.AssetExportTask()
task.object = mesh
task.filename = os.path.join(OUT, "SM_GerPit_Howitzer_LODs.fbx")
task.automated = True
task.replace_identical = True
task.prompt = False
task.exporter = unreal.StaticMeshExporterFBX()
options = unreal.FbxExportOption()
options.collision = False
options.level_of_detail = True
task.options = options
log("export lods %s" % unreal.Exporter.run_asset_export_task(task))

with open(os.path.join(OUT, "..", "dump_pit.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
