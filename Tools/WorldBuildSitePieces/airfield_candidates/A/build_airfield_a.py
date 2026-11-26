"""Builds airfield candidate A ("open hangar"): the German airbase building with its roof posed open and its Ju 87
parked inside, merged into one small static mesh with GeometryScript (headless, no RHI needed).

Run headless:  sh run_headless.sh build_airfield_a
  AIRFIELD_A_MODE=debug  writes SM_AirfieldA_Debug (one material ID per source slot) and exports it to
                         Saved/WorldBuildSitePieces/airfield_candidate_A/debug.fbx for the Blender preview.
  default (final)        writes SM_BuildSitePiece_Airfield_A with the paint materials, and final.fbx.
Log: Saved/WorldBuildSitePieces/airfield_candidate_A/build.txt

Sources (read only):
  SM_Ger_AircraftBuilding_BaseMesh   the airbase building (static part)
  SK_Ger_AircraftBuilding_Roof       the eight roof flaps, posed by Anim_GerAirbase_OpenStationary (bone weights x
                                     AnimPose world transforms; the FBX skeletal exporter asserts under -nullrhi)
  SK_Stuka_VTO                       the Ju 87 VTO this building launches, at the Landing_0 socket
"""
import math
import os
import traceback

import unreal

# AIRFIELD_A_TARGET=site writes the picked piece (2026-11-25) where AWorldBuildNode loads the airfield from.
TARGET = os.environ.get("AIRFIELD_A_TARGET", "candidate")
PIECES = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield"
FOLDER = PIECES if TARGET == "site" else PIECES + "/Candidates/A"
MESH_NAME = "SM_BuildSitePiece_Airfield" if TARGET == "site" else "SM_BuildSitePiece_Airfield_A"
DEBUG_NAME = "SM_AirfieldA_Debug"
MASTER_NAME = "M_AirfieldA_Paint"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_candidate_A")
MODE = os.environ.get("AIRFIELD_A_MODE", "final")

SOURCE = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/BuildingMeshes"
BASE = SOURCE + "/SM_Ger_AircraftBuilding_BaseMesh"
ROOF = SOURCE + "/SK_Ger_AircraftBuilding_Roof"
ROOF_OPEN = SOURCE + "/Anims/Anim_GerAirbase_OpenStationary"
STUKA = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"
STUKA_CAMO = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Materials/Plane_stuka_01_Camo"

# Source material slots (dump.txt). Base: 0 camo concrete, 1 module atlas, 2 PantherII sheets, 3 decals, 4 props.
# Roof: 0 module atlas, 1 PantherII sheets, 2 props, 3 decals.
BASE_DECAL_SLOT = 3
ROOF_DECAL_SLOT = 3
DEBUG_ROOF_OFFSET = 5
DEBUG_STUKA_ID = 9

# Layout, in source units (cm) before the bake scale. The building yaw turns the whole piece about the pivot.
BUILDING_YAW = 0.0
STUKA_SCALE = 1.0              # relative to the building
STUKA_OFFSET = (0.0, 0.0, 0.0)  # added to the Landing_0 socket (0, 0, 0)
STUKA_YAW = 0.0
# B_Slide_003 opens to z -170 (below the floor) while its siblings stop at z 222: pose it like B_Slide_002.
SLIDE_FIX = {"B_Slide_003": ("B_Slide_002", 408.0)}

# Fit: footprint circle of 40 units (radius 20 minus margin), height at most 30.
MAX_RADIUS = 19.3
MAX_HEIGHT = 29.0

LINES = []
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
gs_assets = unreal.GeometryScript_AssetUtils
gs_edits = unreal.GeometryScript_MeshEdits
gs_mat = unreal.GeometryScript_Materials
gs_color = unreal.GeometryScript_VertexColors
gs_query = unreal.GeometryScript_MeshQueries
gs_xform = unreal.GeometryScript_MeshTransforms
gs_select = unreal.GeometryScript_MeshSelection
gs_weights = unreal.GeometryScript_BoneWeights
gs_normals = unreal.GeometryScript_Normals
poses = unreal.AnimPoseExtensions


def log(text):
    LINES.append(str(text))
    unreal.log("AIRFIELD_A " + str(text))


