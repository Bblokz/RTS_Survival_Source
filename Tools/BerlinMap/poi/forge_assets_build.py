"""Builds the Unreal assets of the Forge survey station bonus (BerlinForgeStation.md). Runs inside the throwaway editor
of forge_editor_server.py; send with forge_cmd.py, e.g.
    python forge_cmd.py -c "exec(open(r'.../forge_assets_build.py').read()); build_sounds()"

Steps (each idempotent, each saves what it makes):
    build_sounds()        imports Saved/BerlinForge/sounds/*.wav (forge_sounds_gen.py); the pump loop loops
    build_mpc()           MPC_ForgeSedation (SedationPulse, SedationFrost) read by the sedated crystal material
    build_forces_data()   DA_BerlinForces_<Difficulty> (UBerlinMissionForcesData): station garrison + HQ drop
"""
import os

import unreal

HOME = "/Game/RTS_Survival/Blueprints/Environment/Berlin/ForgeStation"
SOUNDS = HOME + "/Sounds"
DATA = "/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin"
SOUND_SRC = "D:/UE5Projects/RTS_Survival/Saved/BerlinForge/sounds"

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary


def import_file(src, dest, name=None, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if name:
        task.set_editor_property("destination_name", name)
    if options is not None:
        task.set_editor_property("options", options)
    ASSET_TOOLS.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def build_sounds():
    for file_name in sorted(os.listdir(SOUND_SRC)):
        if file_name.lower().endswith(".wav"):
            print("imported", import_file(os.path.join(SOUND_SRC, file_name), SOUNDS))
    loop = unreal.load_asset(SOUNDS + "/S_ForgeStation_PumpLoop")
    loop.set_editor_property("looping", True)
    EAL.save_loaded_asset(loop)
    print("pump loop looping:", loop.get_editor_property("looping"))


def build_mpc():
    path = HOME + "/MPC_ForgeSedation"
    if EAL.does_asset_exist(path):
        mpc = unreal.load_asset(path)
    else:
        mpc = ASSET_TOOLS.create_asset("MPC_ForgeSedation", HOME, unreal.MaterialParameterCollection,
                                       unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name in ("SedationPulse", "SedationFrost"):
        param = unreal.CollectionScalarParameter()
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", 0.0)
        params.append(param)
    mpc.set_editor_property("scalar_parameters", params)
    EAL.save_loaded_asset(mpc)
    print("MPC", path, [p.get_editor_property("parameter_name") for p in mpc.get_editor_property("scalar_parameters")])


# ----- forces per difficulty (BerlinForgeStation.md section 4 and 7) -----
SQ = unreal.SquadSubtype
TK = unreal.TankSubtype


def squad(subtype):
    unit = unreal.EnemyPlacementUnit()
    unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.SQUAD)
    unit.set_editor_property("squad_subtype", subtype)
    return unit


def vehicle(subtype):
    unit = unreal.EnemyPlacementUnit()
    unit.set_editor_property("kind", unreal.EnemyPlacementUnitKind.VEHICLE)
    unit.set_editor_property("vehicle_subtype", subtype)
    return unit


def post(post_id, unit):
    entry = unreal.EnemyStrongpointPostUnits()
    entry.set_editor_property("post_id", post_id)
    entry.set_editor_property("unit_options", [unit])
    return entry


def garrison_for(level):
    """level 0 NewToRTS .. 3 Brutal; each level adds posts (section 4 table)."""
    posts = [post("Arch", squad(SQ.SQUAD_RUS_MOSIN)), post("RimCrewA", squad(SQ.SQUAD_RUS_HAZMAT_ENGINEERS))]
    if level >= 1:
        posts += [post("Console", squad(SQ.SQUAD_RUS_OKHOTNIK)), post("RigFoot", squad(SQ.SQUAD_RUS_HAZMAT_BAZOOKA))]
    if level >= 2:
        posts += [post("RimCrewB", squad(SQ.SQUAD_RUS_HAZMAT_ENGINEERS)), post("CrewHut", vehicle(TK.TANK_BA12))]
    if level >= 3:
        posts += [post("CrewEscort", squad(SQ.SQUAD_RUS_MOSIN))]
    return posts


REINFORCEMENTS = {
    0: ([TK.TANK_PZ_IV_F1, TK.TANK_SDKFZ251], [SQ.SQUAD_GER_JAGER_TRUPP_KAR98K, SQ.SQUAD_GER_LMG_SQUAD]),
    1: ([TK.TANK_PZ_IV_F1], [SQ.SQUAD_GER_JAGER_TRUPP_KAR98K, SQ.SQUAD_GER_LMG_SQUAD]),
    2: ([TK.TANK_PZ_III_J], [SQ.SQUAD_GER_JAGER_TRUPP_KAR98K]),
    3: ([], [SQ.SQUAD_GER_JAGER_TRUPP_KAR98K, SQ.SQUAD_GER_LMG_SQUAD]),
}
DIFFICULTY_ASSETS = [("NewToRTS", 0), ("Normal", 1), ("Hard", 2), ("Brutal", 3)]


def build_forces_data():
    paths = {}
    for name, level in DIFFICULTY_ASSETS:
        asset_name = "DA_BerlinForces_" + name
        path = DATA + "/" + asset_name
        if EAL.does_asset_exist(path):
            asset = unreal.load_asset(path)
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.BerlinMissionForcesData)
            asset = ASSET_TOOLS.create_asset(asset_name, DATA, unreal.BerlinMissionForcesData, factory)
        forces = unreal.BerlinForgeStationForces()
        forces.set_editor_property("garrison", garrison_for(level))
        payload = unreal.PlayerAirDropPayload()
        vehicles, squads = REINFORCEMENTS[level]
        payload.set_editor_property("vehicles", vehicles)
        payload.set_editor_property("squads", squads)
        forces.set_editor_property("reinforcements", payload)
        asset.set_editor_property("forge_station", forces)
        EAL.save_loaded_asset(asset)
        paths[name] = path
        print(asset_name, "posts", [str(p.get_editor_property("post_id")) for p in forces.get_editor_property("garrison")],
              "drop", len(vehicles), "vehicles", len(squads), "squads")
    return paths


