"""Small world-map versions of the cinematic Ural Shield wall (Cinematics/Intro1976/UralShield), headless.

Writes to /Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/UralShield:
  M_WorldWall_UralShield        the kit's three textures (referenced, not copied; mips stream by screen size), an
                                emissive lift for the dim campaign light, usable on spline meshes
  SM_WorldWall_UralShield       two kit segments end to end, drop-in for SM_Global_SovietWall on the wall splines
                                (length on X, same length / height / pivot)
  SM_WorldWall_UralShield_Gate  gate frame with closed leaves and one segment each side, drop-in for
                                SM_Global_SovietWallWithGate2 (placed at scale 2 between the two splines)
The cinematic assets are only read. The level is not touched here (see swap_ural_wall.py).
"""
import os
import traceback

import unreal

KIT = "/Game/RTS_Survival/Cinematics/Intro1976/UralShield/"
FOLDER = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/UralShield"
MATERIAL_NAME = "M_WorldWall_UralShield"
WALL_NAME = "SM_WorldWall_UralShield"
GATE_NAME = "SM_WorldWall_UralShield_Gate"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldMapWall")

# Kit, in cm: a segment is 6000 long on Y, 1400 deep, 4710 high, base at Z 0.
KIT_SEGMENT_LENGTH = 6000.0
KIT_SEGMENT_HEIGHT = 4710.0
# Old spline piece SM_Global_SovietWall: 190 long on X, Z -49..37.
WALL_LENGTH = 190.0
WALL_HEIGHT = 86.0
WALL_BOTTOM_Z = -49.0
SEGMENTS_PER_PIECE = 2
# The kit wall is slim for its length; on the map it needs the old wall's bulk to read as a wall from above.
WALL_DEPTH_FACTOR = 1.5
# Gate: the gap between the two splines is 372 units and the gate actor has scale 2.
GATE_ACTOR_SCALE = 2.0
GATE_GAP = 372.0
# The gate is the landmark: the frame is larger than the wall (lengths before the actor scale).
GATE_FRAME_LENGTH = 100.0
GATE_FRAME_HEIGHT_SCALE = 0.011
GATE_BOTTOM_Z = -21.3                             # world bottom about -9, between the two splines' wall bases
GATE_FRAME_Z = -40.0                              # as the cinematic set places frame and leaves
GATE_LEAF_X = -500.0
GATE_LEAF_Y = 795.0
# Kit -X (the gate's leaf side) to local +Y, kit +Y (length) to local +X.
KIT_YAW = -90.0
EMISSIVE = 0.45

LINES = []
assets = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
materials = unreal.MaterialEditingLibrary
gs_assets = unreal.GeometryScript_AssetUtils
gs_edits = unreal.GeometryScript_MeshEdits
gs_query = unreal.GeometryScript_MeshQueries
gs_xform = unreal.GeometryScript_MeshTransforms


def log(text):
    LINES.append(str(text))
    unreal.log("URALWALL " + str(text))


def kit_part(name, location):
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = True
    gs_assets.copy_mesh_from_static_mesh_v2(unreal.load_asset(KIT + name), mesh, options,
                                            unreal.GeometryScriptMeshReadLOD(), False)
    gs_xform.translate_mesh(mesh, unreal.Vector(*location))
    return mesh


def merge(parts):
    merged = unreal.DynamicMesh()
    for part in parts:
        gs_edits.append_mesh(merged, part, unreal.Transform(), False, unreal.GeometryScriptAppendMeshOptions())
    return merged


def fit(mesh, scale, bottom_z):
    """Kit cm to map units: turn the length onto X, scale, centre the footprint, set the base height."""
    gs_xform.rotate_mesh(mesh, unreal.Rotator(roll=0.0, pitch=0.0, yaw=KIT_YAW), unreal.Vector(0, 0, 0))
    # After the turn the kit's length lies on X and its depth on Y.
    gs_xform.scale_mesh(mesh, unreal.Vector(scale[0], scale[1], scale[2]), unreal.Vector(0, 0, 0), True)
    box = gs_query.get_mesh_bounding_box(mesh)
    gs_xform.translate_mesh(mesh, unreal.Vector(-(box.min.x + box.max.x) * 0.5, 0.0, bottom_z - box.min.z))


