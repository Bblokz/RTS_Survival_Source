"""Builds airfield candidate C ("take-off"): the German airbase building with its roof folded open, a Ju 87 VTO parked
on the landing pad inside, and a second, larger Ju 87 VTO frozen in take-off above it. One static mesh, no skeleton.

Run headless (no GPU):  sh run_py.sh build_piece.py
  AIRFIELDC_PREVIEW=1  only writes OBJ dumps (Saved/WorldBuildSitePieces/airfield_C/obj/piece_*.obj) for blender_preview.py
Log: Saved/WorldBuildSitePieces/airfield_C/build.txt

Output: /Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/C/SM_BuildSitePiece_Airfield_C
Pivot at the base centre, base at Z = 0, Z up, inside a 40 unit circle, no collision, Nanite off, placed at scale 1.
Sources (read only): SM_Ger_AircraftBuilding_BaseMesh, SK_Ger_AircraftBuilding_Roof posed with
Anim_GerAirbase_OpenStationary (vertex by vertex, largest bone weight), SK_Stuka_VTO (x2). The Stuka camo and normal
textures are duplicated into the folder.
"""
import math
import os
import traceback

import unreal

FOLDER = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/C"
MESH_NAME = "SM_BuildSitePiece_Airfield_C"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_C")
PREVIEW = os.environ.get("AIRFIELDC_PREVIEW", "") == "1"
DEBUG_SOURCE_SLOTS = os.environ.get("AIRFIELDC_SOURCE_SLOTS", "") == "1"

AB = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/BuildingMeshes/"
BASE = AB + "SM_Ger_AircraftBuilding_BaseMesh"
ROOF = AB + "SK_Ger_AircraftBuilding_Roof"
ROOF_OPEN = AB + "Anims/Anim_GerAirbase_OpenStationary"
STUKA = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"
STUKA_TEXTURES = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Materials/Plane_stuka_01_"

# Piece slots.
SLOT_WALLS, SLOT_ROOF, SLOT_STUKA, SLOT_ACCENT, SLOT_DARK = range(5)
SLOT_NAMES = ["Walls", "Roof", "Stuka", "Accent", "Dark"]
# Source slot -> piece slot. Base: 0 camo concrete, 1 module atlas, 2 Panther sheets, 3 decals, 4 props atlas.
BASE_SLOTS = {0: SLOT_WALLS, 1: SLOT_WALLS, 2: SLOT_ROOF, 3: SLOT_ACCENT, 4: SLOT_DARK}
# Roof: 0 module atlas, 1 Panther sheets, 2 props atlas, 3 decals.
ROOF_SLOTS = {0: SLOT_ROOF, 1: SLOT_ROOF, 2: SLOT_DARK, 3: SLOT_ACCENT}

# ---- Layout (source units are cm; the piece is in map units) ----
BUILDING_SCALE = 0.009
BUILDING_YAW = 0.0            # turn of the building about Z after centring
BUILDING_CENTRE = (-298.0, -3.7)   # source x/y centre of the base mesh box
BUILDING_SHIFT = (3.5, 3.0)   # piece x/y of the building centre
BUILDING_TRIS = 5200          # simplify targets
ROOF_TRIS = 2200
# Ju 87 VTO: nose +X, box centre x -157, true span 1243 cm.
STUKA_CENTRE = (-157.0, 0.0, 197.0)
PARKED_SCALE = 0.009          # same scale as the building, parked on Landing_0 (source origin)
PARKED_YAW = 0.0
PARKED_TRIS = 1400
FLYER_SCALE = 0.018
FLYER_AT = (-7.0, -5.0, 21.5) # piece position of the flyer's box centre
FLYER_YAW = -50.0             # heading (0 = +X, away from the camera)
FLYER_PITCH = 15.0            # nose up
FLYER_ROLL = -25.0            # bank
FLYER_TRIS = 4200
MAX_RADIUS = 19.6            # footprint circle 40 units, a little margin
# Accent zones on the Ju 87 (source cm; nose +X at 342, tail fin top z 394, span +-622).
STUKA_TIP_Y = 500.0
STUKA_COWL_X = 215.0
STUKA_FIN_X = -540.0
STUKA_FIN_Z = 300.0
STUKA_BAND_X = (-470.0, -420.0)
STAND = True
STAND_RADIUS = 0.3
STAND_SIDES = 6

