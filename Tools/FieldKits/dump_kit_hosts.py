"""Dumps the kit host Blueprints of the MG 42 hatch gunner kit (Docs/Units/FieldKit_TurretMG42Gunner_Plan.md).

Spawning is not possible here (python has no deferred spawn, and EditorActorSubsystem spawning asks the viewport for a
placement and crashes under -nullrhi), so this reads component templates instead:
  - per host Blueprint (bases, variants, controls): every component template (inherited ones included) with class,
    relative transform, mesh asset, child actor class and existing UEmbeddedTurretMountComponent settings;
  - the same for every turret Blueprint the hosts use;
  - per static mesh used: bounds, sockets (mesh space) and its collision/render vertices (for top-surface analysis).
Also lists MG meshes and exports the MG 34 gunner and twin MG 42 pit turret as T3D. Nothing is saved.

Throwaway editor (never the shared one):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -nullrhi
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    "-ExecCmds=py D:/UE5Projects/RTS_Survival/Source/Tools/FieldKits/dump_kit_hosts.py"
    -unattended -nosplash -DisablePlugins=Fab -NoLiveCoding
Output: Saved/FieldKits/kit_hosts.json, Saved/FieldKits/T3D/*.t3d
"""
import json
import os
import traceback

import unreal

OUT_DIR = "D:/UE5Projects/RTS_Survival/Saved/FieldKits"
VEH = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle"
HOSTS = [
    VEH + "/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_M",
    VEH + "/Ger_Medium/PanzerIII/Overcharged/Ger_Vehicle_PanzerIII_M_Overcharged",
    VEH + "/Ger_Medium/PanzerIII/SpacedArmor/Ger_Vehicle_PanzerIII_M_SpacedArmor",
    VEH + "/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_Flamm",
    VEH + "/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_R",
    VEH + "/Ger_Light/Panzer38T/IncendiarySalvo/Ger_Vehicle_Panzer38T_R_IncendiarySalvo",
    VEH + "/Ger_Light/Panzer38T/PrecisionMissile/Ger_Vehicle_Panzer38T_R_PrecisionMissile",
    VEH + "/Ger_Medium/Jaguar/Ger_Vehicle_Jaguar",
    VEH + "/Ger_Medium/Jaguar/Flanker/Ger_Vehicle_Jaguar_Flanker",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G_DoubleTurret",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H_Ostwind",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H_StandardPlus",
    VEH + "/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T",
    VEH + "/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_ATAmmoStock",
    VEH + "/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_Rocketeer",
    VEH + "/Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL",
    VEH + "/Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL_Overcharge",
    VEH + "/Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL_Sniper",
    # Controls: must never get the kit.
    VEH + "/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1",
]
EXTRA_BLUEPRINTS = [
    "/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/German/Ger_EIT_MG34_Scavenger",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Turrets/Ger_BP_EmbTurret_TwinMG42",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Forge/RadixReactor/Ger_Bxp_LFactoryRadixiteReactor",
]
T3D_EXPORTS = EXTRA_BLUEPRINTS[:2] + [
    "/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/RTS_BP_EmbInfantryTurretMaster",
]
MG_MESH_FOLDERS = [
    "/Game/RTS_Survival/Blueprints/Turrets/EmbeddedTurrets/MG34Top",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits",
    "/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German",
    "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/Turrets",
]
MAX_VERTICES = 6000


def vec(value):
    return [round(value.x, 2), round(value.y, 2), round(value.z, 2)]


def rot(value):
    return [round(value.roll, 2), round(value.pitch, 2), round(value.yaw, 2)]


def path_of(obj):
    return obj.get_path_name() if obj else None


def safe(template, key):
    try:
        return template.get_editor_property(key)
    except Exception:  # noqa: BLE001 - property missing on this class
        return None


def gather_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    entries = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = library.get_data(handle)
        template = library.get_object(data)
        if template is None:
            continue
        entry = {"name": str(library.get_variable_name(data)), "template": template.get_name(),
                 "class": template.get_class().get_name(),
                 "inherited": bool(library.is_inherited_component(data))}
        try:
            parent = library.get_data(library.get_parent_handle(data))
            entry["attach_parent"] = str(library.get_variable_name(parent))
        except Exception:  # noqa: BLE001
            pass
        if isinstance(template, unreal.SceneComponent):
            entry["relative_location"] = vec(template.get_editor_property("relative_location"))
            entry["relative_rotation"] = rot(template.get_editor_property("relative_rotation"))
            entry["relative_scale"] = vec(template.get_editor_property("relative_scale3d"))
            socket = safe(template, "attach_socket_name")
            if socket is not None:
                entry["attach_socket"] = str(socket)
        if isinstance(template, unreal.StaticMeshComponent):
            entry["mesh"] = path_of(template.get_editor_property("static_mesh"))
        elif isinstance(template, unreal.SkeletalMeshComponent):
            entry["mesh"] = path_of(safe(template, "skeletal_mesh_asset") or safe(template, "skeletal_mesh"))
        elif isinstance(template, unreal.ChildActorComponent):
            entry["child_class"] = path_of(template.get_editor_property("child_actor_class"))
        if template.get_class().get_name() == "EmbeddedTurretMountComponent":
            for key in ("EmbeddedTurretClasses", "bIsTankInfantryTurret", "MountMesh", "MountSocketName",
                        "MountLocalOffset"):
                value = safe(template, key)
                if isinstance(value, unreal.Vector):
                    value = vec(value)
                elif key == "EmbeddedTurretClasses" and value is not None:
                    value = [path_of(item) for item in value]
                else:
                    value = str(value)
                entry[key] = value
        entries.append(entry)
    return entries


