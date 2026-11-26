"""Shared helpers to turn a modelled world map piece (glb / fbx + loose PNG textures) into Unreal assets.

Used by build_pieces.py, which runs headless:
  UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=build_pieces.py -nullrhi

Every helper is safe to run again: existing assets are loaded and updated, not duplicated.
"""
import json
import math
import os
import struct
import zlib

import unreal

STORY_ROOT = "/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Story"
SHARED_FOLDER = STORY_ROOT + "/Shared"
MISSION_PARENT_CLASS_PATH = ("/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/"
                             "BP_MissionWorldObject.BP_MissionWorldObject_C")
PIECE_COMPONENT_PREFIX = "Piece"
FRONT_TO_CAMERA_YAW = 90.0
# The campaign level lights its pieces dim and from behind. Each story piece carries its own two spot lights. They are
# on lighting channel 1 only, and only the piece's own meshes are on that channel, so the map and the other pieces are
# not lit by them. A first try (one white light of 250 cd from the camera's side) was flat and washed the colours out.
# Now: a warm key from the front left that casts shadows, and a weak fill from the camera's side without highlights.
# Picked from four strengths in one picture (lineup_capture.py, WORLDPIECES_LIGHTTEST=1).
# (component name, offset from the pivot, candelas, colour, specular scale, casts shadows); the camera is at -X.
PIECE_LIGHTS = [
    ("Light", (-200.0, -260.0, 380.0), 400.0, (1.0, 0.90, 0.76), 0.15, True),
    ("FillLight", (-250.0, 0.0, 420.0), 60.0, (1.0, 1.0, 1.0), 0.0, False),
]
PIECE_LIGHT_AIM_HEIGHT = 40.0
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "WorldMapPieces")

LINES = []
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
material_library = unreal.MaterialEditingLibrary
asset_library = unreal.EditorAssetLibrary


def log(text):
    LINES.append(str(text))
    unreal.log("WORLDPIECE " + str(text))


def write_log(file_name):
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, file_name), "w", encoding="utf-8") as handle:
        handle.write("\n".join(LINES))


def save(asset):
    if not asset_library.save_loaded_asset(asset, False):
        log("SAVE FAILED " + asset.get_path_name())


# ---------------------------------------------------------------------------------------------------------------------
# Textures
# ---------------------------------------------------------------------------------------------------------------------

def write_flat_png(path, rgb, size=4):
    """Writes a tiny one-colour PNG; used for the default ORM texture of the master material."""
    row = b"\x00" + bytes(rgb) * size
    raw = row * size

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw))
                     + chunk(b"IEND", b""))


def import_texture(file_path, folder, name, kind, flip_green=False):
    """kind: 'color' (sRGB), 'normal', 'masks' (linear ORM)."""
    task = unreal.AssetImportTask()
    task.filename = file_path
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(folder + "/" + name)
    if texture is None:
        log("TEXTURE IMPORT FAILED " + file_path)
        return None
    if kind == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("flip_green_channel", flip_green)
    elif kind == "masks":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    else:
        texture.set_editor_property("srgb", True)
    save(texture)
    return texture


# ---------------------------------------------------------------------------------------------------------------------
# Master materials
# ---------------------------------------------------------------------------------------------------------------------

def _new_material(name):
    path = SHARED_FOLDER + "/" + name
    if asset_library.does_asset_exist(path):
        return unreal.load_asset(path), False
    return asset_tools.create_asset(name, SHARED_FOLDER, unreal.Material, unreal.MaterialFactoryNew()), True


def _scalar(material, name, default, y):
    node = material_library.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def _vector(material, name, default, y):
    node = material_library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(default[0], default[1], default[2], 1.0))
    return node


def _texture(material, name, sampler, default_texture, y):
    node = material_library.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -1000, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("sampler_type", sampler)
    node.set_editor_property("texture", default_texture)
    return node


def _tinted_base_color(material, default_color_texture):
    base = _texture(material, "BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, default_color_texture, 0)
    tint = _vector(material, "Tint", (1.0, 1.0, 1.0), 260)
    multiply = material_library.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 100)
    material_library.connect_material_expressions(base, "RGB", multiply, "A")
    material_library.connect_material_expressions(tint, "", multiply, "B")
    material_library.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR)


