"""Audits the trench anchors of every defense Blueprint that should offer them (German pits, the Breakthrough bunkers,
the Soviet pits): the TrenchAnchors component is on the Blueprint, its TrenchPort_ sockets exist on the spawned
actor's meshes, lie at ground level and point away from the owner, every listed neck/plug names a real socket and
mesh, and each neck is as wide as the (2026-11-09 widened) trench kit it has to meet.

Reads each Blueprint's component templates (no spawning: that crashes a -nullrhi editor); nothing is saved. Run in a throwaway editor:
    UnrealEditor-Cmd.exe <project>.uproject -nullrhi -unattended -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate
        -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
        -ExecCmds="py Source/Tools/FieldTrench/audit_trench_anchor_blueprints.py"
Result: Saved/TrenchAnchorAudit/result.txt (and .json). The last line is RESULT PASS/FAIL.
"""
import json
import math
import os
import unreal

BXP = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
GER = BXP + "GerPits/"
BUNKERS = BXP + "GerBxps/Bunkers/"
# Blueprint -> (faction, expected TrenchPort_ sockets)
BLUEPRINTS = {
    GER + "Ger_BXP_GerPit_Pak40": ("German", ["TrenchPort_Rear"]),
    GER + "Ger_BXP_GerPit_Flak37": ("German", ["TrenchPort_Rear"]),
    GER + "Ger_BXP_GerPit_LeFH18": ("German", ["TrenchPort_Rear", "TrenchPort_RearLeft", "TrenchPort_RearRight"]),
    GER + "Ger_BXP_GerPit_SFH18": ("German", ["TrenchPort_Rear", "TrenchPort_RearLeft", "TrenchPort_RearRight"]),
    GER + "Ger_BXP_GerPit_Mortar": ("German", ["TrenchPort_RearRight"]),
    GER + "Ger_BXP_GerPit_Flak88": ("German", ["TrenchPort_Rear", "TrenchPort_Right"]),
    GER + "TwinMG42Pit/Ger_BXP_GerPit_TwinMG42": ("German", ["TrenchPort_Left", "TrenchPort_Right"]),
    BUNKERS + "Flak88Bunker/Ger_BXP_Flak88Bunker": ("Bunker", ["TrenchPort_Doors", "TrenchPort_Stairs"]),
    BUNKERS + "FlakvierlingBunker/Ger_BXP_FlakvierlingBunker": ("Bunker", ["TrenchPort_Passage", "TrenchPort_Rear"]),
    BXP + "Rus53KPit/Rus_BXP_53KPit": ("Soviet", ["TrenchPort_Rear"]),
    BXP + "Rus53KPit/Rus_BXP_53KSectorPit": ("Soviet", ["TrenchPort_Rear"]),
    BXP + "RusBS3Pit/Rus_BXP_BS3Pit": ("Soviet", ["TrenchPort_Rear"]),
    BXP + "RusDShKPit/Rus_BXP_DShKPit": ("Soviet", ["TrenchPort_Rear"]),
    BXP + "RusAmpulometPit/Rus_BXP_AmpulometPit": ("Soviet", ["TrenchPort_Rear"]),
    BXP + "RusMortarPit/Rus_BXP_120MortarPit": ("Soviet", ["TrenchPort_Left"]),
}
# The mesh each Blueprint passes to InitBuildingExpansion as its built mesh: a built expansion swaps to it, so its
# sockets are what the anchors read at runtime. The component template's mesh is only the editor default.
BUILT_MESHES = {
    "Ger_BXP_GerPit_Pak40": "SM_GerPit_Pak", "Ger_BXP_GerPit_Flak37": "SM_GerPit_Flak",
    "Ger_BXP_GerPit_LeFH18": "SM_GerPit_Howitzer", "Ger_BXP_GerPit_SFH18": "SM_GerPit_Howitzer",
    "Ger_BXP_GerPit_Mortar": "SM_GerMortarPit", "Ger_BXP_GerPit_Flak88": "SM_GerFlak88Pit",
    "Ger_BXP_GerPit_TwinMG42": "SM_GerPit_TwinMG42_Sandbags",
    "Rus_BXP_53KPit": "SM_53KPit", "Rus_BXP_53KSectorPit": "SM_53KSectorPit", "Rus_BXP_BS3Pit": "SM_BS3Pit",
    "Rus_BXP_DShKPit": "SM_DShKPit", "Rus_BXP_AmpulometPit": "SM_AmpulometPit",
    "Rus_BXP_120MortarPit": "SM_Rus120MortarPit",
}
KIT_STRAIGHT = {
    "German": "/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Meshes/SM_FieldTrench_Straight_125",
    "Soviet": "/Game/RTS_Survival/Blueprints/Environment/Trenches/SovFieldTrench/Meshes/SM_SovFieldTrench_Straight_125",
}
SOCKET_PREFIX = "TrenchPort_"
# "At ground level": a port this far above or below the owner's pivot is suspect.
MAX_PORT_HEIGHT_CM = 60.0
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "TrenchAnchorAudit")