def build_material():
    path = FOLDER + "/" + MATERIAL_NAME
    if assets.does_asset_exist(path):
        log("material kept " + path)
        return unreal.load_asset(path)
    material = asset_tools.create_asset(MATERIAL_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    def node(node_class, x, y, **properties):
        expression = materials.create_material_expression(material, node_class, x, y)
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def texture(parameter, asset, sampler, y):
        return node(unreal.MaterialExpressionTextureSampleParameter2D, -900, y, parameter_name=parameter,
                    texture=unreal.load_asset(KIT + "Textures/" + asset), sampler_type=sampler)

    base = texture("BaseColor", "T_UralShield_BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -400)
    normal = texture("Normal", "T_UralShield_Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -100)
    masks = texture("ORM", "T_UralShield_ORM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 200)
    tint = node(unreal.MaterialExpressionVectorParameter, -900, -650, parameter_name="Tint",
                default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    strength = node(unreal.MaterialExpressionScalarParameter, -900, -800, parameter_name="EmissiveStrength",
                    default_value=EMISSIVE)
    colour = node(unreal.MaterialExpressionMultiply, -500, -450)
    glow = node(unreal.MaterialExpressionMultiply, -250, -650)
    for source, output, target, pin in ((base, "RGB", colour, "A"), (tint, "", colour, "B"), (colour, "", glow, "A"),
                                        (strength, "", glow, "B")):
        if not materials.connect_material_expressions(source, output, target, pin):
            log("CONNECT FAILED %s -> %s.%s" % (source.get_name(), target.get_name(), pin))
    materials.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    materials.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    materials.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    materials.connect_material_property(masks, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    materials.connect_material_property(masks, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    materials.connect_material_property(masks, "B", unreal.MaterialProperty.MP_METALLIC)
    material.set_editor_property("used_with_spline_meshes", True)
    material.set_editor_property("used_with_static_lighting", False)
    materials.recompile_material(material)
    log("material saved %s" % assets.save_loaded_asset(material, False))
    return material


def write_static_mesh(dynamic_mesh, name, material):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        static_mesh = unreal.load_asset(path)
        options = unreal.GeometryScriptCopyMeshToAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.replace_materials = True
        options.new_materials = [material]
        options.emit_transaction = False
        gs_assets.copy_mesh_to_static_mesh(dynamic_mesh, static_mesh, options, unreal.GeometryScriptMeshWriteLOD(),
                                           False)
    else:
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.enable_nanite = False
        options.enable_collision = False
        unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dynamic_mesh, path, options)
        static_mesh = unreal.load_asset(path)
    static_mesh.set_editor_property("static_materials", [
        unreal.StaticMaterial(material_interface=material, material_slot_name="UralShield")])
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    # One box, like a blocker: the old pieces had query collision and something may trace against the wall.
    subsystem.remove_collisions(static_mesh)
    subsystem.add_simple_collisions(static_mesh, unreal.ScriptCollisionShapeType.BOX)
    build = subsystem.get_lod_build_settings(static_mesh, 0)
    build.generate_lightmap_u_vs = False
    build.recompute_normals = False
    build.recompute_tangents = True
    subsystem.set_lod_build_settings(static_mesh, 0, build)
    saved = assets.save_loaded_asset(static_mesh, False)
    box = static_mesh.get_bounding_box()
    log("MESH %s saved=%s tris=%d box (%.1f %.1f %.1f)-(%.1f %.1f %.1f)" % (
        path, saved, static_mesh.get_num_triangles(0), box.min.x, box.min.y, box.min.z, box.max.x, box.max.y,
        box.max.z))


def build():
    if not assets.does_directory_exist(FOLDER):
        assets.make_directory(FOLDER)
    material = build_material()

    plan_scale = WALL_LENGTH / (SEGMENTS_PER_PIECE * KIT_SEGMENT_LENGTH)
    height_scale = WALL_HEIGHT / KIT_SEGMENT_HEIGHT
    wall = merge([kit_part("SM_UralShield_Segment", (0.0, (index - 0.5) * KIT_SEGMENT_LENGTH, 0.0))
                  for index in range(SEGMENTS_PER_PIECE)])
    depth_scale = plan_scale * WALL_DEPTH_FACTOR
    fit(wall, (plan_scale, depth_scale, height_scale), WALL_BOTTOM_Z)
    write_static_mesh(wall, WALL_NAME, material)

    frame_scale = GATE_FRAME_LENGTH / KIT_SEGMENT_LENGTH
    frame = merge([
        kit_part("SM_UralShield_GateFrame", (0.0, 0.0, GATE_FRAME_Z)),
        kit_part("SM_UralShield_GateLeaf_L", (GATE_LEAF_X, -GATE_LEAF_Y, GATE_FRAME_Z)),
        kit_part("SM_UralShield_GateLeaf_R", (GATE_LEAF_X, GATE_LEAF_Y, GATE_FRAME_Z)),
    ])
    fit(frame, (frame_scale, frame_scale, GATE_FRAME_HEIGHT_SCALE), GATE_BOTTOM_Z)
    # One wall segment each side closes the gap to the splines, with the wall's cross-section under the actor scale.
    flank_length = (GATE_GAP / GATE_ACTOR_SCALE - GATE_FRAME_LENGTH) * 0.5
    parts = [frame]
    for side in (-1.0, 1.0):
        flank = kit_part("SM_UralShield_Segment", (0.0, 0.0, 0.0))
        fit(flank, (flank_length / KIT_SEGMENT_LENGTH, depth_scale / GATE_ACTOR_SCALE,
                    height_scale / GATE_ACTOR_SCALE), GATE_BOTTOM_Z)
        gs_xform.translate_mesh(flank, unreal.Vector(side * (GATE_FRAME_LENGTH + flank_length) * 0.5, 0.0, 0.0))
        parts.append(flank)
    gate = merge(parts)
    write_static_mesh(gate, GATE_NAME, material)


os.makedirs(OUT, exist_ok=True)
try:
    build()
except Exception:
    log(traceback.format_exc())
with open(os.path.join(OUT, "build.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