def ensure_master_materials(default_orm_png_dir):
    """Creates the three masters once: M_WorldPiece_PBR (D+N+ORM), _Albedo (D + scalars), _Flat (colour + scalars)."""
    default_color = unreal.load_asset("/Engine/EngineResources/DefaultTexture")
    default_normal = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
    masters = {}

    pbr, is_new = _new_material("M_WorldPiece_PBR")
    if is_new:
        orm_png = os.path.join(default_orm_png_dir, "T_WorldPiece_DefaultORM.png")
        write_flat_png(orm_png, (255, 200, 0))
        default_orm = import_texture(orm_png, SHARED_FOLDER, "T_WorldPiece_DefaultORM", "masks")
        _tinted_base_color(pbr, default_color)
        normal = _texture(pbr, "Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, default_normal, 420)
        material_library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
        orm = _texture(pbr, "ORM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, default_orm, 700)
        material_library.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        roughness_scale = _scalar(pbr, "RoughnessScale", 1.0, 980)
        roughness = material_library.create_material_expression(pbr, unreal.MaterialExpressionMultiply, -400, 800)
        material_library.connect_material_expressions(orm, "G", roughness, "A")
        material_library.connect_material_expressions(roughness_scale, "", roughness, "B")
        material_library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        material_library.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
        material_library.recompile_material(pbr)
        save(pbr)
    masters["pbr"] = pbr

    albedo, is_new = _new_material("M_WorldPiece_Albedo")
    if is_new:
        _tinted_base_color(albedo, default_color)
        material_library.connect_material_property(_scalar(albedo, "Roughness", 0.85, 500), "",
                                                   unreal.MaterialProperty.MP_ROUGHNESS)
        material_library.connect_material_property(_scalar(albedo, "Metallic", 0.0, 640), "",
                                                   unreal.MaterialProperty.MP_METALLIC)
        material_library.recompile_material(albedo)
        save(albedo)
    masters["albedo"] = albedo

    flat, is_new = _new_material("M_WorldPiece_Flat")
    if is_new:
        material_library.connect_material_property(_vector(flat, "Color", (0.5, 0.5, 0.5), 0), "",
                                                   unreal.MaterialProperty.MP_BASE_COLOR)
        material_library.connect_material_property(_scalar(flat, "Roughness", 0.6, 260), "",
                                                   unreal.MaterialProperty.MP_ROUGHNESS)
        material_library.connect_material_property(_scalar(flat, "Metallic", 0.0, 400), "",
                                                   unreal.MaterialProperty.MP_METALLIC)
        material_library.recompile_material(flat)
        save(flat)
    masters["flat"] = flat
    return masters


def make_instance(folder, name, parent, textures=None, vectors=None, scalars=None, two_sided=False):
    path = folder + "/" + name
    instance = unreal.load_asset(path) if asset_library.does_asset_exist(path) else asset_tools.create_asset(
        name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    material_library.set_material_instance_parent(instance, parent)
    for parameter, texture in (textures or {}).items():
        material_library.set_material_instance_texture_parameter_value(instance, parameter, texture)
    for parameter, color in (vectors or {}).items():
        material_library.set_material_instance_vector_parameter_value(
            instance, parameter, unreal.LinearColor(color[0], color[1], color[2], 1.0))
    for parameter, value in (scalars or {}).items():
        material_library.set_material_instance_scalar_parameter_value(instance, parameter, value)
    if two_sided:
        overrides = instance.get_editor_property("base_property_overrides")
        overrides.set_editor_property("override_two_sided", True)
        overrides.set_editor_property("two_sided", True)
        instance.set_editor_property("base_property_overrides", overrides)
    material_library.update_material_instance(instance)
    save(instance)
    return instance


# ---------------------------------------------------------------------------------------------------------------------
# glTF reading (material factors only; Unreal imports the geometry itself)
# ---------------------------------------------------------------------------------------------------------------------

def read_glb_materials(glb_path):
    """@return {material name: {'color': (r,g,b), 'roughness': f, 'metallic': f, 'two_sided': bool}}."""
    with open(glb_path, "rb") as handle:
        handle.read(12)
        chunk_length, _chunk_type = struct.unpack("<II", handle.read(8))
        document = json.loads(handle.read(chunk_length).decode("utf-8"))
    result = {}
    for material in document.get("materials", []):
        pbr = material.get("pbrMetallicRoughness", {})
        color = pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0])
        result[material.get("name", "")] = {
            "color": (color[0], color[1], color[2]),
            "roughness": pbr.get("roughnessFactor", 1.0),
            "metallic": pbr.get("metallicFactor", 1.0),
            "two_sided": bool(material.get("doubleSided", False)),
            "has_texture": "baseColorTexture" in pbr,
        }
    return result


# ---------------------------------------------------------------------------------------------------------------------
# Meshes
# ---------------------------------------------------------------------------------------------------------------------

def import_mesh(file_path, folder, mesh_name):
    """Imports the model, keeps only the static mesh and deletes the materials and textures the importer made."""
    before = set(asset_library.list_assets(folder, recursive=True, include_folder=False))
    task = unreal.AssetImportTask()
    task.filename = file_path
    task.destination_path = folder
    task.destination_name = mesh_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])
    after = set(asset_library.list_assets(folder, recursive=True, include_folder=False))
    created = sorted(after - before)
    log("import %s created %d assets" % (os.path.basename(file_path), len(created)))

    mesh = None
    for path in sorted(after):
        asset = unreal.load_asset(path.split(".")[0])
        if isinstance(asset, unreal.StaticMesh) and asset.get_name() == mesh_name:
            mesh = asset
    if mesh is None:
        for path in created:
            asset = unreal.load_asset(path.split(".")[0])
            if isinstance(asset, unreal.StaticMesh):
                log("mesh imported under another name: " + path)
                mesh = asset
    return mesh, created


