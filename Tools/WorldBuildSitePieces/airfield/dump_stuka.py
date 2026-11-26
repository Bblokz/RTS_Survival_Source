"""Headless read-only: textures the Stuka frame material depends on, and which X end of the Stuka mesh is the nose."""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
LINES = []


def run():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                    include_hard_package_references=True)
    for package in ["/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Materials/M_Stuka_JU87_Frame"]:
        for dependency in registry.get_dependencies(package, options) or []:
            LINES.append("dep %s" % dependency)
    for name in ("BaseColor", "Camo", "Normal", "ORM"):
        texture = unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Materials/Plane_stuka_01_" + name)
        if texture:
            LINES.append("tex %s %dx%d srgb=%s comp=%s" % (name, texture.blueprint_get_size_x(), texture.blueprint_get_size_y(),
                                                           texture.srgb, texture.compression_settings))

    skel = unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO")
    mesh = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        skel, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    result = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)
    LINES.append("get_all_vertex_positions -> %s" % (type(result),))
    positions = result[1]
    points = unreal.GeometryScript_List.convert_vector_list_to_array(positions)
    min_x = min(p.x for p in points)
    max_x = max(p.x for p in points)
    span = max(abs(p.y) for p in points)
    wing = [p for p in points if abs(p.y) > span * 0.8]
    LINES.append("x %.1f..%.1f z %.1f..%.1f span %.1f" % (min_x, max_x, min(p.z for p in points),
                                                         max(p.z for p in points), span))
    LINES.append("wing tips at x mean %.1f (closer to the nose end)" % (sum(p.x for p in wing) / len(wing)))
    # The tail fin is the highest point; the nose is the other end.
    top = max(points, key=lambda p: p.z)
    LINES.append("highest vertex (tail fin) at x %.1f y %.1f z %.1f" % (top.x, top.y, top.z))
    for band in range(10):
        x0 = min_x + (max_x - min_x) * band / 10.0
        x1 = min_x + (max_x - min_x) * (band + 1) / 10.0
        sel = [p for p in points if x0 <= p.x < x1]
        if sel:
            LINES.append("  x %.0f..%.0f: n=%d maxY=%.0f z %.0f..%.0f" % (
                x0, x1, len(sel), max(abs(p.y) for p in sel), min(p.z for p in sel), max(p.z for p in sel)))


try:
    run()
finally:
  with open(os.path.join(OUT, "airfield_stuka.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