def last(result):
    """GeometryScript python calls return (mesh, value...) tuples; take the value."""
    if isinstance(result, tuple):
        return result[-1] if len(result) == 2 else result[1:]
    return result


def transform(location=(0.0, 0.0, 0.0), yaw=0.0, pitch=0.0, roll=0.0, scale=(1.0, 1.0, 1.0)):
    return unreal.Transform(unreal.Vector(*location), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                            unreal.Vector(*scale))


def box_text(mesh):
    box = gs_query.get_mesh_bounding_box(mesh)
    return "(%.0f %.0f %.0f)-(%.0f %.0f %.0f)" % (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)


def vertex_position(mesh, vertex):
    result = gs_query.get_vertex_position(mesh, vertex)
    items = result if isinstance(result, tuple) else (result,)
    vector = next((item for item in items if isinstance(item, unreal.Vector)), None)
    valid = next((item for item in items if isinstance(item, bool)), True)
    return vector if valid else None


def all_positions(mesh):
    return [p for p in (vertex_position(mesh, v) for v in range(mesh.get_vertex_count())) if p is not None]


def append(target, part):
    gs_edits.append_mesh(target, part, transform(), False, unreal.GeometryScriptAppendMeshOptions())


def select_material(mesh, material_id):
    return last(gs_select.select_mesh_elements_by_material_id(
        mesh, material_id, unreal.GeometryScriptMeshSelectionType.TRIANGLES))


def delete_material(mesh, material_id):
    before = mesh.get_triangle_count()
    gs_mat.delete_triangles_by_material_id(mesh, material_id)
    log("  deleted material %d: %d triangles" % (material_id, before - mesh.get_triangle_count()))


def remap_material_ids(mesh, mapping):
    """mapping: {old id: new id}. Done through a temporary high range so chains do not collide."""
    for old, new in mapping.items():
        gs_mat.remap_material_i_ds(mesh, old, 100 + new)
    for new in set(mapping.values()):
        gs_mat.remap_material_i_ds(mesh, 100 + new, new)


def material_report(mesh, label):
    counts = []
    for material_id in range(0, 12):
        selection = select_material(mesh, material_id)
        info = gs_select.get_mesh_selection_info(selection)
        count = info[1] if isinstance(info, tuple) else info
        if count:
            counts.append("%d:%s" % (material_id, count))
    log("  %s material triangles %s" % (label, ", ".join(counts)))


# ---------------------------------------------------------------------------------------------------------------------
# Parts
# ---------------------------------------------------------------------------------------------------------------------

def base_part():
    mesh = unreal.DynamicMesh()
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    options.apply_build_settings = True
    gs_assets.copy_mesh_from_static_mesh_v2(unreal.load_asset(BASE), mesh, options,
                                            unreal.GeometryScriptMeshReadLOD(), False)
    log("base: %d tris box %s" % (mesh.get_triangle_count(), box_text(mesh)))
    material_report(mesh, "base")
    delete_material(mesh, BASE_DECAL_SLOT)
    return mesh


def bone_matrices(skeletal_mesh):
    """Bone name -> skinning transform (reference world inverse, then open-pose world)."""
    reference = poses.get_reference_pose(skeletal_mesh.skeleton)
    opened = poses.get_anim_pose_at_frame(unreal.load_asset(ROOF_OPEN), 0, unreal.AnimPoseEvaluationOptions())
    world = unreal.AnimPoseSpaces.WORLD
    result = {}
    for bone in poses.get_bone_names(reference):
        name = str(bone)
        ref = poses.get_ref_bone_pose(reference, bone, world)
        pose = poses.get_bone_pose(opened, bone, world)
        if name in SLIDE_FIX:
            sibling, x_offset = SLIDE_FIX[name]
            pose = poses.get_bone_pose(opened, unreal.Name(sibling), world)
            pose = unreal.Transform(pose.translation + unreal.Vector(x_offset, 0.0, 0.0), pose.rotation.rotator(),
                                    pose.scale3d)
            sibling_ref = poses.get_ref_bone_pose(reference, unreal.Name(sibling), world)
            ref = unreal.Transform(sibling_ref.translation + unreal.Vector(x_offset, 0.0, 0.0),
                                   sibling_ref.rotation.rotator(), sibling_ref.scale3d)
        matrix = ref.inverse().multiply(pose)
        check = matrix.transform_location(ref.translation)
        log("  bone %s ref (%.0f %.0f %.0f) -> (%.0f %.0f %.0f), pose (%.0f %.0f %.0f)" % (
            name, ref.translation.x, ref.translation.y, ref.translation.z, check.x, check.y, check.z,
            pose.translation.x, pose.translation.y, pose.translation.z))
        result[name] = matrix
    return result