LINES = []
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary
gs_assets = unreal.GeometryScript_AssetUtils
gs_edits = unreal.GeometryScript_MeshEdits
gs_prims = unreal.GeometryScript_Primitives
gs_mat = unreal.GeometryScript_Materials
gs_query = unreal.GeometryScript_MeshQueries
gs_xform = unreal.GeometryScript_MeshTransforms
gs_list = unreal.GeometryScript_List
gs_bw = unreal.GeometryScript_BoneWeights


def log(text):
    LINES.append(str(text))
    unreal.log("AIRFIELDC " + str(text))


def transform(location=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, roll=0.0, scale=(1.0, 1.0, 1.0)):
    return unreal.Transform(unreal.Vector(*location), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                            unreal.Vector(*scale))


def unpack(result):
    """GeometryScript python returns (mesh, value, flags...): take the first value that is not the mesh or a bool."""
    if not isinstance(result, tuple):
        return result
    for item in result:
        if not isinstance(item, (unreal.DynamicMesh, bool)):
            return item
    return result[-1]


def append(target, part):
    gs_edits.append_mesh(target, part, transform(), False, unreal.GeometryScriptAppendMeshOptions())


def remap_material_ids(mesh, mapping, offset=0):
    """Source slot ids -> piece slot ids (or source id + offset in the source-slot debug mode)."""
    ids = gs_list.convert_index_list_to_array(unpack(gs_mat.get_all_triangle_material_i_ds(mesh)))
    triangle_ids = gs_list.convert_index_list_to_array(unpack(gs_query.get_all_triangle_i_ds(mesh)))
    groups = {}
    for triangle, material in zip(triangle_ids, ids):
        new = material + offset if DEBUG_SOURCE_SLOTS else mapping.get(material, SLOT_WALLS)
        groups.setdefault(new, []).append(triangle)
    for new, triangles in groups.items():
        set_ids(mesh, triangles, new)


def set_ids(mesh, triangles, material):
    """Material id on a list of triangle ids (an index list of type ANY gives an empty mesh selection, so no selections)."""
    index_list = unpack(gs_list.convert_array_to_index_list(list(triangles), unreal.GeometryScriptIndexType.TRIANGLE))
    gs_mat.set_material_id_on_triangles(mesh, index_list, material, False)


def material_histogram(mesh):
    ids = gs_list.convert_index_list_to_array(unpack(gs_mat.get_all_triangle_material_i_ds(mesh)))
    histogram = {}
    for value in ids:
        histogram[value] = histogram.get(value, 0) + 1
    return histogram


def set_all_material(mesh, slot):
    gs_mat.enable_material_i_ds(mesh)
    selection = unpack(unreal.GeometryScript_MeshSelection.create_select_all_mesh_selection(
        mesh, unreal.GeometryScriptMeshSelectionType.TRIANGLES))
    gs_mat.set_material_id_for_mesh_selection(mesh, selection, slot, False)


def simplify(mesh, count, name):
    before = mesh.get_triangle_count()
    if before > count:
        options = unreal.GeometryScriptSimplifyMeshOptions()
        try:
            options.set_editor_property("allow_seam_collapse", True)
        except Exception:
            pass
        unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(mesh, count, options)
    log("%s: %d -> %d tris" % (name, before, mesh.get_triangle_count()))


# ---------------------------------------------------------------------------------------------------------------------
# Parts
# ---------------------------------------------------------------------------------------------------------------------

def base_part():
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = True
    gs_assets.copy_mesh_from_static_mesh_v2(unreal.load_asset(BASE), mesh, options, unreal.GeometryScriptMeshReadLOD(),
                                            False)
    log("base copied: %d tris" % mesh.get_triangle_count())
    remap_material_ids(mesh, BASE_SLOTS, 0)
    return mesh


