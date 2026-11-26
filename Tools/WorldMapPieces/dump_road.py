"""Lists how the world map roads are drawn now: renderer Blueprint defaults, road mesh and material.

Log: Saved/WorldMapPieces/road.txt, material text: Saved/WorldMapPieces/M_WorldConnection.t3d
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402

GENERAL = "/Game/RTS_Survival/WorldCampaign/MapObjects/General/"
blueprint = unreal.load_asset(GENERAL + "BP_WorldConnectionRenderer")
defaults = unreal.get_default_object(blueprint.generated_class())
for name in ("m_ribbon_class", "m_ribbon_mesh", "m_ribbon_material", "m_forward_axis", "m_ribbon_width",
             "m_mesh_width_at_unit_scale", "m_ribbon_z_offset", "m_curviness", "m_curve_sample_count",
             "m_cast_shadow", "m_allow_decals", "m_uniform_width"):
    try:
        lib.log("%s = %s" % (name, defaults.get_editor_property(name)))
    except Exception as error:
        lib.log("%s: %s" % (name, error))

mesh = defaults.get_editor_property("m_ribbon_mesh")
if mesh:
    bounds = mesh.get_bounding_box()
    lib.log("mesh %s bounds %s .. %s" % (mesh.get_path_name(), bounds.min, bounds.max))
    for slot in mesh.static_materials:
        lib.log("  slot %s -> %s" % (slot.material_slot_name, slot.material_interface.get_path_name()
                                     if slot.material_interface else None))
    description = mesh.get_static_mesh_description(0)
    us, vs = [], []
    for index in range(description.get_vertex_instance_count()):
        uv = description.get_vertex_instance_uv(unreal.VertexInstanceID(index), 0)
        us.append(uv.x)
        vs.append(uv.y)
    lib.log("  vertices %d triangles %d, u %.2f..%.2f v %.2f..%.2f" % (
        description.get_vertex_count(), description.get_triangle_count(), min(us), max(us), min(vs), max(vs)))

material = unreal.load_asset(GENERAL + "Materials/M_WorldConnection")
lib.log("material blend %s shading %s two sided %s" % (
    material.get_editor_property("blend_mode"), material.get_editor_property("shading_model"),
    material.get_editor_property("two_sided")))
task = unreal.AssetExportTask()
task.object = material
task.filename = os.path.join(lib.OUT_DIR, "M_WorldConnection.t3d")
task.automated = True
task.replace_identical = True
task.prompt = False
lib.log("export ok: %s" % unreal.Exporter.run_asset_export_task(task))
lib.write_log("road.txt")
