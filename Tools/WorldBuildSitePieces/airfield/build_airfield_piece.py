"""Builds the airfield build site piece: one static mesh merged from game models, plus its materials.

Run headless (no GPU):  sh run_py.sh build_airfield_piece.py
Log: Saved/WorldBuildSitePieces/airfield_build.txt

Output (fixed contract, the build site C++ places it at scale 1):
  /Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/SM_BuildSitePiece_Airfield
Pivot at the centre of the base, base at Z = 0, Z up, inside a 40 unit circle, no collision.

Look: a cream field runway on the diagonal with dark markings, the game's Ju 87 parked on it at about 1.6 x true
scale (wingspan 28 units), the German desert open tent as field hangar, an orange-white windsock and fuel drums.
Source assets are only read; textures are duplicated into the folder.
"""
import math
import os
import traceback

import unreal

FOLDER = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield"
MESH_NAME = "SM_BuildSitePiece_Airfield"
MASTER_NAME = "M_BuildSitePiece_Airfield"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")

STUKA = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"
STUKA_TEXTURES = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Materials/Plane_stuka_01_"
TENT = "/Game/MilitaryBaseCamp/Meshes/SM_OpenTent"
TENT_TEXTURE = "/Game/MilitaryBaseCamp/Textures/T_Opentent_GerDesert2"
BARREL = "/Game/MilitaryAirport/Meshes/Props/SM_Barrel_1_Gasoline"

# Layout. The map camera looks along +X, 70 degrees down. The runway runs on a diagonal; "along" is its direction,
# "side" is 90 degrees to the left of it.
RUNWAY_YAW = 30.0
RUNWAY_LENGTH = 37.4
RUNWAY_WIDTH = 10.0
RUNWAY_HEIGHT = 0.5
MARK_HEIGHT = 0.08
# Ju 87: nose on +X, tail fin at -X, bounding box centre at x -157 (true size: length 998, span 1243, height 395 cm).
STUKA_SCALE = 0.0225
STUKA_CENTRE_X = -157.0
STUKA_ALONG = 0.5
TENT_SCALE = 0.0068
TENT_HEIGHT_BOOST = 1.6
TENT_AT = (10.6, -9.6)       # (along, side)
WINDSOCK_AT = (-12.5, 9.5)
DRUMS_AT = (-12.0, -8.6)
BARREL_SCALE = 0.036
STEPS_PER_CIRCLE = 12

# Linear vertex colours of the painted parts (slot Paint: white texture x tint x vertex colour).
CREAM = (0.78, 0.72, 0.56)
MARK = (0.10, 0.11, 0.12)
POLE = (0.20, 0.20, 0.21)
SOCK_ORANGE = (1.0, 0.22, 0.02)
SOCK_WHITE = (0.92, 0.92, 0.88)
DRUM = (0.38, 0.06, 0.03)

SLOT_STUKA, SLOT_TENT, SLOT_PAINT = 0, 1, 2
# Picked from 0 / 0.3 / 0.8 / 1.6 in the campaign level light (airfield_campaign_close_e*.png): the level lights the
# map warm and dim; 0.8 keeps the runway cream and the aircraft readable without flattening the shading.
EMISSIVE = 0.8
SLOT_NAMES = ["Stuka", "Tent", "Paint"]

LINES = []
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary
gs_assets = unreal.GeometryScript_AssetUtils
gs_edits = unreal.GeometryScript_MeshEdits
gs_prims = unreal.GeometryScript_Primitives
gs_mat = unreal.GeometryScript_Materials
gs_color = unreal.GeometryScript_VertexColors
gs_query = unreal.GeometryScript_MeshQueries
gs_xform = unreal.GeometryScript_MeshTransforms


def log(text):
    LINES.append(str(text))
    unreal.log("AIRFIELD " + str(text))


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        log("SAVE FAILED " + asset.get_path_name())


