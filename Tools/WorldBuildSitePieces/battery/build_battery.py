"""Headless: imports the battery world map piece (bl_build.py output) with its own textures and materials.

Run: run_headless.sh build_battery.py   (UnrealEditor-Win64-DebugGame-Cmd -run=pythonscript -nullrhi)
Makes, all in /Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Battery:
  SM_BuildSitePiece_Battery, M_BuildSitePiece_Battery (master), MI_BuildSitePiece_Battery_<part>, T_BuildSitePiece_*.
Log: Saved/WorldBuildSitePieces/battery/build.txt
"""
import os
import struct
import zlib

import unreal

FOLDER = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Battery"
MESH_NAME = "SM_BuildSitePiece_Battery"
MASTER_NAME = "M_BuildSitePiece_Battery"
SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                     "WorldBuildSitePieces", "battery")
FBX = os.path.join(SAVED, MESH_NAME + ".fbx")
TEXTURE_DIR = os.path.join(SAVED, "tex")
WHITE = "T_BuildSitePiece_Battery_White"

# The campaign level lights map pieces dim and brown, and this piece has no light of its own: the colours are pushed
# up (Brightness) and a part of the base colour is added as emissive (Emissive), so the piece reads at map distance.
EMISSIVE = 0.8
# The level light is warm and strong from above: a dark earth floor, so the light gun and the light sandbag ring stand
# out against it (picked from the campaign level render; a light floor washed the gun out).
# slot -> (texture or None, tint, brightness, roughness, metallic)
PARTS = {
    "Pit": (None, (0.11, 0.095, 0.075), 1.0, 0.95, 0.0),        # dark trodden earth floor
    "Ammo": (None, (0.30, 0.30, 0.13), 1.0, 0.7, 0.0),          # olive ammunition boxes
    "Sandbags": ("T_BuildSitePiece_Battery_Sandbags_D", (1.0, 0.92, 0.75), 2.2, 0.95, 0.0),
    "Gun": ("T_BuildSitePiece_Battery_Gun_D", (0.92, 0.97, 1.0), 1.2, 0.55, 0.2),
    "Wheels": ("T_BuildSitePiece_Battery_Wheels_D", (1.0, 1.0, 1.0), 0.8, 0.7, 0.0),
    "Trail": ("T_BuildSitePiece_Battery_Trail_D", (0.92, 0.97, 1.0), 1.2, 0.55, 0.2),
}

LINES = []
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(text):
    LINES.append(str(text))
    unreal.log("BATTERY " + str(text))


def save(asset):
    if not eal.save_loaded_asset(asset, False):
        log("SAVE FAILED " + asset.get_path_name())


def write_flat_png(path, rgb, size=4):
    row = b"\x00" + bytes(rgb) * size

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(row * size))
                     + chunk(b"IEND", b""))


def import_texture(name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(TEXTURE_DIR, name + ".png")
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + name)
    if texture is None:
        log("TEXTURE IMPORT FAILED " + name)
        return None
    texture.set_editor_property("srgb", True)
    save(texture)
    log("texture %s %dx%d" % (name, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
    return texture


def build_master(white):
    path = FOLDER + "/" + MASTER_NAME
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    material = asset_tools.create_asset(MASTER_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    def scalar(name, value, y):
        node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        return node

    texture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1200, 0)
    texture.set_editor_property("parameter_name", "BaseColor")
    texture.set_editor_property("texture", white)
    tint = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1200, 300)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -700, 100)
    mel.connect_material_expressions(texture, "RGB", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")

    bright = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 100)
    mel.connect_material_expressions(tinted, "", bright, "A")
    mel.connect_material_expressions(scalar("Brightness", 1.0, 250), "", bright, "B")
    mel.connect_material_property(bright, "", unreal.MaterialProperty.MP_BASE_COLOR)

    glow = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 400)
    mel.connect_material_expressions(bright, "", glow, "A")
    mel.connect_material_expressions(scalar("Emissive", EMISSIVE, 500), "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.connect_material_property(scalar("Roughness", 0.8, 650), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Metallic", 0.0, 800), "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    save(material)
    return material


