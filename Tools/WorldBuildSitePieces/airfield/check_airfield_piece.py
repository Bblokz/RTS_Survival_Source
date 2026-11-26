"""Headless check of the saved airfield piece. Log: Saved/WorldBuildSitePieces/airfield_check.txt"""
import math
import os

import unreal

PATH = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/SM_BuildSitePiece_Airfield"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
LINES = []


def run():
    mesh = unreal.load_asset(PATH)
    LINES.append("tris lod0 %d, lods %d, nanite %s" % (mesh.get_num_triangles(0), mesh.get_num_lods(),
                                                      mesh.get_editor_property("nanite_settings").enabled))
    for index, slot in enumerate(mesh.static_materials):
        LINES.append("slot %d %s -> %s" % (index, slot.material_slot_name,
                                           slot.material_interface.get_path_name() if slot.material_interface else None))
    body = mesh.get_editor_property("body_setup")
    LINES.append("simple collision shapes %d, trace flag %s" % (
        unreal.GeometryScript_Collision.get_simple_collision_shape_count(
            unreal.GeometryScript_Collision.get_simple_collision_from_static_mesh(mesh)),
        body.get_editor_property("collision_trace_flag") if body else None))
    dynamic = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh, dynamic, options,
                                                                   unreal.GeometryScriptMeshReadLOD(), False)
    points = unreal.GeometryScript_List.convert_vector_list_to_array(
        unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dynamic, False)[1])
    radius = max(math.hypot(p.x, p.y) for p in points)
    LINES.append("vertices %d, max radius %.2f (limit 20), z %.3f..%.3f (height limit 30)" % (
        len(points), radius, min(p.z for p in points), max(p.z for p in points)))
    colors = unreal.GeometryScript_MeshQueries.get_has_vertex_colors(dynamic)
    LINES.append("has vertex colours %s" % (colors,))


try:
    run()
except Exception as error:
    import traceback
    LINES.append(traceback.format_exc())
with open(os.path.join(OUT, "airfield_check.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