lines, failures, warnings, report = [], [], [], {}


def log(text):
    lines.append(str(text))
    unreal.log("[TrenchAnchorAudit] " + str(text))


def check(name, condition, detail=""):
    log(("PASS " if condition else "FAIL ") + name + ("  (" + detail + ")" if detail else ""))
    if not condition:
        failures.append(name)
    return condition


def warn(name, detail):
    log("WARN " + name + "  (" + detail + ")")
    warnings.append(name)


def mesh_width_across(mesh):
    """Full size along local Y: across the trench, for kit pieces and necks laid along their socket's +X."""
    box = mesh.get_bounding_box()
    return box.max.y - box.min.y


def kit_width(faction):
    path = KIT_STRAIGHT.get("Soviet" if faction == "Soviet" else "German")
    mesh = unreal.load_asset(path) if path and unreal.EditorAssetLibrary.does_asset_exist(path) else None
    return mesh_width_across(mesh) if mesh else None


def blueprint_templates(blueprint):
    """Component templates of the Blueprint (its own and inherited), read without spawning: spawning in a -nullrhi
    editor goes through the viewport's hit proxies and crashes."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    templates = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        template = library.get_object_for_blueprint(library.get_data(handle), blueprint)
        if template is not None:
            templates.append(template)
    return templates


def relative_transform(component):
    return unreal.Transform(component.get_editor_property("relative_location"),
                            component.get_editor_property("relative_rotation"),
                            component.get_editor_property("relative_scale3d"))


def socket_ports(templates):
    """(mesh template, socket name, transform in the owner's space) for every TrenchPort_ socket; mesh templates
    are taken as children of the root (their relative transform), which holds for every audited Blueprint."""
    ports = []
    for component in templates:
        if not isinstance(component, unreal.StaticMeshComponent):
            continue
        mesh = component.get_editor_property("static_mesh")
        if mesh is None:
            continue
        for socket in mesh.get_sockets_by_tag(""):
            name = str(socket.get_editor_property("socket_name"))
            if not name.startswith(SOCKET_PREFIX):
                continue
            local = unreal.Transform(socket.get_editor_property("relative_location"),
                                     socket.get_editor_property("relative_rotation"),
                                     socket.get_editor_property("relative_scale"))
            ports.append((component, name, unreal.MathLibrary.compose_transforms(local, relative_transform(component))))
    return ports


def find_built_mesh(blueprint_path, mesh_name):
    """The built mesh among the Blueprint's package dependencies (it is only referenced from the event graph)."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                    include_hard_package_references=True)
    for package in registry.get_dependencies(blueprint_path, options) or []:
        package = str(package)
        if package.rsplit("/", 1)[-1] == mesh_name:
            return unreal.load_asset(package)
    return None


def mesh_ports(mesh):
    ports = []
    for socket in mesh.get_sockets_by_tag(""):
        name = str(socket.get_editor_property("socket_name"))
        if name.startswith(SOCKET_PREFIX):
            ports.append((mesh, name, unreal.Transform(socket.get_editor_property("relative_location"),
                                                      socket.get_editor_property("relative_rotation"),
                                                      socket.get_editor_property("relative_scale"))))
    return ports


def check_port_geometry(label, mesh_name, socket_name, transform, faction):
    location = transform.translation
    forward = unreal.MathLibrary.get_forward_vector(transform.rotation.rotator())
    away_length = math.hypot(location.x, location.y)
    flat_forward_length = math.hypot(forward.x, forward.y)
    check(label + " " + socket_name + " lies flat", abs(forward.z) < 0.1, "forward z %.2f" % forward.z)
    check(label + " " + socket_name + " at ground level", abs(location.z) <= MAX_PORT_HEIGHT_CM,
          "%.0f cm" % location.z)
    if away_length > 1.0 and flat_forward_length > 0.01:
        dot = (location.x * forward.x + location.y * forward.y) / (away_length * flat_forward_length)
        check(label + " " + socket_name + " points away from the owner", dot > 0.0, "cos %.2f" % dot)
    facing = forward.x / max(flat_forward_length, 0.01)
    if faction == "German" and facing > 0.7:
        warn(label + " " + socket_name + " faces the front", "cos to actor forward %.2f" % facing)
    return {"mesh": mesh_name, "socket": socket_name, "x": round(location.x), "y": round(location.y), "z": round(location.z, 1),
            "yaw": round(transform.rotation.rotator().yaw, 1)}


def check_definitions(label, component, port_names, faction, entry):
    definitions = list(component.get_editor_property("anchors"))
    entry["definitions"] = []
    width = kit_width(faction)
    for definition in definitions:
        socket = str(definition.get_editor_property("socket_name"))
        check(label + " anchor " + socket + " names a socket", socket in port_names)
        # Soft mesh pointers come back as the loaded mesh, or None when unset or missing.
        neck_mesh = definition.get_editor_property("neck_mesh")
        plug_mesh = definition.get_editor_property("plug_mesh")
        check(label + " anchor " + socket + " has a neck mesh", isinstance(neck_mesh, unreal.StaticMesh),
              "plug only" if plug_mesh else "none")
        record = {"socket": socket, "neck": neck_mesh.get_name() if neck_mesh else None,
                  "plug": plug_mesh.get_name() if plug_mesh else None}
        if neck_mesh and width:
            # Information only: kit pieces and necks both carry earth banks, so their boxes are not channel widths.
            record["neck_width_cm"] = round(mesh_width_across(neck_mesh))
            record["kit_width_cm"] = round(width)
            log("INFO %s anchor %s neck box %.0f cm across, kit box %.0f cm" % (
                label, socket, mesh_width_across(neck_mesh), width))
        entry["definitions"].append(record)


def audit(index, path, faction, expected_sockets):
    label = path.rsplit("/", 1)[1]
    entry = {"faction": faction}
    report[label] = entry
    if not check(label + " exists", unreal.EditorAssetLibrary.does_asset_exist(path)):
        return
    templates = blueprint_templates(unreal.load_asset(path))
    anchors = [template for template in templates if isinstance(template, unreal.FieldTrenchAnchorComponent)]
    entry["anchor_components"] = len(anchors)
    if not check(label + " has one TrenchAnchors component", len(anchors) == 1, "%d found" % len(anchors)):
        return
    template_ports = socket_ports(templates)
    ports = template_ports
    built_name = BUILT_MESHES.get(label)
    if built_name:
        built_mesh = find_built_mesh(path, built_name)
        if not check(label + " references its built mesh " + built_name, built_mesh is not None):
            return
        ports = mesh_ports(built_mesh)
        template_meshes = sorted({component.get_editor_property("static_mesh").get_name()
                                  for component, _, _ in template_ports})
        if template_meshes and built_name not in template_meshes:
            warn(label + " template mesh differs from its built mesh",
                 "template " + ", ".join(template_meshes) + " vs built " + built_name)
        entry["built_mesh"] = built_name
    port_names = [name for _, name, _ in ports]
    entry["ports"] = []
    for socket in expected_sockets:
        check(label + " has " + socket, socket in port_names, "found " + (", ".join(port_names) or "none"))
    for owner, name, transform in ports:
        mesh_name = owner.get_name() if isinstance(owner, unreal.StaticMesh) else             owner.get_editor_property("static_mesh").get_name()
        entry["ports"].append(check_port_geometry(label, mesh_name, name, transform, faction))
    check_definitions(label, anchors[0], port_names, faction, entry)


def run():
    log("kit width German %.0f cm, Soviet %s" % (kit_width("German") or -1,
                                                  "%.0f cm" % kit_width("Soviet") if kit_width("Soviet") else "n/a"))
    for index, (path, (faction, sockets)) in enumerate(BLUEPRINTS.items()):
        try:
            audit(index, path, faction, sockets)
        except Exception as error:  # noqa: BLE001 - one broken Blueprint must not hide the others
            check(path + " audited without error", False, repr(error))
    log("RESULT %s failures=%d warnings=%d" % ("PASS" if not failures else "FAIL", len(failures), len(warnings)))
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "result.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    with open(os.path.join(OUT_DIR, "result.json"), "w", encoding="utf-8") as handle:
        json.dump({"failures": failures, "warnings": warnings, "blueprints": report}, handle, indent=1)


try:
    run()
finally:
    unreal.SystemLibrary.quit_editor()
