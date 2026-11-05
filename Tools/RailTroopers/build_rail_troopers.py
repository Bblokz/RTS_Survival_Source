"""Builds the Rail Troopers (Squad_Ger_RailTroopers) in Unreal from the Blender export.

Run inside the editor (UECP execute_python with this file's text). Idempotent: existing assets are updated.
Source art: D:/Blender/BlenderProjects/BlenderTanks/Infantry/GerRailTrooper (README there; rebuild with rt_bake.py).

  T_RailTrooperArmor_{BC,ORM,N,E}  armour textures (ORM = AO, roughness, metallic; E = emissive mask)
  M_RailTrooperArmor               armour material (GlowColor / GlowStrength parameters for the rail-blue parts)
  SK_RailTrooper                   scavenger body + armour on SK_PostApoSoldier_Skeleton and its physics asset;
                                   slot 0 the scavenger cloth (M_ScavengerNoCamo), slot 1 the armour
  Ger_Inf_RailTrooper              copy of Ger_Inf_Scavenger: SK_RailTrooper, Ger_Weapon_M920Rail
  Squad_Ger_RailTroopers           copy of Squad_Ger_Scavengers: Squad Subtype = Squad_Ger_RailTroopers, 4 troopers
  RTS_BP_RTSAsyncSpawner           SetupGerSquads row Squad_Ger_RailTroopers -> Squad_Ger_RailTroopers
"""
import unreal

SRC = "D:/Blender/BlenderProjects/BlenderTanks/Infantry/GerRailTrooper"
ROOT = "/Game/RTS_Survival/Blueprints/Squads/Ger/EliteInfantry/RailTroopers"
SCAV = "/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers"
POSTAPO = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic"
SKELETON = POSTAPO + "/Mesh/SK_PostApoSoldier_Skeleton"
PHYSICS = POSTAPO + "/Mesh/SK_PostApoSoldier_PhysicsAsset"
CLOTH_MAT = POSTAPO + "/Material/M_ScavengerNoCamo"
WEAPON = "/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/M920Rail/Ger_Weapon_M920Rail"
SPAWNER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner"
UNIT = ROOT + "/Ger_Inf_RailTrooper"
SQUAD = ROOT + "/Squad_Ger_RailTroopers"
SQUAD_SIZE = 4
GLOW = unreal.LinearColor(0.08, 0.55, 1.0, 1.0)

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary
LOG = "[RailTroopers] "


def save(path):
    ok = eal.save_asset(path, only_if_is_dirty=False)
    print(LOG + "save", path.split("/")[-1], ok)


def import_textures():
    tasks = []
    for key in ("BC", "ORM", "N", "E"):
        t = unreal.AssetImportTask()
        t.filename = f"{SRC}/Textures/T_RailTrooperArmor_{key}.png"
        t.destination_path = ROOT + "/Textures"
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    settings = {"BC": (True, unreal.TextureCompressionSettings.TC_DEFAULT),
                "ORM": (False, unreal.TextureCompressionSettings.TC_MASKS),
                "N": (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                "E": (False, unreal.TextureCompressionSettings.TC_GRAYSCALE)}
    out = {}
    for key, (srgb, comp) in settings.items():
        path = f"{ROOT}/Textures/T_RailTrooperArmor_{key}"
        tex = unreal.load_asset(path)
        tex.set_editor_property("srgb", srgb)
        tex.set_editor_property("compression_settings", comp)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        if key == "N":
            tex.set_editor_property("flip_green_channel", False)   # baked as DirectX already
        save(path)
        out[key] = tex
    return out


def build_material(tex):
    path = ROOT + "/Material/M_RailTrooperArmor"
    mat = unreal.load_asset(path) if eal.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_RailTrooperArmor", ROOT + "/Material", unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_skeletal_mesh", True)

    def sample(key, x, y, sampler):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        n.set_editor_property("parameter_name", "T_" + key)
        n.set_editor_property("texture", tex[key])
        n.set_editor_property("sampler_type", sampler)
        return n
    bc = sample("BC", -800, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    orm = sample("ORM", -800, 0, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    nrm = sample("N", -800, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    # A non-sRGB TC_GRAYSCALE texture needs the linear grayscale sampler, or the material fails to compile.
    em = sample("E", -800, 600, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    mel.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 900)
    col.set_editor_property("parameter_name", "GlowColor")
    col.set_editor_property("default_value", GLOW)
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 1100)
    strength.set_editor_property("parameter_name", "GlowStrength")
    strength.set_editor_property("default_value", 8.0)
    m1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, 700)
    mel.connect_material_expressions(em, "R", m1, "A")
    mel.connect_material_expressions(col, "", m1, "B")
    m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 700)
    mel.connect_material_expressions(m1, "", m2, "A")
    mel.connect_material_expressions(strength, "", m2, "B")
    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    save(path)
    return mat


