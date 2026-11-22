"""Builds the Burrower "Kumpel" look (Berlin cellar miners) in Unreal from the Blender export. Idempotent.

Headless (no project C++ needed, any current editor DLL):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nopause -nosplash
      -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
Source art: D:/Blender/BlenderProjects/BlenderTanks/Infantry/GerBurrowers (README there; Tools/bw_build.py rebuilds the
FBX and textures, Tools/make_kumpel_cloth_texture.py the cloth). Design: Source/Docs/Campaign/BerlinBurrowers.md.

  T_BurrowerKumpel_Diffuse        Scavenger base colour dyed charcoal (cloth), collar dyed dark wool
  MI_BurrowerKumpel_Cloth         child of M_ScavengerNoCamo with that Diffuse
  T_BurrowerKumpelKit_{BC,ORM,N,E} baked kit textures (ORM = AO, roughness, metallic; E = lamp-glass mask)
  MI_BurrowerKumpel_Kit           child of the Rail Troopers' M_RailTrooperArmor (same packed layout), warm glow
  SK_BurrowerKumpel               Scavenger body (hood removed) + kit on InfantryMaster_Skeleton,
                                  SK_PostApoSoldier_PhysicsAsset; slot 0 cloth, slot 1 kit
  Ger_Inf_BurrowerKumpel          copy of Ger_Inf_Scavenger (MP 40) with SK_BurrowerKumpel
  Ger_Inf_BurrowerKumpelRifle     copy of Ger_Inf_ScavengerRifle (Kar98) with SK_BurrowerKumpel
  Squad_Ger_Burrowers             copy of Squad_Ger_Scavengers: Squad Subtype = Squad_Ger_Burrowers (C++, 2026-11-22),
                                  3 MP 40 + 1 Kar98 Kumpels. Owner 2026-11-22: one squad is given on contact in Berlin.
  RTS_BP_RTSAsyncSpawner          SetupGerSquads row Squad_Ger_Burrowers -> Squad_Ger_Burrowers, so missions can spawn
                                  it with AsyncSpawnActorAtLocation(FTrainingOption(Squad, Squad_Ger_Burrowers)).
Health, cost and abilities come from C++ (CPPGameState squad data). Needs an editor DLL that knows the subtype.
Saves are verified by file mtime (a package another process holds fails silently).
"""
import os
import time

import unreal

SRC = "D:/Blender/BlenderProjects/BlenderTanks/Infantry/GerBurrowers"
ROOT = "/Game/RTS_Survival/Blueprints/Squads/Ger/Burrowers"
SCAV = "/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers"
POSTAPO = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic"
SKELETON = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton"
PHYSICS = POSTAPO + "/Mesh/SK_PostApoSoldier_PhysicsAsset"
CLOTH_PARENT = POSTAPO + "/Material/M_ScavengerNoCamo"
KIT_PARENT = "/Game/RTS_Survival/Blueprints/Squads/Ger/EliteInfantry/RailTroopers/Material/M_RailTrooperArmor"
SK = ROOT + "/Mesh/SK_BurrowerKumpel"
MI_CLOTH = ROOT + "/Material/MI_BurrowerKumpel_Cloth"
MI_KIT = ROOT + "/Material/MI_BurrowerKumpel_Kit"
UNIT_SMG = ROOT + "/Ger_Inf_BurrowerKumpel"
UNIT_RIFLE = ROOT + "/Ger_Inf_BurrowerKumpelRifle"
UNITS = {UNIT_SMG: SCAV + "/Ger_Inf_Scavenger", UNIT_RIFLE: SCAV + "/Ger_Inf_ScavengerRifle"}
SQUAD = ROOT + "/Squad_Ger_Burrowers"
SQUAD_COMPOSITION = (UNIT_SMG, UNIT_SMG, UNIT_SMG, UNIT_RIFLE)
SPAWNER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner"
LAMP_GLOW = unreal.LinearColor(1.0, 0.74, 0.42, 1.0)     # warm electric cap lamp
LAMP_STRENGTH = 14.0

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = "[Burrowers] "
FAILURES = []


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save(path, retries=4):
    file_path = disk_path(path)
    for attempt in range(retries):
        before = os.path.getmtime(file_path) if os.path.exists(file_path) else 0.0
        eal.save_asset(path, only_if_is_dirty=False)
        after = os.path.getmtime(file_path) if os.path.exists(file_path) else 0.0
        if after > before:
            print(LOG + "saved", path.split("/")[-1])
            return True
        print(LOG + "save did not reach disk, retry", attempt, path)
        time.sleep(5)
    FAILURES.append("save " + path)
    print(LOG + "SAVE FAILED", path)
    return False


