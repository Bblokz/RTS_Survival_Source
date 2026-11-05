"""Builds the Elite Scavengers (Squad_Ger_EliteScavengers) in Unreal. Idempotent: existing assets are updated.

Needs an editor DLL that knows ESquadSubtype::Squad_Ger_EliteScavengers, so run it headless on the DebugGame build
(until Development is rebuilt):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Run make_elite_scavenger_texture.py first (plain Python) for the diffuse texture.

  T_EliteScavenger_Diffuse          scavenger base colour, coat/trousers dyed dark oxblood
  MI_EliteScavenger                 child of M_ScavengerNoCamo with that Diffuse
  Ger_Inf_EliteScavenger_STG        copy of Ger_Inf_Scavenger: Ger_Weapon_STG44, MI_EliteScavenger
  Ger_Inf_EliteScavenger_Faust      copy of Ger_Inf_Scavenger: Ger_Weapon_PanzerFaust, MI_EliteScavenger
  Squad_Ger_EliteScavengers         copy of Squad_Ger_Scavengers: Squad Subtype = Squad_Ger_EliteScavengers,
                                    3 STG + 2 Faust soldiers
  RTS_BP_RTSAsyncSpawner            SetupGerSquads row Squad_Ger_EliteScavengers -> Squad_Ger_EliteScavengers
Health and cost come from C++ (CPPGameState squad data), not from these Blueprints.
Saves are verified by file mtime: a package another process holds fails with error 32 without raising.
"""
import os
import time

import unreal

TEX_SRC = "D:/Blender/BlenderProjects/BlenderTanks/Infantry/PostApoSoldier/EliteScavenger/T_EliteScavenger_Diffuse.png"
ROOT = "/Game/RTS_Survival/Blueprints/Squads/Ger/EliteInfantry/EliteScavengers"
SCAV = "/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers"
PARENT_MAT = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Material/M_ScavengerNoCamo"
WEAPONS = "/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German"
STG = WEAPONS + "/STG44/Ger_Weapon_STG44"
FAUST = WEAPONS + "/PanzerFaust/Ger_Weapon_PanzerFaust"
SPAWNER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner"
TEX = ROOT + "/Textures/T_EliteScavenger_Diffuse"
MI = ROOT + "/Material/MI_EliteScavenger"
UNIT_STG = ROOT + "/Ger_Inf_EliteScavenger_STG"
UNIT_FAUST = ROOT + "/Ger_Inf_EliteScavenger_Faust"
SQUAD = ROOT + "/Squad_Ger_EliteScavengers"
STG_COUNT = 3
FAUST_COUNT = 2

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = "[EliteScavengers] "
FAILURES = []


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save(path, retries=6):
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


def import_texture():
    task = unreal.AssetImportTask()
    task.filename = TEX_SRC
    task.destination_path = ROOT + "/Textures"
    task.destination_name = "T_EliteScavenger_Diffuse"
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(TEX)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    save(TEX)
    return tex


def build_material(tex):
    if eal.does_asset_exist(MI):
        mi = unreal.load_asset(MI)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_EliteScavenger", ROOT + "/Material", unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(PARENT_MAT))
    mel.set_material_instance_texture_parameter_value(mi, "Diffuse", tex)
    mel.update_material_instance(mi)
    save(MI)
    return mi


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


def build_unit(target, weapon_path, mi):
    bp = duplicate(SCAV + "/Ger_Inf_Scavenger", target)
    bp.modify()
    mesh = component(bp, "CharacterMesh0")
    mesh.set_editor_property("override_materials", [mi])
    weapon = component(bp, "Weapon")
    weapon.set_editor_property("child_actor_class", unreal.load_asset(weapon_path).generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(target)
    return bp


def build_squad(stg_bp, faust_bp):
    bp = duplicate(SCAV + "/Squad_Ger_Scavengers", SQUAD)
    bp.modify()
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", unreal.SquadSubtype.SQUAD_GER_ELITE_SCAVENGERS)
    units = [stg_bp.generated_class()] * STG_COUNT + [faust_bp.generated_class()] * FAUST_COUNT
    cdo.set_editor_property("SquadUnitClasses", units)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # A compile after the CDO edit can re-dirty the package; set again and save after the last compile.
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", unreal.SquadSubtype.SQUAD_GER_ELITE_SCAVENGERS)
    cdo.set_editor_property("SquadUnitClasses", units)
    save(SQUAD)
    return bp


def register_spawner(squad_bp):
    bp = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(bp.generated_class())
    bp.modify()
    key = unreal.SquadSubtype.SQUAD_GER_ELITE_SCAVENGERS
    rows = [r for r in cdo.get_editor_property("SetupGerSquads") if r.get_editor_property("squad_subtype") != key]
    row = unreal.SquadTrainingOptionSetup()
    row.set_editor_property("squad_subtype", key)
    row.set_editor_property("unit_class", squad_bp.generated_class())
    rows.append(row)
    cdo.set_editor_property("SetupGerSquads", rows)
    save(SPAWNER)


def report():
    """Reads the saved state back from the loaded objects."""
    scav_cdo = unreal.get_default_object(unreal.load_asset(SCAV + "/Ger_Inf_Scavenger").generated_class())
    try:
        print(LOG + "scavenger OverwriteUnitArmorAndResistance",
              scav_cdo.get_editor_property("OverwriteUnitArmorAndResistance"))
    except Exception as exc:  # the property name differs in Python on some builds
        print(LOG + "could not read OverwriteUnitArmorAndResistance", exc)
    for path in (UNIT_STG, UNIT_FAUST):
        bp = unreal.load_asset(path)
        weapon = component(bp, "Weapon").get_editor_property("child_actor_class")
        mats = component(bp, "CharacterMesh0").get_editor_property("override_materials")
        print(LOG + path.split("/")[-1], "weapon", weapon.get_name() if weapon else None,
              "materials", [m.get_name() if m else None for m in mats])
    cdo = unreal.get_default_object(unreal.load_asset(SQUAD).generated_class())
    print(LOG + "squad subtype", cdo.get_editor_property("Squad Subtype"),
          "units", [c.get_name() for c in cdo.get_editor_property("SquadUnitClasses")])
    spawner_cdo = unreal.get_default_object(unreal.load_asset(SPAWNER).generated_class())
    rows = [(str(r.get_editor_property("squad_subtype")), str(r.get_editor_property("unit_class")))
            for r in spawner_cdo.get_editor_property("SetupGerSquads")]
    print(LOG + "spawner ger rows", len(rows), [r for r in rows if "ELITE_SCAV" in r[0] or "RAIL" in r[0]])


def run(steps=("texture", "material", "units", "squad", "spawner")):
    tex = import_texture() if "texture" in steps else unreal.load_asset(TEX)
    mi = build_material(tex) if "material" in steps else unreal.load_asset(MI)
    if "units" in steps:
        stg = build_unit(UNIT_STG, STG, mi)
        faust = build_unit(UNIT_FAUST, FAUST, mi)
    else:
        stg, faust = unreal.load_asset(UNIT_STG), unreal.load_asset(UNIT_FAUST)
    squad = build_squad(stg, faust) if "squad" in steps else unreal.load_asset(SQUAD)
    if "spawner" in steps:
        register_spawner(squad)
    report()
    print(LOG + ("DONE OK" if not FAILURES else "DONE WITH FAILURES " + ", ".join(FAILURES)))


run(globals().get("ES_STEPS", ("texture", "material", "units", "squad", "spawner")))