# ----- sedated crystal material (option C): the station's Radixite samples pulse and frost with the cycle -----
CRYSTAL_NORMAL = "/Game/RTS_Survival/Blueprints/Resources/Crystals/Textures/Cristal_3A_LOW_Cristal_03_Normal"


def _expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def build_crystal_material():
    """M_ForgeSedatedCrystal + MI_ForgeSedatedCrystal: amber crystal glow; SedationPulse brightens it, SedationFrost
    turns it pale, rough and dark (MPC_ForgeSedation, driven by ABerlinForgeStation)."""
    mel = unreal.MaterialEditingLibrary
    path = HOME + "/M_ForgeSedatedCrystal"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    material = ASSET_TOOLS.create_asset("M_ForgeSedatedCrystal", HOME, unreal.Material, unreal.MaterialFactoryNew())
    mpc = unreal.load_asset(HOME + "/MPC_ForgeSedation")

    crystal = _expr(material, unreal.MaterialExpressionVectorParameter, -1400, -300)
    crystal.set_editor_property("parameter_name", "CrystalColor")
    crystal.set_editor_property("default_value", unreal.LinearColor(1.0, 0.62, 0.12, 1.0))
    frost_color = _expr(material, unreal.MaterialExpressionVectorParameter, -1400, -60)
    frost_color.set_editor_property("parameter_name", "FrostColor")
    frost_color.set_editor_property("default_value", unreal.LinearColor(0.72, 0.86, 1.0, 1.0))
    glow = _expr(material, unreal.MaterialExpressionScalarParameter, -1400, 160)
    glow.set_editor_property("parameter_name", "GlowStrength")
    glow.set_editor_property("default_value", 6.0)
    boost = _expr(material, unreal.MaterialExpressionScalarParameter, -1400, 300)
    boost.set_editor_property("parameter_name", "PulseBoost")
    boost.set_editor_property("default_value", 4.0)

    pulse = _expr(material, unreal.MaterialExpressionCollectionParameter, -1400, 440)
    pulse.set_editor_property("collection", mpc)
    pulse.set_editor_property("parameter_name", "SedationPulse")
    frost = _expr(material, unreal.MaterialExpressionCollectionParameter, -1400, 580)
    frost.set_editor_property("collection", mpc)
    frost.set_editor_property("parameter_name", "SedationFrost")

    # base colour: dimmed crystal colour -> frost colour
    dim = _expr(material, unreal.MaterialExpressionMultiply, -1100, -300)
    dim.set_editor_property("const_b", 0.45)
    mel.connect_material_expressions(crystal, "", dim, "A")
    base = _expr(material, unreal.MaterialExpressionLinearInterpolate, -800, -200)
    mel.connect_material_expressions(dim, "", base, "A")
    mel.connect_material_expressions(frost_color, "", base, "B")
    mel.connect_material_expressions(frost, "", base, "Alpha")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # emissive: crystal * glow * (1 + pulse * boost) * (1 - 0.9 frost)
    pulse_scaled = _expr(material, unreal.MaterialExpressionMultiply, -1100, 360)
    mel.connect_material_expressions(pulse, "", pulse_scaled, "A")
    mel.connect_material_expressions(boost, "", pulse_scaled, "B")
    pulse_plus = _expr(material, unreal.MaterialExpressionAdd, -950, 360)
    pulse_plus.set_editor_property("const_b", 1.0)
    mel.connect_material_expressions(pulse_scaled, "", pulse_plus, "A")
    frost_scaled = _expr(material, unreal.MaterialExpressionMultiply, -1100, 560)
    frost_scaled.set_editor_property("const_b", 0.9)
    mel.connect_material_expressions(frost, "", frost_scaled, "A")
    frost_damp = _expr(material, unreal.MaterialExpressionOneMinus, -950, 560)
    mel.connect_material_expressions(frost_scaled, "", frost_damp, "")
    glow_colour = _expr(material, unreal.MaterialExpressionMultiply, -800, 100)
    mel.connect_material_expressions(crystal, "", glow_colour, "A")
    mel.connect_material_expressions(glow, "", glow_colour, "B")
    glow_pulse = _expr(material, unreal.MaterialExpressionMultiply, -650, 200)
    mel.connect_material_expressions(glow_colour, "", glow_pulse, "A")
    mel.connect_material_expressions(pulse_plus, "", glow_pulse, "B")
    emissive = _expr(material, unreal.MaterialExpressionMultiply, -500, 300)
    mel.connect_material_expressions(glow_pulse, "", emissive, "A")
    mel.connect_material_expressions(frost_damp, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # roughness 0.15 -> 0.75 with frost
    rough = _expr(material, unreal.MaterialExpressionLinearInterpolate, -650, 700)
    rough.set_editor_property("const_a", 0.15)
    rough.set_editor_property("const_b", 0.75)
    mel.connect_material_expressions(frost, "", rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    normal_tex = unreal.load_asset(CRYSTAL_NORMAL)
    if normal_tex is not None:
        normal = _expr(material, unreal.MaterialExpressionTextureSampleParameter2D, -800, 900)
        normal.set_editor_property("parameter_name", "NormalMap")
        normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        normal.set_editor_property("texture", normal_tex)
        mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(material)
    EAL.save_loaded_asset(material)
    mi_path = HOME + "/MI_ForgeSedatedCrystal"
    if EAL.does_asset_exist(mi_path):
        mi = unreal.load_asset(mi_path)
    else:
        mi = ASSET_TOOLS.create_asset("MI_ForgeSedatedCrystal", HOME, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, material)
    EAL.save_loaded_asset(mi)
    print("material", path, "instance", mi_path)


# ----- split station meshes (Blender v3_split) and the three target buildings -----
SPLIT_SRC = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinForgeStation/v3_split/"
SPLIT = HOME + "/Split"
RUS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
SLOT_MATERIALS = {
    "M_FS_Concrete": RUS + "RusBunkers/Materials/MI_SovietConcrete_Tilling",
    "M_FS_CamoSteel": "/Game/RTS_Survival/Meshes/Buildings/Industrial/Textures/M_Nuclear_Tower_Soviet",
    "M_FS_Plant": RUS + "RusBuildings/CoolingTowersMeshes/Materials/MI_CoolingTowers_Soviet",
    # every Radixite sample on the station freezes and pulses with the sedation cycle
    "M_FS_Radixite": HOME + "/MI_ForgeSedatedCrystal",
}
SPLIT_MESHES = ["SM_ForgeStation_Shell", "SM_ForgeStation_Rig", "SM_ForgeStation_Rig_Destroyed",
                "SM_ForgeStation_DrillString", "SM_ForgeStation_CryoBlock", "SM_ForgeStation_CryoBlock_Destroyed",
                "SM_ForgeStation_CommandBunker", "SM_ForgeStation_CommandBunker_Destroyed", "SM_ForgeStation_Dish",
                "SM_ForgeStation_SampleStake", "SM_ForgeStation_CryoCanister"]
TARGET_BASE = RUS + "RusBuildings/Rus_Building_CoolingTowers_Soviet"
TARGET_BPS = {"BP_ForgeStation_Rig": "SM_ForgeStation_Rig",
              "BP_ForgeStation_CryoBlock": "SM_ForgeStation_CryoBlock",
              "BP_ForgeStation_CommandBunker": "SM_ForgeStation_CommandBunker"}


def _import_fbx(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("one_convex_hull_per_ucx", True)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = SPLIT_SRC + name + ".FBX"
    task.destination_path = SPLIT
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        ASSET_TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")
    mesh = unreal.load_asset(SPLIT + "/" + name)
    if mesh is None:
        raise RuntimeError("import failed " + task.filename)
    mesh.set_editor_property("light_map_coordinate_index", 1)
    slots = mesh.get_editor_property("static_materials")
    rebuilt = []
    for slot in slots:
        imported = str(slot.get_editor_property("imported_material_slot_name"))
        key = next((k for k in SLOT_MATERIALS if imported.startswith(k)), None)
        if key is None:
            raise RuntimeError("%s: unexpected slot %s" % (name, imported))
        slot.set_editor_property("material_interface", unreal.load_asset(SLOT_MATERIALS[key]))
        slot.set_editor_property("material_slot_name", key)
        rebuilt.append(slot)
    mesh.set_editor_property("static_materials", rebuilt)
    EAL.save_loaded_asset(mesh, False)
    body = mesh.get_editor_property("body_setup")
    hulls = len(body.get_editor_property("agg_geom").get_editor_property("convex_elems")) if body else 0
    box = mesh.get_bounding_box()
    print("mesh", name, "tris", mesh.get_num_triangles(0), "hulls", hulls, "bounds", box.min, box.max)
    return mesh


def build_split_meshes():
    return {name: _import_fbx(name) for name in SPLIT_MESHES}


def _subobjects(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    out = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj is not None:
            out.append((handle, obj))
    return out


def build_target_bps():
    """Copies of the passive Forge cooling-tower building (health, selection, holes) with the station part's mesh;
    the cooling tower's own steam emitters are switched off."""
    made = {}
    for bp_name, mesh_name in TARGET_BPS.items():
        path = HOME + "/" + bp_name
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
        bp = EAL.duplicate_asset(TARGET_BASE, path)
        mesh = unreal.load_asset(SPLIT + "/" + mesh_name)
        box = mesh.get_bounding_box()
        size = box.max - box.min
        centre = (box.max + box.min) * 0.5
        for handle, obj in _subobjects(bp):
            if isinstance(obj, unreal.NiagaraComponent):
                obj.set_editor_property("auto_activate", False)
                obj.set_editor_property("asset", None)
            elif isinstance(obj, unreal.StaticMeshComponent) and obj.get_name() == "BuildingMeshComponent":
                obj.set_editor_property("static_mesh", mesh)
            elif isinstance(obj, unreal.BoxComponent) and "Selection" in obj.get_name():
                obj.set_editor_property("box_extent", unreal.Vector(size.x * 0.5, size.y * 0.5, max(size.z * 0.5, 150)))
                obj.set_editor_property("relative_location", unreal.Vector(centre.x, centre.y, centre.z))
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.get_editor_property("building_mesh_component").set_editor_property("static_mesh", mesh)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        EAL.save_loaded_asset(bp, False)
        check = unreal.get_default_object(bp.generated_class()).get_editor_property("building_mesh_component")
        print("bp", path, "mesh", check.get_editor_property("static_mesh").get_name(), "size", size)
        made[bp_name] = path
    return made


def remove_target_smoke():
    """The cooling tower's two barracks-smoke emitters do not belong on the station parts: delete them."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for bp_name in TARGET_BPS:
        bp = unreal.load_asset(HOME + "/" + bp_name)
        handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        root = handles[0]
        removed = 0
        for handle, obj in _subobjects(bp):
            if isinstance(obj, unreal.NiagaraComponent):
                removed += subsystem.delete_subobject(root, handle, bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        EAL.save_loaded_asset(bp, False)
        left = [o.get_name() for h, o in _subobjects(bp) if isinstance(o, unreal.NiagaraComponent)]
        print(bp_name, "removed", removed, "niagara left", left)
