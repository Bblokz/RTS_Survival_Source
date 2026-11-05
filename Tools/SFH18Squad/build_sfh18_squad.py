"""Builds the 15 cm sFH 18 team weapon squad (Squad_Ger_SFH18_150mm) in Unreal. Idempotent: existing assets are updated.

Run headless on the DebugGame build (the shared editor's Development DLL may lack enum values other agents appended,
and its save of the async spawner would garble their rows):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Hold Saved/AgentLocks/async_spawner.lockdir for the spawner step (and the build lock for any DebugGame-Cmd run).

  BP_TW_Sfh18          copy of LFH18/BP_TW_Lefh18, the unused gun Blueprint that already carries SK_Ger_SFH18Anim,
                       AnimBp_Sfh18 (sFH packed/deployed/fire/wheel animations) and Weapon Name sFH18_150MM_TW (the
                       sFH 18 pit's weapon data). Heavier and slower than the leFH 18 (BP_TW_Lefh): see GUN_CONFIG.
                       Dig-in pit: the ML-20 pit (SM_TWDigIn_<gun> lookup has no sFH mesh).
  Ger_SquadTW_Sfh18    copy of LefhHowitzer/Ger_SquadTW_Lefh (5 Ger_Inf_Kar crew) with TeamWeaponClass = BP_TW_Sfh18
                       and Squad Subtype = Squad_Ger_SFH18_150mm.
  RTS_BP_RTSAsyncSpawner  SetupGerSquads row Squad_Ger_SFH18_150mm -> Ger_SquadTW_Sfh18 (no other row touched).
The squad's subtype comes from the copied construction script node "Set Squad Subtype" (each team weapon squad BP sets
its own there; Ger_SquadTW_Lefh says Squad_Ger_LefH18). Python cannot edit pin defaults, so it was set to
Squad_Ger_SFH18_150mm with UECP set_pin_default (UserConstructionScript, node "Set Squad Subtype") and saved; report()
fails while the saved squad asset still names Squad_Ger_LefH18. A fresh duplicate needs that edit again.
Health of the crew and the training cost come from C++ (CPPGameState squad data, DeveloperSettings SFH18_150mm_*).
Saves are verified by file mtime: a package another process holds fails with error 32 without raising.
SFH18_STEPS (a tuple in globals, set by a wrapper) limits the steps: "gun", "squad", "spawner".
"""
import os
import re
import time

import unreal

TW = "/Game/RTS_Survival/Blueprints/Squads/TeamWeapons"
SOURCE_GUN = TW + "/LFH18/BP_TW_Lefh18"
SOURCE_SQUAD = TW + "/LefhHowitzer/Ger_SquadTW_Lefh"
GUN = TW + "/LFH18/BP_TW_Sfh18"
SQUAD = TW + "/LFH18/Ger_SquadTW_Sfh18"
SPAWNER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner"
DIG_IN_PIT = "/Game/RTS_Survival/Meshes/Props/TeamWeaponDigIn/SM_TWDigIn_BP_TW_ML20"
EXPECTED_MESH = "SK_Ger_SFH18Anim"
EXPECTED_WEAPON = "sFH18_150MM_TW"
SUBTYPE = "Squad_Ger_SFH18_150mm"

# leFH 18 (BP_TW_Lefh): HP 450, deploy 4 s, 2 operators, push 180 cm/s, packed turn 10, deployed yaw 15.
# Mörser (BP_TW_Morser): HP 700, deploy 5 s, 3 operators. The sFH 18 sits between them on HP and is the slowest.
GUN_CONFIG = {
    "TeamWeaponHp": "650.000000",
    "M_DeploymentTime": "5.500000",
    "M_OperatorCount": "3",
    "M_TurnSpeedYaw": "8.000000",
    "M_PushedMoveSpeedCmPerSec": "145.000000",
    "WeaponYawRotationSpeed": "12.000000",
}

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = "[SFH18Squad] "
FAILURES = []


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save(path, retries=8):
    """Saves and proves the file changed on disk; retries while another process holds the package (error 32)."""
    file_path = disk_path(path)
    for attempt in range(retries):
        before = os.path.getmtime(file_path) if os.path.exists(file_path) else 0.0
        eal.save_asset(path, only_if_is_dirty=False)
        after = os.path.getmtime(file_path) if os.path.exists(file_path) else 0.0
        if after > before:
            print(LOG + "saved", path.split("/")[-1], time.ctime(after))
            return True
        print(LOG + "save did not reach disk, retry", attempt, path)
        time.sleep(10)
    FAILURES.append("save " + path)
    print(LOG + "SAVE FAILED", path)
    return False


def enum_member(enum_type, name):
    """Python spells enum members in upper snake case; match the C++ name with underscores and case ignored."""
    wanted = name.replace("_", "").lower()
    for member_name in dir(enum_type):
        if member_name.replace("_", "").lower() == wanted:
            return getattr(enum_type, member_name)
    raise KeyError("%s has no member %s" % (enum_type, name))


def duplicate(source, target):
    # Reuse an existing copy: deleting a loaded Blueprint asks for confirmation, which can stall a windowless editor.
    if not eal.does_asset_exist(target):
        print(LOG + "dup", source.split("/")[-1], "->", target.split("/")[-1], eal.duplicate_asset(source, target))
    return unreal.load_asset(target)


def component(bp, name):
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = BFL.get_object_for_blueprint(BFL.get_data(handle), bp)
        if obj is not None and obj.get_name().startswith(name):
            return obj
    raise RuntimeError("no component " + name + " on " + bp.get_name())