def import_texture(filename, folder, name, srgb, comp):
    t = unreal.AssetImportTask()
    t.filename = filename
    t.destination_path = folder
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    tools.import_asset_tasks([t])
    path = f"{folder}/{name}"
    tex = unreal.load_asset(path)
    tex.set_editor_property("srgb", srgb)
    tex.set_editor_property("compression_settings", comp)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    save(path)
    return tex


def import_textures():
    C = unreal.TextureCompressionSettings
    out = {"Diffuse": import_texture(f"{SRC}/Textures/T_BurrowerKumpel_Diffuse.png", ROOT + "/Textures",
                                     "T_BurrowerKumpel_Diffuse", True, C.TC_DEFAULT)}
    for key, srgb, comp in (("BC", True, C.TC_DEFAULT), ("ORM", False, C.TC_MASKS), ("N", False, C.TC_NORMALMAP),
                            ("E", False, C.TC_GRAYSCALE)):
        tex = import_texture(f"{SRC}/Textures/T_BurrowerKumpelKit_{key}.png", ROOT + "/Textures",
                             f"T_BurrowerKumpelKit_{key}", srgb, comp)
        if key == "N":
            tex.set_editor_property("flip_green_channel", False)     # baked as DirectX already
            save(f"{ROOT}/Textures/T_BurrowerKumpelKit_N")
        out[key] = tex
    return out


def material_instance(path, parent):
    folder, name = path.rsplit("/", 1)
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(parent))
    return mi


def build_materials(tex):
    cloth = material_instance(MI_CLOTH, CLOTH_PARENT)
    mel.set_material_instance_texture_parameter_value(cloth, "Diffuse", tex["Diffuse"])
    mel.update_material_instance(cloth)
    save(MI_CLOTH)
    kit = material_instance(MI_KIT, KIT_PARENT)
    for key in ("BC", "ORM", "N", "E"):
        # The setter returns False in a -nullrhi commandlet even when it worked (2026-11-22): read the value back.
        mel.set_material_instance_texture_parameter_value(kit, "T_" + key, tex[key])
        got = mel.get_material_instance_texture_parameter_value(kit, "T_" + key)
        if got is None or got.get_name() != tex[key].get_name():
            FAILURES.append("kit param T_" + key)
    mel.set_material_instance_vector_parameter_value(kit, "GlowColor", LAMP_GLOW)
    mel.set_material_instance_scalar_parameter_value(kit, "GlowStrength", LAMP_STRENGTH)
    mel.update_material_instance(kit)
    save(MI_KIT)
    return cloth, kit


def import_mesh(cloth, kit):
    # Legacy FBX importer (as for the Rail Troopers): Interchange ignores the skeleton option on some builds.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("skeleton", unreal.load_asset(SKELETON))
    ui.set_editor_property("physics_asset", unreal.load_asset(PHYSICS))
    sk_data = ui.get_editor_property("skeletal_mesh_import_data")
    sk_data.set_editor_property("import_morph_targets", False)
    sk_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask()
    t.filename = SRC + "/Export/SK_BurrowerKumpel.fbx"
    t.destination_path = ROOT + "/Mesh"
    t.destination_name = "SK_BurrowerKumpel"
    t.replace_existing = True
    t.automated = True
    t.save = False
    t.options = ui
    tools.import_asset_tasks([t])
    print(LOG + "imported", list(t.get_editor_property("imported_object_paths")))
    sk = unreal.load_asset(SK)
    sk.set_editor_property("physics_asset", unreal.load_asset(PHYSICS))
    new = []
    for i, m in enumerate(sk.get_editor_property("materials")):
        m.set_editor_property("material_interface", cloth if i == 0 else kit)
        new.append(m)
    sk.set_editor_property("materials", new)
    save(SK)
    return sk