def blueprint_for_class(class_path):
    if not class_path:
        return None
    package = class_path.split(".")[0]
    return unreal.load_asset(package)


def mesh_info(mesh_path):
    mesh = unreal.load_asset(mesh_path.split(".")[0])
    info = {"class": mesh.get_class().get_name() if mesh else None}
    if not isinstance(mesh, unreal.StaticMesh):
        if isinstance(mesh, unreal.SkeletalMesh):
            sockets = []
            for index in range(mesh.num_sockets()):
                socket = mesh.get_socket_by_index(index)
                sockets.append({"socket": str(socket.get_editor_property("socket_name")),
                                "bone": str(socket.get_editor_property("bone_name")),
                                "relative_location": vec(socket.get_editor_property("relative_location"))})
            info["sockets"] = sockets
            bounds = mesh.get_bounds()
            info["bounds_origin"] = vec(bounds.origin)
            info["bounds_extent"] = vec(bounds.box_extent)
        return info
    box = mesh.get_bounding_box()
    info["bounds"] = [vec(box.min), vec(box.max)]
    info["sockets"] = []
    for socket in mesh.get_sockets_by_tag(""):
        info["sockets"].append({"socket": str(socket.get_editor_property("socket_name")),
                                "relative_location": vec(socket.get_editor_property("relative_location")),
                                "relative_rotation": rot(socket.get_editor_property("relative_rotation"))})
    try:
        description = mesh.get_static_mesh_description(0)
        count = description.get_vertex_count()
        step = max(1, count // MAX_VERTICES)
        vertices = []
        for index in range(0, count, step):
            vertices.append(vec(description.get_vertex_position(unreal.VertexID(index))))
        info["vertex_count"] = count
        info["vertices"] = vertices
    except Exception as error:  # noqa: BLE001
        info["vertex_error"] = repr(error)
    return info


def list_mg_meshes():
    found = []
    for folder in MG_MESH_FOLDERS:
        for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True, include_folder=False):
            data = unreal.EditorAssetLibrary.find_asset_data(path)
            class_name = str(data.asset_class_path.asset_name)
            name = path.split("/")[-1].lower()
            if class_name in ("StaticMesh", "SkeletalMesh") and ("mg" in name or "weapon" in name or "gun" in name):
                found.append(f"{class_name} {path}")
    return found


def export_t3d(path):
    asset = unreal.load_asset(path)
    if asset is None:
        return f"missing {path}"
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = f"{OUT_DIR}/T3D/{path.split('/')[-1]}.t3d"
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = unreal.Exporter.run_asset_export_task(task)
    return f"{'ok' if ok else 'FAILED'} {task.filename}"


def main():
    os.makedirs(f"{OUT_DIR}/T3D", exist_ok=True)
    result = {"blueprints": {}, "turrets": {}, "meshes": {}, "errors": [], "mg_meshes": [], "t3d": []}
    pending_turrets = []
    for path in HOSTS + EXTRA_BLUEPRINTS:
        try:
            blueprint = unreal.load_asset(path)
            parent = safe(blueprint, "parent_class")
            templates = gather_templates(blueprint)
            result["blueprints"][path] = {"parent_class": path_of(parent), "components": templates}
            pending_turrets += [entry["child_class"] for entry in templates if entry.get("child_class")]
        except Exception:  # noqa: BLE001 - one Blueprint must not hide the others
            result["errors"].append(f"{path}: {traceback.format_exc()}")
    for class_path in sorted(set(pending_turrets)):
        try:
            blueprint = blueprint_for_class(class_path)
            if not isinstance(blueprint, unreal.Blueprint):
                result["errors"].append(f"no blueprint for {class_path}")
                continue
            result["turrets"][class_path] = {"parent_class": path_of(safe(blueprint, "parent_class")),
                                             "components": gather_templates(blueprint)}
        except Exception:  # noqa: BLE001
            result["errors"].append(f"{class_path}: {traceback.format_exc()}")
    mesh_paths = set()
    for group in (result["blueprints"], result["turrets"]):
        for entry in group.values():
            mesh_paths |= {component["mesh"] for component in entry["components"] if component.get("mesh")}
    for mesh_path in sorted(mesh_paths):
        try:
            result["meshes"][mesh_path] = mesh_info(mesh_path)
        except Exception:  # noqa: BLE001
            result["errors"].append(f"{mesh_path}: {traceback.format_exc()}")
    try:
        result["mg_meshes"] = list_mg_meshes()
    except Exception:  # noqa: BLE001
        result["errors"].append(traceback.format_exc())
    for path in T3D_EXPORTS:
        try:
            result["t3d"].append(export_t3d(path))
        except Exception:  # noqa: BLE001
            result["errors"].append(traceback.format_exc())
    with open(f"{OUT_DIR}/kit_hosts.json", "w") as handle:
        json.dump(result, handle)
    unreal.log(f"KIT_DUMP done blueprints={len(result['blueprints'])} turrets={len(result['turrets'])} "
               f"meshes={len(result['meshes'])} errors={len(result['errors'])}")


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