def heavier_config(cdo):
    """A fresh FTeamWeaponConfig from the copied text with GUN_CONFIG applied (struct wrappers can alias)."""
    text = cdo.get_editor_property("m_team_weapon_config").export_text()
    for key, value in GUN_CONFIG.items():
        text, count = re.subn(r"\b%s=[-0-9.]+" % key, "%s=%s" % (key, value), text)
        if count != 1:
            raise RuntimeError("config key %s matched %d times in %s" % (key, count, text))
    config = unreal.TeamWeaponConfig()
    config.import_text(text)
    return config


def apply_gun(bp):
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("m_team_weapon_config", heavier_config(cdo))
    # BP_TeamWeapon's BeginPlay writes this into the gun's RTS component and the crew controller takes the gun's
    # subtype (squad data, cost, crew animation overrides); the copied gun still said Squad_Ger_LefH18.
    cdo.set_editor_property("Squad Subtype", enum_member(unreal.SquadSubtype, SUBTYPE))
    dig_in = component(bp, "TeamWeaponDigIn")
    dig_in.set_editor_property("wall_mesh_override", unreal.load_asset(DIG_IN_PIT))


def build_gun():
    bp = duplicate(SOURCE_GUN, GUN)
    bp.modify()
    apply_gun(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # A compile after the CDO edit can re-dirty the package or reset it; set again and save after the last compile.
    apply_gun(bp)
    save(GUN)
    return bp


def build_squad(gun_bp):
    bp = duplicate(SOURCE_SQUAD, SQUAD)
    bp.modify()

    def apply(cdo):
        cdo.set_editor_property("TeamWeaponClass", gun_bp.generated_class())
        cdo.set_editor_property("Squad Subtype", enum_member(unreal.SquadSubtype, SUBTYPE))

    apply(unreal.get_default_object(bp.generated_class()))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    apply(unreal.get_default_object(bp.generated_class()))
    save(SQUAD)
    return bp


def spawner_rows(cdo):
    return [r.export_text() for r in cdo.get_editor_property("SetupGerSquads")]


def register_spawner(squad_bp):
    bp = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(bp.generated_class())
    before = spawner_rows(cdo)
    print(LOG + "spawner ger rows before", len(before))
    bp.modify()
    key = enum_member(unreal.SquadSubtype, SUBTYPE)
    rows = [r for r in cdo.get_editor_property("SetupGerSquads") if r.get_editor_property("squad_subtype") != key]
    kept = len(rows)
    row = unreal.SquadTrainingOptionSetup()
    row.set_editor_property("squad_subtype", key)
    row.set_editor_property("unit_class", squad_bp.generated_class())
    rows.append(row)
    cdo.set_editor_property("SetupGerSquads", rows)
    save(SPAWNER)
    after = spawner_rows(unreal.get_default_object(unreal.load_asset(SPAWNER).generated_class()))
    others_before = [r for r in before if SUBTYPE not in r]
    others_after = [r for r in after if SUBTYPE not in r]
    if others_before != others_after or len(after) != kept + 1:
        FAILURES.append("spawner rows changed beyond the sFH 18 row")
        print(LOG + "SPAWNER ROW MISMATCH before", before, "after", after)
    print(LOG + "spawner ger rows after", len(after), [r for r in after if SUBTYPE in r])


def report():
    gun_cdo = unreal.get_default_object(unreal.load_asset(GUN).generated_class())
    config = gun_cdo.get_editor_property("m_team_weapon_config").export_text()
    weapon = str(gun_cdo.get_editor_property("Weapon Name"))
    mesh = None
    gun_bp = unreal.load_asset(GUN)
    for handle in sds.k2_gather_subobject_data_for_blueprint(gun_bp):
        obj = BFL.get_object_for_blueprint(BFL.get_data(handle), gun_bp)
        if isinstance(obj, unreal.SkeletalMeshComponent):
            mesh = obj.get_editor_property("skeletal_mesh_asset")
            print(LOG + "gun skeletal mesh component", obj.get_name(), mesh,
                  obj.get_editor_property("anim_class"))
    pit = component(unreal.load_asset(GUN), "TeamWeaponDigIn").get_editor_property("wall_mesh_override")
    gun_subtype = str(gun_cdo.get_editor_property("Squad Subtype"))
    print(LOG + "gun weapon", weapon, "mesh", mesh.get_name() if mesh else None, "pit", pit, "subtype", gun_subtype)
    if "SFH18" not in gun_subtype.upper():
        FAILURES.append("gun squad subtype")
    print(LOG + "gun config", config)
    if EXPECTED_MESH not in str(mesh) or "SFH18_150MM_TW" not in weapon.upper():
        FAILURES.append("gun mesh/weapon")
    for key, value in GUN_CONFIG.items():
        if "%s=%s" % (key, value) not in config:
            FAILURES.append("gun config " + key)
    with open(disk_path(SQUAD), "rb") as handle:
        if b"Squad_Ger_LefH18" in handle.read():
            FAILURES.append("squad construction script still sets Squad_Ger_LefH18 (see the module docstring)")
    squad_cdo = unreal.get_default_object(unreal.load_asset(SQUAD).generated_class())
    units = [c.get_name() for c in squad_cdo.get_editor_property("SquadUnitClasses")]
    print(LOG + "squad subtype", squad_cdo.get_editor_property("Squad Subtype"),
          "gun", squad_cdo.get_editor_property("TeamWeaponClass").get_name(), "units", units)


def run(steps=("gun", "squad", "spawner")):
    gun = build_gun() if "gun" in steps else unreal.load_asset(GUN)
    squad = build_squad(gun) if "squad" in steps else unreal.load_asset(SQUAD)
    if "spawner" in steps:
        register_spawner(squad)
    report()
    print(LOG + ("DONE OK" if not FAILURES else "DONE WITH FAILURES " + ", ".join(FAILURES)))


run(globals().get("SFH18_STEPS", ("gun", "squad", "spawner")))