def duplicate(source, target):
    if not eal.does_asset_exist(target):
        print(LOG + "dup", source.split("/")[-1], "->", target.split("/")[-1], eal.duplicate_asset(source, target))
    return unreal.load_asset(target)


def component(bp, name):
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = BFL.get_object_for_blueprint(BFL.get_data(h), bp)
        if obj is not None and obj.get_name().startswith(name):
            return obj
    raise RuntimeError("no component " + name + " on " + bp.get_name())


def build_units(sk):
    for target, source in UNITS.items():
        bp = duplicate(source, target)
        bp.modify()
        mesh = component(bp, "CharacterMesh0")
        mesh.set_editor_property("skeletal_mesh_asset", sk)
        mesh.set_editor_property("override_materials", [])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(target)


def build_squad():
    bp = duplicate(SCAV + "/Squad_Ger_Scavengers", SQUAD)
    bp.modify()
    units = [unreal.load_asset(u).generated_class() for u in SQUAD_COMPOSITION]
    key = unreal.SquadSubtype.SQUAD_GER_BURROWERS
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", key)
    cdo.set_editor_property("SquadUnitClasses", units)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # A compile after the CDO edit can re-dirty the package: set again and save after the last compile.
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", key)
    cdo.set_editor_property("SquadUnitClasses", units)
    save(SQUAD)
    return bp


def register_spawner(squad_bp):
    bp = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(bp.generated_class())
    bp.modify()
    key = unreal.SquadSubtype.SQUAD_GER_BURROWERS
    rows = [r for r in cdo.get_editor_property("SetupGerSquads") if r.get_editor_property("squad_subtype") != key]
    row = unreal.SquadTrainingOptionSetup()
    row.set_editor_property("squad_subtype", key)
    row.set_editor_property("unit_class", squad_bp.generated_class())
    rows.append(row)
    cdo.set_editor_property("SetupGerSquads", rows)
    save(SPAWNER)


def report():
    sk = unreal.load_asset(SK)
    skel = sk.get_editor_property("skeleton")
    print(LOG + "skeleton", skel.get_path_name() if skel else None)
    print(LOG + "physics", sk.get_editor_property("physics_asset"))
    for i, m in enumerate(sk.get_editor_property("materials")):
        mi = m.get_editor_property("material_interface")
        print(LOG + "slot", i, m.get_editor_property("material_slot_name"), mi.get_name() if mi else None)
    for target in UNITS:
        bp = unreal.load_asset(target)
        mesh = component(bp, "CharacterMesh0").get_editor_property("skeletal_mesh_asset")
        weapon = component(bp, "Weapon").get_editor_property("child_actor_class")
        print(LOG + target.split("/")[-1], "mesh", mesh.get_name() if mesh else None,
              "weapon", weapon.get_name() if weapon else None)
    if eal.does_asset_exist(SQUAD):
        cdo = unreal.get_default_object(unreal.load_asset(SQUAD).generated_class())
        print(LOG + "squad subtype", cdo.get_editor_property("Squad Subtype"),
              "units", [c.get_name() for c in cdo.get_editor_property("SquadUnitClasses")])
    spawner_cdo = unreal.get_default_object(unreal.load_asset(SPAWNER).generated_class())
    rows = [(str(r.get_editor_property("squad_subtype")), str(r.get_editor_property("unit_class")))
            for r in spawner_cdo.get_editor_property("SetupGerSquads")]
    print(LOG + "spawner ger rows", len(rows), [r for r in rows if "BURROW" in r[0]])


def run(steps=("textures", "materials", "mesh", "units", "squad", "spawner")):
    tex = import_textures() if "textures" in steps else None
    if "materials" in steps:
        cloth, kit = build_materials(tex)
    else:
        cloth, kit = unreal.load_asset(MI_CLOTH), unreal.load_asset(MI_KIT)
    sk = import_mesh(cloth, kit) if "mesh" in steps else unreal.load_asset(SK)
    if "units" in steps:
        build_units(sk)
    squad = build_squad() if "squad" in steps else None
    if "spawner" in steps:
        register_spawner(squad or unreal.load_asset(SQUAD))
    report()
    print(LOG + ("DONE OK" if not FAILURES else "DONE WITH FAILURES " + ", ".join(FAILURES)))


run(globals().get("BW_STEPS", ("textures", "materials", "mesh", "units", "squad", "spawner")))