def roof_part():
    mesh = unreal.DynamicMesh()
    gs_assets.copy_mesh_from_skeletal_mesh(unreal.load_asset(ROOF), mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                           unreal.GeometryScriptMeshReadLOD())
    bones = unpack(gs_bw.get_all_bones_info(mesh))
    names = [str(bone.name) for bone in bones]
    anim = unreal.load_asset(ROOF_OPEN)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(anim, 0, unreal.AnimPoseEvaluationOptions())
    deltas = []
    for name in names:
        ref = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
        now = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
        deltas.append((ref, now))
    positions = gs_list.convert_vector_list_to_array(unpack(gs_query.get_all_vertex_positions(mesh, False)))
    bone_of = [gs_bw.get_largest_vertex_bone_weight(mesh, vertex, unreal.GeometryScriptBoneWeightProfile())[1].bone_index
               for vertex in range(len(positions))]
    # Bay 3 of the -Y side is skinned to its hinge (B_SlideBase_003, 226 vertices) instead of its slide, so in the source
    # it tilts but does not slide out. Give its panel the motion of the bay 2 slide, moved over by the bay spacing.
    hinge2, slide2 = names.index("B_SlideBase_002"), names.index("B_Slide_002")
    hinge3, slide3 = names.index("B_SlideBase_003"), names.index("B_Slide_003")
    pivot2, pivot3 = deltas[hinge2][0].translation, deltas[hinge3][0].translation
    bay_offset = pivot3 - pivot2
    hinge_reach = max((positions[v] - pivot2).length() for v in range(len(positions)) if bone_of[v] == hinge2) + 5.0
    moved = 0
    fixed = 0
    new_positions = []
    for vertex, position in enumerate(positions):
        bone = bone_of[vertex]
        if bone == slide3 or (bone == hinge3 and (position - pivot3).length() > hinge_reach):
            ref, now = deltas[slide2]
            posed = now.transform_location(ref.inverse_transform_location(position - bay_offset)) + bay_offset
            fixed += 1
        else:
            ref, now = deltas[bone]
            posed = now.transform_location(ref.inverse_transform_location(position))
        if (posed - position).length() > 0.5:
            moved += 1
        new_positions.append(posed)
    gs_edits.set_all_mesh_vertex_positions(mesh, unpack(gs_list.convert_array_to_vector_list(new_positions)))
    log("roof posed open: %d of %d vertices moved, %d bay 3 panel vertices given the bay 2 slide (hinge reach %.0f)" % (
        moved, len(positions), fixed, hinge_reach))
    recompute_normals(mesh, "roof")
    remap_material_ids(mesh, ROOF_SLOTS, 10)
    return mesh


def recompute_normals(mesh, name):
    """The posed panels still carry their closed-roof normals: rebuild split normals (hard edges above 60 degrees)."""
    try:
        split = unreal.GeometryScriptSplitNormalsOptions()
        split.set_editor_property("split_by_opening_angle", True)
        split.set_editor_property("opening_angle_deg", 60.0)
        unreal.GeometryScript_Normals.compute_split_normals(mesh, split, unreal.GeometryScriptCalculateNormalsOptions())
        log("%s normals rebuilt (split at 60 degrees)" % name)
    except Exception as error:
        log("%s split normals failed (%s), plain recompute" % (name, error))
        unreal.GeometryScript_Normals.recompute_normals(mesh, unreal.GeometryScriptCalculateNormalsOptions())


def building_part():
    mesh = base_part()
    simplify(mesh, BUILDING_TRIS, "base")
    roof = roof_part()
    simplify(roof, ROOF_TRIS, "roof")
    append(mesh, roof)
    gs_xform.translate_mesh(mesh, unreal.Vector(-BUILDING_CENTRE[0], -BUILDING_CENTRE[1], 0.0))
    gs_xform.scale_mesh(mesh, unreal.Vector(BUILDING_SCALE, BUILDING_SCALE, BUILDING_SCALE), unreal.Vector(0, 0, 0), True)
    gs_xform.transform_mesh(mesh, transform((BUILDING_SHIFT[0], BUILDING_SHIFT[1], 0.0), yaw=BUILDING_YAW))
    return mesh


