"""Ju 87 G (cannon Stuka) assets, step 1 of 2: the BK 3,7 pod mesh and its materials.

Run headless (any editor DLL; nothing here needs the new C++ class):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
after build_bk37_pod.py wrote Saved/Ju87GTest/blender/SM_Ju87G_BK37Pod.fbx.
Idempotent: re-imports the pod in place and rebuilds the material graph.
Step 2 (author_ju87g_blueprint.py, DebugGame) copies the Stuka mesh, adds the pod sockets and builds the Blueprint.
"""
import unreal

LOG = "[Ju87G] "
FOLDER = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Ju87G"
POD_FBX = "D:/UE5Projects/RTS_Survival/Saved/Ju87GTest/blender/SM_Ju87G_BK37Pod.fbx"
POD_MESH = FOLDER + "/SM_Ju87G_BK37Pod"
MASTER = FOLDER + "/M_Ju87G_BK37Pod"
MI_BODY = FOLDER + "/MI_Ju87G_BK37Pod_Body"
MI_BARREL = FOLDER + "/MI_Ju87G_BK37Pod_Barrel"

# Linear colours: the desert Stuka's sand-yellow (RLM 79-like, matched to its camo in -game screenshots), dark steel barrel.
BODY = {"Color": (0.42, 0.26, 0.09), "Metallic": 0.1, "Roughness": 0.6}
BARREL = {"Color": (0.018, 0.017, 0.016), "Metallic": 0.85, "Roughness": 0.4}

report = []


def log(line):
    report.append(line)
    unreal.log(LOG + line)


def ensure_folder():
    if not unreal.EditorAssetLibrary.does_directory_exist(FOLDER):
        unreal.EditorAssetLibrary.make_directory(FOLDER)


def build_master():
    mel = unreal.MaterialEditingLibrary
    material = unreal.load_asset(MASTER)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MASTER.split("/")[-1], FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(material)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, -100)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.03, 0.04, 0.03, 1.0))
    metallic = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 100)
    metallic.set_editor_property("parameter_name", "Metallic")
    metallic.set_editor_property("default_value", 0.2)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 200)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.5)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def build_instance(path, master, values):
    mel = unreal.MaterialEditingLibrary
    instance = unreal.load_asset(path)
    if instance is None:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            path.split("/")[-1], FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property("parent", master)
    r, g, b = values["Color"]
    mel.set_material_instance_vector_parameter_value(instance, "Color", unreal.LinearColor(r, g, b, 1.0))
    mel.set_material_instance_scalar_parameter_value(instance, "Metallic", values["Metallic"])
    mel.set_material_instance_scalar_parameter_value(instance, "Roughness", values["Roughness"])
    mel.update_material_instance(instance)
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    return instance


def import_pod(body, barrel):
    task = unreal.AssetImportTask()
    task.filename = POD_FBX
    task.destination_path = FOLDER
    task.destination_name = POD_MESH.split("/")[-1]
    task.replace_existing = True
    task.automated = True
    task.save = False
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.generate_lightmap_u_vs = False
    options.static_mesh_import_data.auto_generate_collision = False
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(POD_MESH)
    if mesh is None:
        raise RuntimeError("pod import failed: " + str(task.imported_object_paths))
    # A new import gets Nanite with a 1 % fallback; this is a 200-vertex prop.
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", nanite)
    slots = mesh.get_editor_property("static_materials")
    log("pod slots %s" % [str(s.get_editor_property("material_slot_name")) for s in slots])
    for index, slot in enumerate(slots):
        name = str(slot.get_editor_property("material_slot_name")).lower()
        mesh.set_material(index, barrel if "barrel" in name else body)
    bounds = mesh.get_bounding_box()
    log("pod bounds min=%s max=%s" % (bounds.min, bounds.max))
    # +X must be the barrel: the hider tip is 270 cm ahead of the pylon, the tail 80 cm behind.
    if not (bounds.max.x > 250.0 and bounds.min.x > -100.0 and bounds.min.z < -40.0):
        raise RuntimeError("pod axes or scale wrong: %s %s" % (bounds.min, bounds.max))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    return mesh


def run():
    ensure_folder()
    master = build_master()
    body = build_instance(MI_BODY, master, BODY)
    barrel = build_instance(MI_BARREL, master, BARREL)
    import_pod(body, barrel)
    for path in (MASTER, MI_BODY, MI_BARREL, POD_MESH):
        log("exists %s %s" % (path, unreal.EditorAssetLibrary.does_asset_exist(path)))


try:
    run()
except Exception as error:
    log("ERROR %s" % error)
