"""Builds the Red Spine walker squad (Squad_Ger_RedSpine) in Unreal. Idempotent: existing assets are updated.

Needs an editor DLL that has the Squad_Ger_RedSpine squad data (CPPGameState), so run it headless on the DebugGame
build until Development is rebuilt, under the async_spawner lock (and the build lock: a running Cmd locks the DLL):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>

  Squad_Ger_RedSpine        copy of Squad_Ger_IronStorm (plain elite squad controller, no extra ability
                            components): Squad Subtype = Squad_Ger_RedSpine, 3x Ger_Inf_Walker_RedSpine
  RTS_BP_RTSAsyncSpawner    SetupGerSquads row Squad_Ger_RedSpine -> Squad_Ger_RedSpine (only that row is touched)
The walker soldier Blueprint Ger_Inf_Walker_RedSpine (EspSphere_01 on the infantry skeleton, AnimBp_SquadUnit, MP46)
is used as it is. Health, armour, speed and cost come from C++ (CPPGameState squad data), not from these Blueprints.
Saves are verified by file mtime: a package another process holds fails with error 32 without raising.
"""
import os
import time

import unreal

ROOT = "/Game/RTS_Survival/Blueprints/Squads/Ger/Walkers/RedSpine"
SOURCE_SQUAD = "/Game/RTS_Survival/Blueprints/Squads/Ger/EliteInfantry/IronStorm/Squad_Ger_IronStorm"
WALKER = ROOT + "/Ger_Inf_Walker_RedSpine"
SQUAD = ROOT + "/Squad_Ger_RedSpine"
SPAWNER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner"
WALKER_COUNT = 3

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = "[RedSpine] "
FAILURES = []


def member(enum_type, name):
    """Enum member by its C++ name (Squad_Ger_RedSpine -> SQUAD_GER_RED_SPINE), whatever the Python spelling."""
    wanted = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.replace("_", "").lower() == wanted:
            return getattr(enum_type, attr)
    raise RuntimeError("no %s member %s" % (enum_type.__name__, name))


RED_SPINE = member(unreal.SquadSubtype, "Squad_Ger_RedSpine")


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save(path, retries=30):
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


def component(bp, name):
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = BFL.get_object_for_blueprint(BFL.get_data(handle), bp)
        if obj is not None and obj.get_name().startswith(name):
            return obj
    raise RuntimeError("no component " + name + " on " + bp.get_name())


def build_squad():
    # Reuse an existing copy: deleting a loaded Blueprint asks for confirmation, which can stall a windowless editor.
    if not eal.does_asset_exist(SQUAD):
        print(LOG + "dup", SOURCE_SQUAD.split("/")[-1], "->", SQUAD.split("/")[-1],
              eal.duplicate_asset(SOURCE_SQUAD, SQUAD))
    bp = unreal.load_asset(SQUAD)
    walker = unreal.load_asset(WALKER).generated_class()
    units = [walker] * WALKER_COUNT
    bp.modify()
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", RED_SPINE)
    cdo.set_editor_property("SquadUnitClasses", units)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # A compile after the CDO edit can re-dirty the package; set again and save after the last compile.
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", RED_SPINE)
    cdo.set_editor_property("SquadUnitClasses", units)
    save(SQUAD)
    return bp


def register_spawner(squad_bp):
    bp = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(bp.generated_class())
    rows_before = cdo.get_editor_property("SetupGerSquads")
    other_rows = [r for r in rows_before if r.get_editor_property("squad_subtype") != RED_SPINE]
    existing = [r for r in rows_before if r.get_editor_property("squad_subtype") == RED_SPINE]
    if len(existing) == 1 and existing[0].get_editor_property("unit_class") == squad_bp.generated_class():
        print(LOG + "spawner row already set, not saving")
        return
    bp.modify()
    row = unreal.SquadTrainingOptionSetup()
    row.set_editor_property("squad_subtype", RED_SPINE)
    row.set_editor_property("unit_class", squad_bp.generated_class())
    cdo.set_editor_property("SetupGerSquads", list(other_rows) + [row])
    print(LOG + "spawner ger rows", len(rows_before), "->", len(other_rows) + 1)
    save(SPAWNER)


def report():
    """Reads the saved state back from the loaded objects."""
    walker_bp = unreal.load_asset(WALKER)
    weapon = component(walker_bp, "Weapon").get_editor_property("child_actor_class")
    mesh = component(walker_bp, "CharacterMesh0")
    print(LOG + "walker weapon", weapon.get_name() if weapon else None,
          "mesh", mesh.get_editor_property("skeletal_mesh_asset").get_name(),
          "anim", mesh.get_editor_property("anim_class").get_name() if mesh.get_editor_property("anim_class") else None)
    walker_cdo = unreal.get_default_object(walker_bp.generated_class())
    try:
        # An override on the walker Blueprint would replace the squad data's health and armour.
        print(LOG + "walker OverwriteUnitArmorAndResistance",
              walker_cdo.get_editor_property("OverwriteUnitArmorAndResistance"))
    except Exception as exc:  # the property name differs in Python on some builds
        print(LOG + "could not read OverwriteUnitArmorAndResistance", exc)
    cdo = unreal.get_default_object(unreal.load_asset(SQUAD).generated_class())
    print(LOG + "squad subtype", cdo.get_editor_property("Squad Subtype"),
          "units", [c.get_name() for c in cdo.get_editor_property("SquadUnitClasses")])
    spawner_cdo = unreal.get_default_object(unreal.load_asset(SPAWNER).generated_class())
    rows = [(str(r.get_editor_property("squad_subtype")), r.get_editor_property("unit_class").get_name()
             if r.get_editor_property("unit_class") else None)
            for r in spawner_cdo.get_editor_property("SetupGerSquads")]
    print(LOG + "spawner ger rows", len(rows), rows)


def run(steps=("squad", "spawner")):
    squad = build_squad() if "squad" in steps else unreal.load_asset(SQUAD)
    if "spawner" in steps:
        register_spawner(squad)
    report()
    print(LOG + ("DONE OK" if not FAILURES else "DONE WITH FAILURES " + ", ".join(FAILURES)))


run(globals().get("RS_STEPS", ("squad", "spawner")))
