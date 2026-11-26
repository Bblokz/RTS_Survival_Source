"""Headless read-only dump of the German airbase building (base mesh + opening roof) and candidate aircraft.
Run: sh run_py.sh dump_sources.py    Log: Saved/WorldBuildSitePieces/airfield_C/dump.txt
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_C")
os.makedirs(OUT, exist_ok=True)
LINES = []
AB = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/"
STATICS = [AB + "BuildingMeshes/SM_Ger_AircraftBuilding_BaseMesh", AB + "BuildingMeshes/SM_Ger_AircraftBuildingPreview",
           AB + "Mount/SM_GerAircraftBase_Mount"]
SKELETALS = [AB + "BuildingMeshes/SK_Ger_AircraftBuilding_Roof",
             "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO",
             "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/ME410/Meshes/SK_Ger_Aircraft_Me410"]
ANIMS = [AB + "BuildingMeshes/Anims/Anim_GerAirbase_OpenStationary", AB + "BuildingMeshes/Anims/Anim_GerAirbase_CloseToOpen",
         AB + "BuildingMeshes/Anims/Anim_GerAirbase_TPose"]


def log(text):
    LINES.append(str(text))


def material_text(material):
    return material.get_path_name() if material else "None"


def run():
    for path in STATICS:
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING " + path)
            continue
        bounds = mesh.get_bounding_box()
        log("SM %s tris=%d lods=%d nanite=%s box (%.1f %.1f %.1f)..(%.1f %.1f %.1f)" % (
            path, mesh.get_num_triangles(0), mesh.get_num_lods(), mesh.get_editor_property("nanite_settings").enabled,
            bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z))
        for index in range(mesh.get_num_lods()):
            log("    lod %d tris %d" % (index, mesh.get_num_triangles(index)))
        for index, static_material in enumerate(mesh.static_materials):
            log("    slot %d %s -> %s" % (index, static_material.material_slot_name,
                                         material_text(static_material.material_interface)))
        for socket in mesh.get_sockets_by_tag(""):
            log("    socket %s at %s" % (socket.socket_name, socket.relative_location))
    for path in SKELETALS:
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING " + path)
            continue
        bounds = mesh.get_bounds()
        log("SK %s origin (%.1f %.1f %.1f) extent (%.1f %.1f %.1f)" % (
            path, bounds.origin.x, bounds.origin.y, bounds.origin.z, bounds.box_extent.x, bounds.box_extent.y,
            bounds.box_extent.z))
        for index, skeletal_material in enumerate(mesh.materials):
            log("    slot %d %s -> %s" % (index, skeletal_material.material_slot_name,
                                         material_text(skeletal_material.material_interface)))
        dynamic = unreal.DynamicMesh()
        unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
            mesh, dynamic, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        log("    ref pose tris %d" % dynamic.get_triangle_count())
        skeleton = mesh.skeleton
        log("    skeleton %s" % (skeleton.get_path_name() if skeleton else None))
        try:
            count = mesh.get_editor_property("skeleton") and 0
        except Exception:
            pass
        try:
            for index in range(200):
                name = unreal.SkeletalMeshEditorSubsystem  # noqa
                break
        except Exception:
            pass
        names = []
        try:
            reference = skeleton.get_reference_pose()
            names = reference.get_bone_names()
            for bone in names:
                log("    bone %s ref %s" % (bone, reference.get_ref_bone_pose(bone, unreal.AnimPoseSpaces.WORLD)))
        except Exception as error:
            log("    bones failed %s" % error)
        for socket_index in range(mesh.num_sockets()):
            socket = mesh.get_socket_by_index(socket_index)
            log("    socket %s bone %s loc %s rot %s" % (socket.socket_name, socket.bone_name, socket.relative_location,
                                                       socket.relative_rotation))
    for path in ANIMS:
        anim = unreal.load_asset(path)
        if anim is None:
            log("MISSING " + path)
            continue
        log("ANIM %s length %.3f frames %d preview %s" % (
            path, anim.get_editor_property("sequence_length") if hasattr(anim, "sequence_length") else -1,
            unreal.AnimationLibrary.get_num_frames(anim),
            anim.get_preview_mesh().get_path_name() if anim.get_preview_mesh() else None))
        try:
            frames = unreal.AnimationLibrary.get_num_frames(anim)
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(anim, frames, unreal.AnimPoseEvaluationOptions())
            for bone in pose.get_bone_names():
                ref = pose.get_ref_bone_pose(bone, unreal.AnimPoseSpaces.LOCAL)
                now = pose.get_bone_pose(bone, unreal.AnimPoseSpaces.LOCAL)
                if (ref.translation - now.translation).length() > 0.5 or \
                        abs(ref.rotation.rotator().pitch - now.rotation.rotator().pitch) + \
                        abs(ref.rotation.rotator().roll - now.rotation.rotator().roll) + \
                        abs(ref.rotation.rotator().yaw - now.rotation.rotator().yaw) > 0.5:
                    log("    moved %s ref %s -> %s" % (bone, ref, now))
        except Exception as error:
            log("    pose failed %s" % error)


try:
    run()
except Exception:
    import traceback
    log(traceback.format_exc())
with open(os.path.join(OUT, "dump.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