def runway_point(along, side):
    """(along, side) on the runway frame -> piece (x, y)."""
    yaw = math.radians(RUNWAY_YAW)
    return (along * math.cos(yaw) - side * math.sin(yaw), along * math.sin(yaw) + side * math.cos(yaw))


def transform(location=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, roll=0.0, scale=(1.0, 1.0, 1.0)):
    return unreal.Transform(unreal.Vector(*location), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                            unreal.Vector(*scale))


# ---------------------------------------------------------------------------------------------------------------------
# Textures and materials
# ---------------------------------------------------------------------------------------------------------------------

def duplicate_texture(source_path, name, max_size=512):
    path = FOLDER + "/" + name
    if not assets.does_asset_exist(path):
        if assets.duplicate_asset(source_path, path) is None:
            log("DUPLICATE FAILED " + source_path)
            return None
    texture = unreal.load_asset(path)
    texture.set_editor_property("max_texture_size", max_size)
    save(texture)
    return texture


def _param(material, cls, name, x, y):
    node = materials.create_material_expression(material, cls, x, y)
    node.set_editor_property("parameter_name", name)
    return node


def ensure_master():
    """BaseColor = texture x Tint x vertex colour; Emissive = BaseColor x EmissiveStrength (map light is dim)."""
    path = FOLDER + "/" + MASTER_NAME
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = asset_tools.create_asset(MASTER_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    texture = _param(material, unreal.MaterialExpressionTextureSampleParameter2D, "BaseColor", -1100, 0)
    texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    texture.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tint = _param(material, unreal.MaterialExpressionVectorParameter, "Tint", -1100, 260)
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    vertex_color = materials.create_material_expression(material, unreal.MaterialExpressionVertexColor, -1100, 420)
    tinted = materials.create_material_expression(material, unreal.MaterialExpressionMultiply, -800, 100)
    materials.connect_material_expressions(texture, "RGB", tinted, "A")
    materials.connect_material_expressions(tint, "", tinted, "B")
    base = materials.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 200)
    materials.connect_material_expressions(tinted, "", base, "A")
    materials.connect_material_expressions(vertex_color, "", base, "B")
    materials.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    emissive_strength = _param(material, unreal.MaterialExpressionScalarParameter, "EmissiveStrength", -800, 420)
    emissive_strength.set_editor_property("default_value", 0.15)
    emissive = materials.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 380)
    materials.connect_material_expressions(base, "", emissive, "A")
    materials.connect_material_expressions(emissive_strength, "", emissive, "B")
    materials.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    normal = _param(material, unreal.MaterialExpressionTextureSampleParameter2D, "Normal", -1100, 600)
    normal.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    materials.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    roughness = _param(material, unreal.MaterialExpressionScalarParameter, "Roughness", -800, 800)
    roughness.set_editor_property("default_value", 0.7)
    materials.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    metallic = _param(material, unreal.MaterialExpressionScalarParameter, "Metallic", -800, 900)
    metallic.set_editor_property("default_value", 0.0)
    materials.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    material.set_editor_property("used_with_static_lighting", False)
    materials.recompile_material(material)
    save(material)
    return material