def stuka_mesh(tris, name):
    mesh = unreal.DynamicMesh()
    gs_assets.copy_mesh_from_skeletal_mesh(unreal.load_asset(STUKA), mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                           unreal.GeometryScriptMeshReadLOD())
    set_all_material(mesh, 20 if DEBUG_SOURCE_SLOTS else SLOT_STUKA)
    paint_stuka_accents(mesh)
    simplify(mesh, tris, name)
    return mesh


def stuka_accent(centre):
    """Eastern front yellow (RLM 04) in Ju 87 source space: wing tips, engine cowling, fin and a fuselage band."""
    x, y, z = centre.x, centre.y, centre.z
    if abs(y) > STUKA_TIP_Y:
        return True
    if x > STUKA_COWL_X:
        return True
    if x < STUKA_FIN_X and z > STUKA_FIN_Z and abs(y) < 40.0:
        return True
    return STUKA_BAND_X[0] < x < STUKA_BAND_X[1] and abs(y) < 70.0


def paint_stuka_accents(mesh):
    positions = gs_list.convert_vector_list_to_array(unpack(gs_query.get_all_vertex_positions(mesh, False)))
    triangle_ids = gs_list.convert_index_list_to_array(unpack(gs_query.get_all_triangle_i_ds(mesh)))
    triangles = unpack(gs_query.get_all_triangle_indices(mesh, False))
    triangles = gs_list.convert_triangle_list_to_array(triangles) if hasattr(gs_list, "convert_triangle_list_to_array") \
        else triangles
    accent = []
    for triangle_id, tri in zip(triangle_ids, triangles):
        a, b, c = positions[tri.x], positions[tri.y], positions[tri.z]
        centre = unreal.Vector((a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0, (a.z + b.z + c.z) / 3.0)
        if stuka_accent(centre):
            accent.append(triangle_id)
    set_ids(mesh, accent, 21 if DEBUG_SOURCE_SLOTS else SLOT_ACCENT)
    log("Ju 87 accent triangles: %d of %d -> ids %s" % (len(accent), len(triangle_ids), material_histogram(mesh)))


def parked_part():
    mesh = stuka_mesh(PARKED_TRIS, "parked Ju 87")
    # Landing_0 is the base mesh origin; the wheels sit at z 0 of the Stuka mesh.
    gs_xform.translate_mesh(mesh, unreal.Vector(-STUKA_CENTRE[0], 0.0, 0.0))
    gs_xform.scale_mesh(mesh, unreal.Vector(PARKED_SCALE, PARKED_SCALE, PARKED_SCALE), unreal.Vector(0, 0, 0), True)
    landing = ((0.0 - BUILDING_CENTRE[0]) * BUILDING_SCALE, (0.0 - BUILDING_CENTRE[1]) * BUILDING_SCALE)
    yaw = math.radians(BUILDING_YAW)
    at = (landing[0] * math.cos(yaw) - landing[1] * math.sin(yaw) + BUILDING_SHIFT[0],
          landing[0] * math.sin(yaw) + landing[1] * math.cos(yaw) + BUILDING_SHIFT[1])
    gs_xform.transform_mesh(mesh, transform((at[0], at[1], 0.0), yaw=BUILDING_YAW + PARKED_YAW))
    return mesh


def flyer_part():
    mesh = stuka_mesh(FLYER_TRIS, "flying Ju 87")
    gs_xform.translate_mesh(mesh, unreal.Vector(-STUKA_CENTRE[0], -STUKA_CENTRE[1], -STUKA_CENTRE[2]))
    gs_xform.scale_mesh(mesh, unreal.Vector(FLYER_SCALE, FLYER_SCALE, FLYER_SCALE), unreal.Vector(0, 0, 0), True)
    gs_xform.transform_mesh(mesh, transform(FLYER_AT, yaw=FLYER_YAW, pitch=FLYER_PITCH, roll=FLYER_ROLL))
    return mesh


def stand_part(flyer):
    """Thin dark rod from the plate up into the flyer's belly (a model display stand)."""
    mesh = unreal.DynamicMesh()
    top = FLYER_AT[2] - 1.0
    gs_prims.append_cylinder(mesh, unreal.GeometryScriptPrimitiveOptions(), transform((FLYER_AT[0], FLYER_AT[1], 0.0)),
                             STAND_RADIUS, top, STAND_SIDES, 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    set_all_material(mesh, 30 if DEBUG_SOURCE_SLOTS else SLOT_DARK)
    return mesh


# ---------------------------------------------------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------------------------------------------------

def write_obj(mesh, name):
    positions = gs_list.convert_vector_list_to_array(unpack(gs_query.get_all_vertex_positions(mesh, False)))
    vertex_ids = gs_list.convert_index_list_to_array(unpack(gs_query.get_all_vertex_i_ds(mesh)))
    index_of = {vid: i for i, vid in enumerate(vertex_ids)}
    triangles = unpack(gs_query.get_all_triangle_indices(mesh, False))
    triangles = gs_list.convert_triangle_list_to_array(triangles) if hasattr(gs_list, "convert_triangle_list_to_array") \
        else triangles
    ids = gs_list.convert_index_list_to_array(unpack(gs_mat.get_all_triangle_material_i_ds(mesh)))
    os.makedirs(os.path.join(OUT, "obj"), exist_ok=True)
    with open(os.path.join(OUT, "obj", name + ".obj"), "w") as handle:
        for p in positions:
            handle.write("v %.3f %.3f %.3f\n" % (p.x * 100.0, p.y * 100.0, p.z * 100.0))
        current = None
        for index, tri in enumerate(triangles):
            material = ids[index] if index < len(ids) else 0
            if material != current:
                handle.write("usemtl m%d\n" % material)
                current = material
            handle.write("f %d %d %d\n" % (index_of.get(tri.x, tri.x) + 1, index_of.get(tri.y, tri.y) + 1,
                                           index_of.get(tri.z, tri.z) + 1))


def report(mesh):
    box = gs_query.get_mesh_bounding_box(mesh)
    positions = gs_list.convert_vector_list_to_array(unpack(gs_query.get_all_vertex_positions(mesh, False)))
    radius = max(math.hypot(p.x, p.y) for p in positions)
    log("piece tris %d box (%.2f %.2f %.2f)..(%.2f %.2f %.2f) radius %.2f" % (
        mesh.get_triangle_count(), box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, radius))


def build():
    building = building_part()
    parked = parked_part()
    flyer = flyer_part()
    merged = unreal.DynamicMesh()
    for name, part in (("building", building), ("parked", parked), ("flyer", flyer)):
        append(merged, part)
        box = gs_query.get_mesh_bounding_box(part)
        part_radius = max(math.hypot(p.x, p.y) for p in gs_list.convert_vector_list_to_array(
            unpack(gs_query.get_all_vertex_positions(part, False))))
        log("%s box (%.2f %.2f %.2f)..(%.2f %.2f %.2f) tris %d radius %.2f" % (
            name, box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, part.get_triangle_count(), part_radius))
    if STAND:
        append(merged, stand_part(flyer))
    radius = max(math.hypot(p.x, p.y) for p in gs_list.convert_vector_list_to_array(
        unpack(gs_query.get_all_vertex_positions(merged, False))))
    if radius > MAX_RADIUS:
        fit = MAX_RADIUS / radius
        gs_xform.scale_mesh(merged, unreal.Vector(fit, fit, fit), unreal.Vector(0, 0, 0), True)
        log("scaled the whole piece by %.4f to fit radius %.1f (was %.2f)" % (fit, MAX_RADIUS, radius))
    box = gs_query.get_mesh_bounding_box(merged)
    if abs(box.min.z) > 0.001:
        gs_xform.translate_mesh(merged, unreal.Vector(0.0, 0.0, -box.min.z))
        log("moved by %.3f in Z so the base is at 0" % -box.min.z)
    report(merged)
    log("material ids %s" % material_histogram(merged))
    if PREVIEW:
        write_obj(merged, "piece")
        return
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import build_asset  # noqa: E402  (asset + materials, see build_asset.py)
    build_asset.write(merged, log)


os.makedirs(OUT, exist_ok=True)
try:
    build()
except Exception:
    log(traceback.format_exc())
with open(os.path.join(OUT, "build.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
