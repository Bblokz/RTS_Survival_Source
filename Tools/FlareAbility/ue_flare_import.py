"""Imports the flare ability meshes (Blender: (KADO-3D)/Sdkfz-231/FlareAbility/build_flare.py), the palette texture,
creates M_Flare and prints the FlareLaunch socket values for the Sd.Kfz. 231 turret. Idempotent; run in the editor:
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/FlareAbility/ue_flare_import.py").read())"""
import json
import unreal

SRC = "D:/UE5Projects/RTS_Survival/Saved/AgentScripts/FlareAbility/src/"  # the Blender exports, copied into the project
FLARE_DIR = "/Game/RTS_Survival/SpecialEffects/Flare"
TURRET_DIR = "/Game/RTS_Survival/Blueprints/Turrets/German/Light/Sdkfz_231Turret/Meshes"
TURRET_MESH = TURRET_DIR + "/SK_sdkfz_231_Turret"
TURRET_MATERIAL = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz231/Materials/MI_Sdkfz_231"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
saved = []


def import_mesh(file_name, dest, name):
    task = unreal.AssetImportTask()
    task.filename = SRC + file_name
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = ui.static_mesh_import_data
    data.combine_meshes = True
    data.generate_lightmap_u_vs = False
    data.auto_generate_collision = False
    data.import_uniform_scale = 1.0
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options = ui
    asset_tools.import_asset_tasks([task])
    mesh = unreal.load_asset(dest + "/" + name)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    return mesh


def import_texture(file_name, dest, name):
    task = unreal.AssetImportTask()
    task.filename = SRC + file_name
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(dest + "/" + name)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    return texture


def make_flare_material(texture):
    path = FLARE_DIR + "/M_Flare"
    material = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if material is None:
        material = asset_tools.create_asset("M_Flare", FLARE_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(material)
    sample = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    sample.set_editor_property("parameter_name", "Palette")
    sample.set_editor_property("texture", texture)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 200)
    rough.set_editor_property("r", 0.85)
    mel.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # the canopy is a single sheet
    material.set_editor_property("two_sided", True)
    mel.recompile_material(material)
    return material


def print_turret_socket():
    """SkeletalMeshSocket.socket_name is read-only from Python, so the socket is added with the UECP bridge action
    add_mesh_socket (it writes to the turret's own skeleton, like the turret's other sockets). Prints its values:
    location / rotation / scale must be passed as arrays. B_Turret has a 33x scale, hence the small relative scale."""
    frame = json.loads(open(SRC + "flare_socket.json").read())
    mesh = unreal.load_asset(TURRET_MESH)
    pose = unreal.AnimPoseExtensions.get_reference_pose(mesh.get_editor_property("skeleton"))
    bone_cs = unreal.AnimPoseExtensions.get_bone_pose(pose, "B_Turret", unreal.AnimPoseSpaces.WORLD)
    pitch, yaw, roll = frame["rotation_pyr"]
    desired = unreal.Transform(unreal.Vector(*frame["location"]), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                               unreal.Vector(1, 1, 1))
    relative = unreal.MathLibrary.compose_transforms(desired, unreal.MathLibrary.invert_transform(bone_cs))
    rotation = relative.rotation.rotator()
    print("add_mesh_socket(mesh_path=%r, socket_name='FlareLaunch', bone_name='B_Turret', location=%s, rotation=%s, "
          "scale=%s)" % (TURRET_MESH, [relative.translation.x, relative.translation.y, relative.translation.z],
                         [rotation.pitch, rotation.yaw, rotation.roll],
                         [relative.scale3d.x, relative.scale3d.y, relative.scale3d.z]))


palette = import_texture("T_Flare_Palette.png", FLARE_DIR, "T_Flare_Palette")
flare_material = make_flare_material(palette)
cartridge = import_mesh("SM_Flare_Cartridge.fbx", FLARE_DIR, "SM_Flare_Cartridge")
parachute = import_mesh("SM_Flare_Parachute.fbx", FLARE_DIR, "SM_Flare_Parachute")
for mesh in (cartridge, parachute):
    mesh.set_material(0, flare_material)
rack = import_mesh("SM_FlareDischarger_231.fbx", TURRET_DIR, "SM_FlareDischarger_231")
rack.set_material(0, unreal.load_asset(TURRET_MATERIAL))
print_turret_socket()

for asset in (palette, flare_material, cartridge, parachute, rack):
    ok = eal.save_loaded_asset(asset, False)
    saved.append((asset.get_name(), ok))
for mesh in (cartridge, parachute, rack):
    bounds = mesh.get_bounds()
    print("MESH", mesh.get_name(), "verts", mesh.get_num_vertices(0), "extent", bounds.box_extent,
          "nanite", mesh.get_editor_property("nanite_settings").enabled)
print("SAVED", saved)