def finish_mesh(mesh, slot_materials, created, keep_paths):
    """Assigns our materials by slot name, turns Nanite off, adds one box for cursor traces, drops importer leftovers."""
    for slot_index, static_material in enumerate(mesh.static_materials):
        slot_name = str(static_material.material_slot_name)
        material = slot_materials.get(slot_name)
        if material is None:
            matches = [value for key, value in slot_materials.items() if key in slot_name or slot_name in key]
            material = matches[0] if len(matches) == 1 else None
        if material is None:
            log("NO MATERIAL FOR SLOT %d '%s' on %s" % (slot_index, slot_name, mesh.get_name()))
            continue
        mesh.set_material(slot_index, material)

    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)

    mesh_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    mesh_subsystem.remove_collisions(mesh)
    mesh_subsystem.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    save(mesh)

    mesh_path = mesh.get_path_name()
    for path in created:
        if path == mesh_path or path.split(".")[0] in keep_paths:
            continue
        asset = unreal.load_asset(path.split(".")[0])
        if isinstance(asset, (unreal.MaterialInterface, unreal.Texture)):
            if not asset_library.delete_asset(path.split(".")[0]):
                log("could not delete importer leftover " + path)

    bounds = mesh.get_bounds()
    log("mesh %s tris=%d origin=(%.1f %.1f %.1f) extent=(%.1f %.1f %.1f)" % (
        mesh.get_name(), mesh.get_num_triangles(0), bounds.origin.x, bounds.origin.y, bounds.origin.z,
        bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z))
    for slot_index, static_material in enumerate(mesh.static_materials):
        interface = static_material.material_interface
        log("  slot %d %s -> %s" % (slot_index, static_material.material_slot_name,
                                    interface.get_name() if interface else None))


# ---------------------------------------------------------------------------------------------------------------------
# Blueprints
# ---------------------------------------------------------------------------------------------------------------------

def _subobject_object(subsystem, handle):
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)


def highest_point(mesh, tolerance=0.05):
    """@return (x, y, z): centre of the topmost vertices of the mesh. Shows how the importer mapped the axes."""
    description = mesh.get_static_mesh_description(0)
    positions = [description.get_vertex_position(unreal.VertexID(index))
                 for index in range(description.get_vertex_count())]
    top = max(position.z for position in positions)
    top_points = [position for position in positions if position.z > top - tolerance]
    count = float(len(top_points))
    return (sum(point.x for point in top_points) / count, sum(point.y for point in top_points) / count, top)


