"""Headless probe: bone weight / anim pose API on the airbase roof, plus OBJ dumps of the sources for Blender previews.
Run: sh run_py.sh probe_pose.py    Log: Saved/WorldBuildSitePieces/airfield_C/probe.txt, OBJs in airfield_C/obj/
"""
import os
import traceback

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_C")
AB = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/BuildingMeshes/"
LINES = []
bw = unreal.GeometryScript_BoneWeights


def log(text):
    LINES.append(str(text))


def doc(obj, name):
    try:
        log("DOC %s: %s" % (name, getattr(obj, name).__doc__.strip().splitlines()[0:6]))
    except Exception as error:
        log("DOC %s failed %s" % (name, error))


def write_obj(mesh, name):
    positions = unreal.GeometryScript_List.convert_vector_list_to_array(
        unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)[1])
    triangles = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, False)
    log("get_all_triangle_indices -> %s" % (type(triangles),))
    tri_list = triangles[1] if isinstance(triangles, tuple) else triangles
    tri_array = unreal.GeometryScript_List.convert_triangle_list_to_array(tri_list) \
        if hasattr(unreal.GeometryScript_List, "convert_triangle_list_to_array") else tri_list
    ids = unreal.GeometryScript_Materials.get_all_triangle_material_i_ds(mesh)
    ids = ids[1] if isinstance(ids, tuple) else ids
    ids = unreal.GeometryScript_List.convert_index_list_to_array(ids)
    os.makedirs(os.path.join(OUT, "obj"), exist_ok=True)
    with open(os.path.join(OUT, "obj", name + ".obj"), "w") as handle:
        for p in positions:
            handle.write("v %.3f %.3f %.3f\n" % (p.x, p.y, p.z))
        current = None
        for index, tri in enumerate(tri_array):
            material = ids[index] if index < len(ids) else 0
            if material != current:
                handle.write("usemtl m%d\n" % material)
                current = material
            handle.write("f %d %d %d\n" % (tri.x + 1, tri.y + 1, tri.z + 1))
    log("OBJ %s verts %d tris %d" % (name, len(positions), len(tri_array)))


def run():
    if os.environ.get("PROBE_OBJ_ONLY") == "1":
        roof_asset = unreal.load_asset(AB + "SK_Ger_AircraftBuilding_Roof")
        roof = unreal.DynamicMesh()
        unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
            roof_asset, roof, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        info = bw.get_all_bones_info(roof)[1]
        log("bones: %s" % [(index, str(b.name)) for index, b in enumerate(info)])
        counts = {}
        for vertex in range(roof.get_vertex_count() if hasattr(roof, "get_vertex_count") else 0):
            weight = bw.get_largest_vertex_bone_weight(roof, vertex, unreal.GeometryScriptBoneWeightProfile())[1]
            counts[weight.bone_index] = counts.get(weight.bone_index, 0) + 1
        log("vertices per bone %s" % counts)
        write_obj(roof, "roof_ref")
        base = unreal.DynamicMesh()
        options = unreal.GeometryScriptCopyMeshFromAssetOptions()
        options.apply_build_settings = True
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
            unreal.load_asset(AB + "SM_Ger_AircraftBuilding_BaseMesh"), base, options,
            unreal.GeometryScriptMeshReadLOD(), False)
        write_obj(base, "base")
        stuka = unreal.DynamicMesh()
        unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
            unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"), stuka,
            unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        write_obj(stuka, "stuka")
        return
    for name in ("get_largest_vertex_bone_weight", "get_vertex_bone_weights", "get_all_bones_info", "mesh_has_bone_weights",
                 "copy_bones_from_skeleton", "get_bone_info"):
        doc(bw, name)
    doc(unreal.AnimPoseExtensions, "get_anim_pose_at_frame")
    doc(unreal.GeometryScript_MeshEdits, "set_vertex_position")
    doc(unreal.GeometryScript_MeshEdits, "set_all_mesh_vertex_positions")
    doc(unreal.GeometryScript_AssetUtils, "copy_mesh_from_skeletal_mesh")

    roof_asset = unreal.load_asset(AB + "SK_Ger_AircraftBuilding_Roof")
    roof = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        roof_asset, roof, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    log("has bone weights: %s" % (bw.mesh_has_bone_weights(roof, unreal.GeometryScriptBoneWeightProfile()),))
    try:
        log("bones info: %s" % (bw.get_all_bones_info(roof),))
    except Exception as error:
        log("bones info failed %s" % error)
    try:
        log("vertex 0 largest: %s" % (bw.get_largest_vertex_bone_weight(roof, 0, unreal.GeometryScriptBoneWeightProfile()),))
        log("vertex 0 weights: %s" % (bw.get_vertex_bone_weights(roof, 0, unreal.GeometryScriptBoneWeightProfile()),))
    except Exception as error:
        log("weights failed %s" % error)

    for anim_name in ("Anim_GerAirbase_OpenStationary", "Anim_GerAirbase_CloseToOpen"):
        anim = unreal.load_asset(AB + "Anims/" + anim_name)
        frames = unreal.AnimationLibrary.get_num_frames(anim)
        log("ANIM %s frames %d" % (anim_name, frames))
        for frame in sorted({0, frames // 2, max(frames - 1, 0)}):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(anim, frame, unreal.AnimPoseEvaluationOptions())
            for bone in unreal.AnimPoseExtensions.get_bone_names(pose):
                ref = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
                now = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
                moved = (ref.translation - now.translation).length()
                if moved > 1.0 or not ref.rotation.equals(now.rotation, 0.01):
                    log("  f%d %s moved %.1f ref t %s r %s -> t %s r %s" % (
                        frame, bone, moved, ref.translation, ref.rotation.rotator(), now.translation,
                        now.rotation.rotator()))

    write_obj(roof, "roof_ref")
    base = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = True
    lod = unreal.GeometryScriptMeshReadLOD()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(unreal.load_asset(AB + "SM_Ger_AircraftBuilding_BaseMesh"),
                                                                   base, options, lod, False)
    write_obj(base, "base")
    stuka = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"), stuka,
        unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    write_obj(stuka, "stuka")


try:
    run()
except Exception:
    log(traceback.format_exc())
os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, "probe.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