def make_instance(name, parent, textures=None, tint=(1.0, 1.0, 1.0), scalars=None, two_sided=False):
    path = FOLDER + "/" + name
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else asset_tools.create_asset(
        name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    materials.set_material_instance_parent(instance, parent)
    for parameter, texture in (textures or {}).items():
        materials.set_material_instance_texture_parameter_value(instance, parameter, texture)
    materials.set_material_instance_vector_parameter_value(instance, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    for parameter, value in (scalars or {}).items():
        materials.set_material_instance_scalar_parameter_value(instance, parameter, value)
    overrides = instance.get_editor_property("base_property_overrides")
    overrides.set_editor_property("override_two_sided", two_sided)
    overrides.set_editor_property("two_sided", two_sided)
    instance.set_editor_property("base_property_overrides", overrides)
    materials.update_material_instance(instance)
    save(instance)
    return instance


# ---------------------------------------------------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------------------------------------------------

def new_mesh():
    return unreal.DynamicMesh()


def paint(mesh, slot, color):
    gs_mat.enable_material_i_ds(mesh)
    selection = unreal.GeometryScript_MeshSelection.create_select_all_mesh_selection(
        mesh, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
    if isinstance(selection, tuple):
        selection = selection[-1]
    gs_mat.set_material_id_for_mesh_selection(mesh, selection, slot, False)
    gs_color.set_mesh_constant_vertex_color(mesh, unreal.LinearColor(color[0], color[1], color[2], 1.0),
                                            unreal.GeometryScriptColorFlags(), False)


def lowest_lod(static_mesh, wanted):
    return min(wanted, static_mesh.get_num_lods() - 1)


def copy_static(path, lod):
    static_mesh = unreal.load_asset(path)
    mesh = new_mesh()
    read_lod = unreal.GeometryScriptMeshReadLOD()
    read_lod.lod_index = lowest_lod(static_mesh, lod)
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = True
    gs_assets.copy_mesh_from_static_mesh_v2(static_mesh, mesh, options, read_lod, False)
    log("copied %s lod %d: %d tris" % (path.split("/")[-1], read_lod.lod_index, mesh.get_triangle_count()))
    return mesh


def stuka_part():
    mesh = new_mesh()
    gs_assets.copy_mesh_from_skeletal_mesh(unreal.load_asset(STUKA), mesh,
                                           unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                           unreal.GeometryScriptMeshReadLOD())
    log("copied Ju 87: %d tris" % mesh.get_triangle_count())
    paint(mesh, SLOT_STUKA, (1.0, 1.0, 1.0))
    centre = runway_point(STUKA_ALONG, 0.0)
    # Move the box centre (x -157) to the seat, scale, then turn onto the runway.
    gs_xform.translate_mesh(mesh, unreal.Vector(-STUKA_CENTRE_X, 0.0, 0.0))
    gs_xform.scale_mesh(mesh, unreal.Vector(STUKA_SCALE, STUKA_SCALE, STUKA_SCALE), unreal.Vector(0, 0, 0), True)
    gs_xform.transform_mesh(mesh, transform((centre[0], centre[1], RUNWAY_HEIGHT), yaw=RUNWAY_YAW))
    return mesh


def tent_part():
    mesh = copy_static(TENT, 0)
    paint(mesh, SLOT_TENT, (1.0, 1.0, 1.0))
    # Tent pivot is near its centre (origin -5.6, -2.0, base 177.5 - 189.8 = -12.3); put its base on Z 0.
    gs_xform.translate_mesh(mesh, unreal.Vector(5.6, 2.0, 12.3))
    gs_xform.scale_mesh(mesh, unreal.Vector(TENT_SCALE, TENT_SCALE, TENT_SCALE * TENT_HEIGHT_BOOST),
                        unreal.Vector(0, 0, 0), True)
    at = runway_point(*TENT_AT)
    gs_xform.transform_mesh(mesh, transform((at[0], at[1], 0.0), yaw=RUNWAY_YAW + 90.0))
    return mesh


def drums_part():
    barrel = copy_static(BARREL, 2)
    unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(
        barrel, 160, unreal.GeometryScriptSimplifyMeshOptions())
    log("barrel simplified to %d tris" % barrel.get_triangle_count())
    paint(barrel, SLOT_PAINT, DRUM)
    gs_xform.scale_mesh(barrel, unreal.Vector(BARREL_SCALE, BARREL_SCALE, BARREL_SCALE), unreal.Vector(0, 0, 0), True)
    mesh = new_mesh()
    at = runway_point(*DRUMS_AT)
    for dx, dy in ((0.0, 0.0), (2.2, 0.3), (1.0, 1.9), (-1.1, 1.8)):
        gs_edits.append_mesh(mesh, barrel, transform((at[0] + dx, at[1] + dy, 0.0)), False,
                             unreal.GeometryScriptAppendMeshOptions())
    return mesh


def box(mesh, centre, size, yaw, color, z0):
    part = new_mesh()
    gs_prims.append_box(part, unreal.GeometryScriptPrimitiveOptions(), transform((centre[0], centre[1], z0), yaw=yaw),
                        size[0], size[1], size[2], 0, 0, 0, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    paint(part, SLOT_PAINT, color)
    gs_edits.append_mesh(mesh, part, transform(), False, unreal.GeometryScriptAppendMeshOptions())


def runway_part():
    mesh = new_mesh()
    box(mesh, (0.0, 0.0), (RUNWAY_LENGTH, RUNWAY_WIDTH, RUNWAY_HEIGHT), RUNWAY_YAW, CREAM, 0.0)
    top = RUNWAY_HEIGHT
    half = RUNWAY_LENGTH * 0.5
    # Threshold bars at both ends, centre dashes in between (kept clear of the aircraft's shadow side is not needed).
    for end in (-1.0, 1.0):
        for side in (-3.4, -1.7, 1.7, 3.4):
            along = end * (half - 1.9)
            box(mesh, runway_point(along, side), (2.6, 0.8, MARK_HEIGHT), RUNWAY_YAW, MARK, top)
    for along in (-12.0, -8.0, 8.0, 12.0):
        box(mesh, runway_point(along, 0.0), (2.2, 0.5, MARK_HEIGHT), RUNWAY_YAW, MARK, top)
    return mesh


def windsock_part():
    mesh = new_mesh()
    at = runway_point(*WINDSOCK_AT)
    pole_height = 10.0
    part = new_mesh()
    gs_prims.append_cylinder(part, unreal.GeometryScriptPrimitiveOptions(), transform((at[0], at[1], 0.0)), 0.3,
                             pole_height, 6, 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    paint(part, SLOT_PAINT, POLE)
    gs_edits.append_mesh(mesh, part, transform(), False, unreal.GeometryScriptAppendMeshOptions())
    # Sock: three cone bands, orange-white-orange, blowing along the runway (+along), drooping a little.
    radii = [1.25, 1.0, 0.78, 0.58]
    band = 1.7
    sock_yaw = RUNWAY_YAW + 20.0
    for index in range(3):
        part = new_mesh()
        # Cone along +Z from its base; turn it to point along the wind (pitch -90 -> +X), then yaw.
        gs_prims.append_cone(part, unreal.GeometryScriptPrimitiveOptions(), transform(), radii[index], radii[index + 1],
                             band, STEPS_PER_CIRCLE, 0, index == 0, unreal.GeometryScriptPrimitiveOriginMode.BASE)
        paint(part, SLOT_PAINT, SOCK_ORANGE if index != 1 else SOCK_WHITE)
        gs_xform.translate_mesh(part, unreal.Vector(0.0, 0.0, 0.3 + index * band))
        gs_xform.transform_mesh(part, transform((at[0], at[1], pole_height - 1.1), pitch=-96.0))
        gs_xform.rotate_mesh(part, unreal.Rotator(roll=0.0, pitch=0.0, yaw=sock_yaw), unreal.Vector(at[0], at[1], 0.0))
        gs_edits.append_mesh(mesh, part, transform(), False, unreal.GeometryScriptAppendMeshOptions())
    return mesh


# ---------------------------------------------------------------------------------------------------------------------
# Asset
# ---------------------------------------------------------------------------------------------------------------------

def write_static_mesh(dynamic_mesh, slot_materials):
    path = FOLDER + "/" + MESH_NAME
    if assets.does_asset_exist(path):
        static_mesh = unreal.load_asset(path)
        options = unreal.GeometryScriptCopyMeshToAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.replace_materials = True
        options.new_materials = slot_materials
        options.new_material_slot_names = SLOT_NAMES
        options.emit_transaction = False
        options.defer_mesh_post_edit_change = False
        gs_assets.copy_mesh_to_static_mesh(dynamic_mesh, static_mesh, options, unreal.GeometryScriptMeshWriteLOD(), False)
        log("updated existing " + path)
    else:
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.enable_nanite = False
        options.enable_collision = False
        options.collision_mode = unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX
        options.use_original_vertex_order = False
        static_mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            dynamic_mesh, path, options)
        log("created %s: %s" % (path, outcome))
        static_mesh = unreal.load_asset(path)
    static_list = []
    for index, material in enumerate(slot_materials):
        static_list.append(unreal.StaticMaterial(material_interface=material, material_slot_name=SLOT_NAMES[index]))
    static_mesh.set_editor_property("static_materials", static_list)

    nanite = static_mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    static_mesh.set_editor_property("nanite_settings", nanite)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(static_mesh)
    body = static_mesh.get_editor_property("body_setup")
    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    build = subsystem.get_lod_build_settings(static_mesh, 0)
    build.generate_lightmap_u_vs = False
    build.recompute_normals = False
    build.recompute_tangents = True
    subsystem.set_lod_build_settings(static_mesh, 0, build)
    static_mesh.set_editor_property("light_map_resolution", 4)
    save(static_mesh)
    return static_mesh


def build():
    if not assets.does_directory_exist(FOLDER):
        assets.make_directory(FOLDER)
    master = ensure_master()
    camo = duplicate_texture(STUKA_TEXTURES + "Camo", "T_BSP_Airfield_StukaCamo")
    stuka_normal = duplicate_texture(STUKA_TEXTURES + "Normal", "T_BSP_Airfield_StukaNormal")
    tent_texture = duplicate_texture(TENT_TEXTURE, "T_BSP_Airfield_Tent")
    mi_stuka = make_instance("MI_BSP_Airfield_Stuka", master, {"BaseColor": camo, "Normal": stuka_normal},
                             tint=(1.25, 1.25, 1.25), scalars={"Roughness": 0.55, "EmissiveStrength": EMISSIVE})
    mi_tent = make_instance("MI_BSP_Airfield_Tent", master, {"BaseColor": tent_texture}, tint=(1.2, 1.2, 1.2),
                            scalars={"Roughness": 0.85, "EmissiveStrength": EMISSIVE}, two_sided=True)
    mi_paint = make_instance("MI_BSP_Airfield_Paint", master, {}, scalars={"Roughness": 0.8, "EmissiveStrength": EMISSIVE})

    merged = new_mesh()
    for part in (runway_part(), stuka_part(), tent_part(), drums_part(), windsock_part()):
        gs_edits.append_mesh(merged, part, transform(), False, unreal.GeometryScriptAppendMeshOptions())
    log("merged: %d tris, material ids max %d" % (merged.get_triangle_count(), gs_mat.get_max_material_id(merged)[0]))
    box_result = gs_query.get_mesh_bounding_box(merged)
    log("bounds min (%.2f %.2f %.2f) max (%.2f %.2f %.2f)" % (box_result.min.x, box_result.min.y, box_result.min.z,
                                                             box_result.max.x, box_result.max.y, box_result.max.z))
    if abs(box_result.min.z) > 0.001:
        gs_xform.translate_mesh(merged, unreal.Vector(0.0, 0.0, -box_result.min.z))
        log("lifted by %.3f so the base is at Z 0" % -box_result.min.z)
    static_mesh = write_static_mesh(merged, [mi_stuka, mi_tent, mi_paint])
    bounds = static_mesh.get_bounds()
    log("asset tris=%d slots=%d extent=(%.2f %.2f %.2f) origin=(%.2f %.2f %.2f)" % (
        static_mesh.get_num_triangles(0), len(static_mesh.static_materials), bounds.box_extent.x, bounds.box_extent.y,
        bounds.box_extent.z, bounds.origin.x, bounds.origin.y, bounds.origin.z))


os.makedirs(OUT, exist_ok=True)
try:
    build()
except Exception:
    log(traceback.format_exc())
with open(os.path.join(OUT, "airfield_build.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
