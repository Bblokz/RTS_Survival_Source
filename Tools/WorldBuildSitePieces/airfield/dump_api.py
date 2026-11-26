"""Headless read-only: GeometryScript python names, Ju 87 triangle count and frame material textures."""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
LINES = []


def log(text):
    LINES.append(str(text))


skip = set(dir(unreal.Object))
for name in sorted(dir(unreal)):
    if name.startswith("GeometryScript") and ("Functions" in name or name.startswith("GeometryScript_")):
        cls = getattr(unreal, name)
        if isinstance(cls, type) and issubclass(cls, unreal.Object):
            members = [m for m in dir(cls) if not m.startswith("_") and m not in skip]
            log(name + ": " + ", ".join(members))

for path in ["/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO",
             "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/ME410/Meshes/SK_Ger_Aircraft_Me410"]:
    skel = unreal.load_asset(path)
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    lod = unreal.GeometryScriptMeshReadLOD()
    result = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(skel, mesh, options, lod)
    log("%s copy -> %s tris=%d" % (path, result, mesh.get_triangle_count()))

material = unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Materials/M_Stuka_JU87_Frame")
log("frame material %s" % material)
for texture in unreal.MaterialEditingLibrary.get_used_textures(material):
    log("  tex %s %dx%d" % (texture.get_path_name(), texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
for kind, getter in (("scalar", unreal.MaterialEditingLibrary.get_scalar_parameter_names),
                     ("vector", unreal.MaterialEditingLibrary.get_vector_parameter_names),
                     ("texture", unreal.MaterialEditingLibrary.get_texture_parameter_names)):
    log("  %s params %s" % (kind, getter(material)))
for path in ["/Game/MilitaryBaseCamp/Textures/T_Opentent_GerDesert2",
             "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/Materials/Textures/T_Props_2_BC"]:
    texture = unreal.load_asset(path)
    log("  tex %s %dx%d" % (path, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))

with open(os.path.join(OUT, "airfield_api.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
