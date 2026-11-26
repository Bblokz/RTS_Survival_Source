"""Headless check of the saved candidate C mesh: triangles, bounds, footprint radius, Nanite, collision, slots.
Run: sh run_py.sh check_piece.py    Log: Saved/WorldBuildSitePieces/airfield_C/check.txt"""
import math
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_C")
PATH = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/C/SM_BuildSitePiece_Airfield_C"
LINES = []
mesh = unreal.load_asset(PATH)
box = mesh.get_bounding_box()
description = mesh.get_static_mesh_description(0)
radius = 0.0
for index in range(description.get_vertex_count()):
    position = description.get_vertex_position(unreal.VertexID(index))
    radius = max(radius, math.hypot(position.x, position.y))
body = mesh.get_editor_property("body_setup")
shapes = -1
if body is not None:
    geometry = body.get_editor_property("agg_geom")
    shapes = sum(len(geometry.get_editor_property(kind)) for kind in
                 ("sphere_elems", "box_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems"))
LINES.append("tris %d lods %d nanite %s collision shapes %d trace %s" % (
    mesh.get_num_triangles(0), mesh.get_num_lods(), mesh.get_editor_property("nanite_settings").enabled, shapes,
    body.get_editor_property("collision_trace_flag") if body else None))
LINES.append("box (%.2f %.2f %.2f)..(%.2f %.2f %.2f) footprint radius %.2f height %.2f" % (
    box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, radius, box.max.z - box.min.z))
for index, material in enumerate(mesh.static_materials):
    LINES.append("slot %d %s -> %s" % (index, material.material_slot_name,
                                       material.material_interface.get_path_name() if material.material_interface else None))
with open(os.path.join(OUT, "check.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
