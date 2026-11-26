"""Builds the MG 42 hatch gunner field kit (Docs/Units/FieldKit_TurretMG42Gunner.md). Idempotent; run again after a
failure. Needs the C++ of 2026-11-26 (Tech_FieldKit_HatchGunnerMG42, UTE_EmbeddedGunnerKit, TankTurretThenHullMesh).

Steps (python setup_field_kit_mg42.py [step ...], default all):
  gunner    Ger_EIT_MG42_Scavenger: copy of the MG 34 scavenger with the twin MG 42 pit's weapon (Ger_PitMG42_7_92MM,
            10-round bursts 0.055 s apart, Cue_MG42_GerTank_11Burst). Still the MG 34 model (no MG 42 static mesh yet).
  sockets   KitGunner_<family> sockets (the gunner's feet) on the turret collision meshes and two engine decks
  mounts    KitGunnerMount (RequiredTechnology = the kit) on the 8 base tanks, StandardPlus CupolaMount unmounts on it
  effect    TEBP_Ger_FieldKitHatchMG42 (child of UTE_EmbeddedGunnerKit)
  buttons   research row (title, text, icon) in Buttons_DataTableb
  research  research slot on the Light Steel Forge's Radixite Reactor, both German factions
  techmap   Tech_FieldKit_HatchGunnerMG42 -> TEBP in RTS_BP_PlayerController GetPlayerTechs
Throwaway headless editor (not the shared one):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -nullrhi
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    "-ExecCmds=py D:/UE5Projects/RTS_Survival/Source/Tools/FieldKits/setup_field_kit_mg42.py" -unattended -nosplash
    -DisablePlugins=Fab -NoLiveCoding
Grep FIELDKIT_SETUP in the log. A save fails while another editor holds the package (FIELDKIT_SETUP_FAIL).
"""
import json
import sys
import traceback

import unreal

KIT_TECH = "Tech_FieldKit_HatchGunnerMG42"
GERMAN = "/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/German"
MG34_GUNNER = GERMAN + "/Ger_EIT_MG34_Scavenger"
MG42_GUNNER = GERMAN + "/Ger_EIT_MG42_Scavenger"
MG42_SOUND = ("/Game/RTS_Survival/Sounds/WeaponSounds/SmallArms/MachineGuns/MG42_Tank_AJ/11Burst/"
              "Cue_MG42_GerTank_11Burst.Cue_MG42_GerTank_11Burst")
# The twin MG 42 pit's weapon setup: the same gun, so it shares the balance entry.
GUNNER_PINS = {"WeaponName": "Ger_PitMG42_7_92MM", "BurstCooldown": "0.055",
               "SingleBurstAmountMaxBurstAmount": "10", "MinBurstAmount": "6", "LaunchSound": MG42_SOUND}