AXIS_MAPS = [((1, 0), (0, 1)), ((1, 0), (0, -1)), ((-1, 0), (0, 1)), ((-1, 0), (0, -1)),
             ((0, 1), (1, 0)), ((0, 1), (-1, 0)), ((0, -1), (1, 0)), ((0, -1), (-1, 0))]


def _apply_axis_map(axis_map, point):
    return (axis_map[0][0] * point[0] + axis_map[0][1] * point[1],
            axis_map[1][0] * point[0] + axis_map[1][1] * point[1])


def infer_axis_map(source_xy, unreal_xy):
    """Finds which signed axis swap the importer applied, from one known point in both frames."""
    best = min(AXIS_MAPS, key=lambda axis_map: (_apply_axis_map(axis_map, source_xy)[0] - unreal_xy[0]) ** 2
               + (_apply_axis_map(axis_map, source_xy)[1] - unreal_xy[1]) ** 2)
    log("axis map %s: source %s -> %s (mesh has %s)" % (
        best, source_xy, _apply_axis_map(best, source_xy), unreal_xy))
    return lambda point: _apply_axis_map(best, point)


def map_yaw(axis_function, source_yaw_degrees):
    """Source yaw (counter-clockwise from +X, Z up) to the Unreal yaw of the same direction."""
    radians = math.radians(source_yaw_degrees)
    direction = axis_function((math.cos(radians), math.sin(radians)))
    return math.degrees(math.atan2(direction[1], direction[0]))


