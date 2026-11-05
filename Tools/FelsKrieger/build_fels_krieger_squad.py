"""Builds the Fels Krieger medium walker squad (Squad_Ger_FelsKrieger) in Unreal. Idempotent: existing assets are updated.

Run in the shared editor (UECP execute_python: exec(open(<this file>).read())) or headless:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Steps (FK_STEPS, default all): "walker", "squad", "spawner". The spawner step needs async_spawner.lockdir.

  SK_FelsKrieger            the Etasphere07 walker mesh (SK_Etasphera07, committed 2025-12 without animations or a
                            Blueprint, on its own skeleton SK_Etasphera07_Skeleton) re-imported onto
                            InfantryMaster_Skeleton, like EspSphere_01 (Red Spine) and EspSphere_04 (KR-12 Dachs).
                            Both skeletons have the same 68 mannequin bones; only the bone lengths differ. Python
                            cannot reassign a mesh's skeleton, so on 2026-11-05 SK_Etasphera07 was exported to FBX
                            (Saved/FelsKriegerTest/src) and imported with the legacy FBX importer
                            (Interchange.FeatureFlags.Import.FBX 0, restored after) with Skeleton = InfantryMaster,
                            materials/textures/animations off and SK_Etasphera07_PhysicsAsset: same 18122 vertices,
                            ref pose within 1e-6 cm, material slot resolved to M_EtaSphere07 by name.
                            The pack has no walker animations: the walker uses the infantry AnimBp_SquadUnit (idle,
                            walk, fire) exactly as Red Spine and KR-12 Dachs do.
  Ger_Inf_Walker_FelsKrieger copy of Ger_Inf_Walker_RedSpine: mesh SK_FelsKrieger at scale MESH_SCALE (Red Spine 1.25),
                            AnimBp_SquadUnit, weapon Ger_Weapon_RipperGun at the Sturm Pioneer Ripper's hand offset.
  Squad_Ger_FelsKrieger     copy of Squad_Ger_RedSpine: Squad Subtype = Squad_Ger_FelsKrieger, 3x the walker.
  RTS_BP_RTSAsyncSpawner    SetupGerSquads row Squad_Ger_FelsKrieger -> Squad_Ger_FelsKrieger (only that row).
The squad copy's construction script sets Squad Subtype with a literal (copied from Red Spine), which wins over the
CDO. Python cannot edit K2 node pins: set it through UECP set_pin_default(blueprint_path=SQUAD, graph_name=
"UserConstructionScript", node_id=<"Set Squad Subtype" node>, pin_name="Squad Subtype", value="Squad_Ger_FelsKrieger"),
then compile + save. A fresh duplicate needs that step again; -FelsKriegerValidate checks the subtype.
Health, armour, speed and cost come from C++ (CPPGameState squad data), not from these Blueprints.
"""
import time

import unreal

try:  # the UECP python sandbox blocks os; headless runs have it
    import os
except ImportError:
    os = None

ROOT = "/Game/RTS_Survival/Blueprints/Squads/Ger/Walkers/FelsKrieger"
SOURCE_WALKER = "/Game/RTS_Survival/Blueprints/Squads/Ger/Walkers/RedSpine/Ger_Inf_Walker_RedSpine"
SOURCE_SQUAD = "/Game/RTS_Survival/Blueprints/Squads/Ger/Walkers/RedSpine/Squad_Ger_RedSpine"
MESH = ROOT + "/SK_FelsKrieger"
WALKER = ROOT + "/Ger_Inf_Walker_FelsKrieger"
SQUAD = ROOT + "/Squad_Ger_FelsKrieger"
SPAWNER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner"
WEAPON = "/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/RifleMG/Ger_Weapon_RipperGun"
ANIM_BP = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimBp_SquadUnit"
WALKER_COUNT = 3
# Etasphere07 is 183 cm tall at scale 1, Red Spine's Etasphere01 219 cm at 1.25 (274 cm). The medium walker stands a
# head taller than the light one.
MESH_SCALE = globals().get("FK_MESH_SCALE", 1.6)
# Hand offset of the Ripper Gun on the Sturm Pioneer Ripper (GunSocket on hand_r); rotation as every rifle soldier.
WEAPON_LOCATION = unreal.Vector(-17.356119, 2.884437, 2.949723)
WEAPON_ROTATION = unreal.Rotator(roll=2.000944, pitch=-5.0, yaw=-179.923454)

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = "[FelsKrieger] "
FAILURES = []


def member(enum_type, name):
    """Enum member by its C++ name (Squad_Ger_FelsKrieger -> SQUAD_GER_FELS_KRIEGER), whatever the Python spelling."""
    wanted = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.replace("_", "").lower() == wanted:
            return getattr(enum_type, attr)
    raise RuntimeError("no %s member %s" % (enum_type.__name__, name))


FELS_KRIEGER = member(unreal.SquadSubtype, "Squad_Ger_FelsKrieger")


def disk_mtime(asset_path):
    if os is None:
        return None
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    file_path = os.path.join(content, asset_path[len("/Game/"):] + ".uasset")
    return os.path.getmtime(file_path) if os.path.exists(file_path) else 0.0