VEH = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle"
TUR = "/Game/RTS_Survival/Blueprints/Turrets/German/Medium"
# Socket = where the gunner's feet go (the mount adds no offset). Cupolas: hatch top - 80 (only his upper body shows,
# like the Soviet gunners); engine decks: on the deck. Positions from the collision meshes' vertices
# (Saved/FieldKits/kit_hosts.json; the Pz IV value matches the existing Infantry socket - 80).
SOCKETS = [
    (TUR + "/PanzerIIITurret/Meshes/SM_Collision_PanzerIII_Turret", "KitGunner_PzIII", (-56.4, 0.0, 11.8)),
    (TUR + "/PanzerIVTurret/Meshes/SM_Collision_PanzerIV_Turret", "KitGunner_PzIV", (-65.3, 0.0, 13.0)),
    (TUR + "/PanzerIVTurret/Meshes/SM_Collision_PzIVF1_Spotter_Turret", "KitGunner_PzIV", (-65.3, 0.1, 13.0)),
    (TUR + "/PanzerIVTurret/PzIVG_DoubleTurret/Meshes/SM_Collision_PzIVG_DT_FrontTurret", "KitGunner_PzIV",
     (-25.0, 10.0, -25.0)),
    # Ostwind: its open flak turret is skipped (TurretClassesWithoutMount), so the gunner stands on the engine deck.
    ("SM_PanzerIV_H", "KitGunner_PzIV", (-185.0, 0.0, 184.0)),
    ("SM_Collision_Turret_Pz38T", "KitGunner_38t", (-9.6, -19.2, -1.3)),
    # Pz 38(t) R: its rocket rack has no hatch, so the gunner stands on the engine deck.
    ("SM_Hull_PZ38_T", "KitGunner_38t", (-112.0, -11.0, 126.0)),
    ("SM_Ger_JaguarTurretBase", "KitGunner_Jaguar", (-40.0, -30.0, -5.0)),
    # Variants that set their own turret collision or hull mesh at runtime (found by the test's MESH lines).
    # Pz III M SpacedArmor: the Schreck turret's collision; its two-man Schreck crew stands front right.
    ("SM_PanzerIIIM_SchreckShield", "KitGunner_PzIII", (-56.4, 0.0, 11.8)),
    ("SM_Hull_Pz38tRockets_IncendiarySalvo", "KitGunner_38t", (-112.0, -11.0, 126.0)),
    ("SM_Jaguar_Flanker_TurretBase", "KitGunner_Jaguar", (-40.0, -30.0, -5.0)),
    ("SM_Collision_Turret_Pz38t_ATAmmoStock", "KitGunner_38t", (-9.6, -19.2, -1.3)),
    ("SM_Collision_Turret_Pz38t_Rocketeer", "KitGunner_38t", (-9.6, -19.2, -1.3)),
]
# Base tank -> family socket. Variants inherit the mount.
KIT_HOSTS = {
    VEH + "/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_M": "KitGunner_PzIII",
    VEH + "/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_Flamm": "KitGunner_PzIII",
    VEH + "/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T_R": "KitGunner_38t",
    VEH + "/Ger_Medium/Jaguar/Ger_Vehicle_Jaguar": "KitGunner_Jaguar",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G": "KitGunner_PzIV",
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H": "KitGunner_PzIV",
    VEH + "/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T": "KitGunner_38t",
    VEH + "/Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL": "KitGunner_38t",
}
TURRETS_WITHOUT_MOUNT = {
    VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H": ["Ger_Turret_PanzerIV_H_Ostwind"],
}
STANDARD_PLUS = VEH + "/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H_StandardPlus"
KIT_MOUNT_NAME = "KitGunnerMount"
EFFECT_FOLDER = "/Game/RTS_Survival/Blueprints/TechTree/GerTechs/FieldKits"
EFFECT_NAME = "TEBP_Ger_FieldKitHatchMG42"
BUTTONS = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
ICON = "/Game/RTS_Survival/Textures/GameUI/TechTree/GerTech/T_TechIcon_InfantryKit"
RESEARCH_TITLE = "<Text_Title>Field Kit: Hatch MG 42 Gunner</>"
RESEARCH_TEXT = ("A scavenger with an MG 42 mans the hatch of every Pz III M, Pz III Flamm, Pz IV G, Pz IV H, Jaguar, "
                 "Pz 38(t), Pz 38(t) R and Pz 38(t) Railgun, variants included; on the Pz IV H StandardPlus he "
                 "replaces the MG 34 gunner. \\n<Text_sExp>Field kit:</> fitted to tanks already built and to every "
                 "new one, for this mission only")
REACTOR = ("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Forge/RadixReactor/"
           "Ger_Bxp_LFactoryRadixiteReactor")
RESEARCH_COMPONENT = "HatchMG42KitResearch"
# Slot of the old Spall Liners research (Saved/ClaudeExports/bxp_research_removal.json); above the old single techs.
RESEARCH_INDEX = 10
RESEARCH_COSTS = {"Resource_Radixite": 900, "Resource_VehicleParts": 250}
GERMAN_FACTIONS = ("GER_BREAKTHROUGH_DOCTRINE", "GER_STRIKE_DIVISION")
CONTROLLER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_PlayerController"