def roof_part():
    skeletal_mesh = unreal.load_asset(ROOF)
    mesh = unreal.DynamicMesh()
    gs_assets.copy_mesh_from_skeletal_mesh(skeletal_mesh, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                           unreal.GeometryScriptMeshReadLOD())
    log("roof: %d tris box %s" % (mesh.get_triangle_count(), box_text(mesh)))
    material_report(mesh, "roof")
    matrices = bone_matrices(skeletal_mesh)
    bones = last(gs_weights.get_all_bones_info(mesh))
    index_to_name = {}
    for position, info in enumerate(bones):
        index = info.get_editor_property("index") if hasattr(info, "index") else position
        index_to_name[index] = str(info.get_editor_property("name"))
    log("  mesh bones %s" % sorted(index_to_name.items()))
    profile = unreal.GeometryScriptBoneWeightProfile()
    moved = 0
    for vertex in range(mesh.get_vertex_count()):
        result = gs_weights.get_vertex_bone_weights(mesh, vertex, profile)
        weights = result[1]
        original = vertex_position(mesh, vertex)
        if original is None:
            continue
        total = 0.0
        accumulated = unreal.Vector(0.0, 0.0, 0.0)
        for entry in weights:
            matrix = matrices.get(index_to_name.get(entry.bone_index, ""), None)
            if matrix is None or entry.weight <= 0.0:
                continue
            accumulated = accumulated + matrix.transform_location(original) * entry.weight
            total += entry.weight
        if total <= 0.0:
            continue
        new_position = accumulated * (1.0 / total)
        if (new_position - original).length() > 0.01:
            moved += 1
        gs_edits.set_vertex_position(mesh, vertex, new_position, True)
    gs_normals.recompute_normals(mesh, unreal.GeometryScriptCalculateNormalsOptions(), False)
    log("  posed open: %d of %d verts moved, box %s" % (moved, mesh.get_vertex_count(), box_text(mesh)))
    delete_material(mesh, ROOF_DECAL_SLOT)
    return mesh


def stuka_part():
    mesh = unreal.DynamicMesh()
    gs_assets.copy_mesh_from_skeletal_mesh(unreal.load_asset(STUKA), mesh,
                                           unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                           unreal.GeometryScriptMeshReadLOD())
    log("stuka: %d tris box %s" % (mesh.get_triangle_count(), box_text(mesh)))
    gs_xform.scale_mesh(mesh, unreal.Vector(STUKA_SCALE, STUKA_SCALE, STUKA_SCALE), unreal.Vector(0, 0, 0), True)
    gs_xform.transform_mesh(mesh, transform(STUKA_OFFSET, yaw=STUKA_YAW))
    return mesh


# The bay floor is spread over the source's module and prop slots, so pick it by shape: faces that look up, low down.
FLOOR_MAX_Z = 60.0
FLOOR_MAX_ANGLE = 25.0


def paint_floor(mesh, floor_z, material_id):
    triangles = unreal.GeometryScriptMeshSelectionType.TRIANGLES
    low = unreal.Box(unreal.Vector(-100000.0, -100000.0, floor_z - 5.0), unreal.Vector(100000.0, 100000.0, FLOOR_MAX_Z))
    in_box = last(gs_select.select_mesh_elements_in_box(mesh, low, triangles, False, 3))
    facing_up = last(gs_select.select_mesh_elements_by_normal_angle(
        mesh, unreal.Vector(0.0, 0.0, 1.0), FLOOR_MAX_ANGLE, triangles, False, 3))
    floor = last(gs_select.combine_mesh_selections(in_box, facing_up,
                                                   unreal.GeometryScriptCombineSelectionMode.INTERSECTION))
    gs_mat.set_material_id_for_mesh_selection(mesh, floor, material_id, False)
    material_report(mesh, "base after floor paint")