def save(path, retries=30):
    """Saves; with os available proves the file changed on disk and retries while another process holds it."""
    for attempt in range(retries):
        before = disk_mtime(path)
        ok = eal.save_asset(path, only_if_is_dirty=False)
        after = disk_mtime(path)
        if (before is None and ok) or (before is not None and after > before):
            print(LOG + "saved", path.split("/")[-1], time.ctime(after) if after else "")
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


def build_walker():
    if not eal.does_asset_exist(WALKER):
        print(LOG + "dup", SOURCE_WALKER.split("/")[-1], "->", WALKER.split("/")[-1],
              eal.duplicate_asset(SOURCE_WALKER, WALKER))
    bp = unreal.load_asset(WALKER)
    mesh_asset = unreal.load_asset(MESH)
    if mesh_asset.skeleton.get_name() != "InfantryMaster_Skeleton":
        FAILURES.append("SK_FelsKrieger not on InfantryMaster_Skeleton")
    bp.modify()
    mesh = component(bp, "CharacterMesh0")
    anim_class = unreal.load_asset(ANIM_BP).generated_class()
    mesh.set_editor_property("skeletal_mesh_asset", mesh_asset)
    mesh.set_editor_property("anim_class", anim_class)
    mesh.set_editor_property("relative_scale3d", unreal.Vector(MESH_SCALE, MESH_SCALE, MESH_SCALE))
    weapon = component(bp, "Weapon")
    weapon.set_editor_property("child_actor_class", unreal.load_asset(WEAPON).generated_class())
    weapon.set_editor_property("relative_location", WEAPON_LOCATION)
    weapon.set_editor_property("relative_rotation", WEAPON_ROTATION)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(WALKER)
    return bp


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
    cdo.set_editor_property("Squad Subtype", FELS_KRIEGER)
    cdo.set_editor_property("SquadUnitClasses", units)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # A compile after the CDO edit can re-dirty the package; set again and save after the last compile.
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", FELS_KRIEGER)
    cdo.set_editor_property("SquadUnitClasses", units)
    save(SQUAD)
    return bp


def register_spawner(squad_bp):
    bp = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(bp.generated_class())
    rows_before = cdo.get_editor_property("SetupGerSquads")
    other_rows = [r for r in rows_before if r.get_editor_property("squad_subtype") != FELS_KRIEGER]
    existing = [r for r in rows_before if r.get_editor_property("squad_subtype") == FELS_KRIEGER]
    if len(existing) == 1 and existing[0].get_editor_property("unit_class") == squad_bp.generated_class():
        print(LOG + "spawner row already set, not saving")
        return
    bp.modify()
    row = unreal.SquadTrainingOptionSetup()
    row.set_editor_property("squad_subtype", FELS_KRIEGER)
    row.set_editor_property("unit_class", squad_bp.generated_class())
    cdo.set_editor_property("SetupGerSquads", list(other_rows) + [row])
    print(LOG + "spawner ger rows", len(rows_before), "->", len(other_rows) + 1)
    save(SPAWNER)


def report():
    """Reads the saved state back from the loaded objects."""
    walker_bp = unreal.load_asset(WALKER)
    weapon = component(walker_bp, "Weapon")
    mesh = component(walker_bp, "CharacterMesh0")
    mesh_asset = mesh.get_editor_property("skeletal_mesh_asset")
    anim = mesh.get_editor_property("anim_class")
    print(LOG + "walker weapon", weapon.get_editor_property("child_actor_class").get_name(),
          weapon.get_editor_property("relative_location"),
          "mesh", mesh_asset.get_name() if mesh_asset else None,
          "skeleton", mesh_asset.skeleton.get_name() if mesh_asset else None,
          "anim", anim.get_name() if anim else None,
          "scale", mesh.get_editor_property("relative_scale3d"), "rel", mesh.get_editor_property("relative_location"))
    cdo = unreal.get_default_object(unreal.load_asset(SQUAD).generated_class())
    print(LOG + "squad CDO subtype", cdo.get_editor_property("Squad Subtype"),
          "units", [c.get_name() for c in cdo.get_editor_property("SquadUnitClasses")])
    spawner_cdo = unreal.get_default_object(unreal.load_asset(SPAWNER).generated_class())
    rows = [(str(r.get_editor_property("squad_subtype")), r.get_editor_property("unit_class").get_name()
             if r.get_editor_property("unit_class") else None)
            for r in spawner_cdo.get_editor_property("SetupGerSquads")]
    print(LOG + "spawner ger rows", len(rows), [r for r in rows if "FELS" in r[0].upper()])


def run(steps=("walker", "squad", "spawner")):
    if "walker" in steps:
        build_walker()
    squad = build_squad() if "squad" in steps else unreal.load_asset(SQUAD)
    if "spawner" in steps:
        register_spawner(squad)
    report()
    print(LOG + ("DONE OK" if not FAILURES else "DONE WITH FAILURES " + ", ".join(FAILURES)))


run(globals().get("FK_STEPS", ("walker", "squad", "spawner")))
