"""Read-only: how many Ju 87 triangles each accent box of build_airfield_a.py selects, and where the wing tips are.

Run headless: sh run_headless.sh dump_stuka_boxes   Log: Saved/WorldBuildSitePieces/airfield_candidate_A/stuka_boxes.txt
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_candidate_A")
STUKA = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"
BOXES = [((-320.0, 430.0, -50.0), (320.0, 700.0, 450.0)),
         ((-320.0, -700.0, -50.0), (320.0, -430.0, 450.0)),
         ((215.0, -160.0, -50.0), (420.0, 160.0, 450.0)),
         ((-440.0, -75.0, -50.0), (-380.0, 75.0, 450.0))]
LINES = []
try:
    mesh = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        unreal.load_asset(STUKA), mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD())
    for low, high in BOXES:
        for label, box in (("positional", unreal.Box(unreal.Vector(*low), unreal.Vector(*high))),
                           ):
            for points in (1, 3):
                result = unreal.GeometryScript_MeshSelection.select_mesh_elements_in_box(
                    mesh, box, unreal.GeometryScriptMeshSelectionType.TRIANGLES, False, points)
                selection = result[-1] if isinstance(result, tuple) else result
                info = unreal.GeometryScript_MeshSelection.get_mesh_selection_info(selection)
                LINES.append("box %s-%s %s points %d valid %s -> %s" % (low, high, label, points,
                                                                        box.get_editor_property("is_valid"), info))
    # Wing tip vertices: the 20 largest |y|.
    positions = []
    for vertex in range(mesh.get_vertex_count()):
        result = unreal.GeometryScript_MeshQueries.get_vertex_position(mesh, vertex)
        vector = next((item for item in (result if isinstance(result, tuple) else (result,))
                       if isinstance(item, unreal.Vector)), None)
        if vector is not None:
            positions.append(vector)
    for vector in sorted(positions, key=lambda p: -abs(p.y))[:12]:
        LINES.append("tip vertex (%.0f %.0f %.0f)" % (vector.x, vector.y, vector.z))
except Exception:
    import traceback
    LINES.append(traceback.format_exc())
os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, "stuka_boxes.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
unreal.log("AIRFIELD_A stuka boxes written")