FAILURES = []


def log(message):
    unreal.log("FIELDKIT_SETUP " + message)


def fail(message):
    FAILURES.append(message)
    unreal.log_error("FIELDKIT_SETUP_FAIL " + message)


def enum_value(enum_class, cpp_name):
    wanted = cpp_name.replace("_", "").lower()
    for attribute in dir(enum_class):
        if attribute.isupper() and attribute.replace("_", "").lower() == wanted:
            return getattr(enum_class, attribute)
    raise RuntimeError(f"{enum_class.__name__} has no {cpp_name}; is this editor running the new C++?")


def save(path, label):
    if not unreal.EditorAssetLibrary.save_asset(path.split(".")[0], only_if_is_dirty=False):
        fail(f"save {label} ({path}) - another editor probably has it loaded")
        return False
    log(f"saved {label}")
    return True


def find_asset(name_or_path):
    if name_or_path.startswith("/"):
        return name_or_path
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    matches = [str(data.package_name) for data in registry.get_assets_by_path("/Game/RTS_Survival", recursive=True)
               if str(data.asset_name) == name_or_path]
    if len(matches) != 1:
        raise RuntimeError(f"{name_or_path}: {len(matches)} matches {matches}")
    return matches[0]


def generated_class(path):
    return unreal.BlueprintEditorLibrary.generated_class(unreal.load_asset(path))


# ---------------------------------------------------------------------------------------------------------- gunner

def bridge(action, **params):
    raw = unreal.UECPScriptBridge.dispatch("blueprint", action, json.dumps(params))
    data = json.loads(raw, strict=False)
    if isinstance(data, dict) and (data.get("ok") is False or data.get("success") is False or data.get("error")):
        raise RuntimeError(f"{action} {json.dumps(params)[:200]} -> {raw[:600]}")
    return data


def iter_nodes(data):
    if isinstance(data, dict):
        if "pins" in data and any(key in data for key in ("node_id", "id", "guid")):
            yield data
        for value in data.values():
            yield from iter_nodes(value)
    elif isinstance(data, list):
        for value in data:
            yield from iter_nodes(value)


def step_gunner():
    if not unreal.EditorAssetLibrary.does_asset_exist(MG42_GUNNER):
        if unreal.EditorAssetLibrary.duplicate_asset(MG34_GUNNER, MG42_GUNNER) is None:
            fail("duplicate MG 34 gunner")
            return
    graph = bridge("get_blueprint_graph", blueprint_path=MG42_GUNNER, graph_name="EventGraph", detail="full",
                   include_unconnected_pins=True)
    changed = 0
    for node in iter_nodes(graph):
        node_id = node.get("node_id") or node.get("id") or node.get("guid")
        for pin in node["pins"]:
            name = pin.get("name") or pin.get("pin_name")
            if name not in GUNNER_PINS or str(pin.get("direction", "input")).lower().find("out") != -1:
                continue
            if pin.get("connected_to") or pin.get("linked_to"):
                continue
            bridge("set_pin_default", blueprint_path=MG42_GUNNER, graph_name="EventGraph", node_id=node_id,
                   pin_name=name, value=GUNNER_PINS[name])
            changed += 1
            log(f"gunner pin {name} = {GUNNER_PINS[name]} on {node_id}")
    if changed < len(GUNNER_PINS):
        fail(f"gunner: set {changed} pins, expected {len(GUNNER_PINS)}; graph head {json.dumps(graph)[:1500]}")
    blueprint = unreal.load_asset(MG42_GUNNER)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(MG42_GUNNER, "Ger_EIT_MG42_Scavenger")


# --------------------------------------------------------------------------------------------------------- sockets

