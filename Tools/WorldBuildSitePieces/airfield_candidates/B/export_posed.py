"""Candidate B: writes the source models as OBJ files for the Blender step, with the airbase roof in its OPEN pose.

Skeletal FBX export asserts under -nullrhi (FFbxMaterialBakingMeshData always builds a skinned mesh object), so the
meshes are read with Geometry Script and the roof is skinned here in python (linear blend skinning of the reference
mesh with the bone transforms of Anim_GerAirbase_OpenStationary). Read-only for every source asset.

Run headless: sh run_py.sh export_posed.py
Out: Saved/WorldBuildSitePieces/airfield_B/<name>.obj + posed.txt
OBJ coordinates: metres, Blender axes (x, -y, z of Unreal), triangles wound for the mirrored frame.
"""
import os
import traceback

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_B")
BUILDING = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/"
BASE = BUILDING + "BuildingMeshes/SM_Ger_AircraftBuilding_BaseMesh"
ROOF = BUILDING + "BuildingMeshes/SK_Ger_AircraftBuilding_Roof"
OPEN_ANIM = BUILDING + "BuildingMeshes/Anims/Anim_GerAirbase_OpenStationary"
STUKA = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"
LINES = []
gs_assets = unreal.GeometryScript_AssetUtils
gs_query = unreal.GeometryScript_MeshQueries
gs_bones = unreal.GeometryScript_BoneWeights
gs_mat = unreal.GeometryScript_Materials


def log(text):
    LINES.append(str(text))
    unreal.log("AIRFIELDB " + str(text))
    with open(os.path.join(OUT, "posed.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(LINES))


def last(result):
    return result[-1] if isinstance(result, tuple) else result


def pick(result, kind):
    """Python returns out-parameters as a tuple in no fixed order: take the first value of the wanted type."""
    values = result if isinstance(result, tuple) else (result,)
    for value in values:
        if kind is int and isinstance(value, bool):
            continue
        if isinstance(value, kind):
            return value
    raise TypeError("no %s in %r" % (kind, result))


def mesh_data(mesh):
    """Vertex positions by id, triangles (a, b, c, material id, uv triples)."""
    positions = unreal.GeometryScript_List.convert_vector_list_to_array(
        pick(gs_query.get_all_vertex_positions(mesh, False), unreal.GeometryScriptVectorList))
    triangle_ids = unreal.GeometryScript_List.convert_index_list_to_array(
        pick(gs_query.get_all_triangle_i_ds(mesh), unreal.GeometryScriptIndexList))
    triangles = []
    for triangle_id in triangle_ids:
        indices = pick(gs_query.get_triangle_indices(mesh, triangle_id), unreal.IntVector)
        material = pick(gs_mat.get_triangle_material_id(mesh, triangle_id), int)
        uvs = gs_query.get_triangle_u_vs(mesh, 0, triangle_id)
        uv_list = [value for value in uvs if isinstance(value, unreal.Vector2D)] if isinstance(uvs, tuple) else []
        if len(uv_list) != 3:
            uv_list = [unreal.Vector2D(0, 0)] * 3
        triangles.append((indices.x, indices.y, indices.z, material, uv_list))
    return positions, triangles


def write_obj(name, positions, triangles, material_names):
    path = os.path.join(OUT, name + ".obj")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("o %s\n" % name)
        for position in positions:
            handle.write("v %.5f %.5f %.5f\n" % (position.x / 100.0, -position.y / 100.0, position.z / 100.0))
        uv_index = 1
        current = None
        for a, b, c, material, uvs in sorted(triangles, key=lambda t: t[3]):
            if material != current:
                current = material
                label = material_names[material] if 0 <= material < len(material_names) else "slot%d" % material
                handle.write("usemtl %s\n" % label)
            # Mirrored Y: reverse the winding (a, c, b) so the normals stay outward.
            for uv in (uvs[0], uvs[2], uvs[1]):
                handle.write("vt %.5f %.5f\n" % (uv.x, 1.0 - uv.y))
            handle.write("f %d/%d %d/%d %d/%d\n" % (a + 1, uv_index, c + 1, uv_index + 2, b + 1, uv_index + 1))
            uv_index += 3
    log("wrote %s: %d verts %d tris materials %s" % (path, len(positions), len(triangles), material_names))


def material_names_static(static_mesh):
    return ["%d_%s" % (index, str(m.material_slot_name)) for index, m in enumerate(static_mesh.static_materials)]


def material_names_skeletal(skeletal_mesh):
    return ["%d_%s" % (index, str(m.material_slot_name)) for index, m in enumerate(skeletal_mesh.materials)]


def bounds_text(positions):
    xs = [p.x for p in positions]
    ys = [p.y for p in positions]
    zs = [p.z for p in positions]
    return "x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f" % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))


