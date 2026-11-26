"""Headless probe: does setting material ids from a triangle id list work? Log: airfield_C/probe_ids.txt"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_C")
LINES = []
gs_list = unreal.GeometryScript_List
gs_mat = unreal.GeometryScript_Materials
mesh = unreal.DynamicMesh()
unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
    unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"), mesh,
    unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())


def counts():
    result = gs_mat.get_all_triangle_material_i_ds(mesh)
    ids = gs_list.convert_index_list_to_array([r for r in result if isinstance(r, unreal.GeometryScriptIndexList)][0])
    histogram = {}
    for value in ids:
        histogram[value] = histogram.get(value, 0) + 1
    return histogram


try:
    LINES.append("start %s" % counts())
    r = gs_list.convert_array_to_index_list(list(range(100)))
    LINES.append("convert_array_to_index_list -> %r" % (r,))
    index_list = r[-1] if isinstance(r, tuple) else r
    s = unreal.GeometryScript_MeshSelection.convert_index_list_to_mesh_selection(
        mesh, index_list, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
    LINES.append("convert_index_list_to_mesh_selection -> %r" % (s,))
    LINES.append("doc %s" % unreal.GeometryScript_MeshSelection.convert_index_list_to_mesh_selection.__doc__.splitlines()[0])
    LINES.append("doc %s" % gs_list.convert_array_to_index_list.__doc__.splitlines()[0])
    selection = [x for x in (s if isinstance(s, tuple) else (s,)) if isinstance(x, unreal.GeometryScriptMeshSelection)][0]
    LINES.append("selection info %s" % (unreal.GeometryScript_MeshSelection.get_mesh_selection_info(selection),))
    gs_mat.set_material_id_for_mesh_selection(mesh, selection, 7, False)
    LINES.append("after %s" % counts())
except Exception:
    import traceback
    LINES.append(traceback.format_exc())
with open(os.path.join(OUT, "probe_ids.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
