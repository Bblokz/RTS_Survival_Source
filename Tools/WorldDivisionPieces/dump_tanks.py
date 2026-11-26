"""Headless read-only dump: how the Soviet tanks (and their Env_Dest wrecks) are built.

Run:  sh run_py.sh dump_tanks.py [T34_76,...]
Log:  Saved/WorldDivisionPieces/tank_dump.txt
Saves nothing. Lists the Blueprint components (also of child actor classes), tries to spawn the Blueprint in the
commandlet world and lists every mesh component with its transform relative to the actor and its box in actor space.
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tank_config  # noqa: E402

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldDivisionPieces")
os.makedirs(OUT, exist_ok=True)
LOG_PATH = os.path.join(OUT, "tank_dump.txt")
HANDLE = open(LOG_PATH, "w", encoding="utf-8")
gs_assets = unreal.GeometryScript_AssetUtils
gs_scene = unreal.GeometryScript_SceneUtils
gs_query = unreal.GeometryScript_MeshQueries
SEEN_MESHES = {}

EXTRA_MESHES = [
    tank_config.V + "Rus_Medium/TrainingPreviews/SM_TrainingPreview_T34",
    tank_config.V + "Rus_Medium/TrainingPreviews/SM_TrainingPreview_T34E",
    "/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/CargoTrain/SM_CargoTank_T3476",
]


def log(text):
    HANDLE.write(str(text) + "\n")
    HANDLE.flush()


def vec(v):
    return "(%.1f %.1f %.1f)" % (v.x, v.y, v.z)


def describe_material(material):
    if material is None:
        return "None"
    text = material.get_path_name().split(".")[0]
    if isinstance(material, unreal.MaterialInstance):
        parent = material.get_editor_property("parent")
        text += " [parent %s]" % (parent.get_name() if parent else None)
    return text


def describe_mesh(mesh):
    if mesh is None:
        return
    path = mesh.get_path_name().split(".")[0]
    if path in SEEN_MESHES:
        return
    SEEN_MESHES[path] = True
    try:
        if isinstance(mesh, unreal.StaticMesh):
            bounds = mesh.get_bounds()
            lods = mesh.get_num_lods()
            log("  MESH SM %s lods=%d tris=%s nanite=%s origin=%s extent=%s" % (
                path, lods, [mesh.get_num_triangles(i) for i in range(lods)],
                mesh.get_editor_property("nanite_settings").enabled, vec(bounds.origin), vec(bounds.box_extent)))
            for index, slot in enumerate(mesh.static_materials):
                log("      slot %d %s -> %s" % (index, slot.material_slot_name, describe_material(slot.material_interface)))
        elif isinstance(mesh, unreal.SkeletalMesh):
            bounds = mesh.get_bounds()
            tris = []
            for lod in range(8):
                dyn = unreal.DynamicMesh()
                read = unreal.GeometryScriptMeshReadLOD()
                read.lod_index = lod
                result = gs_assets.copy_mesh_from_skeletal_mesh(mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), read)
                outcome = result[-1] if isinstance(result, tuple) else None
                if outcome != unreal.GeometryScriptOutcomePins.SUCCESS or dyn.get_triangle_count() == 0:
                    break
                tris.append(dyn.get_triangle_count())
            log("  MESH SK %s lodtris=%s origin=%s extent=%s" % (path, tris, vec(bounds.origin), vec(bounds.box_extent)))
            for index, slot in enumerate(mesh.materials):
                log("      slot %d %s -> %s" % (index, slot.material_slot_name, describe_material(slot.material_interface)))
    except Exception:
        log("  MESH ERROR %s\n%s" % (path, traceback.format_exc()))


def component_mesh(component):
    if isinstance(component, unreal.StaticMeshComponent):
        return component.get_editor_property("static_mesh")
    if isinstance(component, unreal.SkinnedMeshComponent):
        return component.get_editor_property("skinned_asset")
    return None


def dump_blueprint(path, depth=0, seen=None):
    seen = seen if seen is not None else set()
    if path in seen:
        return
    seen.add(path)
    pad = "  " * depth
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        log(pad + "BP MISSING " + path)
        return
    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) if hasattr(
        unreal.BlueprintEditorLibrary, "get_blueprint_parent_class") else None
    log(pad + "BP %s parent=%s" % (path, parent.get_path_name() if parent else "?"))
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None or not isinstance(obj, unreal.SceneComponent):
            continue
        text = pad + "  scs %s %s" % (obj.get_name(), obj.get_class().get_name())
        try:
            text += " loc=%s rot=%s scale=%s" % (vec(obj.get_editor_property("relative_location")),
                                                 obj.get_editor_property("relative_rotation"),
                                                 vec(obj.get_editor_property("relative_scale3d")))
            text += " socket=%s visible=%s hidden=%s" % (obj.get_editor_property("attach_socket_name"),
                                                        obj.get_editor_property("visible"),
                                                        obj.get_editor_property("hidden_in_game"))
        except Exception as error:
            text += " (%s)" % error
        mesh = component_mesh(obj)
        if mesh is not None:
            text += " mesh=" + mesh.get_path_name().split(".")[0]
            overrides = obj.get_editor_property("override_materials")
            if overrides:
                text += " overrides=%s" % [m.get_name() if m else None for m in overrides]
        child_class = None
        if isinstance(obj, unreal.ChildActorComponent):
            child_class = obj.get_editor_property("child_actor_class")
            text += " child_class=%s" % (child_class.get_path_name() if child_class else None)
        log(text)
        describe_mesh(mesh)
        if child_class is not None:
            child_path = child_class.get_path_name().split(".")[0]
            if child_path.startswith("/Game/"):
                dump_blueprint(child_path, depth + 2, seen)


def find_world():
    world = None
    for label, getter in (
            ("UnrealEditorSubsystem.get_editor_world", lambda: unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()),
            ("EditorLevelLibrary.get_editor_world", lambda: unreal.EditorLevelLibrary.get_editor_world())):
        try:
            world = getter()
            log("WORLD %s -> %s" % (label, world.get_path_name() if world else None))
        except Exception as error:
            log("WORLD %s failed: %s" % (label, error))
        if world is not None:
            return world
    try:
        result = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        log("WORLD new_blank_map -> %s" % (result.get_path_name() if result else None))
        return result
    except Exception as error:
        log("WORLD new_blank_map failed: %s" % error)
    return None


def all_actors(actor, chain, seen, out):
    if actor is None or actor in seen:
        return
    seen.add(actor)
    out.append((actor, chain))
    for component in actor.get_components_by_class(unreal.ChildActorComponent):
        all_actors(component.get_editor_property("child_actor"), chain + [component.get_name()], seen, out)
    for attached in actor.get_attached_actors():
        all_actors(attached, chain + ["attached"], seen, out)


def dump_spawn(path):
    actors_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls is None:
        log("SPAWN no class " + path)
        return
    actor = actors_subsystem.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        log("SPAWN failed " + path)
        return
    log("SPAWN %s -> %s at %s rot %s" % (path, actor.get_class().get_name(), vec(actor.get_actor_location()),
                                         actor.get_actor_rotation()))
    found = []
    try:
        base = actor.get_actor_transform()
        all_actors(actor, [], set(), found)
        options = unreal.GeometryScriptCopyMeshFromComponentOptions()
        for owner, chain in found:
            log("  actor %s chain=%s" % (owner.get_class().get_name(), chain))
            for component in owner.get_components_by_class(unreal.SceneComponent):
                mesh = component_mesh(component)
                if mesh is None and not isinstance(component, unreal.ChildActorComponent):
                    continue
                rel = component.get_world_transform().make_relative(base)
                parent = component.get_attach_parent()
                text = "    %s %s vis=%s hid=%s parent=%s socket=%s loc=%s rot=%s scale=%s" % (
                    component.get_name(), component.get_class().get_name(), component.is_visible(),
                    component.get_editor_property("hidden_in_game"), parent.get_name() if parent else None,
                    component.get_attach_socket_name(), vec(rel.translation), rel.rotation.rotator(), vec(rel.scale3d))
                if mesh is not None:
                    text += " mesh=%s mats=%s" % (mesh.get_name(), [m.get_name() if m else None for m in component.get_materials()])
                log(text)
                if mesh is None:
                    continue
                describe_mesh(mesh)
                try:
                    dyn = unreal.DynamicMesh()
                    result = gs_scene.copy_mesh_from_component(component, dyn, options, True)
                    box = gs_query.get_mesh_bounding_box(dyn)
                    log("      copied tris=%d world box min=%s max=%s outcome=%s" % (
                        dyn.get_triangle_count(), vec(box.min), vec(box.max), result[-1] if isinstance(result, tuple) else result))
                except Exception:
                    log("      copy failed\n" + traceback.format_exc())
    finally:
        for owner, _chain in reversed(found):
            try:
                actors_subsystem.destroy_actor(owner)
            except Exception:
                pass


def run():
    tanks = tank_config.selected(os.environ.get("DIVISIONTANKS", ""))
    for tank in tanks:
        log("=" * 30 + " " + tank["name"])
        for path in [tank["vehicle"]] + tank["wrecks"]:
            try:
                dump_blueprint(path)
            except Exception:
                log("BP ERROR %s\n%s" % (path, traceback.format_exc()))
    log("=" * 30 + " extra meshes")
    for path in EXTRA_MESHES:
        mesh = unreal.load_asset(path)
        if mesh is None:
            log("MISSING " + path)
        describe_mesh(mesh)
    log("=" * 30 + " spawn")
    world = find_world()
    if world is None:
        log("no world; spawn skipped")
        return
    for tank in tanks:
        log("-" * 20 + " " + tank["name"])
        for path in [tank["vehicle"]] + tank["wrecks"]:
            try:
                dump_spawn(path)
            except Exception:
                log("SPAWN ERROR %s\n%s" % (path, traceback.format_exc()))


try:
    run()
except Exception:
    log(traceback.format_exc())
log("DONE")
HANDLE.close()
unreal.log("DIVTANK dump written " + LOG_PATH)