def step_sockets():
    for mesh_name, socket_name, location in SOCKETS:
        path = find_asset(mesh_name)
        mesh = unreal.load_asset(path)
        socket = mesh.find_socket(socket_name)
        if socket is None:
            socket = unreal.StaticMeshSocket(mesh)
            socket.set_editor_property("socket_name", socket_name)
            mesh.add_socket(socket)
            socket = mesh.find_socket(socket_name)
        socket.set_editor_property("relative_location", unreal.Vector(*location))
        socket.set_editor_property("relative_rotation", unreal.Rotator(0, 0, 0))
        mesh.modify()
        log(f"socket {socket_name} {location} on {path}")
        save(path, mesh.get_name())


# ---------------------------------------------------------------------------------------------------------- mounts

def gather(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = library.get_data(handle)
        template = library.get_object(data)
        if template is not None:
            result.append((str(library.get_variable_name(data)), template, bool(library.is_inherited_component(data))))
    return result


def find_own_component(blueprint, name):
    for variable_name, template, inherited in gather(blueprint):
        if not inherited and (variable_name == name or template.get_name().startswith(name)):
            return template
    return None


def add_component(blueprint, component_class, name):
    template = find_own_component(blueprint, name)
    if template:
        return template
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)[0]
    params = unreal.AddNewSubobjectParams()
    params.set_editor_property("parent_handle", root)
    params.set_editor_property("new_class", component_class)
    params.set_editor_property("blueprint_context", blueprint)
    handle, reason = subsystem.add_new_subobject(params)
    if not unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle):
        fail(f"add {name} to {blueprint.get_name()}: {reason}")
        return None
    subsystem.rename_subobject(handle, unreal.Text(name))
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(
        unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))


def step_mounts():
    kit = enum_value(unreal.Technology, KIT_TECH)
    gunner_class = generated_class(MG42_GUNNER)
    for host, socket_name in KIT_HOSTS.items():
        blueprint = unreal.load_asset(host)
        mount = add_component(blueprint, unreal.EmbeddedTurretMountComponent, KIT_MOUNT_NAME)
        if mount is None:
            continue
        mount.set_editor_property("EmbeddedTurretClasses", [gunner_class])
        mount.set_editor_property("MountMesh", unreal.EmbeddedTurretMountMesh.TANK_TURRET_THEN_HULL_MESH)
        mount.set_editor_property("MountSocketName", socket_name)
        mount.set_editor_property("MountLocalOffset", unreal.Vector(0, 0, 0))
        mount.set_editor_property("RequiredTechnology", kit)
        mount.set_editor_property("TurretClassesWithoutMount",
                                  [generated_class(find_asset(name)) for name in TURRETS_WITHOUT_MOUNT.get(host, [])])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        log(f"{blueprint.get_name()} {KIT_MOUNT_NAME} socket {socket_name}")
        save(host, blueprint.get_name())

    standard_plus = unreal.load_asset(STANDARD_PLUS)
    cupola = find_own_component(standard_plus, "CupolaMount")
    if cupola is None:
        fail("StandardPlus CupolaMount not found")
        return
    cupola.set_editor_property("UnmountOnTechnology", kit)
    unreal.BlueprintEditorLibrary.compile_blueprint(standard_plus)
    save(STANDARD_PLUS, "Ger_Vehicle_PanzerIV_H_StandardPlus")


# ---------------------------------------------------------------------------------------------------------- effect

def step_effect():
    path = f"{EFFECT_FOLDER}/{EFFECT_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        blueprint = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.TE_EmbeddedGunnerKit)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(EFFECT_NAME, EFFECT_FOLDER, unreal.Blueprint,
                                                                            factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(path, EFFECT_NAME)
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(blueprint))
    if cdo.get_editor_property("Technology") != enum_value(unreal.Technology, KIT_TECH):
        fail(f"{EFFECT_NAME} technology is {cdo.get_editor_property('Technology')}")


# --------------------------------------------------------------------------------------------------------- buttons

def text_value(text):
    return '"' + text.replace('"', "'") + '"'