def read_instanced_layout(blueprint_path):
    """@return [{name, mesh, transforms, materials}] for every instanced static mesh component of a Blueprint."""
    blueprint = unreal.load_asset(blueprint_path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    layout = []
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        template = _subobject_object(subsystem, handle)
        if not isinstance(template, unreal.InstancedStaticMeshComponent) or template.get_name() in seen:
            continue
        seen.add(template.get_name())
        base = unreal.Transform(template.relative_location, template.relative_rotation, template.relative_scale3d)
        transforms = []
        for index in range(template.get_instance_count()):
            instance = template.get_instance_transform(index, False)
            transforms.append(unreal.MathLibrary.compose_transforms(instance, base))
        materials = {index: template.get_material(index) for index in range(template.get_num_materials())}
        layout.append({"name": template.get_name().replace("_GEN_VARIABLE", ""), "mesh": template.static_mesh,
                       "transforms": transforms, "materials": materials})
    return layout


def build_mission_blueprint(folder, name, components, ui_texts=None, parent_class_path=None):
    """
    Creates (or updates) a child of BP_MissionWorldObject (or of parent_class_path) whose look is a set of mesh
    components.

    components: list of dicts: name, kind (mesh by default, scene, instanced), mesh (asset), location (x,y,z),
    rotation (pitch,yaw,roll), scale (float or (x,y,z)), optional parent (name of an earlier component), materials
    {slot index: material}, collision (default True), transforms (instanced only).
    """
    path = folder + "/" + name
    if asset_library.does_asset_exist(path):
        blueprint = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_object(
            None, parent_class_path or MISSION_PARENT_CLASS_PATH))
        blueprint = asset_tools.create_asset(name, folder, unreal.Blueprint, factory)

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    root_handle = handles[0]
    scene_root_handle = None
    for handle in handles:
        template = _subobject_object(subsystem, handle)
        if template is None:
            continue
        template_name = template.get_name()
        if template_name.startswith(PIECE_COMPONENT_PREFIX):
            subsystem.delete_subobject(root_handle, handle, blueprint)
        elif template_name.startswith("DefaultSceneRoot"):
            scene_root_handle = handle

    classes = {"mesh": unreal.StaticMeshComponent, "scene": unreal.SceneComponent,
               "instanced": unreal.InstancedStaticMeshComponent, "spot": unreal.SpotLightComponent}
    handle_by_name = {}
    # The models have their front on +Y; the map camera looks along +X, so the front must face -X.
    components = [{"name": "Root", "kind": "scene", "rotation": (0.0, FRONT_TO_CAMERA_YAW, 0.0)}] + [
        dict(spec, parent=spec.get("parent", "Root")) for spec in components]
    # No parent name: the lights hang on the scene root, so the yaw of PieceRoot does not turn them.
    for light_name, offset, candelas, colour, specular, shadows in PIECE_LIGHTS:
        to_aim = (-offset[0], -offset[1], PIECE_LIGHT_AIM_HEIGHT - offset[2])
        pitch = math.degrees(math.atan2(to_aim[2], math.hypot(to_aim[0], to_aim[1])))
        yaw = math.degrees(math.atan2(to_aim[1], to_aim[0]))
        components.append({"name": light_name, "kind": "spot", "parent": None, "location": offset,
                           "rotation": (pitch, yaw, 0.0), "candelas": candelas, "colour": colour,
                           "specular": specular, "shadows": shadows})
    for spec in components:
        kind = spec.get("kind", "mesh")
        parent_handle = handle_by_name.get(spec.get("parent"), scene_root_handle or root_handle)
        parameters = unreal.AddNewSubobjectParams(
            parent_handle=parent_handle, new_class=classes[kind], blueprint_context=blueprint)
        handle, fail_reason = subsystem.add_new_subobject(parameters)
        if not fail_reason.is_empty():
            log("ADD COMPONENT FAILED %s: %s" % (spec["name"], fail_reason))
            continue
        subsystem.rename_subobject(handle, unreal.Text(PIECE_COMPONENT_PREFIX + spec["name"]))
        handle_by_name[spec["name"]] = handle
        component = _subobject_object(subsystem, handle)
        location = spec.get("location", (0.0, 0.0, 0.0))
        rotation = spec.get("rotation", (0.0, 0.0, 0.0))
        scale = spec.get("scale", 1.0)
        if not isinstance(scale, (tuple, list)):
            scale = (scale, scale, scale)
        component.set_editor_property("relative_location", unreal.Vector(*location))
        component.set_editor_property("relative_rotation", unreal.Rotator(
            pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
        component.set_editor_property("relative_scale3d", unreal.Vector(*scale))
        if kind == "scene":
            continue
        if kind == "spot":
            component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            component.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
            component.set_editor_property("intensity", spec["candelas"])
            # The colours above are linear; the property is an sRGB byte colour.
            component.set_editor_property("light_color", unreal.Color(
                r=int(round(spec["colour"][0] ** (1.0 / 2.2) * 255)), g=int(round(spec["colour"][1] ** (1.0 / 2.2) * 255)),
                b=int(round(spec["colour"][2] ** (1.0 / 2.2) * 255)), a=255))
            component.set_editor_property("specular_scale", spec["specular"])
            component.set_editor_property("outer_cone_angle", 24.0)
            component.set_editor_property("inner_cone_angle", 14.0)
            component.set_editor_property("attenuation_radius", 900.0)
            component.set_editor_property("cast_shadows", spec["shadows"])
            component.set_editor_property("lighting_channels", unreal.LightingChannels(
                channel0=False, channel1=True, channel2=False))
            continue
        component.set_editor_property("lighting_channels", unreal.LightingChannels(
            channel0=True, channel1=True, channel2=False))
        component.set_static_mesh(spec["mesh"])
        component.set_collision_profile_name("BlockAllDynamic" if spec.get("collision", True) else "NoCollision")
        for slot_index, material in (spec.get("materials") or {}).items():
            if material is not None:
                component.set_material(slot_index, material)
        if kind == "instanced":
            component.add_instances(spec["transforms"], False, False)
            if component.get_instance_count() != len(spec["transforms"]):
                log("INSTANCE COUNT WRONG on %s: %d of %d" % (
                    spec["name"], component.get_instance_count(), len(spec["transforms"])))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if ui_texts:
        set_ui_texts(blueprint, ui_texts)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    log("blueprint %s with %d piece components" % (path, len(components)))
    return blueprint


def set_ui_texts(blueprint, ui_texts):
    """ui_texts: {'DescriptionText': str, 'PrimaryObjectiveText': str, 'SecondaryObjectiveText': str, ...}."""
    default_object = unreal.get_default_object(blueprint.generated_class())
    ui_data = default_object.get_editor_property("m_map_item_ui_data")
    for field, text in ui_texts.items():
        python_name = "".join(("_" + letter.lower()) if letter.isupper() else letter for letter in field).lstrip("_")
        ui_data.set_editor_property(python_name, unreal.Text(text))
    default_object.set_editor_property("m_map_item_ui_data", ui_data)
    log("  ui data now: " + default_object.get_editor_property("m_map_item_ui_data").export_text()[:400])