def export_static(name, path):
    static_mesh = unreal.load_asset(path)
    mesh = unreal.DynamicMesh()
    read_lod = unreal.GeometryScriptMeshReadLOD()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = True
    gs_assets.copy_mesh_from_static_mesh_v2(static_mesh, mesh, options, read_lod, False)
    positions, triangles = mesh_data(mesh)
    log("%s %s" % (name, bounds_text(positions)))
    write_obj(name, positions, triangles, material_names_static(static_mesh))


def skeletal_dynamic(path):
    skeletal_mesh = unreal.load_asset(path)
    mesh = unreal.DynamicMesh()
    gs_assets.copy_mesh_from_skeletal_mesh(skeletal_mesh, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                           unreal.GeometryScriptMeshReadLOD())
    return skeletal_mesh, mesh


def export_stuka():
    skeletal_mesh, mesh = skeletal_dynamic(STUKA)
    positions, triangles = mesh_data(mesh)
    log("stuka %s" % bounds_text(positions))
    write_obj("stuka", positions, triangles, material_names_skeletal(skeletal_mesh))


def export_roof():
    skeletal_mesh, mesh = skeletal_dynamic(ROOF)
    has_weights = gs_bones.mesh_has_bone_weights(mesh)
    log("roof has bone weights: %s" % (has_weights,))
    bones = pick(gs_bones.get_all_bones_info(mesh), (list, unreal.Array))
    bone_names = {}
    for info in bones:
        bone_names[info.index] = str(info.name)
    log("roof bones %d: %s" % (len(bone_names), sorted(bone_names.values())[:60]))

    anim = unreal.load_asset(OPEN_ANIM)
    evaluation = unreal.AnimPoseEvaluationOptions()
    evaluation.set_editor_property("optional_skeletal_mesh", skeletal_mesh)
    length = unreal.AnimationLibrary.get_sequence_length(anim)
    pose = pick(unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, length * 0.5, evaluation), unreal.AnimPose)
    matrices = {}
    space = unreal.AnimPoseSpaces.WORLD
    for index, name in bone_names.items():
        reference = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, name, space)
        animated = unreal.AnimPoseExtensions.get_bone_pose(pose, name, space)
        matrices[index] = (reference, animated)
        moved = (animated.translation - reference.translation).length()
        if moved > 1.0 or abs(animated.rotation.get_angle() - reference.rotation.get_angle()) > 0.01:
            log("  bone %s moves %.0f cm: ref t %s r %s -> t %s r %s" % (
                name, moved, reference.translation, reference.rotation.rotator(), animated.translation,
                animated.rotation.rotator()))

    positions, triangles = mesh_data(mesh)
    log("roof reference %s" % bounds_text(positions))
    posed = []
    unweighted = 0
    for vertex_id, position in enumerate(positions):
        result = gs_bones.get_vertex_bone_weights(mesh, vertex_id)
        weights = [w for w in pick(result, (list, unreal.Array)) if w.weight > 0.0]
        if not weights:
            unweighted += 1
            posed.append(position)
            continue
        total = sum(w.weight for w in weights)
        moved = unreal.Vector(0.0, 0.0, 0.0)
        for weight in weights:
            reference, animated = matrices[weight.bone_index]
            local = reference.inverse_transform_location(position)
            moved = moved + animated.transform_location(local) * (weight.weight / total)
        posed.append(moved)
    log("roof posed %s (unweighted verts %d)" % (bounds_text(posed), unweighted))
    names = material_names_skeletal(skeletal_mesh)
    write_obj("roof_ref", positions, triangles, names)
    write_obj("roof_open", posed, triangles, names)


def run():
    os.makedirs(OUT, exist_ok=True)
    export_static("base", BASE)
    export_stuka()
    export_roof()


try:
    run()
except Exception:
    log(traceback.format_exc())