def step_buttons():
    table = unreal.load_asset(BUTTONS)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    research = next(row for row in rows if row["Name"] == "IdResearchTechnology")
    icon_name = ICON.split("/")[-1]
    key = str(int(enum_value(unreal.Technology, KIT_TECH).value))
    research["CustomTypeMapping"][key] = (
        f"(CustomTitle={text_value(RESEARCH_TITLE)},CustomDescription={text_value(RESEARCH_TEXT)},"
        f"CustomIcon=\"/Script/Engine.Texture2D'{ICON}.{icon_name}'\")")
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        fail("Buttons_DataTableb import")
        return
    check = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    if len(check) != len(rows) or key not in next(r for r in check if r["Name"] == "IdResearchTechnology")[
            "CustomTypeMapping"]:
        fail("Buttons_DataTableb readback")
        return
    log(f"research row key {key}")
    save(BUTTONS, "Buttons_DataTableb")


# -------------------------------------------------------------------------------------------------------- research

def step_research():
    blueprint = unreal.load_asset(REACTOR)
    existing = [name for name, template, _ in gather(blueprint)
                if template.get_class().get_name() == "ResearchTechnologyAbilityComp"]
    if any(not name.startswith(RESEARCH_COMPONENT) for name in existing):
        fail(f"reactor still has other research components {existing}; run remove_bxp_research.py first")
    template = add_component(blueprint, unreal.ResearchTechnologyAbilityComp, RESEARCH_COMPONENT)
    if template is None:
        return
    unit_cost = unreal.UnitCost()
    unit_cost.import_text("(ResourceCosts=(" + ",".join(f"({name}, {amount})" for name, amount in
                                                         RESEARCH_COSTS.items()) + "))")
    settings_map = template.get_editor_property("TechAbilitySettingsPerFaction")
    for faction in GERMAN_FACTIONS:
        data = unreal.new_object(unreal.ResearchTechnologyAbilityTechnologyData, outer=template)
        data.set_editor_property("technology", enum_value(unreal.Technology, KIT_TECH))
        data.set_editor_property("costs", unit_cost)
        settings = unreal.ResearchTechnologyAbilitySettings()
        settings.set_editor_property("PreferredAbilityIndex", RESEARCH_INDEX)
        settings.set_editor_property("TechnologyData", data)
        settings_map[getattr(unreal.RTSFaction, faction)] = settings
    template.set_editor_property("TechAbilitySettingsPerFaction", settings_map)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    research = [name for name, template_, _ in gather(blueprint)
                if template_.get_class().get_name() == "ResearchTechnologyAbilityComp"]
    log(f"reactor research components {research}")
    save(REACTOR, "Ger_Bxp_LFactoryRadixiteReactor")


# --------------------------------------------------------------------------------------------------------- techmap

def step_techmap():
    effect = f"{EFFECT_FOLDER}/{EFFECT_NAME}.{EFFECT_NAME}_C"
    command = f"RTS.TechSetup.AddMakeMapEntries {CONTROLLER} GetPlayerTechs {KIT_TECH}={effect}"
    unreal.SystemLibrary.execute_console_command(None, command)
    log("techmap command sent; see RTS_TECH_SETUP RESULT in the log")


STEPS = {"gunner": step_gunner, "sockets": step_sockets, "mounts": step_mounts, "effect": step_effect,
         "buttons": step_buttons, "research": step_research, "techmap": step_techmap}
ORDER = ["gunner", "sockets", "mounts", "effect", "buttons", "research", "techmap"]


def main(arguments):
    selected = ORDER if not arguments or arguments[0] == "all" else arguments
    for step in selected:
        log(f"step {step}")
        try:
            STEPS[step]()
        except Exception:  # noqa: BLE001 - one failing step must not hide the others
            fail(f"step {step} raised {traceback.format_exc()}")
    log("RESULT " + ("PASS" if not FAILURES else "FAIL " + " | ".join(f[:300] for f in FAILURES)))


try:
    main(sys.argv[1:])
finally:
    if "--keep-open" not in sys.argv:
        unreal.SystemLibrary.quit_editor()