def import_mesh(armor_mat):
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
    t.filename = SRC + "/Export/SK_RailTrooper.fbx"
    t.destination_path = ROOT + "/Mesh"
    t.destination_name = "SK_RailTrooper"
    t.replace_existing = True
    t.automated = True
    t.save = False
    t.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    print(LOG + "imported", list(t.get_editor_property("imported_object_paths")))
    sk = unreal.load_asset(ROOT + "/Mesh/SK_RailTrooper")
    sk.set_editor_property("physics_asset", unreal.load_asset(PHYSICS))
    mats = sk.get_editor_property("materials")
    for i, m in enumerate(mats):
        print(LOG + "slot", i, m.get_editor_property("material_slot_name"))
    # Slot order from the FBX: cloth, armour. The array is written back whole (its items are copies).
    new = []
    for i, m in enumerate(mats):
        m.set_editor_property("material_interface", unreal.load_asset(CLOTH_MAT) if i == 0 else armor_mat)
        new.append(m)
    sk.set_editor_property("materials", new)
    save(ROOT + "/Mesh/SK_RailTrooper")
    skel = sk.get_editor_property("skeleton")
    print(LOG + "skeleton", skel.get_path_name() if skel else None)
    return sk


def duplicate(source, target):
    # Reuse an existing copy: deleting a loaded Blueprint asks for confirmation, which can kill a windowless editor.
    if not eal.does_asset_exist(target):
        print(LOG + "dup", eal.duplicate_asset(source, target))
    return unreal.load_asset(target)


def component(bp, name):
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = BFL.get_object_for_blueprint(BFL.get_data(h), bp)
        if obj is not None and obj.get_name().startswith(name):
            return obj
    raise RuntimeError("no component " + name)


def build_unit(sk):
    bp = duplicate(SCAV + "/Ger_Inf_Scavenger", UNIT)
    bp.modify()
    mesh = component(bp, "CharacterMesh0")
    mesh.set_editor_property("skeletal_mesh_asset", sk)
    mesh.set_editor_property("override_materials", [])
    weapon = component(bp, "Weapon")
    weapon.set_editor_property("child_actor_class", unreal.load_asset(WEAPON).generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(UNIT)
    return bp


def build_squad(unit_bp):
    bp = duplicate(SCAV + "/Squad_Ger_Scavengers", SQUAD)
    bp.modify()
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("Squad Subtype", unreal.SquadSubtype.SQUAD_GER_RAIL_TROOPERS)
    cdo.set_editor_property("SquadUnitClasses", [unit_bp.generated_class()] * SQUAD_SIZE)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(SQUAD)
    return bp


def register_spawner(squad_bp):
    bp = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(bp.generated_class())
    bp.modify()
    rows = [r for r in cdo.get_editor_property("SetupGerSquads")
            if r.get_editor_property("squad_subtype") != unreal.SquadSubtype.SQUAD_GER_RAIL_TROOPERS]
    row = unreal.SquadTrainingOptionSetup()
    row.set_editor_property("squad_subtype", unreal.SquadSubtype.SQUAD_GER_RAIL_TROOPERS)
    row.set_editor_property("unit_class", squad_bp.generated_class())
    rows.append(row)
    cdo.set_editor_property("SetupGerSquads", rows)
    unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    print(LOG + "spawner ger squads", len(cdo.get_editor_property("SetupGerSquads")))


def run(steps=("textures", "material", "mesh", "unit", "squad", "spawner")):
    tex = import_textures() if "textures" in steps else {k: unreal.load_asset(f"{ROOT}/Textures/T_RailTrooperArmor_{k}")
                                                           for k in ("BC", "ORM", "N", "E")}
    mat = build_material(tex) if "material" in steps else unreal.load_asset(ROOT + "/Material/M_RailTrooperArmor")
    sk = import_mesh(mat) if "mesh" in steps else unreal.load_asset(ROOT + "/Mesh/SK_RailTrooper")
    unit = build_unit(sk) if "unit" in steps else unreal.load_asset(UNIT)
    squad = build_squad(unit) if "squad" in steps else unreal.load_asset(SQUAD)
    if "spawner" in steps:
        register_spawner(squad)


run(globals().get("RT_STEPS", ("textures", "material", "mesh", "unit", "squad", "spawner")))