def build_instance(master, part, texture, tint, brightness, roughness, metallic):
    name = "MI_BuildSitePiece_Battery_" + part
    path = FOLDER + "/" + name
    instance = unreal.load_asset(path) if eal.does_asset_exist(path) else asset_tools.create_asset(
        name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, master)
    if texture is not None:
        mel.set_material_instance_texture_parameter_value(instance, "BaseColor", texture)
    mel.set_material_instance_vector_parameter_value(instance, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(instance, "Brightness", brightness)
    mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
    mel.set_material_instance_scalar_parameter_value(instance, "Metallic", metallic)
    mel.update_material_instance(instance)
    save(instance)
    return instance


def import_mesh():
    task = unreal.AssetImportTask()
    task.filename = FBX
    task.destination_path = FOLDER
    task.destination_name = MESH_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.combine_meshes = True
    data.auto_generate_collision = False
    data.generate_lightmap_u_vs = False
    data.import_uniform_scale = 1.0
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options = options
    before = set(eal.list_assets(FOLDER, recursive=False, include_folder=False))
    asset_tools.import_asset_tasks([task])
    created = set(eal.list_assets(FOLDER, recursive=False, include_folder=False)) - before
    log("import created %s" % sorted(created))
    return unreal.load_asset(FOLDER + "/" + MESH_NAME)


def finish_mesh(mesh, instances):
    for index, static_material in enumerate(mesh.static_materials):
        slot = str(static_material.material_slot_name)
        part = slot.replace("BuildSitePiece_Battery_", "")
        if part in instances:
            mesh.set_material(index, instances[part])
        else:
            log("NO MATERIAL FOR SLOT %d %s" % (index, slot))
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    save(mesh)


def report(mesh):
    bounds = mesh.get_bounds()
    box = mesh.get_bounding_box()
    log("mesh %s tris=%d lods=%d" % (mesh.get_path_name(), mesh.get_num_triangles(0), mesh.get_num_lods()))
    log("  box min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)" % (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y,
                                                         box.max.z))
    log("  bounds origin=(%.2f %.2f %.2f) extent=(%.2f %.2f %.2f)" % (
        bounds.origin.x, bounds.origin.y, bounds.origin.z, bounds.box_extent.x, bounds.box_extent.y,
        bounds.box_extent.z))
    description = mesh.get_static_mesh_description(0)
    radius = 0.0
    for index in range(description.get_vertex_count()):
        position = description.get_vertex_position(unreal.VertexID(index))
        radius = max(radius, (position.x ** 2 + position.y ** 2) ** 0.5)
    log("  footprint radius %.2f (diameter %.2f)" % (radius, 2.0 * radius))
    log("  nanite=%s simple collision shapes=%d" % (
        mesh.get_editor_property("nanite_settings").enabled,
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(mesh)))
    for index, static_material in enumerate(mesh.static_materials):
        interface = static_material.material_interface
        log("  slot %d %s -> %s" % (index, static_material.material_slot_name,
                                    interface.get_name() if interface else None))


def main():
    os.makedirs(TEXTURE_DIR, exist_ok=True)
    write_flat_png(os.path.join(TEXTURE_DIR, WHITE + ".png"), (255, 255, 255))
    white = import_texture(WHITE)
    master = build_master(white)
    instances = {}
    for part, (texture_name, tint, brightness, roughness, metallic) in PARTS.items():
        texture = import_texture(texture_name) if texture_name else None
        instances[part] = build_instance(master, part, texture, tint, brightness, roughness, metallic)
    mesh = import_mesh()
    if mesh is None:
        log("MESH IMPORT FAILED")
        return
    finish_mesh(mesh, instances)
    report(mesh)
    log("folder now: %s" % sorted(eal.list_assets(FOLDER, recursive=True, include_folder=False)))


try:
    main()
finally:
    with open(os.path.join(SAVED, "build.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(LINES))
