"""Headless read-only: do two mesh parts of a spawned tank carry the same geometry twice?

Run:  sh run_py.sh diag_overlap.py T26
Log:  Saved/WorldDivisionPieces/tank_overlap.txt
Written for the T-26, whose skeletal mesh (tracks, turret) and static hull seemed to both reach up to the gun. For
every pair of parts it reports the share of vertices that sit on a vertex of the other part (1 cm grid), per material
id and per height band. Saves nothing.
Result 2026-11-25: no doubled geometry (0 % shared). The baked T-26 hull only carries vertices without triangles at
the turret's place, which widened its box; build_tank_pieces.py now removes unused vertices.
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tank_config  # noqa: E402

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldDivisionPieces")
os.makedirs(OUT, exist_ok=True)
HANDLE = open(os.path.join(OUT, "tank_overlap.txt"), "w", encoding="utf-8")
gs_assets = unreal.GeometryScript_AssetUtils
gs_scene = unreal.GeometryScript_SceneUtils
gs_mat = unreal.GeometryScript_Materials
gs_query = unreal.GeometryScript_MeshQueries
gs_xform = unreal.GeometryScript_MeshTransforms
gs_list = unreal.GeometryScript_List
gs_split = unreal.GeometryScript_MeshDecomposition
SUCCESS = unreal.GeometryScriptOutcomePins.SUCCESS
BAND = 40.0


def log(text):
    HANDLE.write(str(text) + "\n")
    HANDLE.flush()


def positions(mesh):
    result = gs_query.get_all_vertex_positions(mesh, False)
    for item in result if isinstance(result, tuple) else (result,):
        if isinstance(item, unreal.GeometryScriptVectorList):
            return gs_list.convert_vector_list_to_array(item)
    return []


def key(point):
    return (round(point.x), round(point.y), round(point.z))


def copy_part(component, mesh):
    dynamic = unreal.DynamicMesh()
    result = gs_scene.copy_mesh_from_component(component, dynamic, unreal.GeometryScriptCopyMeshFromComponentOptions(), True)
    outcome = result[-1] if isinstance(result, tuple) else result
    if (outcome != SUCCESS or dynamic.get_triangle_count() == 0) and isinstance(mesh, unreal.SkeletalMesh):
        dynamic = unreal.DynamicMesh()
        gs_assets.copy_mesh_from_skeletal_mesh(mesh, dynamic, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                               unreal.GeometryScriptMeshReadLOD())
        gs_xform.transform_mesh(dynamic, component.get_world_transform())
    return dynamic


def split_by_material(mesh):
    """[(material id, vertex positions)]."""
    result = gs_split.split_mesh_by_material_i_ds(mesh, None)
    meshes, ids = None, None
    for item in result if isinstance(result, tuple) else (result,):
        if isinstance(item, (list, unreal.Array)) and len(item) > 0:
            if isinstance(item[0], unreal.DynamicMesh):
                meshes = item
            elif isinstance(item[0], int):
                ids = item
    if not meshes:
        return [(-1, positions(mesh))]
    return [(ids[index] if ids else index, positions(sub)) for index, sub in enumerate(meshes)]


def run():
    actors_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for tank in tank_config.selected(os.environ.get("DIVISIONTANKS", "T26")):
        log("== " + tank["name"])
        cls = unreal.EditorAssetLibrary.load_blueprint_class(tank["vehicle"])
        actor = actors_subsystem.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        parts = []
        for component in actor.get_components_by_class(unreal.MeshComponent):
            if isinstance(component, unreal.StaticMeshComponent):
                mesh = component.get_editor_property("static_mesh")
            elif isinstance(component, unreal.SkinnedMeshComponent):
                mesh = component.get_editor_property("skinned_asset")
            else:
                mesh = None
            if mesh is None or not component.is_visible():
                continue
            dynamic = copy_part(component, mesh)
            materials = [m.get_name() if m else None for m in component.get_materials()]
            log("part %s mesh %s tris %d materials %s" % (component.get_name(), mesh.get_name(),
                                                          dynamic.get_triangle_count(), materials))
            for name in ("visible", "hidden_in_game", "cast_shadow", "render_in_main_pass", "render_in_depth_pass",
                         "visible_in_scene_capture_only", "owner_no_see", "only_owner_see"):
                try:
                    log("    %s = %s" % (name, component.get_editor_property(name)))
                except Exception:
                    pass
            parts.append((component.get_name(), split_by_material(dynamic), materials))
        actors_subsystem.destroy_actor(actor)
        for name, groups, materials in parts:
            others = set()
            for other_name, other_groups, _materials in parts:
                if other_name != name:
                    for _material_id, points in other_groups:
                        others.update(key(point) for point in points)
            for material_id, points in groups:
                label = materials[material_id] if 0 <= material_id < len(materials) else "?"
                shared = sum(1 for point in points if key(point) in others)
                log("%s material id %d (%s): %d vertices, %d (%.0f %%) on a vertex of another part" % (
                    name, material_id, label, len(points), shared, 100.0 * shared / max(1, len(points))))
                bands = {}
                for point in points:
                    bands.setdefault(int(point.z // BAND), []).append(point)
                for band in sorted(bands):
                    group = bands[band]
                    band_shared = sum(1 for point in group if key(point) in others)
                    log("    z %3.0f..%3.0f: %5d vertices, %3.0f %% shared, x %.0f..%.0f y %.0f..%.0f" % (
                        band * BAND, (band + 1) * BAND, len(group), 100.0 * band_shared / len(group),
                        min(p.x for p in group), max(p.x for p in group), min(p.y for p in group), max(p.y for p in group)))


try:
    run()
except Exception:
    log(traceback.format_exc())
HANDLE.close()
unreal.log("DIVTANK overlap written")
