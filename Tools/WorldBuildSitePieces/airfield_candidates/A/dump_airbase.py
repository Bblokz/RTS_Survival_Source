"""Read-only dump of the German airbase building (Ger_Airbase_Truck) for airfield candidate A.

Run headless: sh run_headless.sh dump_airbase   Log: Saved/WorldBuildSitePieces/airfield_candidate_A/dump.txt
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_candidate_A")
ROOT = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding"
LINES = []


def log(text):
    LINES.append(str(text))


def describe_static(path):
    mesh = unreal.load_asset(path)
    if mesh is None:
        log("MISSING " + path)
        return
    bounds = mesh.get_bounding_box()
    log("SM %s tris=%d lods=%d nanite=%s box min (%.1f %.1f %.1f) max (%.1f %.1f %.1f)" % (
        path, mesh.get_num_triangles(0), mesh.get_num_lods(), mesh.get_editor_property("nanite_settings").enabled,
        bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z))
    for lod in range(mesh.get_num_lods()):
        log("    lod %d tris %d" % (lod, mesh.get_num_triangles(lod)))
    for index, static_material in enumerate(mesh.static_materials):
        interface = static_material.material_interface
        log("    slot %d %s -> %s" % (index, static_material.material_slot_name,
                                     interface.get_path_name() if interface else None))
    for socket in mesh.get_sockets_by_tag(""):
        location = socket.get_editor_property("relative_location")
        rotation = socket.get_editor_property("relative_rotation")
        log("    socket %s at (%.1f %.1f %.1f) rot (p %.1f y %.1f r %.1f)" % (
            socket.get_editor_property("socket_name"), location.x, location.y, location.z,
            rotation.pitch, rotation.yaw, rotation.roll))


def describe_skeletal(path):
    mesh = unreal.load_asset(path)
    if mesh is None:
        log("MISSING " + path)
        return
    bounds = mesh.get_bounds()
    log("SK %s origin (%.1f %.1f %.1f) extent (%.1f %.1f %.1f)" % (
        path, bounds.origin.x, bounds.origin.y, bounds.origin.z, bounds.box_extent.x, bounds.box_extent.y,
        bounds.box_extent.z))
    for index, material in enumerate(mesh.materials):
        interface = material.material_interface
        log("    slot %d %s -> %s" % (index, material.material_slot_name,
                                     interface.get_path_name() if interface else None))
    skeleton = mesh.skeleton
    log("    skeleton " + (skeleton.get_path_name() if skeleton else "None"))
    try:
        count = mesh.get_editor_property("lod_info")
        log("    lods %d" % len(count))
    except Exception as error:
        log("    lod info: %s" % error)
    for index in range(200):
        try:
            name = mesh.get_bone_name(index) if hasattr(mesh, "get_bone_name") else None
        except Exception:
            break
        if name is None or str(name) == "None":
            break
        log("    bone %d %s" % (index, name))


def describe_anim(path):
    anim = unreal.load_asset(path)
    if anim is None:
        log("MISSING " + path)
        return
    length = anim.get_editor_property("sequence_length") if hasattr(anim, "sequence_length") else None
    try:
        length = unreal.AnimationLibrary.get_sequence_length(anim)
        frames = unreal.AnimationLibrary.get_num_frames(anim)
    except Exception as error:
        frames = str(error)
    log("ANIM %s length %s frames %s" % (path, length, frames))


def describe_blueprint(path):
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        log("MISSING " + path)
        return
    generated = unreal.load_object(None, path + "." + path.split("/")[-1] + "_C")
    default = unreal.get_default_object(generated)
    log("BP %s class %s" % (path, generated.get_name() if generated else None))
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if component is None:
            continue
        text = "    comp %s (%s)" % (component.get_name(), component.get_class().get_name())
        try:
            text += describe_component(component)
        except Exception as error:
            text += " [%s]" % error
        log(text)


def describe_component(component):
    text = ""
    if True:
        if isinstance(component, unreal.SceneComponent):
            location = component.get_editor_property("relative_location")
            rotation = component.get_editor_property("relative_rotation")
            scale = component.get_editor_property("relative_scale3d")
            text += " loc (%.1f %.1f %.1f) rot (p %.1f y %.1f r %.1f) scale (%.2f %.2f %.2f) parent socket %s" % (
                location.x, location.y, location.z, rotation.pitch, rotation.yaw, rotation.roll, scale.x, scale.y,
                scale.z, component.get_attach_socket_name())
            parent = component.get_attach_parent()
            text += " parent %s" % (parent.get_name() if parent else None)
        if isinstance(component, unreal.StaticMeshComponent):
            mesh = component.get_editor_property("static_mesh")
            text += " mesh " + (mesh.get_path_name() if mesh else "None")
        if isinstance(component, unreal.SkinnedMeshComponent):
            mesh = component.get_skinned_asset() if hasattr(component, "get_skinned_asset") else None
            text += " skmesh " + (mesh.get_path_name() if mesh else "None")
            if isinstance(component, unreal.SkeletalMeshComponent):
                anim_class = component.get_editor_property("anim_class")
                text += " animclass %s" % (anim_class.get_name() if anim_class else None)
    return text


os.makedirs(OUT, exist_ok=True)
try:
    for name in ("SM_Ger_AircraftBuilding_BaseMesh", "SM_Ger_AircraftBuildingPreview"):
        describe_static(ROOT + "/BuildingMeshes/" + name)
    describe_static(ROOT + "/Mount/SM_GerAircraftBase_Mount")
    describe_static(ROOT + "/SM_DestroyedGerAircraftBase_Mount")
    describe_skeletal(ROOT + "/BuildingMeshes/SK_Ger_AircraftBuilding_Roof")
    for name in ("Anim_GerAirbase_CloseToOpen", "Anim_GerAirbase_OpenStationary", "Anim_GerAirbase_OpenToClose",
                 "Anim_GerAirbase_TPose"):
        describe_anim(ROOT + "/BuildingMeshes/Anims/" + name)
    describe_blueprint(ROOT + "/Ger_Airbase_Truck")
    for path in ("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO",
                 "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BF109/Meshes/SK_Bf109"):
        describe_skeletal(path)
    log("--- BF109 folder")
    for path in unreal.EditorAssetLibrary.list_assets("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BF109",
                                                      recursive=True, include_folder=False):
        log("    " + path)
except Exception:
    import traceback
    log(traceback.format_exc())
with open(os.path.join(OUT, "dump.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
unreal.log("AIRFIELD_A dump written")
