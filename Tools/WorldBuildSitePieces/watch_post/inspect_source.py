"""Read-only look at the Soviet metal guard tower the watch post piece is made from.

Run headless:
  UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nullrhi
Log: Saved/WorldBuildSitePieces/inspect.txt
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
RUS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/"
MESHES = [RUS + "GuardMGTower/SM_Rus_GuardMgTower", RUS + "GuardTower/SM_GuardTowerBase",
          RUS + "LargeGuardTower/SM_Rus_Watchtower",
          "/Game/RTS_Survival/Blueprints/Turrets/Russian/HullWeapons/Meshes/IS-3_TopMG"]
MATERIALS = [RUS + "Materials/MI_Tower", RUS + "Materials/MI_Tower_Soviet",
             "/Game/RTS_Survival/Meshes/Industrial/Container/MeshesMat/M_container_Soviet"]
BLUEPRINTS = [RUS + "Rus_BXP_GuardTowerMG", RUS + "Rus_BXP_GuardTowerMG_Soviet"]
LINES = []


def log(text):
    LINES.append(str(text))


def describe_mesh(path):
    mesh = unreal.load_asset(path)
    if mesh is None:
        log("MISSING " + path)
        return
    bounds = mesh.get_bounds()
    nanite = mesh.get_editor_property("nanite_settings")
    log("=== %s lods=%d tris0=%d nanite=%s" % (path, mesh.get_num_lods(), mesh.get_num_triangles(0), nanite.enabled))
    box = mesh.get_bounding_box()
    log("  box min (%.1f %.1f %.1f) max (%.1f %.1f %.1f)" % (box.min.x, box.min.y, box.min.z,
                                                           box.max.x, box.max.y, box.max.z))
    log("  bounds origin (%.1f %.1f %.1f) extent (%.1f %.1f %.1f)" % (
        bounds.origin.x, bounds.origin.y, bounds.origin.z, bounds.box_extent.x, bounds.box_extent.y,
        bounds.box_extent.z))
    for index, static_material in enumerate(mesh.static_materials):
        interface = static_material.material_interface
        log("  slot %d %s -> %s" % (index, static_material.material_slot_name,
                                    interface.get_path_name() if interface else None))
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for lod in range(mesh.get_num_lods()):
        log("  lod %d tris %d sections %d" % (lod, mesh.get_num_triangles(lod), mesh.get_num_sections(lod)))
    log("  uv channels lod0: %d" % subsystem.get_num_uv_channels(mesh, 0))


def describe_material(path):
    material = unreal.load_asset(path)
    if material is None:
        log("MISSING " + path)
        return
    log("=== %s (%s)" % (path, material.get_class().get_name()))
    if isinstance(material, unreal.MaterialInstance):
        parent = material.get_editor_property("parent")
        log("  parent " + (parent.get_path_name() if parent else "None"))
        for value in material.get_editor_property("texture_parameter_values"):
            log("  tex %s = %s" % (value.parameter_info.name,
                                   value.parameter_value.get_path_name() if value.parameter_value else None))
        for value in material.get_editor_property("vector_parameter_values"):
            color = value.parameter_value
            log("  vec %s = (%.3f %.3f %.3f %.3f)" % (value.parameter_info.name, color.r, color.g, color.b, color.a))
        for value in material.get_editor_property("scalar_parameter_values"):
            log("  scalar %s = %.3f" % (value.parameter_info.name, value.parameter_value))
    base = material.get_base_material()
    log("  base " + base.get_path_name())
    for texture in unreal.MaterialEditingLibrary.get_used_textures(base):
        log("  base uses " + texture.get_path_name())
    for name in unreal.MaterialEditingLibrary.get_texture_parameter_names(base):
        log("  base tex param " + str(name))
    for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(base):
        log("  base vec param " + str(name))
    for name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(base):
        log("  base scalar param " + str(name))


def describe_blueprint(path):
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        log("MISSING " + path)
        return
    log("=== " + path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is None or template.get_name() in seen or not isinstance(template, unreal.SceneComponent):
            continue
        seen.add(template.get_name())
        mesh = template.static_mesh if isinstance(template, unreal.StaticMeshComponent) else None
        materials = []
        if isinstance(template, unreal.MeshComponent):
            materials = [m.get_name() if m else None for m in template.get_editor_property("override_materials")]
        parent = template.get_attach_parent()
        log("  %s (%s) parent=%s loc=(%.1f %.1f %.1f) rot=(%.1f %.1f %.1f) scale=(%.3f %.3f %.3f) mesh=%s over=%s" % (
            template.get_name(), template.get_class().get_name(), parent.get_name() if parent else None,
            template.relative_location.x, template.relative_location.y, template.relative_location.z,
            template.relative_rotation.pitch, template.relative_rotation.yaw, template.relative_rotation.roll,
            template.relative_scale3d.x, template.relative_scale3d.y, template.relative_scale3d.z,
            mesh.get_path_name() if mesh else None, materials))


for function, paths in ((describe_mesh, MESHES),):
    for asset_path in paths:
        try:
            function(asset_path)
        except Exception as error:
            log("FAILED %s: %s" % (asset_path, error))

os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, "inspect.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
