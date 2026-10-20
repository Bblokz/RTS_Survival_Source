"""Imports the Soviet grenade props (RGD-33, F-1, RG-42, RPG-43) built by
D:/Blender/BlenderProjects/BlenderTanks/Infantry/Infweapons/SovietGrenades/build_grenades.py.
They reuse the existing grenade/mine atlas material, so no textures are imported. Run through the UECP bridge's
execute_python. Docs: Source/Docs/Abilities/SovietGrenades.md"""
import unreal

SRC = "D:/Blender/BlenderProjects/BlenderTanks/Infantry/Infweapons/SovietGrenades/Exports/"
DEST = "/Game/RTS_Survival/Meshes/Weapons/Grenades/Soviet"
MATERIAL = "/Game/RTS_Survival/Meshes/Weapons/Grenades/Materials/M_GrenadesMinesAtlas"
NAMES = ["SM_Rus_RGD33", "SM_Rus_F1", "SM_Rus_RG42", "SM_Rus_RPG43"]

material = unreal.load_asset(MATERIAL)
tasks = []
for name in NAMES:
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = ui.static_mesh_import_data
    # The FBX is in metres (1 Blender unit = 100 cm); the importer honours the units.
    data.import_uniform_scale = 1.0
    data.combine_meshes = True
    data.auto_generate_collision = False
    data.generate_lightmap_u_vs = False
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task = unreal.AssetImportTask()
    task.filename = SRC + name + ".fbx"
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = ui
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in NAMES:
    path = DEST + "/" + name
    mesh = unreal.load_asset(path)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    mesh.set_material(0, material)
    box = mesh.get_bounding_box()
    print(name, "min", box.min, "max", box.max, "tris", mesh.get_num_triangles(0),
          "saved", unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
