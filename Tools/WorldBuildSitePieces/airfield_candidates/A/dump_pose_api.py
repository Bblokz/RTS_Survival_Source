"""Read-only: which python APIs exist to pose the airbase roof without an RHI, plus the roof bones open vs reference.

Run headless: sh run_headless.sh dump_pose_api   Log: Saved/WorldBuildSitePieces/airfield_candidate_A/pose_api.txt
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_candidate_A")
ROOT = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/BuildingMeshes"
LINES = []


def log(text):
    LINES.append(str(text))


def members(obj):
    return sorted(entry for entry in dir(obj) if not entry.startswith("_") and entry not in dir(unreal.Object))


try:
    for name in sorted(dir(unreal)):
        lowered = name.lower()
        if any(key in lowered for key in ("boneweight", "skinweight", "animpose", "geometryscript_skel",
                                          "skeletalmesh", "skinning")):
            log("unreal." + name)
    for name in ("AnimPoseExtensions", "GeometryScript_BoneWeights", "GeometryScript_MeshBoneWeights",
                 "GeometryScript_Bones"):
        library = getattr(unreal, name, None)
        log("%s: %s" % (name, ", ".join(members(library)) if library else "MISSING"))

    roof = unreal.load_asset(ROOT + "/SK_Ger_AircraftBuilding_Roof")
    skeleton = roof.skeleton
    reference = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    names = unreal.AnimPoseExtensions.get_bone_names(reference)
    log("bones %d" % len(names))
    anim = unreal.load_asset(ROOT + "/Anims/Anim_GerAirbase_OpenStationary")
    options = unreal.AnimPoseEvaluationOptions()
    opened = unreal.AnimPoseExtensions.get_anim_pose_at_frame(anim, 0, options)
    close_to_open = unreal.load_asset(ROOT + "/Anims/Anim_GerAirbase_CloseToOpen")
    last = unreal.AnimPoseExtensions.get_anim_pose_at_frame(close_to_open, 239, options)
    for bone in names:
        parent = unreal.AnimPoseExtensions.get_bone_parent_name(reference, bone) if hasattr(
            unreal.AnimPoseExtensions, "get_bone_parent_name") else "?"
        a = unreal.AnimPoseExtensions.get_ref_bone_pose(reference, bone, unreal.AnimPoseSpaces.WORLD)
        b = unreal.AnimPoseExtensions.get_bone_pose(opened, bone, unreal.AnimPoseSpaces.WORLD)
        c = unreal.AnimPoseExtensions.get_bone_pose(last, bone, unreal.AnimPoseSpaces.WORLD)

        def text(t):
            r = t.rotation.rotator()
            return "(%.0f %.0f %.0f | p%.0f y%.0f r%.0f | s%.2f)" % (t.translation.x, t.translation.y, t.translation.z,
                                                                   r.pitch, r.yaw, r.roll, t.scale3d.x)
        log("bone %s parent %s ref %s open %s ctoo_end %s" % (bone, parent, text(a), text(b), text(c)))

    mesh = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        roof, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    log("roof dynamic mesh tris %d verts %d" % (mesh.get_triangle_count(), mesh.get_vertex_count()))
    weights = unreal.GeometryScript_BoneWeights
    log("has bone weights %s" % (weights.mesh_has_bone_weights(mesh),))
    info = weights.get_all_bones_info(mesh)
    log("bones info %s" % (info,))
    profile = unreal.GeometryScriptBoneWeightProfile()
    counts = {}
    multi = 0
    for vertex in range(0, mesh.get_vertex_count()):
        result = weights.get_vertex_bone_weights(mesh, vertex, profile)
        if vertex < 3:
            log("vertex %d weights %s" % (vertex, result))
        bone_list = result[0] if isinstance(result, tuple) else result
        if len(bone_list) > 1:
            multi += 1
        for entry in bone_list:
            counts[entry.bone_index] = counts.get(entry.bone_index, 0) + 1
    log("weights per bone index %s, verts with >1 bone %d" % (sorted(counts.items()), multi))
    stuka = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"), stuka,
        unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(stuka)
    log("stuka tris %d box (%.0f %.0f %.0f)-(%.0f %.0f %.0f)" % (stuka.get_triangle_count(), box.min.x, box.min.y,
                                                                 box.min.z, box.max.x, box.max.y, box.max.z))
except Exception:
    import traceback
    log(traceback.format_exc())
os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, "pose_api.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
unreal.log("AIRFIELD_A pose api written")
