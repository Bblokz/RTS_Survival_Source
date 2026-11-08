"""Read-only dump for the round-5 fixes: every DA_GlobalAbilitySystem template (class, tier, requirements, title,
description, aircraft-drop settings), the Ju 390 skeletal mesh (bounds, bones, sockets) and the mesh component of the
aircraft classes the Red Spine drops use. Prints lines prefixed [Round5Dump]; saves nothing.
Run headless on the DebugGame build (hold the build lock):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
"""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
JU390_MESH = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU390B/Mesh/SK_JU309B_Animated"
LOG = "[Round5Dump] "


def log(text):
    print(LOG + text)


def name_of(value):
    return str(value).split(".")[-1].split(":")[0].strip(">")


def text_of(value):
    return value.export_text() if hasattr(value, "export_text") else str(value)


def dump_templates():
    templates = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    log("entries=%d" % len(templates))
    aircraft_classes = set()
    for key, obj in templates.items():
        if obj is None:
            log("TEMPLATE %s NONE" % name_of(key))
            continue
        req = obj.get_editor_property("m_ability_requirements")
        ui = obj.get_editor_property("m_ui_settings")
        log("TEMPLATE %s class=%s tier=%s tech=%s unit=%s title=%r" % (
            name_of(key), obj.get_class().get_name(), name_of(obj.get_editor_property("m_ability_tier")),
            name_of(req.get_editor_property("required_technology")),
            text_of(req.get_editor_property("required_unit")), str(ui.get_editor_property("title"))))
        log("  DESC %s %r" % (name_of(key), str(ui.get_editor_property("description"))))
        try:
            drop = obj.get_editor_property("m_aircraft_drop_settings")
        except Exception:
            drop = None
        if drop is not None:
            aircraft = drop.get_editor_property("aircraft_class")
            aircraft_classes.add(str(aircraft))
            log("  DROP %s %s" % (name_of(key), text_of(drop)))
        try:
            strafe = obj.get_editor_property("m_aircraft_strafing_settings")
            log("  STRAFE %s %s" % (name_of(key), text_of(strafe)))
        except Exception:
            pass
    return aircraft_classes


def dump_mesh():
    mesh = unreal.load_asset(JU390_MESH)
    if mesh is None:
        log("MESH missing " + JU390_MESH)
        return
    bounds = mesh.get_bounds()
    log("MESH bounds origin=%s extent=%s" % (bounds.origin, bounds.box_extent))
    try:
        imported = mesh.get_imported_bounds()
        log("MESH imported origin=%s extent=%s" % (imported.origin, imported.box_extent))
    except Exception as error:
        log("MESH imported bounds n/a %s" % error)
    for index in range(mesh.num_sockets()):
        socket = mesh.get_socket_by_index(index)
        log("SOCKET %s bone=%s loc=%s rot=%s" % (socket.get_editor_property("socket_name"),
                                                 socket.get_editor_property("bone_name"),
                                                 socket.get_editor_property("relative_location"),
                                                 socket.get_editor_property("relative_rotation")))
    skeleton = mesh.get_editor_property("skeleton")
    try:
        lib = unreal.AnimationLibrary
        names = lib.get_bone_names(skeleton) if hasattr(lib, "get_bone_names") else []
    except Exception:
        names = []
    if not names:
        try:
            ref = unreal.SkeletalMeshEditorSubsystem
        except Exception:
            ref = None
    for bone in names:
        try:
            pose = unreal.AnimPoseExtensions.get_ref_bone_pose(
                unreal.AnimPoseExtensions.get_reference_pose(skeleton), bone, unreal.AnimPoseSpaces.WORLD)
            log("BONE %s world=%s rot=%s" % (bone, pose.translation, pose.rotation.rotator()))
        except Exception as error:
            log("BONE %s (%s)" % (bone, error))


def dump_aircraft(class_paths):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for path in sorted(class_paths):
        clean = path.split("'")[1] if "'" in path else path
        bp_path = clean.rsplit(".", 1)[0]
        bp = unreal.load_asset(bp_path)
        if bp is None:
            log("AIRCRAFT %s not loadable" % clean)
            continue
        log("AIRCRAFT %s" % bp_path)
        try:
            handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        except Exception as error:
            log("  gather failed %s" % error)
            continue
        for handle in handles:
            data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
            template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
            if template is None or not isinstance(template, unreal.SceneComponent):
                continue
            extra = ""
            if isinstance(template, unreal.SkeletalMeshComponent):
                extra = " mesh=%s" % template.get_editor_property("skeletal_mesh_asset")
            log("  COMP %s %s loc=%s rot=%s scale=%s%s" % (
                template.get_name(), template.get_class().get_name(), template.get_editor_property("relative_location"),
                template.get_editor_property("relative_rotation"), template.get_editor_property("relative_scale3d"),
                extra))


classes = dump_templates()
dump_mesh()
dump_aircraft(classes)
log("DONE")