def set_all_material_id(mesh, material_id):
    gs_mat.enable_material_i_ds(mesh)
    selection = last(gs_select.create_select_all_mesh_selection(mesh, unreal.GeometryScriptMeshSelectionType.TRIANGLES))
    gs_mat.set_material_id_for_mesh_selection(mesh, selection, material_id, False)


# ---------------------------------------------------------------------------------------------------------------------
# Asset
# ---------------------------------------------------------------------------------------------------------------------

def write_static_mesh(dynamic_mesh, name, slot_materials, slot_names):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        static_mesh = unreal.load_asset(path)
        options = unreal.GeometryScriptCopyMeshToAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.replace_materials = True
        options.new_materials = slot_materials
        options.new_material_slot_names = slot_names
        options.emit_transaction = False
        options.defer_mesh_post_edit_change = False
        gs_assets.copy_mesh_to_static_mesh(dynamic_mesh, static_mesh, options, unreal.GeometryScriptMeshWriteLOD(),
                                           False)
        log("updated existing " + path)
    else:
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.enable_nanite = False
        options.enable_collision = False
        options.collision_mode = unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX
        static_mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            dynamic_mesh, path, options)
        log("created %s: %s" % (path, outcome))
        static_mesh = unreal.load_asset(path)
    static_mesh.set_editor_property("static_materials", [
        unreal.StaticMaterial(material_interface=material, material_slot_name=slot_names[index])
        for index, material in enumerate(slot_materials)])
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
    if not assets.save_loaded_asset(static_mesh, False):
        log("SAVE FAILED " + path)
    return static_mesh


def export_fbx(static_mesh, file_name):
    task = unreal.AssetExportTask()
    task.object = static_mesh
    task.filename = os.path.join(OUT, file_name)
    task.exporter = unreal.StaticMeshExporterFBX()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    options.vertex_color = True
    task.options = options
    log("export %s ok=%s" % (file_name, unreal.Exporter.run_asset_export_task(task)))


def fit_and_centre(merged, floor_z):
    """Pivot at the footprint centre, base at Z 0, then the bake scale that fits the circle and the height."""
    gs_xform.translate_mesh(merged, unreal.Vector(0.0, 0.0, -floor_z))
    positions = all_positions(merged)
    xs = [p.x for p in positions]
    ys = [p.y for p in positions]
    centre = unreal.Vector((min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5, 0.0)
    gs_xform.translate_mesh(merged, unreal.Vector(-centre.x, -centre.y, 0.0))
    radius = max(math.hypot(p.x - centre.x, p.y - centre.y) for p in positions)
    height = max(p.z for p in positions)
    scale = min(MAX_RADIUS / radius, MAX_HEIGHT / height)
    gs_xform.scale_mesh(merged, unreal.Vector(scale, scale, scale), unreal.Vector(0, 0, 0), True)
    log("fit: footprint centre (%.0f %.0f), source radius %.0f height %.0f -> scale %.5f" % (
        centre.x, centre.y, radius, height, scale))
    return scale, centre


def build_debug():
    base = base_part()
    roof = roof_part()
    remap_material_ids(roof, {old: old + DEBUG_ROOF_OFFSET for old in range(4)})
    stuka = stuka_part()
    set_all_material_id(stuka, DEBUG_STUKA_ID)
    floor_z = gs_query.get_mesh_bounding_box(base).min.z
    merged = unreal.DynamicMesh()
    for part in (base, roof, stuka):
        append(merged, part)
    gs_xform.rotate_mesh(merged, unreal.Rotator(roll=0.0, pitch=0.0, yaw=BUILDING_YAW), unreal.Vector(0, 0, 0))
    fit_and_centre(merged, floor_z)
    material_report(merged, "merged")
    log("merged %d tris box %s" % (merged.get_triangle_count(), box_text(merged)))
    grid = unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial")
    names = ["B_Concrete", "B_Module", "B_Sheets", "B_Decals", "B_Props", "R_Module", "R_Sheets", "R_Props",
             "R_Decals", "Stuka"]
    static_mesh = write_static_mesh(merged, DEBUG_NAME, [grid] * len(names), names)
    export_fbx(static_mesh, "debug.fbx")


# ---------------------------------------------------------------------------------------------------------------------
# Final piece: paint slots and materials
# ---------------------------------------------------------------------------------------------------------------------

# Final slots. Body: walls, bunker, control tower (Dunkelgelb with olive / red-brown patches, like the watch post).
# Detail: doors, windows, pipes, flap frames (darker). Floor: hangar floor (dark concrete so the aircraft stands out).
# Flaps: the opened roof flaps (cream, the campaign's light accent, frames the open bay). Stuka: the Ju 87's own camo
# texture, brightened. Accent: Eastern Front yellow (RLM 04) on wing tips, cowling and fuselage band.
SLOT_BODY, SLOT_DETAIL, SLOT_FLOOR, SLOT_FLAPS, SLOT_STUKA = range(5)
SLOT_NAMES = ["Body", "Detail", "Floor", "Flaps", "Stuka"]
BASE_TO_SLOT = {0: SLOT_FLOOR, 1: SLOT_BODY, 2: SLOT_DETAIL, 4: SLOT_DETAIL}
ROOF_TO_SLOT = {0: SLOT_FLAPS, 1: SLOT_FLAPS, 2: SLOT_DETAIL}
FINAL_BUILDING_YAW = 180.0     # open bay end (+X in the source) to the camera side (-X), bunker and tower behind
FINAL_STUKA_SCALE = 1.3        # over true scale so the aircraft carries the read at map distance
STUKA_BOX_CENTRE_X = -157.0
# Accent (Eastern Front yellow, RLM 04) on the Ju 87, painted per pixel in object space by the Stuka instance (the wing
# skin has long triangles, so a triangle selection misses the tips). Source cm: wing tips beyond |y| 430, cowling and
# spinner forward of x 215, fuselage band x -440..-380 within |y| 100.
ACCENT_TIP_Y = 430.0
ACCENT_NOSE_X = 215.0
ACCENT_BAND_X = (-440.0, -380.0)
ACCENT_BAND_HALF_Y = 100.0
ACCENT_COLOR = (1.0, 0.68, 0.02)
SIMPLIFY = {"base": 5600, "roof": 2300, "stuka": 4300}

EMISSIVE = 0.8
# name, slot, texture?, base colour (linear), scalars
INSTANCES = [
    ("MI_AirfieldA_Body", SLOT_BODY, None, (0.68, 0.46, 0.165), {"CamoAmount": 1.0, "EmissiveStrength": EMISSIVE}),
    ("MI_AirfieldA_Detail", SLOT_DETAIL, None, (0.36, 0.25, 0.10), {"CamoAmount": 0.0, "EmissiveStrength": EMISSIVE}),
    # Floor and aircraft: render 1 (camo texture x 1.35 on a 0.075 floor) showed the Ju 87 as dark as the floor in
    # the campaign light, only the yellow read. Now a near-black floor and a flat pale aircraft for a clear silhouette.
    ("MI_AirfieldA_Floor", SLOT_FLOOR, None, (0.025, 0.026, 0.028), {"CamoAmount": 0.0, "BottomShade": 1.0,
                                                                     "Roughness": 0.9, "EmissiveStrength": 0.2}),
    ("MI_AirfieldA_Flaps", SLOT_FLAPS, None, (0.87, 0.77, 0.55), {"CamoAmount": 0.0, "BottomShade": 0.85,
                                                                  "Roughness": 0.55, "EmissiveStrength": EMISSIVE}),
    ("MI_AirfieldA_Stuka", SLOT_STUKA, None, (0.78, 0.80, 0.76), {"CamoAmount": 0.0, "BottomShade": 0.8,
                                                                 "Roughness": 0.5, "EmissiveStrength": 1.0}),
]

PAINT_CODE = """// Texture x colour, then German three-tone camo patches in object units (3D value noise, no UVs), height shade.
struct AFACamo
{
    float hash(float3 p)
    {
        p=frac(p*0.3183099+0.1);
        p*=17.0;
        return frac(p.x*p.y*p.z*(p.x+p.y+p.z));
    }
    float noise(float3 x)
    {
        float3 i=floor(x);
        float3 f=frac(x);
        f=f*f*(3.0-2.0*f);
        float a=lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y);
        float b=lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y);
        return lerp(a,b,f.z);
    }
    float fbm(float3 x)
    {
        return noise(x)*0.62+noise(x*2.03+11.7)*0.28+noise(x*4.11+3.1)*0.10;
    }
};
AFACamo c;
float3 q=P/max(Scale,0.01)+Seed;
q.z*=1.5;
float e=0.03;
float green=smoothstep(GreenCut-e,GreenCut+e,c.fbm(q));
float brown=smoothstep(BrownCut-e,BrownCut+e,c.fbm(q+float3(37.1,-12.4,5.3)))*(1.0-green);
float3 col=Tex*Base;
col=lerp(col,A,green*Amount);
col=lerp(col,B,brown*Amount);
// Accent (object space): wing tips, nose, fuselage band. Off unless AccentOn = 1 (Stuka instance only).
float tip=step(TipY,abs(P.y));
float nose=step(P.x,NoseX);
float band=step(BandX0,P.x)*step(P.x,BandX1)*step(abs(P.y),BandHalfY);
col=lerp(col,AccentColor,saturate(tip+nose+band)*AccentOn);
float shade=lerp(Bottom,1.0,saturate(P.z/max(Top,0.01)));
return col*shade;"""

MASTER_SCALARS = {"CamoAmount": 0.0, "CamoScale": 3.0, "GreenCut": 0.53, "BrownCut": 0.56, "Seed": 3.0,
                  "BottomShade": 0.7, "ShadeTop": 12.0, "Roughness": 0.7, "EmissiveStrength": 0.8,
                  "AccentOn": 0.0, "AccentTipY": 1000.0, "AccentNoseX": -1000.0, "AccentBandX0": 1000.0,
                  "AccentBandX1": -1000.0, "AccentBandHalfY": 0.0}
MASTER_VECTORS = {"BaseColor": (1.0, 1.0, 1.0), "CamoColorA": (0.117, 0.15, 0.034),
                  "CamoColorB": (0.216, 0.058, 0.023), "AccentColor": ACCENT_COLOR}
materials = unreal.MaterialEditingLibrary


def material_node(material, node_class, x, y, **properties):
    expression = materials.create_material_expression(material, node_class, x, y)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def connect(source, output, target, target_input):
    if not materials.connect_material_expressions(source, output, target, target_input):
        log("CONNECT FAILED %s.%s -> %s.%s" % (source.get_name(), output, target.get_name(), target_input))


def build_master():
    path = FOLDER + "/" + MASTER_NAME
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = asset_tools.create_asset(MASTER_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    scalars = {name: material_node(material, unreal.MaterialExpressionScalarParameter, -1400, -600 + index * 110,
                                   parameter_name=name, default_value=value)
               for index, (name, value) in enumerate(MASTER_SCALARS.items())}
    vectors = {name: material_node(material, unreal.MaterialExpressionVectorParameter, -1400, -1100 + index * 160,
                                   parameter_name=name,
                                   default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))
               for index, (name, value) in enumerate(MASTER_VECTORS.items())}
    texture = material_node(material, unreal.MaterialExpressionTextureSampleParameter2D, -1400, -1500,
                            parameter_name="BaseTexture",
                            texture=unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"),
                            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    world_position = material_node(material, unreal.MaterialExpressionWorldPosition, -1400, -1800)
    local_position = material_node(material, unreal.MaterialExpressionTransformPosition, -1150, -1800,
                                   transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                                   transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    connect(world_position, "", local_position, "")
    pins = [("P", local_position, ""), ("Tex", texture, "RGB"), ("Base", vectors["BaseColor"], ""),
            ("A", vectors["CamoColorA"], ""), ("B", vectors["CamoColorB"], ""), ("Scale", scalars["CamoScale"], ""),
            ("Amount", scalars["CamoAmount"], ""), ("GreenCut", scalars["GreenCut"], ""),
            ("BrownCut", scalars["BrownCut"], ""), ("Seed", scalars["Seed"], ""),
            ("Bottom", scalars["BottomShade"], ""), ("Top", scalars["ShadeTop"], ""),
            ("AccentColor", vectors["AccentColor"], ""), ("AccentOn", scalars["AccentOn"], ""),
            ("TipY", scalars["AccentTipY"], ""), ("NoseX", scalars["AccentNoseX"], ""),
            ("BandX0", scalars["AccentBandX0"], ""), ("BandX1", scalars["AccentBandX1"], ""),
            ("BandHalfY", scalars["AccentBandHalfY"], "")]
    inputs = []
    for name, _source, _output in pins:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    paint = material_node(material, unreal.MaterialExpressionCustom, -800, -800, description="AFA_Paint",
                          code=PAINT_CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=inputs)
    for name, source, output in pins:
        connect(source, output, paint, name)
    materials.connect_material_property(paint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emissive = material_node(material, unreal.MaterialExpressionMultiply, -500, -500)
    connect(paint, "", emissive, "A")
    connect(scalars["EmissiveStrength"], "", emissive, "B")
    materials.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    materials.connect_material_property(scalars["Roughness"], "", unreal.MaterialProperty.MP_ROUGHNESS)
    material.set_editor_property("used_with_static_lighting", False)
    materials.recompile_material(material)
    assets.save_loaded_asset(material, False)
    log("master built with %d expressions" % materials.get_num_material_expressions(material))
    return material


def stuka_texture():
    path = FOLDER + "/T_AirfieldA_StukaCamo"
    if not assets.does_asset_exist(path):
        if assets.duplicate_asset(STUKA_CAMO, path) is None:
            log("DUPLICATE FAILED " + STUKA_CAMO)
            return None
    texture = unreal.load_asset(path)
    texture.set_editor_property("max_texture_size", 512)
    assets.save_loaded_asset(texture, False)
    return texture


def make_instances(master, extra_scalars):
    textures = {}
    old_texture = FOLDER + "/T_AirfieldA_StukaCamo"
    if assets.does_asset_exist(old_texture):
        instance_path = FOLDER + "/MI_AirfieldA_Stuka"
        if assets.does_asset_exist(instance_path):
            # Release the old texture reference before deleting it.
            materials.set_material_instance_texture_parameter_value(
                unreal.load_asset(instance_path), "BaseTexture",
                unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
            assets.save_loaded_asset(unreal.load_asset(instance_path), False)
        log("deleted unused %s: %s" % (old_texture, assets.delete_asset(old_texture)))
    result = [None] * len(SLOT_NAMES)
    for name, slot, texture_key, color, scalars in INSTANCES:
        scalars = dict(scalars, **extra_scalars.get(name, {}))
        path = FOLDER + "/" + name
        instance = unreal.load_asset(path) if assets.does_asset_exist(path) else asset_tools.create_asset(
            name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        materials.set_material_instance_parent(instance, master)
        if texture_key and textures.get(texture_key):
            materials.set_material_instance_texture_parameter_value(instance, "BaseTexture", textures[texture_key])
        materials.set_material_instance_vector_parameter_value(instance, "BaseColor",
                                                               unreal.LinearColor(color[0], color[1], color[2], 1.0))
        for parameter, value in scalars.items():
            materials.set_material_instance_scalar_parameter_value(instance, parameter, value)
        materials.update_material_instance(instance)
        assets.save_loaded_asset(instance, False)
        result[slot] = instance
    return result


def simplify(mesh, label, target, keep_seams):
    before = mesh.get_triangle_count()
    options = unreal.GeometryScriptSimplifyMeshOptions()
    try:
        options.allow_seam_collapse = not keep_seams
    except Exception as error:
        log("  simplify option: %s" % error)
    unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(mesh, target, options)
    log("  %s simplified %d -> %d" % (label, before, mesh.get_triangle_count()))


def hard_normals(mesh):
    try:
        split = unreal.GeometryScriptSplitNormalsOptions()
        split.split_by_opening_angle = True
        split.opening_angle_deg = 40.0
        gs_normals.compute_split_normals(mesh, split, unreal.GeometryScriptCalculateNormalsOptions())
    except Exception as error:
        log("  split normals failed: %s" % error)


def accent_parameters(scale, centre):
    """Ju 87 source cm -> piece object space (Stuka scale about its box centre, building yaw 180, centre, bake scale)."""
    def piece_x(source_x):
        seated = STUKA_BOX_CENTRE_X + FINAL_STUKA_SCALE * (source_x - STUKA_BOX_CENTRE_X)
        return (-seated - centre.x) * scale
    values = {"AccentOn": 1.0,
              "AccentTipY": ACCENT_TIP_Y * FINAL_STUKA_SCALE * scale,
              "AccentNoseX": piece_x(ACCENT_NOSE_X),
              "AccentBandX0": piece_x(ACCENT_BAND_X[1]),
              "AccentBandX1": piece_x(ACCENT_BAND_X[0]),
              "AccentBandHalfY": ACCENT_BAND_HALF_Y * FINAL_STUKA_SCALE * scale}
    log("accent (centre y %.1f must be ~0): %s" % (centre.y, {k: round(v, 3) for k, v in values.items()}))
    return {"MI_AirfieldA_Stuka": values}


def build_final():
    global STUKA_SCALE
    master = build_master()
    stale = FOLDER + "/MI_AirfieldA_Accent"
    if assets.does_asset_exist(stale):
        log("deleted stale %s: %s" % (stale, assets.delete_asset(stale)))

    base = base_part()
    floor_z = gs_query.get_mesh_bounding_box(base).min.z
    remap_material_ids(base, BASE_TO_SLOT)
    paint_floor(base, floor_z, SLOT_FLOOR)
    simplify(base, "base", SIMPLIFY["base"], keep_seams=False)
    hard_normals(base)

    roof = roof_part()
    remap_material_ids(roof, ROOF_TO_SLOT)
    simplify(roof, "roof", SIMPLIFY["roof"], keep_seams=False)
    hard_normals(roof)

    STUKA_SCALE = 1.0
    stuka = stuka_part()
    set_all_material_id(stuka, SLOT_STUKA)
    simplify(stuka, "stuka", SIMPLIFY["stuka"], keep_seams=True)
    # Same seat as the game (Landing_0 at the origin), scaled about the aircraft's own box centre on the floor.
    gs_xform.scale_mesh(stuka, unreal.Vector(FINAL_STUKA_SCALE, FINAL_STUKA_SCALE, FINAL_STUKA_SCALE),
                        unreal.Vector(STUKA_BOX_CENTRE_X, 0.0, 0.0), True)
    log("stuka placed box %s" % box_text(stuka))

    merged = unreal.DynamicMesh()
    for part in (base, roof, stuka):
        append(merged, part)
    gs_xform.rotate_mesh(merged, unreal.Rotator(roll=0.0, pitch=0.0, yaw=FINAL_BUILDING_YAW), unreal.Vector(0, 0, 0))
    scale, centre = fit_and_centre(merged, floor_z)
    slot_materials = make_instances(master, accent_parameters(scale, centre))
    material_report(merged, "final")
    positions = all_positions(merged)
    log("FINAL tris %d, z %.2f..%.2f, max radius %.2f, box %s" % (
        merged.get_triangle_count(), min(p.z for p in positions), max(p.z for p in positions),
        max(math.hypot(p.x, p.y) for p in positions), box_text(merged)))
    static_mesh = write_static_mesh(merged, MESH_NAME, slot_materials, SLOT_NAMES)
    bounds = static_mesh.get_bounding_box()
    log("ASSET %s tris=%d lods=%d nanite=%s slots=%d box (%.2f %.2f %.2f)-(%.2f %.2f %.2f)" % (
        static_mesh.get_path_name(), static_mesh.get_num_triangles(0), static_mesh.get_num_lods(),
        static_mesh.get_editor_property("nanite_settings").enabled, len(static_mesh.static_materials),
        bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z))
    body = static_mesh.get_editor_property("body_setup")
    if body is not None:
        geometry = body.get_editor_property("agg_geom")
        shapes = sum(len(geometry.get_editor_property(kind)) for kind in
                     ("sphere_elems", "box_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems"))
        log("  collision shapes %d, trace %s" % (shapes, body.get_editor_property("collision_trace_flag")))
    debug_path = FOLDER + "/" + DEBUG_NAME
    if assets.does_asset_exist(debug_path):
        log("deleted scratch %s: %s" % (DEBUG_NAME, assets.delete_asset(debug_path)))
    export_fbx(static_mesh, "final.fbx")
    for path in sorted(assets.list_assets(FOLDER, recursive=True, include_folder=False)):
        log("  asset " + path)


os.makedirs(OUT, exist_ok=True)
try:
    if not assets.does_directory_exist(FOLDER):
        assets.make_directory(FOLDER)
    if MODE == "debug":
        build_debug()
    else:
        build_final()
except Exception:
    log(traceback.format_exc())
with open(os.path.join(OUT, "build.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
